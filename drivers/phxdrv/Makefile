obj-m := phddrv.o
phddrv-objs := phd.o cdadrv.o cdapci.o

KDIR		?= /lib/modules/$(shell uname -r)/build
PWD		:= $(shell pwd)

KVER    := $(shell uname -r | head -n1 | cut -d"-" -f1)
KMAJ = $(shell echo $(KVER) | \
sed -e 's/^\([0-9][0-9]*\)\.[0-9][0-9]*\.[0-9][0-9]*.*/\1/')
KMIN = $(shell echo $(KVER) | \
sed -e 's/^[0-9][0-9]*\.\([0-9][0-9]*\)\.[0-9][0-9]*.*/\1/')
KREV = $(shell echo $(KVER) | \
sed -e 's/^[0-9][0-9]*\.[0-9][0-9]*\.\([0-9][0-9]*\).*/\1/')

KVER_GE = $(shell \
echo test | awk '{if($(KMAJ) < $(1)) {print 0} else { \
if($(KMAJ) > $(1)) {print 1} else { \
if($(KMIN) < $(2)) {print 0} else { \
if($(KMIN) > $(2)) {print 1} else { \
if($(KREV) < $(3)) {print 0} else { print 1 } \
}}}}}' \
)

EXTRA_CFLAGS += -I$(obj) -I$(obj)/include
EXTRA_CFLAGS += -D_PHX_LINUX -DNDEBUG -D_PHX_CDA -D_CDA_SG64

all: driver cleanup

driver:
	make -C $(KDIR) SUBDIRS=$(PWD) modules 

ifeq ($(call KVER_GE,4,0,0), 1)
# new kernel, load the driver only, that creates the character devices, so only change mode
EXTRA_CFLAGS += -Wno-error=date-time
install: 
	-/sbin/modprobe -r phddrv > /dev/null 2>&1
	install -D -gwheel -oroot -m744 phddrv.ko /lib/modules/$(shell uname -r)/kernel/drivers/pci/phd/phddrv.ko
	/sbin/depmod -a
	/sbin/modprobe phddrv
	$(PWD)/scripts/set_permission_phx
else
# old kernel, create the character device file system nodes before loading the driver
install:
	-/sbin/modprobe -r phddrv > /dev/null 2>&1
	install -D -gwheel -oroot -m744 phddrv.ko /lib/modules/$(shell uname -r)/kernel/drivers/pci/phd/phddrv.ko
	$(PWD)/scripts/create_special_phx 243 > /dev/null 2>&1
	/sbin/depmod -a
	/sbin/modprobe phddrv
endif

cleanup:
	rm -f *.o .*.cmd *.mod.c *.order *.symvers; rm -rf .tmp_versions

clean: cleanup
	rm -f *.ko test
