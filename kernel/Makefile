obj-m += nxp_simtemp.o

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(CURDIR)
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	
