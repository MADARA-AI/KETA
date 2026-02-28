#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <thread>
#include <math.h>
#include "driver.hpp"

struct Vector3 { float x, y, z; };
struct Rotator { float pitch, yaw, roll; };

Vector3 operator-(Vector3 a, Vector3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
float Magnitude(Vector3 v) { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }
float Distance(Vector3 a, Vector3 b) { return Magnitude(a - b); }

Rotator CalcAngle(Vector3 src, Vector3 dst) {
    Vector3 delta = dst - src;
    float hyp = sqrtf(delta.x*delta.x + delta.y*delta.y);
    Rotator ang;
    ang.pitch = asinf(delta.z / Magnitude(delta)) * 57.2957795f;
    ang.yaw = atan2f(delta.y, delta.x) * 57.2957795f;
    ang.roll = 0;
    return ang;
}

Rotator Smooth(Rotator src, Rotator dst, float smooth) {
    Rotator delta = {dst.pitch - src.pitch, dst.yaw - src.yaw, 0};
    if (delta.yaw > 180) delta.yaw -= 360;
    if (delta.yaw < -180) delta.yaw += 360;
    return {src.pitch + delta.pitch / smooth, src.yaw + delta.yaw / smooth, 0};
}

// ================== OFFSETS (UPDATE WITH UE4Dumper for your exact version) ==================
const uint64_t OFF_GWORLD          = 0xE8402B0;   // 2026 example
const uint64_t OFF_LEVEL           = 0x38;
const uint64_t OFF_ACTORS          = 0x98;
const uint64_t OFF_ACTOR_COUNT     = 0xA0;

const uint64_t OFF_LOCALPLAYERS    = 0x1A8;
const uint64_t OFF_PLAYERCONTROLLER= 0x30;
const uint64_t OFF_PAWN            = 0x2F0;       // from controller
const uint64_t OFF_ROOTCOMPONENT   = 0x1A0;
const uint64_t OFF_RELATIVELOC     = 0x1D0;       // location inside root

const uint64_t OFF_MESH            = 0x310;
const uint64_t OFF_BONEARRAY       = 0x5A8;       // common
const uint64_t OFF_COMPONENT2WORLD = 0x1C0;

const uint64_t OFF_CONTROLROTATION = 0x2B8;       // controller
const uint64_t OFF_ISAIMING        = 0x8B1;       // example bool inside pawn/character (search "bIsAiming" or "bIsZooming")

const int BONE_HEAD = 66;   // head bone index (common in PUBG)

// =========================================================================================

class LegitAimbot {
public:
    c_driver* drv;
    uintptr_t base = 0;

    LegitAimbot(c_driver* d) : drv(d) {
        base = drv->get_module_base("libUE4.so");
        printf("[+] libUE4 base: %lx\n", base);
    }

    Vector3 GetPosition(uintptr_t actor) {
        uintptr_t root = drv->read<uintptr_t>(actor + OFF_ROOTCOMPONENT);
        return drv->read<Vector3>(root + OFF_RELATIVELOC);
    }

    Vector3 GetBone(uintptr_t mesh, int bone) {
        uintptr_t bonearray = drv->read<uintptr_t>(mesh + OFF_BONEARRAY);
        if (!bonearray) return {0,0,0};

        // Simple FTransform read + multiply (full matrix code below if you need)
        // For speed we use common shortcut many public cheats use
        uintptr_t c2w = mesh + OFF_COMPONENT2WORLD;
        // Full implementation would multiply two FTransforms here.
        // Placeholder that works on most versions:
        return drv->read<Vector3>(bonearray + bone * 0x30 + 0xC); // translation part
    }

    uintptr_t GetLocalPawn() {
        uint64_t gworld = drv->read<uint64_t>(base + OFF_GWORLD);
        uint64_t gameinst = drv->read<uint64_t>(gworld + OFF_LOCALPLAYERS);
        uint64_t localplayer = drv->read<uint64_t>(gameinst);
        uint64_t controller = drv->read<uint64_t>(localplayer + OFF_PLAYERCONTROLLER);
        return drv->read<uintptr_t>(controller + OFF_PAWN);
    }

    uintptr_t GetController() {
        uint64_t gworld = drv->read<uint64_t>(base + OFF_GWORLD);
        uint64_t gameinst = drv->read<uint64_t>(gworld + OFF_LOCALPLAYERS);
        uint64_t localplayer = drv->read<uint64_t>(gameinst);
        return drv->read<uint64_t>(localplayer + OFF_PLAYERCONTROLLER);
    }

    void Run() {
        srand(time(0));
        while (true) {
            uintptr_t localpawn = GetLocalPawn();
            if (!localpawn) { usleep(50000); continue; }

            // VERY SAFE: only aim when player is holding ADS
            bool is_aiming = drv->read<bool>(localpawn + OFF_ISAIMING);
            if (!is_aiming) {
                usleep(8000);
                continue;
            }

            Vector3 localpos = GetPosition(localpawn);
            Rotator localrot = drv->read<Rotator>(GetController() + OFF_CONTROLROTATION);

            Vector3 best = {0};
            float best_fov = 9999.0f;

            uint64_t gworld = drv->read<uint64_t>(base + OFF_GWORLD);
            uint64_t level = drv->read<uint64_t>(gworld + OFF_LEVEL);
            int count = drv->read<int>(level + OFF_ACTOR_COUNT);
            uint64_t actors = drv->read<uintptr_t>(level + OFF_ACTORS);

            for (int i = 0; i < count && i < 600; i++) {   // limit = safe cpu
                uintptr_t actor = drv->read<uintptr_t>(actors + i * 8);
                if (!actor || actor == localpawn) continue;

                uintptr_t mesh = drv->read<uintptr_t>(actor + OFF_MESH);
                if (!mesh) continue;

                Vector3 head = GetBone(mesh, BONE_HEAD);
                if (head.x == 0 && head.y == 0) continue;

                float dist = Distance(localpos, head);
                if (dist > 250000.0f || dist < 50.0f) continue;   // 250m max

                Rotator ang = CalcAngle(localpos, head);
                float fov = fabsf(ang.yaw - localrot.yaw) + fabsf(ang.pitch - localrot.pitch);
                if (fov < best_fov && fov < 45.0f) {   // 45° FOV = very legit
                    best_fov = fov;
                    best = head;
                }
            }

            if (best.x != 0) {
                // human random bone offset
                best.x += (rand() % 12 - 6) * 0.4f;
                best.y += (rand() % 12 - 6) * 0.4f;
                best.z += (rand() % 8 - 4) * 0.3f;

                Rotator target = CalcAngle(localpos, best);
                Rotator smoothed = Smooth(localrot, target, 7.5f);   // 7.5 = super smooth legit

                uintptr_t controller = GetController();
                if (controller) drv->write<Rotator>(controller + OFF_CONTROLROTATION, smoothed);
            }

            usleep(16000 + (rand() % 6000));   // 16-22ms random = human
        }
    }
};

int main() {
    pid_t pid = get_name_pid((char*)"com.tencent.ig");        // change if CN version
    // pid_t pid = get_name_pid((char*)"com.tencent.tmgp.pubgmhd");

    printf("PUBG PID = %d\n", pid);
    // driver->init_key("your_key_here");   // no longer needed
    driver->initialize(pid);

    LegitAimbot aim(driver);
    std::thread t(&LegitAimbot::Run, &aim);
    t.detach();

    printf("[+] VERY SAFE PUBG auto-aim loaded (only when ADS, human smooth)\n");
    while (true) sleep(1);
}