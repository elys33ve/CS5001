

proc generate {drv_handle} {
	xdefine_include_file $drv_handle "xparameters.h" "ddr_test_wr" "NUM_INSTANCES" "DEVICE_ID"  "C_axi_lite_BASEADDR" "C_axi_lite_HIGHADDR"
}
