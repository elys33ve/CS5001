# User Guide: Zynq UltraScale+ Hardware Acceleration Project
This guide provides instructions to reproduce the hardware-accelerated testing environment on the UltraZed™-EG SOM.

---

## 1. System Requirements

### Hardware
* **Board:** UltraZed™-EG SOM + Zynq UltraScale+ MPSoC.
* **Peripherals:** USB Serial adapter, Custom serial header adapter, Ethernet cable, Micro SD card (and reader).
* **Host Machine:** Linux PC (for building the kernel and Vivado projects).


### Software Tools
| Tool | Purpose |
| :--- | :--- |
| **Vivado 2025.1** | FPGA synthesis and bitstream generation. |
| **ARM64 Build Tools** | GNU cross-compiler for kernel/utility compilation. |
| **Minicom** | Serial communication with the board. |
| **Rsync / SSH** | File transfer and remote command execution. |

---

## 2. Directory Structure
* `/linux-zynqmp`: Kernel source, drivers, and Device Tree (.dts).
* `/utils-3eg`: Vivado project files for FPGA logic.
* `/ip`: IP cores for AXI bus transfers and testbench code (Accelerated vs. Unaccelerated).
* `/platform_boot`: Staging area for boot binaries (FSBL, U-Boot, Bitstream).

---

## 3. Build & Setup Instructions
Generating the hardware bitstream, building the Linux kernel, and deploying the binaries to the UltraZed™-EG platform.

### 3.1 Build the FPGA Hardware
The programmable logic (PL) must be synthesized to enable hardware acceleration.
1. Open **Vivado 2025.1**.
2. Load the project located in the `/utils-3eg` directory.
3. Execute **Generate Bitstream** (produces the FPGA firmware required for the boot image).

### 3.2 Build the Linux Kernel
The kernel is compiled on a Linux host PC for the ARM64 architecture. Go to the `/linux-zynqmp` directory and run:
```bash
# set environment variables for the cross-compiler
export CROSS_COMPILE=aarch64-linux-gnu-
export ARCH=arm64
# clean the build tree and apply the default ZynqMP configuration
make mrproper
make zynqmp_defconfig
# compile the kernel using all available cores
make -j $(nproc)
# add sd card path
sudo make INSTALL_MOD_PATH=/<sdcard> modules_install

### 3.3 Prepare the Boot Binaries
`/platform_boot/utils-3eg` to pull in and process all relevant files and copy to the SD card. Run these commands from the host PC terminal:
```bash
cd platform_boot/utils-3eg
# initialize environment paths for the build tools
source setpaths.sh
# collect the required binaries (FPGA firmware, kernel binary, u-boot, fsbl, etc.)
make update
# package binaries into BOOT.BIN
make boot.bin
# copy the generated files to the boot partition of the SD card
make install
	
### 3.4 Hardware Startup and Connection
Once the SD card is prepared and inserted into the UltraZed™-EG board:
1. **Power:** Power on the board.
2. **Serial Connection:** Connect the board to host PC using the USB serial adapter.
3. **Minicom:** Open a terminal on your host PC and use `minicom` to establish a connection to the board's serial console.
```bash
# (adjust ttyUSB0 as needed)
minicom -D /dev/ttyUSB0 -b 115200
