BASEPATH?=~/projects/utils-3eg
SDCARD?=/media/${USER}/boot
BOOTGEN?=~/local/Xilinx/Vitis/2022.1/bin/bootgen
KERNELPATH?=~/projects/xilinx/linux-zynqmp-2025.1
UBOOTPATH?=~/projects/xilinx/u-boot-utils-3eg

boot.bin:
	@if [ "$(PLATFORM)" = "axim_ddr_perf" ]; then \
		echo --------------------; \
		echo Building utils-3eg axim_ddr_perf boot.bin...; \
		echo --------------------; \
		cd ./axim_ddr_perf && ${BOOTGEN} -arch zynqmp -image boot.bif -w -o boot.bin; \
	fi

update: 
	@if [ "$(PLATFORM)" = "axim_ddr_perf" ]; then\
		echo --------------------;\
		echo Updating utils-3eg axim_ddr_perf files...;\
		echo --------------------;\
		cp ${BASEPATH}/axim_ddr_perf/workspace/axim_ddr_perf/zynqmp_fsbl/build/fsbl.elf ./axim_ddr_perf/fsbl.elf;\
		cp ${BASEPATH}/axim_ddr_perf/workspace/axim_ddr_perf/zynqmp_pmufw/build/pmufw.elf ./axim_ddr_perf/pmu_fw.elf;\
		cp ${BASEPATH}/axim_ddr_perf/tmoip.runs/impl_1/top.bit ./axim_ddr_perf/;\
		cp ${UBOOTPATH}/u-boot.elf ./;\
		cp ${KERNELPATH}/arch/arm64/boot/dts/quasonix/utils-3eg/axim_ddr_perf/axim_ddr_perf.dtb ./axim_ddr_perf/system.dtb;\
		cp ${KERNELPATH}/arch/arm64/boot/Image.gz .;\
	else \
		echo --------------------;\
		echo no valid platform found;\
		echo --------------------;\
	fi

update_kernel: 
	@if [ "$(PLATFORM)" = "axim_ddr_perf" ]; then\
		echo --------------------;\
		echo Updating utils-3eg axim_ddr_perf Image.gz and dtb files...;\
		echo --------------------;\
		cp ${KERNELPATH}/arch/arm64/boot/dts/quasonix/utils-3eg/axim_ddr_perf/axim_ddr_perf.dtb ./axim_ddr_perf/system.dtb;\
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
	@if [ "$(PLATFORM)" = "axim_ddr_perf" ]; then\
		echo installing utils-3eg axim_ddr_perf SD boot system...;\
		cp axim_ddr_perf/boot.bin ${SDCARD}/;\
		cp axim_ddr_perf/system.dtb ${SDCARD}/system.dtb;\
	else \
		echo no valid platform found;\
	fi

clean:
	rm -f scripts/*.scr

