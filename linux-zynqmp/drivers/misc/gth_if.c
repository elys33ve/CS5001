#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/ctype.h>

#define GTH_IF_DEV_NAME "gth_if"

// Registers
#define CTRL_OFFSET (0)
#define STATUS_OFFSET (1)

// CTRL BITS
#define NRST_BIT (1)

// STATUS BITS
#define CHANNEL_UP_BIT (1)
#define LANE_UP_MASK 0x0F000
#define LANE_UP_SHIFT 12
#define GT_PLL_LOCK_BIT (1<<16)
#define MMCM_NOT_LOCKED_BIT (1<<17)
#define SOFT_ERR_BIT (1<<18)
#define HARD_ERR_BIT (1<<19)

void format_date_time(char *date, char *time, char *str);

struct  gth_if_dev {
	struct device *dev;
	struct platform_device *pdev;

	const char *compatible;
	u32 __iomem *preg;
	struct resource *gth_if_reg_res;

    struct mutex reg_mutex;
};

// gth_if ip core access
static inline u32 read_gth_if_reg(struct gth_if_dev *gth_if,
					 int offset) {
    return ioread32(gth_if->preg+offset);
}

static inline void write_gth_if_reg(struct gth_if_dev *gth_if,
					   int offset, u32 data) {
    iowrite32(data, gth_if->preg+offset);
}

static inline int gth_if_bitget(struct gth_if_dev *gth_if,
				      u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_gth_if_reg(gth_if, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void gth_if_bitset(struct gth_if_dev *gth_if,
				       u32 offset, u32 bitmask) {
	u32 reg;
	
    mutex_lock(&gth_if->reg_mutex);
	reg = read_gth_if_reg(gth_if, offset);
	reg |= bitmask;
	write_gth_if_reg(gth_if, offset, reg);
    mutex_unlock(&gth_if->reg_mutex);
}

static inline void gth_if_bitclr(struct gth_if_dev *gth_if,
				       u32 offset, u32 bitmask) {
	u32 reg;
	
    mutex_lock(&gth_if->reg_mutex);
	reg = read_gth_if_reg(gth_if, offset);
	reg &= ~bitmask;
	write_gth_if_reg(gth_if, offset, reg);
    mutex_unlock(&gth_if->reg_mutex);
}

// add a build time to the sysfs vars
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

static ssize_t reset_gth_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct gth_if_dev *gth_if = dev_get_drvdata(dev);
    dev_info(gth_if->dev, "resetting gth");
    // GTH reset sequence
    gth_if_bitclr(gth_if, CTRL_OFFSET, NRST_BIT);
    msleep(1);
    gth_if_bitset(gth_if, CTRL_OFFSET, NRST_BIT);
	return count;
}
static	DEVICE_ATTR_WO(reset_gth);

static ssize_t status_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct gth_if_dev *gth_if = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_gth_if_reg(gth_if, STATUS_OFFSET);
    return sprintf(buf, "status:\n\t"
                   "channel_up: %d\n\t"
                   "lane up: 0x%x\n\t"
                   "gt pll lock: %d\n\t"
                   "mmcm not locked: %d\n\t"
                   "soft err: %d\n\t"
                   "hard_err: %u\n",
                   reg & CHANNEL_UP_BIT ? 1 : 0,
                   (reg & LANE_UP_MASK)>>LANE_UP_SHIFT,
                   reg & GT_PLL_LOCK_BIT ? 1 : 0,
                   reg & MMCM_NOT_LOCKED_BIT ? 1 : 0,
                   reg & SOFT_ERR_BIT ? 1 : 0,
                   reg & HARD_ERR_BIT ? 1 : 0);
}
static	DEVICE_ATTR_RO(status);

static ssize_t gth_up_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct gth_if_dev *gth_if = dev_get_drvdata(dev);
    unsigned reg;
    int up = 1;
    int channel_up;
    int lane_up;
    int gt_pll_lock;
    int mmcm_not_locked;
    int soft_err;
    int hard_err;
    reg = read_gth_if_reg(gth_if, STATUS_OFFSET);
    channel_up = reg & CHANNEL_UP_BIT ? 1 : 0;
    lane_up = (reg & LANE_UP_MASK)>>LANE_UP_SHIFT;
    gt_pll_lock = reg & GT_PLL_LOCK_BIT ? 1 : 0;
    mmcm_not_locked = reg & MMCM_NOT_LOCKED_BIT ? 1 : 0;
    soft_err = reg & SOFT_ERR_BIT ? 1 : 0;
    hard_err = reg & HARD_ERR_BIT ? 1 : 0;

    if ( (!gt_pll_lock) )
        up = 0;
    if (!channel_up || (lane_up != 0x0F))
        up = 0;
    if (soft_err || hard_err || mmcm_not_locked)
        up = 0;

    if (up)
        return sprintf(buf, "1\n");
    else
        return sprintf(buf, "0\n");
}
static	DEVICE_ATTR_RO(gth_up);

static struct attribute *gth_if_attrs[] = {
	&dev_attr_build_time.attr,
	&dev_attr_reset_gth.attr,
	&dev_attr_status.attr,
	&dev_attr_gth_up.attr,
	NULL,
};

static struct attribute_group gth_if_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = gth_if_attrs,
};

static int gth_if_parse_of(struct gth_if_dev *gth_if)
{
	struct device *dev = gth_if->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &gth_if->compatible);
	if (ret < 0) {
		dev_err(dev, "gth_if not in DT");
		return ret;
	}

	// reg
	res = platform_get_resource(gth_if->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	gth_if->gth_if_reg_res = res;

	dev_info(dev, "Done parsing DT");

	return 0;
}

static int gth_if_allocate_resources(struct gth_if_dev *gth_if) {
	int ret = -EINVAL;
	int addr_size;

	u32 start;
	u32 end;

        // map gth_if device memory
	start = (u32)gth_if->gth_if_reg_res->start;
	end = (u32)gth_if->gth_if_reg_res->end;
	addr_size = end - start + 1;
	
	gth_if->preg =
		(u32*)devm_ioremap(gth_if->dev,
				   gth_if->gth_if_reg_res->start,
				   addr_size);
	if (gth_if->preg == NULL) {
		dev_err(gth_if->dev, "Error mapping gth_if base memory");
		ret = -ENOMEM;
		goto FAIL;
	}

    // init mutex
    mutex_init(&gth_if->reg_mutex);

    msleep(50);

    // gth reset is now released in u-boot
	//dev_info(gth_if->dev, "Resetting GTH");
    //write_gth_if_reg(gth_if, CTRL_OFFSET, 0);
    //gth_if_bitset(gth_if, CTRL_OFFSET, NRST_BIT);
    
	return 0;

FAIL:
	return ret;
}

static int gth_if_probe(struct platform_device *pdev) {
	struct gth_if_dev *gth_if;
	struct device *dev;
	int ret;

	// gth_if
	gth_if = devm_kzalloc(&pdev->dev, sizeof(*gth_if), GFP_KERNEL);
	if (!gth_if)
		return -ENOMEM;

	gth_if->dev = &pdev->dev;
	gth_if->pdev = pdev;
	dev = gth_if->dev;

	dev_set_drvdata(&pdev->dev, gth_if);
	
	ret = gth_if_parse_of(gth_if);
	if (ret < 0)
		return ret;

	ret = gth_if_allocate_resources(gth_if);
	if (ret < 0)
		return ret;

	if (sysfs_create_group(&pdev->dev.kobj, &gth_if_attr_group))
		dev_err(gth_if->dev, "failed to create sysfs entries\n");

	return 0;
}

static void gth_if_remove(struct platform_device *pdev) {
	struct gth_if_dev *gth_if = dev_get_drvdata(&pdev->dev);

	dev_info(gth_if->dev, "unmapping io memory");
	devm_iounmap(gth_if->dev, gth_if->preg);

    mutex_destroy(&gth_if->reg_mutex);

	sysfs_remove_group(&pdev->dev.kobj, &gth_if_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(gth_if->dev, "remove complete");
}

static const struct of_device_id gth_if_of_id_table[] = {
	{ .compatible = "gth_if-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, gth_if_of_id_table);

static struct platform_driver gth_if_driver = {
	.driver = {
		.name = "gth_if",
		.of_match_table = gth_if_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = gth_if_probe,
	.remove = gth_if_remove,
};

module_platform_driver(gth_if_driver);

MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("gth_if: 1.0: zynqmp gth control");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("gth_if");

