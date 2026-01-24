# mkimage -c none -A arm -T script -d test.cmd test.scr
#
env set set_mac 0x300000;

echo Booting.....
echo mmcsel = $mmcsel
echo kernelname = $kernelname
echo devicetreename = $devicetreename
echo ramfs_addr = $ramfs_addr

env set kernel_addr 0x80000

# Move all other buffers past the 16MB mark (above 0x1080000)
env set set_mac 0x2000000      # 32MB offset
env set fdt_addr 0x4000000     # 64MB offset
env set gzkernel_addr 0x6000000 # 96MB offset

if test ${ramfs_addr} = -; then
   echo Get files from SD: $devicetreename and $kernelname;
   mmc dev $mmcsel && mmcinfo;
   load mmc ${mmcsel}:1 $fdt_addr $devicetreename;
   load mmc ${mmcsel}:1 $gzkernel_addr $kernelname;
   echo "bootargs for utils-3eg"
   env set bootargs "console=ttyPS1,115200 root=$rootpart rw earlycon ipv6.disable=1 panic=5 mem=2048M audit=0 loglevel=7 isolcpus=3 nohz_full=3 rcu_nocbs=3 clk_ignore_unused pm_genpd.ignore_unused rootwait";
   unzip $gzkernel_addr $kernel_addr;
fi;

echo bootargs = $bootargs

fdt addr $fdt_addr
fdt resize 0x1000
fatload mmc $mmcsel:1 $set_mac set_mac.scr
source $set_mac

echo Increasing PL-PS HP0-HP3 priority

echo Force static QoS
mw.l 0xFD380000 0
mw.l 0xFD380014 0
mw.l 0xFD390000 0
mw.l 0xFD390014 0
mw.l 0xFD3A0000 0
mw.l 0xFD3A0014 0
mw.l 0xFD3B0000 0
mw.l 0xFD3B0014 0

echo QoS priority to max
mw.l 0xFD380008 0xF
mw.l 0xFD38001C 0xF
mw.l 0xFD390008 0xF
mw.l 0xFD39001C 0xF
mw.l 0xFD3A0008 0xF
mw.l 0xFD3A001C 0xF
mw.l 0xFD3B0008 0xF
mw.l 0xFD3B001C 0xF

#mw.l 0xFD360008 0xF
#mw.l 0xFD36000C 0xF
#mw.l 0xFD370008 0xF
#mw.l 0xFD37000C 0xF
#mw.l 0xFD380008 0xF
#mw.l 0xFD38000C 0xF
#mw.l 0xFD390008 0xF
#mw.l 0xFD39000C 0xF

zynqmp mmio_write 0xFD0705C4 0xFFFFFFFF 0x01000E00
zynqmp mmio_write 0xFD070604 0xFFFFFFFF 0x01000E00
zynqmp mmio_write 0xFD070644 0xFFFFFFFF 0x01000E00

booti $kernel_addr $ramfs_addr $fdt_addr

