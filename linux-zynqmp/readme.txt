starting xilinx branch : xlnx_rebase_v5.15_LTS_202
2.1_update

----------------------------------------------------------
--------- build instructions for the kernel -- zynqmp
----------------------------------------------------------
export CROSS_COMPILE=aarch64-linux-gnu-
export ARCH=arm64
make mrproper

-- zynqmp based boards
make zynqmp_defconfig

-- common
make -j $(nproc)
sudo make INSTALL_MOD_PATH=/<sdcard> modules_install
make dtbs

--- FOR HEADERS
make headers_install INSTALL_HDR_PATH=<deploy location>

NOTE: modules_install to tmoip-common for all tmoip platforms
            
----------------------------------------------------------
-------- build instructions for the kernel -- zynq-7000
----------------------------------------------------------
export CROSS_COMPILE=arm-linux-gnueabihf-
export ARCH=arm
make mrproper

-- zynq-7000 based boards
make rc_based_defconfig

make -j $(nproc) UIMAGE_LOADADDR=0x8000 uImage
make -j $(nproc) modules
sudo make INSTALL_MOD_PATH=/<sdcard> modules_install
make dtbs
