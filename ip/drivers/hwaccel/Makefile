obj-m +=  hwaccel.o

export CROSS_COMPILE=aarch64-linux-gnu-
export KROOT=~/projects/CS5001/linux-zynqmp
export ARCH=arm64

allofit:  modules
modules:
	@$(MAKE) -C $(KROOT) M=$(shell pwd) modules
modules_install:
	@$(MAKE) -C $(KROOT) M=$(shell pwd) modules_install
kernel_clean:
	@$(MAKE) -C $(KROOT) M=$(shell pwd) clean

clean: kernel_clean
	rm -rf  Module.symvers modules.order
install:
	cp hwaccel.c $(KROOT)/drivers/misc
