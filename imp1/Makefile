-include ../../arch.mk
-include ../../path.mk

.PHONY: all
all: module applet

.PHONY: module
module:
	@echo -e "\033[32mBuilding kernel module: imp1k\033[0m"
	@cd ./imp1k/ && [ -f imp1k.h ] || ln -s ../imp1.h ./imp1k.h 
	make -C $(KERNELDIR) M=$(shell pwd)/imp1k modules

.PHONY: applet
applet:
	@echo -e "\033[32mBuilding applet: imp1u\033[0m"
	@cd ./imp1u/ && [ -f imp1u.h ] || ln -s ../imp1.h ./imp1u.h
	@make -C imp1u CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" STRIP="$(STRIP)"

.PHONY: clean
clean: module_clean applet_clean

.PHONY: module_clean
module_clean:
	@echo -e "\033[32mCleaning kernel module\033[0m"
	@cd ./imp1k && rm -rf *.o *.ko *.mod.c .*.cmd .*.o.d .tmp_versions *.symvers *.order

.PHONY: applet_clean
applet_clean:
	@echo -e "\033[32mCleaning user applet\033[0m"
	@make -C imp1u clean

.PHONY: distclean
distclean: clean

.PHONY: install
install:
	@echo -e "\033[32mInstalling gpio kernel module and utility\033[0m"
	@[ -d $(TARGET)/lib/modules ] || mkdir -p $(TARGET)/lib/modules
	@cp ./imp1k/imp1k.ko $(TARGET)/lib/modules/.
	@cp ./imp1u/imp1u $(TARGET)/usr/sbin/.
