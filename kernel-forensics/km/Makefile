obj-m += cheat.o
cheat-objs += memory_cache.o memory.o process.o verify.o entry.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(STRIP) -g -S *.ko

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.ko *.o *.mod.c *.mod.o Module.symvers modules.order

