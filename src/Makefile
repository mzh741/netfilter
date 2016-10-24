PRODUCER := pptmonitor
CONSUMER := pptconsumer
MONITOR_DIR := pptmonitor
PT_PRODUCER_DIR := ptproducer
PT_PRODUCER := ptproducer
COMMON := common
obj-m := ExaOTag.o
KDIR := /lib/modules/$(shell uname -r)/build
MPWD := $(shell pwd)/exaotag/

default:
	./configure.sh
	gcc -o $(MONITOR_DIR)/$(PRODUCER) $(MONITOR_DIR)/$(PRODUCER).c -I. -lrt
	gcc -o $(MONITOR_DIR)/$(CONSUMER) $(MONITOR_DIR)/$(CONSUMER).c -I. -lrt
	gcc -o $(PT_PRODUCER_DIR)/$(PT_PRODUCER) $(PT_PRODUCER_DIR)/$(PT_PRODUCER).c -I. -lrt
	$(MAKE) -C $(KDIR) SUBDIRS=$(MPWD) modules
clean:
	rm -f $(MONITOR_DIR)/$(PRODUCER) $(MONITOR_DIR)/$(CONSUMER)
	rm -f $(COMMON)/niciplist $(COMMON)/sysspec.h
	$(MAKE) -C $(KDIR) SUBDIRS=$(MPWD) clean
