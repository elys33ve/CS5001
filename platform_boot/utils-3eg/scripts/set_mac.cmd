echo "***********************************"
echo "** set_mac.cmd. "
echo "***********************************"

if test -n $mac; then
  echo "** Setting mac (eth0) from env var to ${mac}"
  fdt addr $fdt_addr
  fdt set /axi/ethernet@ff0b0000 local-mac-address $mac
else
  echo "** No mac (eth0) address assignment found"
fi

if test -n $mac2; then
  echo "** Setting mac2 (eth1) from env var to ${mac2}"
  fdt addr $fdt_addr
  fdt set /axi/ethernet@ff0e0000 local-mac-address $mac2
else
  echo "** No mac2 (eth1) address assignment found"
fi

echo "** set_mac.cmd completed"
