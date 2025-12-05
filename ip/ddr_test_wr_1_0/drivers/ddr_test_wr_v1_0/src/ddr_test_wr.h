
#ifndef DDR_TEST_WR_H
#define DDR_TEST_WR_H


/****************** Include Files ********************/
#include "xil_types.h"
#include "xstatus.h"

#define DDR_TEST_WR_axi_lite_SLV_REG0_OFFSET 0
#define DDR_TEST_WR_axi_lite_SLV_REG1_OFFSET 4
#define DDR_TEST_WR_axi_lite_SLV_REG2_OFFSET 8
#define DDR_TEST_WR_axi_lite_SLV_REG3_OFFSET 12
#define DDR_TEST_WR_axi_lite_SLV_REG4_OFFSET 16
#define DDR_TEST_WR_axi_lite_SLV_REG5_OFFSET 20
#define DDR_TEST_WR_axi_lite_SLV_REG6_OFFSET 24
#define DDR_TEST_WR_axi_lite_SLV_REG7_OFFSET 28
#define DDR_TEST_WR_axi_lite_SLV_REG8_OFFSET 32
#define DDR_TEST_WR_axi_lite_SLV_REG9_OFFSET 36
#define DDR_TEST_WR_axi_lite_SLV_REG10_OFFSET 40
#define DDR_TEST_WR_axi_lite_SLV_REG11_OFFSET 44
#define DDR_TEST_WR_axi_lite_SLV_REG12_OFFSET 48
#define DDR_TEST_WR_axi_lite_SLV_REG13_OFFSET 52
#define DDR_TEST_WR_axi_lite_SLV_REG14_OFFSET 56
#define DDR_TEST_WR_axi_lite_SLV_REG15_OFFSET 60


/**************************** Type Definitions *****************************/
/**
 *
 * Write a value to a DDR_TEST_WR register. A 32 bit write is performed.
 * If the component is implemented in a smaller width, only the least
 * significant data is written.
 *
 * @param   BaseAddress is the base address of the DDR_TEST_WRdevice.
 * @param   RegOffset is the register offset from the base to write to.
 * @param   Data is the data written to the register.
 *
 * @return  None.
 *
 * @note
 * C-style signature:
 * 	void DDR_TEST_WR_mWriteReg(u32 BaseAddress, unsigned RegOffset, u32 Data)
 *
 */
#define DDR_TEST_WR_mWriteReg(BaseAddress, RegOffset, Data) \
  	Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))

/**
 *
 * Read a value from a DDR_TEST_WR register. A 32 bit read is performed.
 * If the component is implemented in a smaller width, only the least
 * significant data is read from the register. The most significant data
 * will be read as 0.
 *
 * @param   BaseAddress is the base address of the DDR_TEST_WR device.
 * @param   RegOffset is the register offset from the base to write to.
 *
 * @return  Data is the data from the register.
 *
 * @note
 * C-style signature:
 * 	u32 DDR_TEST_WR_mReadReg(u32 BaseAddress, unsigned RegOffset)
 *
 */
#define DDR_TEST_WR_mReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))

/************************** Function Prototypes ****************************/
/**
 *
 * Run a self-test on the driver/device. Note this may be a destructive test if
 * resets of the device are performed.
 *
 * If the hardware system is not built correctly, this function may never
 * return to the caller.
 *
 * @param   baseaddr_p is the base address of the DDR_TEST_WR instance to be worked on.
 *
 * @return
 *
 *    - XST_SUCCESS   if all self-test code passed
 *    - XST_FAILURE   if any self-test code failed
 *
 * @note    Caching must be turned off for this function to work.
 * @note    Self test may fail if data memory and device are not on the same bus.
 *
 */
XStatus DDR_TEST_WR_Reg_SelfTest(void * baseaddr_p);

#endif // DDR_TEST_WR_H
