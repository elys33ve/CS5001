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

#define CLOCKTRACK_DEV_NAME "clocktrack"

// clockgen accumulator width
// hard-coded in vhdl (32 bits)
#define ACCUM_SHIFT 32

// clocktrack registers
#define CTRL_OFFSET (0)

#define AVG0_OFFSET (2)
#define AVG1_OFFSET (3)
#define AVG2_OFFSET (4)
#define AVG3_OFFSET (5)

#define ADJ0_OFFSET (6)
#define ADJ1_OFFSET (7)
#define ADJ2_OFFSET (8)
#define ADJ3_OFFSET (9)

#define DMABUF0_OFFSET (10)
#define DMABUF1_OFFSET (11)
#define DMABUF2_OFFSET (12)
#define DMABUF3_OFFSET (13)

#define SCALE0_OFFSET (14)
#define SCALE1_OFFSET (15)
#define SCALE2_OFFSET (16)
#define SCALE3_OFFSET (17)

#define SETPOINT_CENTER_OFFSET (18)
#define SETPOINT_OFFSET_OFFSET (19)

#define CENTER0_ADJUSTED_OFFSET (20)
#define OFFSET0_ADJUSTED_OFFSET (21)
#define HI0_ADJUSTED_OFFSET (22)
#define LOW0_ADJUSTED_OFFSET (23)

#define TRACKER_MIN_CENTER_OFFSET (24)
#define TRACKER_MIN_OFFSET_OFFSET (25)

#define VERSION_OFFSET (31)

void format_date_time(char *date, char *time, char *str);

struct  clocktrack_dev {
    struct device *dev;
    struct platform_device *pdev;

    const char *compatible;

    u32 __iomem *preg;

    u32 refclk_hz;
    u32 setpoint_center_base;
    u32 setpoint_offset_base;
    u32 tracker_min_center_base;
    u32 tracker_min_offset_base;

    struct resource *clocktrack_reg_res;
};

/////////////////////////////////////////////////////////////////////////////////////////
// register and bit accessors
/////////////////////////////////////////////////////////////////////////////////////////
static inline u32 read_clocktrack_reg(struct clocktrack_dev *clocktrack,
				      int offset) {
    return ioread32(clocktrack->preg+offset);
}
static inline void write_clocktrack_reg(struct clocktrack_dev *clocktrack,
					int offset, u32 data) {
    iowrite32(data, clocktrack->preg+offset);
}
static inline int clocktrack_bitget(struct clocktrack_dev *clocktrack,
				    int offset, u32 bitmask) {
    u32 reg;
	
    reg = read_clocktrack_reg(clocktrack, offset);
    return (reg & bitmask) ? 1 : 0;
}
static inline void clocktrack_bitset(struct clocktrack_dev *clocktrack,
				     int offset, u32 bitmask) {
    u32 reg;
    u32 reg_set;
	
    reg = read_clocktrack_reg(clocktrack, offset);
    reg_set = reg | bitmask;
    write_clocktrack_reg(clocktrack, offset, reg_set);
}
static inline void clocktrack_bitclr(struct clocktrack_dev *clocktrack,
				     int offset, u32 bitmask) {
    u32 reg;
    u32 reg_clr;
	
    reg = read_clocktrack_reg(clocktrack, offset);
    reg_clr = reg & ~bitmask;
    write_clocktrack_reg(clocktrack, offset, reg_clr);
}
static inline void clocktrack_bitwrite(struct clocktrack_dev *clocktrack,
				       int offset, u32 bitmask, int value) {
    if (value)
        clocktrack_bitset(clocktrack, offset, bitmask);
    else
        clocktrack_bitclr(clocktrack, offset, bitmask);
}
static inline void clocktrack_bittoggle(struct clocktrack_dev *clocktrack,
					int offset, u32 bitmask) {
    clocktrack_bitset(clocktrack, offset, bitmask);
    clocktrack_bitclr(clocktrack, offset, bitmask);
}

/////////////////////////////////////////////////////////////////////////////////////////
// helper functions
/////////////////////////////////////////////////////////////////////////////////////////
static s32 step_to_rate(struct clocktrack_dev *clocktrack, s32 step_in) {
    s32 rate;
    s32 refclk = clocktrack->refclk_hz;
    s64 dividend;
    dividend = (s64)step_in * (s64)refclk;
    rate = (s32)(dividend >> ACCUM_SHIFT);
    return (s32)rate;
}

void format_date_time(char *date, char *time, char *str) {
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

/////////////////////////////////////////////////////////////////////////////////////////
// sysfs
/////////////////////////////////////////////////////////////////////////////////////////
static ssize_t center0_adj_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, CENTER0_ADJUSTED_OFFSET));
}
static	DEVICE_ATTR_RO(center0_adj);

static ssize_t offset0_adj_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, OFFSET0_ADJUSTED_OFFSET));
}
static	DEVICE_ATTR_RO(offset0_adj);

static ssize_t hi0_adj_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, HI0_ADJUSTED_OFFSET));
}
static	DEVICE_ATTR_RO(hi0_adj);

static ssize_t low0_adj_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, LOW0_ADJUSTED_OFFSET));
}
static	DEVICE_ATTR_RO(low0_adj);

static ssize_t avg0_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, AVG0_OFFSET));
}
static	DEVICE_ATTR_RO(avg0);

static ssize_t avg1_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, AVG1_OFFSET));
}
static	DEVICE_ATTR_RO(avg1);

static ssize_t avg2_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, AVG2_OFFSET));
}
static	DEVICE_ATTR_RO(avg2);

static ssize_t avg3_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, AVG3_OFFSET));
}
static	DEVICE_ATTR_RO(avg3);

static ssize_t adj0_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%d Hz\n", step_to_rate(
                       clocktrack, read_clocktrack_reg(clocktrack, ADJ0_OFFSET)));
}
static	DEVICE_ATTR_RO(adj0);

static ssize_t adj1_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%d Hz\n", step_to_rate(
                       clocktrack, read_clocktrack_reg(clocktrack, ADJ1_OFFSET)));
}
static	DEVICE_ATTR_RO(adj1);

static ssize_t adj2_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%d Hz\n", step_to_rate(
                       clocktrack, read_clocktrack_reg(clocktrack, ADJ2_OFFSET)));
}
static	DEVICE_ATTR_RO(adj2);

static ssize_t adj3_show(struct device *dev, struct device_attribute *attr,
			 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%d Hz\n", step_to_rate(
                       clocktrack, read_clocktrack_reg(clocktrack, ADJ3_OFFSET)));
}
static	DEVICE_ATTR_RO(adj3);

static ssize_t dmabuf0_show(struct device *dev, struct device_attribute *attr,
			    char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, DMABUF0_OFFSET));
}
static	DEVICE_ATTR_RO(dmabuf0);

static ssize_t dmabuf1_show(struct device *dev, struct device_attribute *attr,
			    char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, DMABUF1_OFFSET));
}
static	DEVICE_ATTR_RO(dmabuf1);

static ssize_t dmabuf2_show(struct device *dev, struct device_attribute *attr,
			    char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, DMABUF2_OFFSET));
}
static	DEVICE_ATTR_RO(dmabuf2);

static ssize_t dmabuf3_show(struct device *dev, struct device_attribute *attr,
			    char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, DMABUF3_OFFSET));
}
static	DEVICE_ATTR_RO(dmabuf3);

static ssize_t setpoint_center_show(struct device *dev, struct device_attribute *attr,
				 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", clocktrack->setpoint_center_base);
}
static ssize_t setpoint_center_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
	dev_info(clocktrack->dev, "Error in sysfs/setpoint_center");
	return count;
    }
    clocktrack->setpoint_center_base = val;
    write_clocktrack_reg(clocktrack, SETPOINT_CENTER_OFFSET, val);
    return count;
}
static	DEVICE_ATTR_RW(setpoint_center);

static ssize_t setpoint_offset_show(struct device *dev, struct device_attribute *attr,
				 char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", clocktrack->setpoint_offset_base);
}
static ssize_t setpoint_offset_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
	dev_info(clocktrack->dev, "Error in sysfs/setpoint_offset");
	return count;
    }
    clocktrack->setpoint_offset_base = val;
    write_clocktrack_reg(clocktrack, SETPOINT_OFFSET_OFFSET, val);
    return count;
}
static	DEVICE_ATTR_RW(setpoint_offset);

static ssize_t build_time_show(struct device *dev, struct device_attribute *attr,
			       char *buf) {
    char build_time[20];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdate-time"
    format_date_time(__DATE__, __TIME__, build_time);
#pragma GCC diagnostic pop
    return sprintf(buf, "%s\n", build_time);
}
static	DEVICE_ATTR_RO(build_time);

static ssize_t scale0_show(struct device *dev, struct device_attribute *attr,
			    char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, SCALE0_OFFSET));
}
static	DEVICE_ATTR_RO(scale0);

static ssize_t scale1_show(struct device *dev, struct device_attribute *attr,
			    char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, SCALE1_OFFSET));
}
static	DEVICE_ATTR_RO(scale1);

static ssize_t scale2_show(struct device *dev, struct device_attribute *attr,
			    char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, SCALE2_OFFSET));
}
static	DEVICE_ATTR_RO(scale2);

static ssize_t scale3_show(struct device *dev, struct device_attribute *attr,
			    char *buf) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_clocktrack_reg(clocktrack, SCALE3_OFFSET));
}
static	DEVICE_ATTR_RO(scale3);

static struct attribute *clocktrack_attrs[] = {
    &dev_attr_center0_adj.attr,
    &dev_attr_offset0_adj.attr,
    &dev_attr_hi0_adj.attr,
    &dev_attr_low0_adj.attr,
    &dev_attr_avg0.attr,
    &dev_attr_avg1.attr,
    &dev_attr_avg2.attr,
    &dev_attr_avg3.attr,
    &dev_attr_adj0.attr,
    &dev_attr_adj1.attr,
    &dev_attr_adj2.attr,
    &dev_attr_adj3.attr,
    &dev_attr_dmabuf0.attr,
    &dev_attr_dmabuf1.attr,
    &dev_attr_dmabuf2.attr,
    &dev_attr_dmabuf3.attr,
    &dev_attr_setpoint_center.attr,
    &dev_attr_setpoint_offset.attr,
    &dev_attr_build_time.attr,
    &dev_attr_scale0.attr,
    &dev_attr_scale1.attr,
    &dev_attr_scale2.attr,
    &dev_attr_scale3.attr,
    NULL,
};

static struct attribute_group clocktrack_attr_group = {
    .name = NULL, /* put in device directory */
    .attrs = clocktrack_attrs,
};

static int clocktrack_parse_of(struct clocktrack_dev *clocktrack)
{
    struct device *dev = clocktrack->dev;
    struct device_node *node = dev->of_node;

    struct resource *res;

    int ret;

    // of node
    ret = of_property_read_string(node, "compatible", &clocktrack->compatible);
    if (ret < 0) {
        dev_err(dev, "clocktrack not in DT");
        return ret;
    }

    // reg
    res = platform_get_resource(clocktrack->pdev, IORESOURCE_MEM, 0);
    if (res == NULL) {
        dev_err(dev, "error getting reg addr from dts");
        return -EINVAL;
    }
    clocktrack->clocktrack_reg_res = res;
		
    // refclk_hz
    ret = of_property_read_u32(node, "refclk_hz", &clocktrack->refclk_hz);
    if (ret < 0) {
        dev_err(dev, "error parsing refclk_hz");
        return ret;
    }

    // setpoints
    ret = of_property_read_u32(node, "setpointcenter", &clocktrack->setpoint_center_base);
    if (ret < 0) {
        dev_err(dev, "error parsing setpointcenter");
        return ret;
    }
    ret = of_property_read_u32(node, "setpointoffset", &clocktrack->setpoint_offset_base);
    if (ret < 0) {
        dev_err(dev, "error parsing setpointoffset");
        return ret;
    }

    // min center/offset (clock tracker)
    ret = of_property_read_u32(node, "mincenter", &clocktrack->tracker_min_center_base);
    if (ret < 0) {
        dev_err(dev, "error parsing mincenter");
        return ret;
    }
    ret = of_property_read_u32(node, "minoffset", &clocktrack->tracker_min_offset_base);
    if (ret < 0) {
        dev_err(dev, "error parsing minoffset");
        return ret;
    }

    dev_info(dev, "Done parsing DT");

    return 0;
}

static int clocktrack_allocate_resources(struct clocktrack_dev *clocktrack) {
    int ret = -EINVAL;
    int addr_size;

    u32 start;
    u32 end;

    // map clocktrack device memory
    start = (u32)clocktrack->clocktrack_reg_res->start;
    end = (u32)clocktrack->clocktrack_reg_res->end;
    addr_size = end - start + 1;
    dev_info(clocktrack->dev, "Mapping CLOCKTRACK 0x%x, size 0x%x", start, addr_size);
	
    clocktrack->preg =
	(u32*)devm_ioremap(clocktrack->dev,
			   clocktrack->clocktrack_reg_res->start,
			   addr_size);
    if (clocktrack->preg == NULL) {
	dev_err(clocktrack->dev, "Error mapping clocktrack base memory");
	ret = -ENOMEM;
	goto FAIL;
    }

    // setpoint center and offset
    write_clocktrack_reg(clocktrack, SETPOINT_CENTER_OFFSET, clocktrack->setpoint_center_base);
    write_clocktrack_reg(clocktrack, SETPOINT_OFFSET_OFFSET, clocktrack->setpoint_offset_base);

    // min center and offset (tracker)
    write_clocktrack_reg(clocktrack, TRACKER_MIN_CENTER_OFFSET, clocktrack->tracker_min_center_base);
    write_clocktrack_reg(clocktrack, TRACKER_MIN_OFFSET_OFFSET, clocktrack->tracker_min_offset_base);

    // start enabled
    write_clocktrack_reg(clocktrack, CTRL_OFFSET, 0);
    return 0;

FAIL:
    return ret;
}

static int clocktrack_probe(struct platform_device *pdev) {
    struct clocktrack_dev *clocktrack;
    struct device *dev;
    int ret;

    // clocktrack
    clocktrack = devm_kzalloc(&pdev->dev, sizeof(*clocktrack), GFP_KERNEL);
    if (!clocktrack)
	return -ENOMEM;

    clocktrack->dev = &pdev->dev;
    clocktrack->pdev = pdev;
    dev = clocktrack->dev;

    dev_set_drvdata(&pdev->dev, clocktrack);
	
    ret = clocktrack_parse_of(clocktrack);
    if (ret < 0)
	return ret;

    ret = clocktrack_allocate_resources(clocktrack);
    if (ret < 0)
	return ret;
    if (sysfs_create_group(&pdev->dev.kobj, &clocktrack_attr_group))
	dev_err(clocktrack->dev, "failed to create sysfs entries\n");

    return 0;
}

static void clocktrack_remove(struct platform_device *pdev) {
    struct clocktrack_dev *clocktrack = dev_get_drvdata(&pdev->dev);

    dev_info(clocktrack->dev, "unmapping io memory");
    devm_iounmap(clocktrack->dev, clocktrack->preg);

    sysfs_remove_group(&pdev->dev.kobj, &clocktrack_attr_group);

    dev_set_drvdata(&pdev->dev, NULL);
    dev_info(clocktrack->dev, "remove complete");
}

static const struct of_device_id clocktrack_of_id_table[] = {
    { .compatible = "clocktrack-1.0" },
    {}
};

MODULE_DEVICE_TABLE(of, clocktrack_of_id_table);

static struct platform_driver clocktrack_driver = {
    .driver = {
	.name = "clocktrack",
	.of_match_table = clocktrack_of_id_table,
	.owner = THIS_MODULE,
    },
    .probe = clocktrack_probe,
    .remove = clocktrack_remove,
};

module_platform_driver(clocktrack_driver);

MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("clocktrack: 1.0: monitor the pcmout clock tracker");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("clocktrack");

