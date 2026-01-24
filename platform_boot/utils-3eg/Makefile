BASEPATH?=~/projects/utils-3eg/hwaccel
SDCARD?=/media/${USER}/boot
BOOTGEN?=~/local/Xilinx/2025.1/Vitis/bin/bootgen
KERNELPATH?=../../linux-zynqmp

boot.bin:
	@if [ "$(PLATFORM)" = "hwaccel" ]; then \
		echo --------------------; \
		echo Building utils-3eg hwaccel boot.bin...; \
		echo --------------------; \
		cd ./hwaccel && ${BOOTGEN} -arch zynqmp -image boot.bif -w -o boot.bin; \
	fi

update: 
	@if [ "$(PLATFORM)" = "hwaccel" ]; then\
		echo --------------------;\
		echo Updating utils-3eg hwaccel files...;\
		echo --------------------;\
		cp ${BASEPATH}/workspace/hwaccel/zynqmp_fsbl/build/fsbl.elf ./hwaccel/fsbl.elf;\
		cp ${BASEPATH}/workspace/hwaccel/zynqmp_pmufw/build/pmufw.elf ./hwaccel/pmu_fw.elf;\
		cp ${BASEPATH}/tmoip.runs/impl_1/top.bit ./hwaccel/;\
		cp ${KERNELPATH}/arch/arm64/boot/dts/quasonix/utils-3eg/hwaccel/hwaccel.dtb ./hwaccel/system.dtb;\
		cp ${KERNELPATH}/arch/arm64/boot/Image.gz .;\
	else \
		echo --------------------;\
		echo no valid platform found;\
		echo --------------------;\
	fi

bootscripts: 
	cd scripts && mkimage -c none -A arm -T script -d boot.cmd boot.scr
	cd scripts && mkimage -c none -A arm -T script -d init.cmd init.scr
	cd scripts && mkimage -c none -A arm -T script -d read_environment.cmd read_environment.scr
	cd scripts && mkimage -c none -A arm -T script -d som_boot.cmd som_boot.scr
	cd scripts && mkimage -c none -A arm -T script -d set_mac.cmd set_mac.scr

install:
	rm -f ${SDCARD}/TRY*
	rm -f ${SDCARD}/BOOT*
	cp scripts/*.scr ${SDCARD}
	cp Image.gz ${SDCARD}/Image.gz
	@if [ "$(PLATFORM)" = "hwaccel" ]; then\
		echo installing utils-3eg hwaccel SD boot system...;\
		cp hwaccel/boot.bin ${SDCARD}/;\
		cp hwaccel/system.dtb ${SDCARD}/system.dtb;\
	else \
		echo no valid platform found;\
	fi

clean:
	rm -f scripts/*.scr

