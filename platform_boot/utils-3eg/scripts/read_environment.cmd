echo "***********************************"
echo "** read_environment.cmd. "
echo "***********************************"
setenv uenv_addr 0x1000000

if test -e mmc 1:1 uEnv.txt; then 
   echo Found uEnv.txt on mmc 1:1
   fatload mmc 1:1 $uenv_addr uEnv.txt
   env import -t $uenv_addr $filesize
   md.b $uenv_addr $filesize
fi
echo "uEnv.txt version is $uenv_version"
