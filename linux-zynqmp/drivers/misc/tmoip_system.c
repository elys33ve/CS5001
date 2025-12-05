#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/ctype.h>
#include <linux/timekeeping.h>

#define TMOIP_SYSTEM_DEV_NAME "tmoip_system"

#define INT_CTR_BITS 29
#define TRACK_PGAIN_ACQ_INIT 0x080000000
#define TRACK_ACQ_THRESH_INIT 100

#define STRACK_FREQ_TRACK 0
#define STRACK_FREQ_LOCK 1

#define FREQ_STEP_LOCKED_INIT 5

#define STEP_SIZE_INIT 0x2AABC2D0
#define MOD_VAL_INIT  0x8000017A

// tmoip_system registers (3ch and 12ch)
#define PERIPH_CTRL_0_OFFSET (0)
#define PERIPH_CTRL_1_OFFSET (1)
#define PERIPH_CTRL_2_OFFSET (2)
// tmoip-mlsc
#define PCM_IN_NOUT_OFFSET (3)
// 12ch
#define PERIPH_SPARE_OFFSET (4)
// tmoip-mlsc
#define TMOIP_MLSC_TP_OFFSET (4)
// tmoip-mlsc
#define MLSC_SOM_CTRL_OFFSET (5)
//12ch and 3ch
#define FREQ_STEP_LOCKED_OFFSET (6)
#define FREQ_LOCK_MASK 0x80000000
#define MOD_VAL_OFFSET (7)
#define PROD_ID_VAR_OFFSET (8)
#define PWRDWN_NRES_OFFSET (9)
// 12ch
#define DIPSW_OFFSET (10)
// 3ch
#define GPIN_OFFSET (10)

#define FAN_TACH_OFFSET (11)
#define FPGA_CPU_ERR_OFFSET (12)
#define VERSION_OFFSET (15)

#define PLATFORM_TYPE_OFFSET (16)

// tmoip_system registers -- legacy (3eg)
// todo - remap 3eg registers to
// be closer to the 3ch/12ch
#define EXTERNAL_PIN_CTRL_OFFSET (7)
#define SRC_SINK_LED_CTRL_OFFSET (13)
#define TEST_POINT_OFFSET (14)
#define NRES_OFFSET (25)
#define ENET_LED_OFFSET (26)

// Bits -- LEGACY
#define BIT_NRE_422 (1)
#define BIT_DE_422 (1<<1)
#define BIT_SINKBUF_OE (1<<2)
#define BIT_NSRCBUF_OE (1<<3)
#define BIT_ANMUX_SEL (1<<4)

#define BIT_SRC_LED (1)
#define BIT_SINK_LED (1<<1)

// other bit assignments
// PROD_ID_VAR_OFFSET BITS
#define PROD_ID_MASK 0x01F
#define PROD_ID_SHIFT 16
#define PROD_VAR_MASK 0x01F
// PWRDWN_NRES_OFFSET BITS
#define BIT_NRES_DEFAULTS (1<<1)
#define BIT_SOM_PWR_DOWN (1<<2)
// FAN_CTRL BITS
//#define FAN_12_MASK 0x0FFFF
//#define FAN_34_MASK 0x0FFFF
//#define FAN_34_SHIFT 16
// PERIPH_CTRL_1: DISPLAY_CS/DC, CH1_6_NRES, CH7_12NRES (12CH)
#define BIT_DISPLAY_CS (1)
#define BIT_DISPLAY_DC (1<<1)
#define BIT_CH1_6_NRES (1<<2)
#define BIT_CH7_12_NRES (1<<3)    
#define BIT_OK_TO_PWR_DWN (1<<4)    
// PERIPH_CTRL_2: ETH_CTRL/DATA_YELLOW (12CH)
#define BIT_ETH_CTRL_YELLOW (1)
#define BIT_ETH_DATA_YELLOW (1<<1)
// PERIPH_SPARE bits
// 3ch
#define GP_OUT_MASK 0x0F
// 12ch
#define SPARE_MASK 0x0FF
#define HP_SPARE_MASK 0x0F
#define HP_SPARE_SHIFT 8

// tmoip-mlsc som ctrl
#define MLSC_SOM_nRST_BIT (1)

// platforms    
#define PLATFORM_TYPE_8_CHAN_3EG 0
#define PLATFORM_TYPE_TMOIP_3CH 1
#define PLATFORM_TYPE_TMOIP_12CH 2
#define PLATFORM_TYPE_AVNET_CC7EV 3
#define PLATFORM_TYPE_XILINX_ZCU102 4
#define PLATFORM_TYPE_TMOIP_MLSC 5
#define PLATFORM_TYPE_TMOIP_BSSA 6

void format_date_time(char *date, char *time, char *str);

struct  tmoip_system_dev {
	struct device *dev;
	struct platform_device *pdev;

	const char *compatible;

	u32 __iomem *preg;
	
	struct resource *tmoip_system_reg_res;
	struct resource *tmoip_system_axi_res;

	int clkadj_restart;

    u64 clocktime;
    u64 clocktime_diff;

    u32 step_size;
    s32 mod_val;
    
    u64 fpga_time;
    s64 fpga_cpu_offset;
    s64 freq_err;
    s64 freq_err_avg;

    u32 freq_acq_pgain;
    u32 freq_thresh;
    s32 adj_freq;
    int track_state;
    s32 fpga_cpu_err;

    // fpga fine-tune adjustments after freq lock
    u32 freq_step_locked;

    u32 step_size_before;
    u32 step_size_after;

	u32 platform_type;
	u32 output_ninput;
	u32 ttl_nrs422;

    // mlsc 2-som som2som pcm lines
    u32 pcm_in_nout;

    u32 mlsc_som_nrst;
};

// tmoip_system ip core access
static inline u32 read_tmoip_system_reg(struct tmoip_system_dev *tmoip_system,
					 int offset) {
	return *((u32*)(tmoip_system->preg+offset));
}

static inline void write_tmoip_system_reg(struct tmoip_system_dev *tmoip_system,
					   int offset, u32 data) {
	*((u32*)(tmoip_system->preg+offset)) = data;
}

static inline int tmoip_system_bitget(struct tmoip_system_dev *tmoip_system,
				      u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_tmoip_system_reg(tmoip_system, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void tmoip_system_bitset(struct tmoip_system_dev *tmoip_system,
				       u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_tmoip_system_reg(tmoip_system, offset);
	reg |= bitmask; 
	write_tmoip_system_reg(tmoip_system, offset, reg);
}

static inline void tmoip_system_bitclr(struct tmoip_system_dev *tmoip_system,
				       u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_tmoip_system_reg(tmoip_system, offset);
	reg &= ~bitmask;
	write_tmoip_system_reg(tmoip_system, offset, reg);
}

void format_date_time(char *date, char *time, char *str){
    char months[12][4] = {"jan\0","feb\0","mar\0","apr\0","may\0","jun\0","jul\0","aug\0","sep\0","oct\0","nov\0","dec\0"};
    char year[5];
    char month[4];
    char day[3];
    char temp[4];
    int i;
/////   get date
    strncpy(year, date+7, 5);       // year
//
    strncpy(month, date, 3);        // month
    month[3] = 0;
    month[0] = tolower(month[0]);
    for (i=0;i<12;i++){
        strncpy(temp, months[i], 4);
        if (strcmp(temp, month) == 0){
            if (i<9){
                sprintf(month, "0%d", i+1);
            }
            else {
                sprintf(month, "%d", i+1);
            }
            break;
        }
    }
//
    strncpy(day, date+4, 2);        // day
    day[2] = 0;
    if (isspace(day[0]) != 0){
        day[0] = '0';
    }
/////   format str
    strcpy(str, year); strcat(str, ":");
    strcat(str, month); strcat(str, ":");
    strcat(str, day); strcat(str, ":");
    strcat(str, time);
}
static inline void tmoip_system_set_output_ninput(struct tmoip_system_dev *tmoip_system) {
	if (tmoip_system->output_ninput) { // output = 0x1D to external pin ctrl
		tmoip_system_bitset(tmoip_system, EXTERNAL_PIN_CTRL_OFFSET, BIT_ANMUX_SEL);
		tmoip_system_bitset(tmoip_system, EXTERNAL_PIN_CTRL_OFFSET, BIT_NSRCBUF_OE);
		tmoip_system_bitset(tmoip_system, EXTERNAL_PIN_CTRL_OFFSET, BIT_SINKBUF_OE);
		tmoip_system_bitclr(tmoip_system, EXTERNAL_PIN_CTRL_OFFSET, BIT_DE_422);    
		tmoip_system_bitset(tmoip_system, EXTERNAL_PIN_CTRL_OFFSET, BIT_NRE_422);
		// set leds
		tmoip_system_bitset(tmoip_system, SRC_SINK_LED_CTRL_OFFSET, BIT_SRC_LED);
		tmoip_system_bitclr(tmoip_system, SRC_SINK_LED_CTRL_OFFSET, BIT_SINK_LED);
		dev_info(tmoip_system->dev, "Setting unit for output\n");
        }
        // enable input
        else { // input = 0x1
		tmoip_system_bitclr(tmoip_system, EXTERNAL_PIN_CTRL_OFFSET, BIT_ANMUX_SEL);
		tmoip_system_bitclr(tmoip_system, EXTERNAL_PIN_CTRL_OFFSET, BIT_NSRCBUF_OE);
		tmoip_system_bitclr(tmoip_system, EXTERNAL_PIN_CTRL_OFFSET, BIT_SINKBUF_OE);
		tmoip_system_bitclr(tmoip_system, EXTERNAL_PIN_CTRL_OFFSET, BIT_DE_422);    
		tmoip_system_bitset(tmoip_system, EXTERNAL_PIN_CTRL_OFFSET, BIT_NRE_422);   
		// set leds
		tmoip_system_bitset(tmoip_system, SRC_SINK_LED_CTRL_OFFSET, BIT_SRC_LED);
		tmoip_system_bitclr(tmoip_system, SRC_SINK_LED_CTRL_OFFSET, BIT_SINK_LED);
		dev_info(tmoip_system->dev, "Setting unit for input\n");
        }
}

static ssize_t clkadj_restart_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    if (tmoip_system->platform_type == PLATFORM_TYPE_8_CHAN_3EG)
        return sprintf(buf, "%d\n", tmoip_system->clkadj_restart);
    else
        return sprintf(buf, "not applicable to this platform\n");
}
static ssize_t clkadj_restart_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	unsigned int val;
    if (tmoip_system->platform_type != PLATFORM_TYPE_8_CHAN_3EG) {
        dev_info(tmoip_system->dev, "not applicable to this platform\n");
        return count;
    }
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/clkadj_restart");
		return count;
	}
	tmoip_system->clkadj_restart = val;
	return count;
}
static	DEVICE_ATTR_RW(clkadj_restart);

static ssize_t output_ninput_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);

    if (tmoip_system->platform_type == PLATFORM_TYPE_8_CHAN_3EG)
        return sprintf(buf, "%u\n", tmoip_system->output_ninput);
    else
        return sprintf(buf, "not applicable to this platform\n");
}
static ssize_t output_ninput_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	unsigned int val;
	int ret;

	ret = kstrtouint(buf, 0, &val);

    if (tmoip_system->platform_type != PLATFORM_TYPE_8_CHAN_3EG) {
        dev_info(tmoip_system->dev, "not applicable to this platform\n");
        return count;
    }

	if (ret) 
		dev_err(tmoip_system->dev,
			"Error parsing value to reset in sysfs (ret = %d)\n",
			ret);

	// update tmoip_system->output_ninput then call tmoip_system_set_output_ninput()
	tmoip_system->output_ninput = val ? 1:0;
	tmoip_system_set_output_ninput(tmoip_system);

	return count;
}
static DEVICE_ATTR_RW(output_ninput);

static ssize_t version_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_12CH)
	  return sprintf(buf, "0x%x\n", read_tmoip_system_reg(tmoip_system, VERSION_OFFSET));
	else if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_MLSC)
	  return sprintf(buf, "0x%x\n", read_tmoip_system_reg(tmoip_system, VERSION_OFFSET));
	else if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_BSSA)
	  return sprintf(buf, "0x%x\n", read_tmoip_system_reg(tmoip_system, VERSION_OFFSET));
	else
	  return sprintf(buf, "%d\n", read_tmoip_system_reg(tmoip_system, VERSION_OFFSET));
}
static	DEVICE_ATTR_RO(version);

static ssize_t build_time_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	char build_time[20];
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdate-time"
		format_date_time(__DATE__, __TIME__, build_time);
	#pragma GCC diagnostic pop
	return sprintf(buf, "%s\n",build_time);
}
static	DEVICE_ATTR_RO(build_time);

static ssize_t platform_type_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    if (tmoip_system->platform_type == PLATFORM_TYPE_8_CHAN_3EG)
        return sprintf(buf, "Quasonix TMoIP Ultrazed 3EG board (legacy)\n");
    else if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_3CH)
        return sprintf(buf, "Quasonix TMoIP 3 channel board (RDMS)\n");
    else if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_12CH)
        return sprintf(buf, "Quasonix TMoIP standalone 12 channel board\n");
    else if (tmoip_system->platform_type == PLATFORM_TYPE_AVNET_CC7EV)
        return sprintf(buf, "Avnet Ultrazed 7EV with carrier card\n");
    else if (tmoip_system->platform_type == PLATFORM_TYPE_XILINX_ZCU102)
        return sprintf(buf, "Xilinx ZCU102 development board\n");
    else if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_MLSC)
        return sprintf(buf, "Quasonix TMoIP on the MLSC platform\n");
    else if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_BSSA)
        return sprintf(buf, "Quasonix TMoIP on the BSSA platform\n");
    else
        return sprintf(buf, "Unknown platform\n");
}
static	DEVICE_ATTR_RO(platform_type);

static ssize_t gp_in_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_3CH)
        return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, GPIN_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(gp_in);

static ssize_t dipsw_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) 
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC)
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA)
                return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, DIPSW_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(dipsw);

static ssize_t fan_tach_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH)
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC)
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA)
                return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, FAN_TACH_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(fan_tach);

static ssize_t product_id_var_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    u32 id;
    u32 var;
    reg = read_tmoip_system_reg(tmoip_system, PROD_ID_VAR_OFFSET);
    id = (reg>>PROD_ID_SHIFT) & PROD_ID_MASK;
    var = (reg & PROD_VAR_MASK);
    return sprintf(buf, "%d,%d\n", id, var);
}
static	DEVICE_ATTR_RO(product_id_var);

static ssize_t nres_defaults_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", tmoip_system_bitget(
                       tmoip_system, PWRDWN_NRES_OFFSET, BIT_NRES_DEFAULTS));
}
static	DEVICE_ATTR_RO(nres_defaults);

static ssize_t som_pwr_down_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH)
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC)
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA)
                return sprintf(buf, "not applicable to this platform\n");

    return sprintf(buf, "%u\n", tmoip_system_bitget(
                       tmoip_system, PWRDWN_NRES_OFFSET, BIT_SOM_PWR_DOWN));
}
static	DEVICE_ATTR_RO(som_pwr_down);

static ssize_t display_cs_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) 
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) 
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) 
                return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET);
    return sprintf(buf, "%d\n", reg & BIT_DISPLAY_CS ? 1 : 0);
}
static ssize_t display_cs_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	u32 val;
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
                dev_info(tmoip_system->dev, "not applicable to this platform\n");
                return count;
            }
        }
    }
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/display_cs");
		return count;
	}
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET);
    reg &= ~BIT_DISPLAY_CS;
    reg |= (val ? BIT_DISPLAY_CS : 0);
    write_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET, reg);
	return count;
}
static	DEVICE_ATTR_RW(display_cs);

static ssize_t display_dc_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH)
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC)
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA)
                return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET);
    return sprintf(buf, "%d\n", reg & BIT_DISPLAY_DC ? 1 : 0);
}
static ssize_t display_dc_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	u32 val;
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
                dev_info(tmoip_system->dev, "not applicable to this platform\n");
                return count;
            }
        }
    }
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/display_dc");
		return count;
	}
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET);
    reg &= ~BIT_DISPLAY_DC;
    reg |= (val ? BIT_DISPLAY_DC : 0);
    write_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET, reg);
	return count;
}
static	DEVICE_ATTR_RW(display_dc);

static ssize_t ch1_6_nres_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH)
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC)
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA)
                return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET);
    return sprintf(buf, "%d\n", reg & BIT_CH1_6_NRES ? 1 : 0);
}
static ssize_t ch1_6_nres_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	u32 val;
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
                dev_info(tmoip_system->dev, "not applicable to this platform\n");
                return count;
            }
        }
    }
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/ch1_6_nres");
		return count;
	}
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET);
    reg &= ~BIT_CH1_6_NRES;
    reg |= (val ? BIT_CH1_6_NRES : 0);
    write_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET, reg);
	return count;
}
static	DEVICE_ATTR_RW(ch1_6_nres);

static ssize_t ch7_12_nres_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
                return sprintf(buf, "not applicable to this platform\n");
            }
        }
    }
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET);
    return sprintf(buf, "%d\n", reg & BIT_CH7_12_NRES ? 1 : 0);
}
static ssize_t ch7_12_nres_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	u32 val;
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
                dev_info(tmoip_system->dev, "not applicable to this platform\n");
                return count;
            }
        }
    }
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/ch7_12_nres");
		return count;
	}
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET);
    reg &= ~BIT_CH7_12_NRES;
    reg |= (val ? BIT_CH7_12_NRES : 0);
    write_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET, reg);
	return count;
}
static	DEVICE_ATTR_RW(ch7_12_nres);

static ssize_t gp_out_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_3CH)
        return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_SPARE_OFFSET);
    return sprintf(buf, "0x%x\n", reg & GP_OUT_MASK);
}
static ssize_t gp_out_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	u32 val;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_3CH) {
        dev_info(tmoip_system->dev, "not applicable to this platform\n");
        return count;
    }
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/gp_out");
		return count;
	}
    val &= GP_OUT_MASK;
    write_tmoip_system_reg(tmoip_system, PERIPH_SPARE_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(gp_out);

static ssize_t spare_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC)
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH)
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA)
                return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_SPARE_OFFSET);
    return sprintf(buf, "0x%x\n", reg & SPARE_MASK);
}
static ssize_t spare_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	u32 val;
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
                dev_info(tmoip_system->dev, "not applicable to this platform\n");
                return count;
            }
        }
    }
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/spare");
		return count;
	}
    val &= SPARE_MASK;
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_SPARE_OFFSET);
    reg &= ~SPARE_MASK;
    reg |= val;
    write_tmoip_system_reg(tmoip_system, PERIPH_SPARE_OFFSET, reg);
	return count;
}
static	DEVICE_ATTR_RW(spare);

static ssize_t hp_spare_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH)
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC)
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA)
                return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_SPARE_OFFSET);
    return sprintf(buf, "0x%x\n", (reg>>HP_SPARE_SHIFT) & HP_SPARE_MASK);
}
static ssize_t hp_spare_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	u32 val;
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
                dev_info(tmoip_system->dev, "not applicable to this platform\n");
                return count;
            }
        }
    }
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/hp_spare");
		return count;
	}
    val &= HP_SPARE_MASK;
    val <<= HP_SPARE_SHIFT;
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_SPARE_OFFSET);
    reg &= ~(HP_SPARE_MASK<<HP_SPARE_SHIFT);
    reg |= val;
    write_tmoip_system_reg(tmoip_system, PERIPH_SPARE_OFFSET, reg);
	return count;
}
static	DEVICE_ATTR_RW(hp_spare);

static ssize_t eth_ctrl_yellow_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH)
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC)
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA)
                return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_2_OFFSET);
    return sprintf(buf, "%d\n", reg & BIT_ETH_CTRL_YELLOW ? 1 : 0);
}
static ssize_t eth_ctrl_yellow_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	u32 val;
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
                dev_info(tmoip_system->dev, "not applicable to this platform\n");
                return count;
            }
        }
    }
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/eth_ctrl_yellow");
		return count;
	}
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_2_OFFSET);
    reg &= ~BIT_ETH_CTRL_YELLOW;
    reg |= val ? BIT_ETH_CTRL_YELLOW : 0;
    write_tmoip_system_reg(tmoip_system, PERIPH_CTRL_2_OFFSET, reg);
	return count;
}
static	DEVICE_ATTR_RW(eth_ctrl_yellow);

static ssize_t eth_data_yellow_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC)
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH)
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA)
                return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_2_OFFSET);
    return sprintf(buf, "%d\n", reg & BIT_ETH_DATA_YELLOW ? 1 : 0);
}
static ssize_t eth_data_yellow_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	u32 val;
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
                dev_info(tmoip_system->dev, "not applicable to this platform\n");
                return count;
            }
        }
    }
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/eth_data_yellow");
		return count;
	}
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_2_OFFSET);
    reg &= ~BIT_ETH_DATA_YELLOW;
    reg |= val ? BIT_ETH_DATA_YELLOW : 0;
    write_tmoip_system_reg(tmoip_system, PERIPH_CTRL_2_OFFSET, reg);
	return count;
}
static	DEVICE_ATTR_RW(eth_data_yellow);

static ssize_t ok_to_pwr_dwn_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH)
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC)
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA)
                return sprintf(buf, "not applicable to this platform\n");
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET);
    return sprintf(buf, "%d\n", reg & BIT_OK_TO_PWR_DWN ? 1 : 0);
}
static ssize_t ok_to_pwr_dwn_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	u32 val;
    u32 reg;
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_12CH) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
            if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
                dev_info(tmoip_system->dev, "not applicable to this platform\n");
                return count;
            }
        }
    }
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/ok_to_pwr_dwn");
		return count;
	}
    reg = read_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET);
    reg &= ~BIT_OK_TO_PWR_DWN;
    reg |= (val ? BIT_OK_TO_PWR_DWN : 0);
    write_tmoip_system_reg(tmoip_system, PERIPH_CTRL_1_OFFSET, reg);
	return count;
}
static	DEVICE_ATTR_RW(ok_to_pwr_dwn);

static ssize_t step_size_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", tmoip_system->step_size);
}
static	DEVICE_ATTR_RO(step_size);

static ssize_t mod_val_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", tmoip_system->mod_val);
}
static ssize_t mod_val_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	u32 val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/mod_val");
		return count;
	}
    write_tmoip_system_reg(tmoip_system, MOD_VAL_OFFSET, val);
    tmoip_system->mod_val = val;
	return count;
}
static	DEVICE_ATTR_RW(mod_val);

static ssize_t clocktime_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%llu\n", tmoip_system->clocktime);
}
static	DEVICE_ATTR_RO(clocktime);

static ssize_t clocktime_diff_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%llu\n", tmoip_system->clocktime_diff);
}
static	DEVICE_ATTR_RO(clocktime_diff);

static ssize_t fpga_time_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%llu\n", tmoip_system->fpga_time);
}
static	DEVICE_ATTR_RO(fpga_time);

static ssize_t fpga_cpu_offset_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%lld\n", tmoip_system->fpga_cpu_offset);
}
static	DEVICE_ATTR_RO(fpga_cpu_offset);

static ssize_t freq_err_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%lld\n", tmoip_system->freq_err);
}
static	DEVICE_ATTR_RO(freq_err);

static ssize_t freq_err_avg_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%lld\n", tmoip_system->freq_err_avg);
}
static	DEVICE_ATTR_RO(freq_err_avg);

static ssize_t track_freq_acq_pgain_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "0x%x\n", tmoip_system->freq_acq_pgain);
}
static ssize_t track_freq_acq_pgain_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/track_freq_acq_pgain");
		return count;
	}
	tmoip_system->freq_acq_pgain = val;
	return count;
}
static	DEVICE_ATTR_RW(track_freq_acq_pgain);

static ssize_t track_freq_thresh_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", tmoip_system->freq_thresh);
}
static ssize_t track_freq_thresh_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/track_freq_thresh");
		return count;
	}
	tmoip_system->freq_thresh = val;
	return count;
}
static	DEVICE_ATTR_RW(track_freq_thresh);

static ssize_t track_state_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 state = tmoip_system->track_state;
    if (state == STRACK_FREQ_TRACK)
        return sprintf(buf, "freq tracking\n");
    if (state == STRACK_FREQ_LOCK)
        return sprintf(buf, "freq lock\n");
    else 
        return sprintf(buf, "unknown state\n");
}
static	DEVICE_ATTR_RO(track_state);

static ssize_t adj_freq_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", tmoip_system->adj_freq);
}
static	DEVICE_ATTR_RO(adj_freq);

static ssize_t step_size_before_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", tmoip_system->step_size_before);
}
static	DEVICE_ATTR_RO(step_size_before);

static ssize_t step_size_after_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", tmoip_system->step_size_after);
}
static	DEVICE_ATTR_RO(step_size_after);

static ssize_t fpga_cpu_err_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", read_tmoip_system_reg(tmoip_system, FPGA_CPU_ERR_OFFSET));
}
static	DEVICE_ATTR_RO(fpga_cpu_err);

static ssize_t freq_step_locked_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    return sprintf(buf, "0x%x\n", tmoip_system->freq_step_locked & ~FREQ_LOCK_MASK);
}
static ssize_t freq_step_locked_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	unsigned int val;
	unsigned int reg;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(tmoip_system->dev, "Error in sysfs/freq_step_locked");
		return count;
	}
    val &= FREQ_LOCK_MASK;
    reg = read_tmoip_system_reg(tmoip_system, FREQ_STEP_LOCKED_OFFSET) & FREQ_LOCK_MASK;
    reg |= val;
	tmoip_system->freq_step_locked = reg;
	return count;
}
static	DEVICE_ATTR_RW(freq_step_locked);

static ssize_t mlsc_som_nrst_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    u32 reg;
    reg = tmoip_system_bitget(tmoip_system, MLSC_SOM_CTRL_OFFSET, MLSC_SOM_nRST_BIT);
    if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_MLSC) 
        return sprintf(buf, "%u\n", reg);
    else if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_BSSA) 
        return sprintf(buf, "%u\n", reg);
    else
        return sprintf(buf, "not applicable to this platform\n");
}
static ssize_t mlsc_som_nrst_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	unsigned int val;
	int ret;

	ret = kstrtouint(buf, 0, &val);

    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
            dev_info(tmoip_system->dev, "not applicable to this platform\n");
            return count;
        }
    }
	if (ret) 
		dev_err(tmoip_system->dev,
			"Error parsing value to reset in sysfs (ret = %d)\n",
			ret);

	// update tmoip_system->mlsc_som_nrst then call tmoip_system_set_mlsc_som_nrst()
	tmoip_system->mlsc_som_nrst = val ? 1:0;
    if (tmoip_system->mlsc_som_nrst) {
        tmoip_system_bitset(tmoip_system, MLSC_SOM_CTRL_OFFSET, MLSC_SOM_nRST_BIT);
    }
    else {
        tmoip_system_bitclr(tmoip_system, MLSC_SOM_CTRL_OFFSET, MLSC_SOM_nRST_BIT);
    }
	return count;
}
static DEVICE_ATTR_RW(mlsc_som_nrst);

static ssize_t pcm_in_nout_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
    int x;
    int mask=1;
    u32 v = tmoip_system->pcm_in_nout;
    char tmp[128];

    if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_MLSC ||
        tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_BSSA) {
        for (x=0; x<12; x++) {
            sprintf(tmp, "pcm ch%d: %s\n", x, (v & (mask<<x)) ? "pcm in" : "pcm_out");
            if (x==0) 
                strcpy(buf, tmp);
            else 
                strcat(buf, tmp);
        }
        return strlen(buf);
    }
    else
        return sprintf(buf, "not applicable to this platform\n");
}
static ssize_t pcm_in_nout_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(dev);
	unsigned int val;
	int ret;

	ret = kstrtouint(buf, 0, &val);
    if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_MLSC) {
        if (tmoip_system->platform_type != PLATFORM_TYPE_TMOIP_BSSA) {
            dev_info(tmoip_system->dev, "not applicable to this platform\n");
            return count;
        }
    }
	if (ret) 
		dev_err(tmoip_system->dev,
			"Error parsing value to reset in sysfs (ret = %d)\n",
			ret);
	tmoip_system->pcm_in_nout = val;
    write_tmoip_system_reg(tmoip_system, PCM_IN_NOUT_OFFSET, val);
	return count;
}
static DEVICE_ATTR_RW(pcm_in_nout);

static struct attribute *tmoip_system_attrs[] = {
	&dev_attr_output_ninput.attr,
	&dev_attr_clkadj_restart.attr,
	&dev_attr_version.attr,
	&dev_attr_build_time.attr,
	&dev_attr_platform_type.attr,
	&dev_attr_gp_in.attr,
	&dev_attr_dipsw.attr,
	&dev_attr_fan_tach.attr,
	&dev_attr_product_id_var.attr,
	&dev_attr_nres_defaults.attr,
	&dev_attr_som_pwr_down.attr,
    &dev_attr_display_cs.attr,
    &dev_attr_display_dc.attr,
    &dev_attr_ch1_6_nres.attr,
    &dev_attr_ch7_12_nres.attr,
    &dev_attr_gp_out.attr,
    &dev_attr_spare.attr,
    &dev_attr_hp_spare.attr,
    &dev_attr_eth_ctrl_yellow.attr,
    &dev_attr_eth_data_yellow.attr,
    &dev_attr_ok_to_pwr_dwn.attr,
	&dev_attr_step_size.attr,
	&dev_attr_mod_val.attr,
	&dev_attr_clocktime.attr,
	&dev_attr_clocktime_diff.attr,
	&dev_attr_fpga_time.attr,
	&dev_attr_fpga_cpu_offset.attr,
	&dev_attr_freq_err.attr,
	&dev_attr_freq_err_avg.attr,
	&dev_attr_track_freq_acq_pgain.attr,
	&dev_attr_track_freq_thresh.attr,
	&dev_attr_track_state.attr,
	&dev_attr_adj_freq.attr,
	&dev_attr_step_size_before.attr,
	&dev_attr_step_size_after.attr,
	&dev_attr_fpga_cpu_err.attr,
	&dev_attr_freq_step_locked.attr,
	&dev_attr_mlsc_som_nrst.attr,
	&dev_attr_pcm_in_nout.attr,
	NULL,
};

static struct attribute_group tmoip_system_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = tmoip_system_attrs,
};

static int tmoip_system_parse_of(struct tmoip_system_dev *tmoip_system)
{
	struct device *dev = tmoip_system->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &tmoip_system->compatible);
	if (ret < 0) {
		dev_err(dev, "tmoip_system not in DT");
		return ret;
	}

	// reg -- axi lite
	res = platform_get_resource(tmoip_system->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	tmoip_system->tmoip_system_reg_res = res;
		
    // reg -- axi full
    res = platform_get_resource(tmoip_system->pdev, IORESOURCE_MEM, 1);
    if (res == NULL) {
        dev_err(dev, "error getting reg addr from dts");
        return -EINVAL;
    }
    tmoip_system->tmoip_system_axi_res = res;
		
	// platform_type
	ret = of_property_read_u32(node, "platform_type", &tmoip_system->platform_type);
	if (ret < 0) {
		dev_err(dev, "error parsing platform_type");
		return ret;
	}

    if (tmoip_system->platform_type == PLATFORM_TYPE_8_CHAN_3EG) {
	// output_ninput
        ret = of_property_read_u32(node, "output_ninput", &tmoip_system->output_ninput);
        if (ret) {
            dev_err(tmoip_system->dev, "error parsing output_ninput");
            return ret;
        }
        else
            tmoip_system->output_ninput = tmoip_system->output_ninput ? 1 : 0;

        // ttl_nrs422
        ret = of_property_read_u32(node, "ttl_nrs422", &tmoip_system->ttl_nrs422);
        if (ret) {
            dev_err(tmoip_system->dev, "error parsing ttl_nrs422");
            return ret;
        }
        else
            tmoip_system->ttl_nrs422 = tmoip_system->ttl_nrs422 ? 1 : 0;
    }

	dev_info(dev, "Done parsing DT");

	return 0;
}

static int tmoip_system_allocate_resources(struct tmoip_system_dev *tmoip_system) {
	int ret = -EINVAL;
	int addr_size;

	u32 start;
	u32 end;

    // map tmoip_system device memory
	start = (u32)tmoip_system->tmoip_system_reg_res->start;
	end = (u32)tmoip_system->tmoip_system_reg_res->end;
	addr_size = end - start + 1;
	dev_info(tmoip_system->dev, "Mapping TMOIP_SYSTEM 0x%x, size 0x%x", start, addr_size);
	
	tmoip_system->preg =
		(u32*)devm_ioremap(tmoip_system->dev,
				   tmoip_system->tmoip_system_reg_res->start,
				   addr_size);
	if (tmoip_system->preg == NULL) {
		dev_err(tmoip_system->dev, "Error mapping tmoip_system base memory");
		ret = -ENOMEM;
		goto FAIL;
	}

	tmoip_system->clkadj_restart = 0;

	// initialize anything that required the fpga logic to be mapped above
	tmoip_system_set_output_ninput(tmoip_system);

    tmoip_system->clocktime = 0;
    tmoip_system->clocktime_diff = 0;
    tmoip_system->step_size = STEP_SIZE_INIT;
    tmoip_system->mod_val = MOD_VAL_INIT;
    tmoip_system->freq_acq_pgain = TRACK_PGAIN_ACQ_INIT;
    tmoip_system->freq_thresh = TRACK_ACQ_THRESH_INIT;
    tmoip_system->track_state = STRACK_FREQ_TRACK;
    tmoip_system->adj_freq = 0;
    tmoip_system->fpga_cpu_offset = 0;
    tmoip_system->freq_step_locked = FREQ_STEP_LOCKED_INIT;
	tmoip_system->mlsc_som_nrst = 0;

    tmoip_system_bitclr(tmoip_system, MLSC_SOM_CTRL_OFFSET, MLSC_SOM_nRST_BIT);
    write_tmoip_system_reg(tmoip_system, MOD_VAL_OFFSET, tmoip_system->mod_val);

    if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_MLSC) {
        dev_info(tmoip_system->dev, "Setting PCM direction from MLSC to TMoIP");
        tmoip_system->pcm_in_nout = 0xFFF;
        write_tmoip_system_reg(tmoip_system, PCM_IN_NOUT_OFFSET, tmoip_system->pcm_in_nout);
    }
    if (tmoip_system->platform_type == PLATFORM_TYPE_TMOIP_BSSA) {
        dev_info(tmoip_system->dev, "Setting PCM direction from BSSA to TMoIP");
        tmoip_system->pcm_in_nout = 0xFFF;
        write_tmoip_system_reg(tmoip_system, PCM_IN_NOUT_OFFSET, tmoip_system->pcm_in_nout);
    }
    write_tmoip_system_reg(tmoip_system, FREQ_STEP_LOCKED_OFFSET, tmoip_system->freq_step_locked);

    // write platform type to tmoip_system so it can make it available to other drivers
    write_tmoip_system_reg(tmoip_system, PLATFORM_TYPE_OFFSET, tmoip_system->platform_type);
	return 0;

FAIL:
	return ret;
}

static int tmoip_system_probe(struct platform_device *pdev) {
	struct tmoip_system_dev *tmoip_system;
	struct device *dev;
	int ret;

	// tmoip_system
	tmoip_system = devm_kzalloc(&pdev->dev, sizeof(*tmoip_system), GFP_KERNEL);
	if (!tmoip_system)
		return -ENOMEM;

	tmoip_system->dev = &pdev->dev;
	tmoip_system->pdev = pdev;
	dev = tmoip_system->dev;

	dev_set_drvdata(&pdev->dev, tmoip_system);
	
	ret = tmoip_system_parse_of(tmoip_system);
	if (ret < 0)
		return ret;

	ret = tmoip_system_allocate_resources(tmoip_system);
	if (ret < 0)
		return ret;

	if (sysfs_create_group(&pdev->dev.kobj, &tmoip_system_attr_group))
		dev_err(tmoip_system->dev, "failed to create sysfs entries\n");

	return 0;
}

static void tmoip_system_remove(struct platform_device *pdev) {
	struct tmoip_system_dev *tmoip_system = dev_get_drvdata(&pdev->dev);

	dev_info(tmoip_system->dev, "unmapping io memory");
	devm_iounmap(tmoip_system->dev, tmoip_system->preg);

	sysfs_remove_group(&pdev->dev.kobj, &tmoip_system_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(tmoip_system->dev, "remove complete");
}

static const struct of_device_id tmoip_system_of_id_table[] = {
	{ .compatible = "tmoip_system-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, tmoip_system_of_id_table);

static struct platform_driver tmoip_system_driver = {
	.driver = {
		.name = "tmoip_system",
		.of_match_table = tmoip_system_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = tmoip_system_probe,
	.remove = tmoip_system_remove,
};

module_platform_driver(tmoip_system_driver);

MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("tmoip_system: 1.0: TMOIP system-level control");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("tmoip_system");

