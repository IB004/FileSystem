obj-m += source/vtfs.o 

PWD := $(CURDIR) 
KDIR = /lib/modules/`uname -r`/build
EXTRA_CFLAGS = -Wall -g

all:
	make -C $(KDIR) M=$(PWD) modules 

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -rf .cache
	
insmod:
	sudo insmod source/vtfs.ko

rmmod:
	sudo rmmod vtfs

mount:
	sudo mount -t vtfs "TODO" /mnt/vt

unmount:
	sudo umount /mnt/vt


start: all insmod mount
	

stop: unmount rmmod
	
