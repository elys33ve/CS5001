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

// Registers
#define CTRL_OFFSET (0)
#define STATUS_OFFSET (1)

// CTRL BITS
#define RST_BIT (1)
#define CLR_BIT (1<<1)

// STATUS BITS
#define GTH_UP_BIT (1<<1)

void format_date_time(char *date, char *time, char *str);

struct  som2som_gth_dev {
	struct device *dev;
	struct platform_device *pdev;

	const char *compatible;
	u32 __iomem *preg;
	struct resource *som2som_gth_reg_res;
};

// som2som_gth ip core access
static inline u32 read_som2som_gth_reg(struct som2som_gth_dev *som2som_gth,
					 int offset) {
    return ioread32(som2som_gth->preg+offset);
}

static inline void write_som2som_gth_reg(struct som2som_gth_dev *som2som_gth,
					   int offset, u32 data) {
    iowrite32(data, som2som_gth->preg+offset);
}

static inline int som2som_gth_bitget(struct som2som_gth_dev *som2som_gth,
				      u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_som2som_gth_reg(som2som_gth, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void som2som_gth_bitset(struct som2som_gth_dev *som2som_gth,
				       u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_som2som_gth_reg(som2som_gth, offset);
	reg |= bitmask;
	write_som2som_gth_reg(som2som_gth, offset, reg);
}

static inline void som2som_gth_bitclr(struct som2som_gth_dev *som2som_gth,
				       u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_som2som_gth_reg(som2som_gth, offset);
	reg &= ~bitmask;
	write_som2som_gth_reg(som2som_gth, offset, reg);
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

static ssize_t reset_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct som2som_gth_dev *som2som_gth = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_som2som_gth_reg(som2som_gth, CTRL_OFFSET);
    return sprintf(buf, reg & RST_BIT ? "reset\n" : "not reset\n");
}
static ssize_t reset_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct som2som_gth_dev *som2som_gth = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(som2som_gth->dev, "Error in sysfs/pcmin_data_src_bert_npcmin");
        return count;
    }
    if (val)
        som2som_gth_bitset(som2som_gth, CTRL_OFFSET, RST_BIT);
    else
        som2som_gth_bitclr(som2som_gth, CTRL_OFFSET, RST_BIT);
    return count;
}
static	DEVICE_ATTR_RW(reset);

static ssize_t clr_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct som2som_gth_dev *som2som_gth = dev_get_drvdata(dev);
    som2som_gth_bitset(som2som_gth, CTRL_OFFSET, CLR_BIT);
    som2som_gth_bitclr(som2som_gth, CTRL_OFFSET, CLR_BIT);
    return count;
}
static DEVICE_ATTR_WO(clr);

static struct attribute *som2som_gth_attrs[] = {
	&dev_attr_build_time.attr,
	&dev_attr_reset.attr,
	&dev_attr_clr.attr,
	NULL,
};

static struct attribute_group som2som_gth_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = som2som_gth_attrs,
};

static int som2som_gth_parse_of(struct som2som_gth_dev *som2som_gth)
{
	struct device *dev = som2som_gth->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &som2som_gth->compatible);
	if (ret < 0) {
		dev_err(dev, "som2som_gth not in DT");
		return ret;
	}

	// reg
	res = platform_get_resource(som2som_gth->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	som2som_gth->som2som_gth_reg_res = res;

	dev_info(dev, "Done parsing DT");

	return 0;
}

static int som2som_gth_allocate_resources(struct som2som_gth_dev *som2som_gth) {
	int ret = -EINVAL;
	int addr_size;

	u32 start;
	u32 end;

        // map som2som_gth device memory
	start = (u32)som2som_gth->som2som_gth_reg_res->start;
	end = (u32)som2som_gth->som2som_gth_reg_res->end;
	addr_size = end - start + 1;
	
	som2som_gth->preg =
		(u32*)devm_ioremap(som2som_gth->dev,
				   som2som_gth->som2som_gth_reg_res->start,
				   addr_size);
	if (som2som_gth->preg == NULL) {
		dev_err(som2som_gth->dev, "Error mapping som2som_gth base memory");
		ret = -ENOMEM;
		goto FAIL;
	}
	return 0;

FAIL:
	return ret;
}

static int som2som_gth_probe(struct platform_device *pdev) {
	struct som2som_gth_dev *som2som_gth;
	struct device *dev;
	int ret;

	// som2som_gth
	som2som_gth = devm_kzalloc(&pdev->dev, sizeof(*som2som_gth), GFP_KERNEL);
	if (!som2som_gth)
		return -ENOMEM;

	som2som_gth->dev = &pdev->dev;
	som2som_gth->pdev = pdev;
	dev = som2som_gth->dev;

	dev_set_drvdata(&pdev->dev, som2som_gth);
	
	ret = som2som_gth_parse_of(som2som_gth);
	if (ret < 0)
		return ret;

	ret = som2som_gth_allocate_resources(som2som_gth);
	if (ret < 0)
		return ret;

	if (sysfs_create_group(&pdev->dev.kobj, &som2som_gth_attr_group))
		dev_err(som2som_gth->dev, "failed to create sysfs entries\n");

	return 0;
}

static void som2som_gth_remove(struct platform_device *pdev) {
	struct som2som_gth_dev *som2som_gth = dev_get_drvdata(&pdev->dev);

	dev_info(som2som_gth->dev, "unmapping io memory");
	devm_iounmap(som2som_gth->dev, som2som_gth->preg);

	sysfs_remove_group(&pdev->dev.kobj, &som2som_gth_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(som2som_gth->dev, "remove complete");
}

static const struct of_device_id som2som_gth_of_id_table[] = {
	{ .compatible = "som2som_gth-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, som2som_gth_of_id_table);

static struct platform_driver som2som_gth_driver = {
	.driver = {
		.name = "som2som_gth",
		.of_match_table = som2som_gth_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = som2som_gth_probe,
	.remove = som2som_gth_remove,
};

module_platform_driver(som2som_gth_driver);

MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("som2som_gth: 1.0: two SOM board som interface using GTH");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("som2som_gth");

