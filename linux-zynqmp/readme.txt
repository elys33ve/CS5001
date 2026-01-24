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

