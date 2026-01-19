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

#define BSSAIF_COMMON_DEV_NAME "bssaif_common"

#define CTRL_OFFSET (0)
#define BIT_RST_MASK (1)

#define TOP_BUILD_OFFSET (0)
#define TOP_TIME_OFFSET (1)
#define DG_SYNC_CTRL_OFFSET (2)
#define OUT_CF_CTRL_OFFSET (3)

void format_date_time(char *date, char *time, char *str);

struct bssaif_common_dev {
    struct device *dev;
    struct platform_device *pdev;

    const char *compatible;

    u32 __iomem *preg;
	
    struct resource *bssaif_common_reg_res;
};

// bssaif_common ip core access
static inline u32 read_bssaif_common_reg(struct bssaif_common_dev *bssaif_common, int offset) {
	return *((u32*)(bssaif_common->preg+offset));
}

static inline void write_bssaif_common_reg(struct bssaif_common_dev *bssaif_common, int offset, u32 data) {
	*((u32*)(bssaif_common->preg+offset)) = data;
}

static inline int bssaif_common_bitget(struct bssaif_common_dev *bssaif_common, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssaif_common_reg(bssaif_common, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void bssaif_common_bitset(struct bssaif_common_dev *bssaif_common, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssaif_common_reg(bssaif_common, offset);
	reg |= bitmask; 
	write_bssaif_common_reg(bssaif_common, offset, reg);
}

static inline void bssaif_common_bitclr(struct bssaif_common_dev *bssaif_common, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssaif_common_reg(bssaif_common, offset);
	reg &= ~bitmask;
	write_bssaif_common_reg(bssaif_common, offset, reg);
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


// TOP_BUILD
static ssize_t top_build_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_common_dev *bssaif_common = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssaif_common_reg(bssaif_common, TOP_BUILD_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(top_build);

// TOP_TIME
static ssize_t top_time_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_common_dev *bssaif_common = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_common_reg(bssaif_common, TOP_TIME_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(top_time);

// DG_SYNC_CTRL
static ssize_t dg_sync_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_common_dev *bssaif_common = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssaif_common_reg(bssaif_common, DG_SYNC_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dg_sync_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_common_dev *bssaif_common = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_common->dev, "Error in sysfs/dg_sync_ctrl");
		return count;
	}
    write_bssaif_common_reg(bssaif_common, DG_SYNC_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dg_sync_ctrl);

// OUT_CF_CTRL
static ssize_t out_cf_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_common_dev *bssaif_common = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssaif_common_reg(bssaif_common, OUT_CF_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t out_cf_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_common_dev *bssaif_common = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_common->dev, "Error in sysfs/out_cf_ctrl");
		return count;
	}
    write_bssaif_common_reg(bssaif_common, OUT_CF_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(out_cf_ctrl);

static struct attribute *bssaif_common_attrs[] = {
    &dev_attr_build_time.attr,
    &dev_attr_top_build.attr,
    &dev_attr_top_time.attr,
    &dev_attr_dg_sync_ctrl.attr,
    &dev_attr_out_cf_ctrl.attr,
	NULL,
};

static struct attribute_group bssaif_common_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = bssaif_common_attrs,
};

static int bssaif_common_parse_of(struct bssaif_common_dev *bssaif_common) {
	struct device *dev = bssaif_common->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &bssaif_common->compatible);
	if (ret < 0) {
		dev_err(dev, "bssaif_common not in DT");
		return ret;
	}

	// reg -- axi lite
	res = platform_get_resource(bssaif_common->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	bssaif_common->bssaif_common_reg_res = res;
		
	dev_info(dev, "Done parsing DT");

	return 0;
}

static int bssaif_common_allocate_resources(struct bssaif_common_dev *bssaif_common) {
    int ret = -EINVAL;
    int addr_size;

    u32 start;
    u32 end;

    // map bssaif_common device memory
    start = (u32)bssaif_common->bssaif_common_reg_res->start;
    end = (u32)bssaif_common->bssaif_common_reg_res->end;
    addr_size = end - start + 1;
    dev_info(bssaif_common->dev, "Mapping BSSAIF_COMMON 0x%x, size 0x%x", start, addr_size);
	
    bssaif_common->preg =
	(u32*)devm_ioremap(bssaif_common->dev, bssaif_common->bssaif_common_reg_res->start, addr_size);
    if (bssaif_common->preg == NULL) {
        dev_err(bssaif_common->dev, "Error mapping bssaif_common base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    return 0;

FAIL:
    return ret;
}

static int bssaif_common_probe(struct platform_device *pdev) {
	struct bssaif_common_dev *bssaif_common;
	struct device *dev;
	int ret;

	// bssaif_common
	bssaif_common = devm_kzalloc(&pdev->dev, sizeof(*bssaif_common), GFP_KERNEL);
	if (!bssaif_common)
		return -ENOMEM;

	bssaif_common->dev = &pdev->dev;
	bssaif_common->pdev = pdev;
	dev = bssaif_common->dev;

	dev_set_drvdata(&pdev->dev, bssaif_common);
	
	ret = bssaif_common_parse_of(bssaif_common);
	if (ret < 0) {
		return ret;
    }

	ret = bssaif_common_allocate_resources(bssaif_common);
	if (ret < 0) {
		return ret;
    }

	if (sysfs_create_group(&pdev->dev.kobj, &bssaif_common_attr_group)) {
		dev_err(bssaif_common->dev, "failed to create sysfs entries\n");
    }

	return 0;
}

static void bssaif_common_remove(struct platform_device *pdev) {
	struct bssaif_common_dev *bssaif_common = dev_get_drvdata(&pdev->dev);

	dev_info(bssaif_common->dev, "unmapping io memory");
	devm_iounmap(bssaif_common->dev, bssaif_common->preg);

	sysfs_remove_group(&pdev->dev.kobj, &bssaif_common_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(bssaif_common->dev, "remove complete");
}

static const struct of_device_id bssaif_common_of_id_table[] = {
	{ .compatible = "bssaif_common-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, bssaif_common_of_id_table);

static struct platform_driver bssaif_common_driver = {
	.driver = {
		.name = "bssaif_common",
		.of_match_table = bssaif_common_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = bssaif_common_probe,
	.remove = bssaif_common_remove,
};

module_platform_driver(bssaif_common_driver);

MODULE_AUTHOR("Fiona O'Connell");
MODULE_DESCRIPTION("bssaif_common: 1.0: mlsc ip control and status (common component)");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("bssaif_common");

