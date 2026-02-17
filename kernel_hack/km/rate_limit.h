#ifndef _RATE_LIMIT_H_
#define _RATE_LIMIT_H_

#include <linux/jiffies.h>

struct rate_limiter {
    unsigned long last_request;
    unsigned int requests_in_window;
    unsigned int max_requests;  
    unsigned long window_jiffies;
};

static inline void rate_limiter_init(struct rate_limiter *rl, 
                                     unsigned int max_req_per_sec) {
    rl->last_request = 0;
    rl->requests_in_window = 0;
    rl->max_requests = max_req_per_sec;
    rl->window_jiffies = HZ;  // 1 second
}

static inline int rate_limit_check(struct rate_limiter *rl) {
    unsigned long now = jiffies;
    
    if (time_after(now, rl->last_request + rl->window_jiffies)) {
        rl->requests_in_window = 0;
        rl->last_request = now;
    }
    
    if (rl->requests_in_window >= rl->max_requests) {
        return -EBUSY;
    }
    
    rl->requests_in_window++;
    return 0;
}

#endif /* _RATE_LIMIT_H_ */
