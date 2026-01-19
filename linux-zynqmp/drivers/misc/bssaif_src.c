#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/dmaengine.h>
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

#define BSSAIF_SRC_DEV_NAME "bssaif_src"

#define DG_SYNC_CTRL_OFFSET (0)
// added
#define DG_RATE_CTRL_OFFSET (1)
#define DG_RATE_OFFSET (2)
#define DG_RATE_OFFSET_LIM_OFFSET (3)
#define DG_RATE_PHASE_OFFSET (4)
#define DG_RATE_RATE_OFFSET (5)
#define DG_RATE_PERIOD_OFFSET (6)
#define DG_RATE_NG_SCALE_OFFSET (7)
#define DG_RATE_NG_PERIOD_OFFSET (8)
#define DG_DQM_CTRL_OFFSET (9)
#define DG_DQM_OFFSET (10)
#define DG_DQM_OFFSET_LIM_OFFSET (11)
#define DG_DQM_PHASE_OFFSET (12)
#define DG_DQM_RATE_OFFSET (13)
#define DG_DQM_PERIOD_OFFSET (14)
#define DG_DELAY_OFFSET (15)
#define DG_TAPS_OFFSET (16)
#define DG_PATTERN_OFFSET (17)
#define DQE_CTRL_OFFSET (18)

void format_date_time(char *date, char *time, char *str);

struct bssaif_src_dev {
    struct device *dev;
    struct platform_device *pdev;

    const char *compatible;

    u32 __iomem *preg;
	
    struct resource *bssaif_src_reg_res;
};

// bssaif_src ip core access
static inline u32 read_bssaif_src_reg(struct bssaif_src_dev *bssaif_src, int offset) {
	return *((u32*)(bssaif_src->preg+offset));
}

static inline void write_bssaif_src_reg(struct bssaif_src_dev *bssaif_src, int offset, u32 data) {
	*((u32*)(bssaif_src->preg+offset)) = data;
}

static inline int bssaif_src_bitget(struct bssaif_src_dev *bssaif_src, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void bssaif_src_bitset(struct bssaif_src_dev *bssaif_src, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, offset);
	reg |= bitmask; 
	write_bssaif_src_reg(bssaif_src, offset, reg);
}

static inline void bssaif_src_bitclr(struct bssaif_src_dev *bssaif_src, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, offset);
	reg &= ~bitmask;
	write_bssaif_src_reg(bssaif_src, offset, reg);
}

void format_date_time(char *date, char *time, char *str){
    char months[12][4] = {"jan\0","feb\0","mar\0","apr\0","may\0","jun\0","jul\0","aug\0","sep\0","oct\0","nov\0","dec\0"};
    char year[5];
    char month[4];
    char day[3];
    char temp[4];
    int i;

    strncpy(year, date+7, 5);       // year
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
    strncpy(day, date+4, 2);        // day
    day[2] = 0;
    if (isspace(day[0]) != 0){
        day[0] = '0';
    }
    
    // format str
    strcpy(str, year); strcat(str, ":");
    strcat(str, month); strcat(str, ":");
    strcat(str, day); strcat(str, ":");
    strcat(str, time);
}

static ssize_t build_time_show(struct device *dev, struct device_attribute *attr, char *buf) {
	char build_time[20];
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdate-time"
		format_date_time(__DATE__, __TIME__, build_time);
	#pragma GCC diagnostic pop
	return sprintf(buf, "%s\n",build_time);
}
static	DEVICE_ATTR_RO(build_time);


// DG_SYNC_CTRL
static ssize_t dg_sync_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_SYNC_CTRL_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_sync_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_sync_ctrl");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_SYNC_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_sync_ctrl);

// DG_RATE_CTRL
static ssize_t dg_rate_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_RATE_CTRL_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_rate_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_rate_ctrl");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_RATE_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_rate_ctrl);

// DG_RATE
static ssize_t dg_rate_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_RATE_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_rate");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_RATE_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_rate);


// DG_RATE_OFFSET_LIM
static ssize_t dg_rate_offset_lim_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_RATE_OFFSET_LIM_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_rate_offset_lim_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_rate_offset_lim");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_RATE_OFFSET_LIM_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_rate_offset_lim);


// DG_RATE_PHASE
static ssize_t dg_rate_phase_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_RATE_PHASE_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_rate_phase_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_rate_phase");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_RATE_PHASE_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_rate_phase);


// DG_RATE_RATE
static ssize_t dg_rate_rate_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_RATE_RATE_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_rate_rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_rate_rate");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_RATE_RATE_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_rate_rate);


// DG_RATE_PERIOD
static ssize_t dg_rate_period_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_RATE_PERIOD_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_rate_period_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_rate_period");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_RATE_PERIOD_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_rate_period);


// DG_RATE_NG_SCALE
static ssize_t dg_rate_ng_scale_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_RATE_NG_SCALE_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_rate_ng_scale_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_rate_ng_scale");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_RATE_NG_SCALE_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_rate_ng_scale);


// DG_RATE_NG_PERIOD
static ssize_t dg_rate_ng_period_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_RATE_NG_PERIOD_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_rate_ng_period_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_rate_ng_period");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_RATE_NG_PERIOD_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_rate_ng_period);


// DG_DQM_CTRL
static ssize_t dg_dqm_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_DQM_CTRL_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_dqm_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_dqm_ctrl");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_DQM_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_dqm_ctrl);


// DG_DQM
static ssize_t dg_dqm_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_DQM_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_dqm_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_dqm");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_DQM_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_dqm);


// DG_DQM_OFFSET_LIM
static ssize_t dg_dqm_offset_lim_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_DQM_OFFSET_LIM_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_dqm_offset_lim_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_dqm_offset_lim");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_DQM_OFFSET_LIM_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_dqm_offset_lim);


// DG_DQM_PHASE
static ssize_t dg_dqm_phase_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_DQM_PHASE_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_dqm_phase_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_dqm_phase");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_DQM_PHASE_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_dqm_phase);


// DG_DQM_RATE
static ssize_t dg_dqm_rate_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_DQM_RATE_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_dqm_rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_dqm_rate");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_DQM_RATE_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_dqm_rate);


// DG_DQM_PERIOD
static ssize_t dg_dqm_period_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_DQM_PERIOD_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_dqm_period_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_dqm_period");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_DQM_PERIOD_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_dqm_period);


// DG_DELAY
static ssize_t dg_delay_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_DELAY_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_delay_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_delay");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_DELAY_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_delay);


// DG_TAPS
static ssize_t dg_taps_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_TAPS_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_taps_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_taps");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_TAPS_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_taps);


// DG_PATTERN
static ssize_t dg_pattern_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DG_PATTERN_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_pattern_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dg_pattern");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DG_PATTERN_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_pattern);


// DQE_CTRL
static ssize_t dqe_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_src_reg(bssaif_src, DQE_CTRL_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dqe_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_src->dev, "Error in sysfs/dqe_ctrl");
		return count;
	}
	write_bssaif_src_reg(bssaif_src, DQE_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dqe_ctrl);



static struct attribute *bssaif_src_attrs[] = {
	&dev_attr_build_time.attr,
    &dev_attr_dg_sync_ctrl.attr,
    &dev_attr_dg_rate_ctrl.attr,
    &dev_attr_dg_rate.attr,
    &dev_attr_dg_rate_offset_lim.attr,
    &dev_attr_dg_rate_phase.attr,
    &dev_attr_dg_rate_rate.attr,
    &dev_attr_dg_rate_period.attr,
	&dev_attr_dg_rate_ng_scale.attr,
	&dev_attr_dg_rate_ng_period.attr,
	&dev_attr_dg_dqm_ctrl.attr,
	&dev_attr_dg_dqm.attr,
	&dev_attr_dg_dqm_offset_lim.attr,
	&dev_attr_dg_dqm_phase.attr,
	&dev_attr_dg_dqm_rate.attr,
	&dev_attr_dg_dqm_period.attr,
	&dev_attr_dg_delay.attr,
	&dev_attr_dg_taps.attr,
	&dev_attr_dg_pattern.attr,
	&dev_attr_dqe_ctrl.attr,
	NULL,
};

static struct attribute_group bssaif_src_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = bssaif_src_attrs,
};

static int bssaif_src_parse_of(struct bssaif_src_dev *bssaif_src) {
	struct device *dev = bssaif_src->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &bssaif_src->compatible);
	if (ret < 0) {
		dev_err(dev, "bssaif_src not in DT");
		return ret;
	}

	// reg -- axi lite
	res = platform_get_resource(bssaif_src->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	bssaif_src->bssaif_src_reg_res = res;
		
	dev_info(dev, "Done parsing DT");

	return 0;
}

static int bssaif_src_allocate_resources(struct bssaif_src_dev *bssaif_src) {
    int ret = -EINVAL;
    int addr_size;

    u32 start;
    u32 end;

    // map bssaif_src device memory
    start = (u32)bssaif_src->bssaif_src_reg_res->start;
    end = (u32)bssaif_src->bssaif_src_reg_res->end;
    addr_size = end - start + 1;
    dev_info(bssaif_src->dev, "Mapping BSSAIF_SRC 0x%x, size 0x%x", start, addr_size);
	
    bssaif_src->preg =
	(u32*)devm_ioremap(bssaif_src->dev, bssaif_src->bssaif_src_reg_res->start, addr_size);
    if (bssaif_src->preg == NULL) {
        dev_err(bssaif_src->dev, "Error mapping bssaif_src base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    return 0;

FAIL:
    return ret;
}

static int bssaif_src_probe(struct platform_device *pdev) {
	struct bssaif_src_dev *bssaif_src;
	struct device *dev;
	int ret;

	// bssaif_src
	bssaif_src = devm_kzalloc(&pdev->dev, sizeof(*bssaif_src), GFP_KERNEL);
	if (!bssaif_src)
		return -ENOMEM;

	bssaif_src->dev = &pdev->dev;
	bssaif_src->pdev = pdev;
	dev = bssaif_src->dev;

	dev_set_drvdata(&pdev->dev, bssaif_src);
	
	ret = bssaif_src_parse_of(bssaif_src);
	if (ret < 0) {
		return ret;
    }

	ret = bssaif_src_allocate_resources(bssaif_src);
	if (ret < 0) {
		return ret;
    }

	if (sysfs_create_group(&pdev->dev.kobj, &bssaif_src_attr_group)) {
		dev_err(bssaif_src->dev, "failed to create sysfs entries\n");
    }

	return 0;
}

static void bssaif_src_remove(struct platform_device *pdev) {
	struct bssaif_src_dev *bssaif_src = dev_get_drvdata(&pdev->dev);

	dev_info(bssaif_src->dev, "unmapping io memory");
	devm_iounmap(bssaif_src->dev, bssaif_src->preg);

	sysfs_remove_group(&pdev->dev.kobj, &bssaif_src_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(bssaif_src->dev, "remove complete");
}

static const struct of_device_id bssaif_src_of_id_table[] = {
	{ .compatible = "bssaif_src-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, bssaif_src_of_id_table);

static struct platform_driver bssaif_src_driver = {
	.driver = {
		.name = "bssaif_src",
		.of_match_table = bssaif_src_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = bssaif_src_probe,
	.remove = bssaif_src_remove,
};

module_platform_driver(bssaif_src_driver);

MODULE_AUTHOR("Fiona O'Connell");
MODULE_DESCRIPTION("bssaif_src: 1.0: mlsc source control and status");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("bssaif_src");

