obj-m += SimpleCharacterDriver.o


KERNEL_DIR=/lib/modules/$(shell uname -r)/build/


#  If we were cross-compiling we would add: 	ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \  to each of these
all:
	make -C $(KERNEL_DIR) M=$(shell pwd) modules

clean:
	make -C $(KERNEL_DIR) M=$(shell pwd) clean 



