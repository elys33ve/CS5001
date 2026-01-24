echo SD boot. DEV. Loading scripts...

env set mmcsel 1

env set init 0x100000;
env set som_boot 0x1000000;

fatload mmc $mmcsel:1 $init init.scr
source $init

env set ramfs_addr -
env set rootpart /dev/mmcblk1p2
env set kernelname Image.gz
env set devicetreename system.dtb

echo assert enet1 nrst (0) 
mw.w 0x80000018 0
mw.w 0x80000018 1

fatload mmc $mmcsel:1 $som_boot som_boot.scr
source $som_boot




