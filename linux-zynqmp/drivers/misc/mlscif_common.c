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

#define MLSCIF_COMMON_DEV_NAME "mlscif_common"

#define CTRL_OFFSET (0)
#define BIT_RST_MASK (1)

#define TOP_BUILD_OFFSET (0)
#define TOP_TIME_OFFSET (1)
#define OUT_CF_CTRL_OFFSET (2)

void format_date_time(char *date, char *time, char *str);

struct mlscif_common_dev {
    struct device *dev;
    struct platform_device *pdev;

    const char *compatible;

    u32 __iomem *preg;
	
    struct resource *mlscif_common_reg_res;
};

// mlscif_common ip core access
static inline u32 read_mlscif_common_reg(struct mlscif_common_dev *mlscif_common, int offset) {
	return *((u32*)(mlscif_common->preg+offset));
}

static inline void write_mlscif_common_reg(struct mlscif_common_dev *mlscif_common, int offset, u32 data) {
	*((u32*)(mlscif_common->preg+offset)) = data;
}

static inline int mlscif_common_bitget(struct mlscif_common_dev *mlscif_common, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_mlscif_common_reg(mlscif_common, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void mlscif_common_bitset(struct mlscif_common_dev *mlscif_common, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_mlscif_common_reg(mlscif_common, offset);
	reg |= bitmask; 
	write_mlscif_common_reg(mlscif_common, offset, reg);
}

static inline void mlscif_common_bitclr(struct mlscif_common_dev *mlscif_common, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_mlscif_common_reg(mlscif_common, offset);
	reg &= ~bitmask;
	write_mlscif_common_reg(mlscif_common, offset, reg);
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

static ssize_t top_build_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_common_dev *mlscif_common = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_common_reg(mlscif_common, TOP_BUILD_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(top_build);

// TOP_TIME
static ssize_t top_time_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_common_dev *mlscif_common = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_common_reg(mlscif_common, TOP_TIME_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(top_time);

// OUT_CF_CTRL
static ssize_t out_cf_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_common_dev *mlscif_common = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_common_reg(mlscif_common, OUT_CF_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t out_cf_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_common_dev *mlscif_common = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_common->dev, "Error in sysfs/out_cf_ctrl");
		return count;
	}
    write_mlscif_common_reg(mlscif_common, OUT_CF_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(out_cf_ctrl);

static struct attribute *mlscif_common_attrs[] = {
    &dev_attr_build_time.attr,
    &dev_attr_top_build.attr,
    &dev_attr_top_time.attr,
    &dev_attr_out_cf_ctrl.attr,
	NULL,
};

static struct attribute_group mlscif_common_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = mlscif_common_attrs,
};

static int mlscif_common_parse_of(struct mlscif_common_dev *mlscif_common) {
	struct device *dev = mlscif_common->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &mlscif_common->compatible);
	if (ret < 0) {
		dev_err(dev, "mlscif_common not in DT");
		return ret;
	}

	// reg -- axi lite
	res = platform_get_resource(mlscif_common->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	mlscif_common->mlscif_common_reg_res = res;
		
	dev_info(dev, "Done parsing DT");

	return 0;
}

static int mlscif_common_allocate_resources(struct mlscif_common_dev *mlscif_common) {
    int ret = -EINVAL;
    int addr_size;

    u32 start;
    u32 end;

    // map mlscif_common device memory
    start = (u32)mlscif_common->mlscif_common_reg_res->start;
    end = (u32)mlscif_common->mlscif_common_reg_res->end;
    addr_size = end - start + 1;
    dev_info(mlscif_common->dev, "Mapping MLSCIF_COMMON 0x%x, size 0x%x", start, addr_size);
	
    mlscif_common->preg =
	(u32*)devm_ioremap(mlscif_common->dev, mlscif_common->mlscif_common_reg_res->start, addr_size);
    if (mlscif_common->preg == NULL) {
        dev_err(mlscif_common->dev, "Error mapping mlscif_common base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    return 0;

FAIL:
    return ret;
}

static int mlscif_common_probe(struct platform_device *pdev) {
	struct mlscif_common_dev *mlscif_common;
	struct device *dev;
	int ret;

	// mlscif_common
	mlscif_common = devm_kzalloc(&pdev->dev, sizeof(*mlscif_common), GFP_KERNEL);
	if (!mlscif_common)
		return -ENOMEM;

	mlscif_common->dev = &pdev->dev;
	mlscif_common->pdev = pdev;
	dev = mlscif_common->dev;

	dev_set_drvdata(&pdev->dev, mlscif_common);
	
	ret = mlscif_common_parse_of(mlscif_common);
	if (ret < 0) {
		return ret;
    }

	ret = mlscif_common_allocate_resources(mlscif_common);
	if (ret < 0) {
		return ret;
    }

	if (sysfs_create_group(&pdev->dev.kobj, &mlscif_common_attr_group)) {
		dev_err(mlscif_common->dev, "failed to create sysfs entries\n");
    }

	return 0;
}

static void mlscif_common_remove(struct platform_device *pdev) {
	struct mlscif_common_dev *mlscif_common = dev_get_drvdata(&pdev->dev);

	dev_info(mlscif_common->dev, "unmapping io memory");
	devm_iounmap(mlscif_common->dev, mlscif_common->preg);

	sysfs_remove_group(&pdev->dev.kobj, &mlscif_common_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(mlscif_common->dev, "remove complete");
}

static const struct of_device_id mlscif_common_of_id_table[] = {
	{ .compatible = "mlscif_common-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, mlscif_common_of_id_table);

static struct platform_driver mlscif_common_driver = {
	.driver = {
		.name = "mlscif_common",
		.of_match_table = mlscif_common_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = mlscif_common_probe,
	.remove = mlscif_common_remove,
};

module_platform_driver(mlscif_common_driver);

MODULE_AUTHOR("Fiona O'Connell");
MODULE_DESCRIPTION("mlscif_common: 1.0: mlsc ip control and status (common component)");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("mlscif_common");

