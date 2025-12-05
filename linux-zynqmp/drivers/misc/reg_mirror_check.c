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

#define REG_MIRROR_CHECK_DEV_NAME "reg_mirror_check"

// Registers
#define CTRL_OFFSET (0)
#define CNTTO_OFFSET (1)
#define COMMON_OFFSET (2)
#define SELECTOR1_OFFSET (3)
#define SELECTOR2_OFFSET (4)
#define SELECTOR3_OFFSET (5)
#define SELECTOR4_OFFSET (6)
#define SRC1_OFFSET (7)
#define SRC2_OFFSET (8)
#define SRC3_OFFSET (9)
#define SRC4_OFFSET (10)
#define SRC5_OFFSET (11)
#define SRC6_OFFSET (12)
#define SRC7_OFFSET (13)
#define SRC8_OFFSET (14)
#define SRC9_OFFSET (15)
#define SRC10_OFFSET (16)
#define SRC11_OFFSET (17)
#define SRC12_OFFSET (18)

#define RST_MASK (1)

#define MLSC_SHIFT (16)
#define CNT_MASK (0x0FFFF)

void format_date_time(char *date, char *time, char *str);

struct  reg_mirror_check_dev {
	struct device *dev;
	struct platform_device *pdev;

	const char *compatible;
	u32 __iomem *preg;
	struct resource *reg_mirror_check_reg_res;
};

// reg_mirror_check ip core access
static inline u32 read_reg_mirror_check_reg(struct reg_mirror_check_dev *reg_mirror_check,
					 int offset) {
    return ioread32(reg_mirror_check->preg+offset);
}

static inline void write_reg_mirror_check_reg(struct reg_mirror_check_dev *reg_mirror_check,
					   int offset, u32 data) {
    iowrite32(data, reg_mirror_check->preg+offset);
}

static inline int reg_mirror_check_bitget(struct reg_mirror_check_dev *reg_mirror_check,
				      u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_reg_mirror_check_reg(reg_mirror_check, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void reg_mirror_check_bitset(struct reg_mirror_check_dev *reg_mirror_check,
				       u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_reg_mirror_check_reg(reg_mirror_check, offset);
	reg |= bitmask;
	write_reg_mirror_check_reg(reg_mirror_check, offset, reg);
}

static inline void reg_mirror_check_bitclr(struct reg_mirror_check_dev *reg_mirror_check,
				       u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_reg_mirror_check_reg(reg_mirror_check, offset);
	reg &= ~bitmask;
	write_reg_mirror_check_reg(reg_mirror_check, offset, reg);
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

static ssize_t cntto_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, CNTTO_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static ssize_t cntto_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(reg_mirror_check->dev, "Error in sysfs/cntto");
        return count;
    }
    write_reg_mirror_check_reg(reg_mirror_check, CNTTO_OFFSET, val);
    return count;
}
static	DEVICE_ATTR_RW(cntto);

static ssize_t rst_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, CTRL_OFFSET);
    return sprintf(buf, "%u\n", (reg & RST_MASK));
}
static ssize_t rst_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(reg_mirror_check->dev, "Error in sysfs/rst");
        return count;
    }
    write_reg_mirror_check_reg(reg_mirror_check, CTRL_OFFSET, val ? 1 : 0);
    return count;
}
static	DEVICE_ATTR_RW(rst);

static ssize_t bssa_common_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, COMMON_OFFSET);
    return sprintf(buf, "%u\n", (reg & CNT_MASK));
}
static	DEVICE_ATTR_RO(bssa_common);

static ssize_t mlsc_common_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, COMMON_OFFSET);
    return sprintf(buf, "%u\n", ((reg>>MLSC_SHIFT) & CNT_MASK));
}
static	DEVICE_ATTR_RO(mlsc_common);

static ssize_t bssa_selector1_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SELECTOR1_OFFSET);
    return sprintf(buf, "%u\n", (reg & CNT_MASK));
}
static	DEVICE_ATTR_RO(bssa_selector1);

static ssize_t mlsc_selector1_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SELECTOR1_OFFSET);
    return sprintf(buf, "%u\n", ((reg>>MLSC_SHIFT) & CNT_MASK));
}
static	DEVICE_ATTR_RO(mlsc_selector1);

static ssize_t bssa_selector2_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SELECTOR2_OFFSET);
    return sprintf(buf, "%u\n", (reg & CNT_MASK));
}
static	DEVICE_ATTR_RO(bssa_selector2);

static ssize_t mlsc_selector2_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SELECTOR2_OFFSET);
    return sprintf(buf, "%u\n", ((reg>>MLSC_SHIFT) & CNT_MASK));
}
static	DEVICE_ATTR_RO(mlsc_selector2);

static ssize_t bssa_selector3_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SELECTOR3_OFFSET);
    return sprintf(buf, "%u\n", (reg & CNT_MASK));
}
static	DEVICE_ATTR_RO(bssa_selector3);

static ssize_t mlsc_selector3_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SELECTOR3_OFFSET);
    return sprintf(buf, "%u\n", ((reg>>MLSC_SHIFT) & CNT_MASK));
}
static	DEVICE_ATTR_RO(mlsc_selector3);

static ssize_t bssa_selector4_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SELECTOR4_OFFSET);
    return sprintf(buf, "%u\n", (reg & CNT_MASK));
}
static	DEVICE_ATTR_RO(bssa_selector4);

static ssize_t mlsc_selector4_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SELECTOR4_OFFSET);
    return sprintf(buf, "%u\n", ((reg>>MLSC_SHIFT) & CNT_MASK));
}
static	DEVICE_ATTR_RO(mlsc_selector4);

static ssize_t bssa_src1_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC1_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src1);

static ssize_t bssa_src2_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC2_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src2);

static ssize_t bssa_src3_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC3_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src3);

static ssize_t bssa_src4_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC4_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src4);

static ssize_t bssa_src5_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC5_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src5);

static ssize_t bssa_src6_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC6_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src6);

static ssize_t bssa_src7_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC7_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src7);

static ssize_t bssa_src8_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC8_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src8);

static ssize_t bssa_src9_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC9_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src9);

static ssize_t bssa_src10_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC10_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src10);

static ssize_t bssa_src11_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC11_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src11);

static ssize_t bssa_src12_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_reg_mirror_check_reg(reg_mirror_check, SRC12_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(bssa_src12);

static struct attribute *reg_mirror_check_attrs[] = {
	&dev_attr_build_time.attr,
	&dev_attr_cntto.attr,
	&dev_attr_rst.attr,
	&dev_attr_bssa_common.attr,
	&dev_attr_mlsc_common.attr,
	&dev_attr_bssa_selector1.attr,
	&dev_attr_mlsc_selector1.attr,
	&dev_attr_bssa_selector2.attr,
	&dev_attr_mlsc_selector2.attr,
	&dev_attr_bssa_selector3.attr,
	&dev_attr_mlsc_selector3.attr,
	&dev_attr_bssa_selector4.attr,
	&dev_attr_mlsc_selector4.attr,
	&dev_attr_bssa_src1.attr,
	&dev_attr_bssa_src2.attr,
	&dev_attr_bssa_src3.attr,
	&dev_attr_bssa_src4.attr,
	&dev_attr_bssa_src5.attr,
	&dev_attr_bssa_src6.attr,
	&dev_attr_bssa_src7.attr,
	&dev_attr_bssa_src8.attr,
	&dev_attr_bssa_src9.attr,
	&dev_attr_bssa_src10.attr,
	&dev_attr_bssa_src11.attr,
	&dev_attr_bssa_src12.attr,
	NULL,
};

static struct attribute_group reg_mirror_check_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = reg_mirror_check_attrs,
};

static int reg_mirror_check_parse_of(struct reg_mirror_check_dev *reg_mirror_check)
{
	struct device *dev = reg_mirror_check->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &reg_mirror_check->compatible);
	if (ret < 0) {
		dev_err(dev, "reg_mirror_check not in DT");
		return ret;
	}

	// reg
	res = platform_get_resource(reg_mirror_check->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	reg_mirror_check->reg_mirror_check_reg_res = res;

	dev_info(dev, "Done parsing DT");

	return 0;
}

static int reg_mirror_check_allocate_resources(struct reg_mirror_check_dev *reg_mirror_check) {
	int ret = -EINVAL;
	int addr_size;

	u32 start;
	u32 end;

        // map reg_mirror_check device memory
	start = (u32)reg_mirror_check->reg_mirror_check_reg_res->start;
	end = (u32)reg_mirror_check->reg_mirror_check_reg_res->end;
	addr_size = end - start + 1;
	
	reg_mirror_check->preg =
		(u32*)devm_ioremap(reg_mirror_check->dev,
				   reg_mirror_check->reg_mirror_check_reg_res->start,
				   addr_size);
	if (reg_mirror_check->preg == NULL) {
		dev_err(reg_mirror_check->dev, "Error mapping reg_mirror_check base memory");
		ret = -ENOMEM;
		goto FAIL;
	}
	return 0;

FAIL:
	return ret;
}

static int reg_mirror_check_probe(struct platform_device *pdev) {
	struct reg_mirror_check_dev *reg_mirror_check;
	struct device *dev;
	int ret;

	// reg_mirror_check
	reg_mirror_check = devm_kzalloc(&pdev->dev, sizeof(*reg_mirror_check), GFP_KERNEL);
	if (!reg_mirror_check)
		return -ENOMEM;

	reg_mirror_check->dev = &pdev->dev;
	reg_mirror_check->pdev = pdev;
	dev = reg_mirror_check->dev;

	dev_set_drvdata(&pdev->dev, reg_mirror_check);
	
	ret = reg_mirror_check_parse_of(reg_mirror_check);
	if (ret < 0)
		return ret;

	ret = reg_mirror_check_allocate_resources(reg_mirror_check);
	if (ret < 0)
		return ret;

	if (sysfs_create_group(&pdev->dev.kobj, &reg_mirror_check_attr_group))
		dev_err(reg_mirror_check->dev, "failed to create sysfs entries\n");

	return 0;
}

static void reg_mirror_check_remove(struct platform_device *pdev) {
	struct reg_mirror_check_dev *reg_mirror_check = dev_get_drvdata(&pdev->dev);

	dev_info(reg_mirror_check->dev, "unmapping io memory");
	devm_iounmap(reg_mirror_check->dev, reg_mirror_check->preg);

	sysfs_remove_group(&pdev->dev.kobj, &reg_mirror_check_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(reg_mirror_check->dev, "remove complete");
}

static const struct of_device_id reg_mirror_check_of_id_table[] = {
	{ .compatible = "reg_mirror_check-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, reg_mirror_check_of_id_table);

static struct platform_driver reg_mirror_check_driver = {
	.driver = {
		.name = "reg_mirror_check",
		.of_match_table = reg_mirror_check_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = reg_mirror_check_probe,
	.remove = reg_mirror_check_remove,
};

module_platform_driver(reg_mirror_check_driver);

MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("reg_mirror_check: 1.0: two SOM board reg mirror verification");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("reg_mirror_check");

