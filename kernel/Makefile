MODULE_NAME := nxp_simtemp

obj-m += nxp_simtemp.o

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(CURDIR)

DTS_DIR := dts
DTS_FILE := $(DTS_DIR)/nxp-simtemp.dtsi
DBT_FILE := $(DTS_DIR)/nxp-simtemp.dtb

# Default target: Build kernel module
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
# Compile the Device Tree Source (.dts) to a Device Tree Blob Overlay (.dtbo)
dtb:
	dtc -@ -I dts -O dtb -o $(DBT_FILE) $(DTS_FILE)
# Clean build artifacts
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	@rm -f $(DBT_FILE)
	
.PHONY: all clean dtb
	
