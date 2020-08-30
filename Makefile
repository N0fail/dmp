obj-m +=dmp.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

test_start:
	insmod dmp.ko
	dmsetup create zero1 --table "0 20000 zero"
	dmsetup create dmp1 --table "0 20000 dmp /dev/mapper/zero1"

test_finish:
	dmsetup remove dmp1
	dmsetup remove zero1
	rmmod dmp

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
