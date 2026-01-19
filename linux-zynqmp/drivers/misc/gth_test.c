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

#define GTH_TEST_DEV_NAME "gth_test"

#define CTRL_OFFSET (0)
#define SYNC_OFFSET (1)
#define LANE0_CHECK_OFFSET (2)
#define LANE0_ERRS_OFFSET (3)
#define LANE1_CHECK_OFFSET (4)
#define LANE1_ERRS_OFFSET (5)
#define LANE2_CHECK_OFFSET (6)
#define LANE2_ERRS_OFFSET (7)
#define LANE3_CHECK_OFFSET (8)
#define LANE3_ERRS_OFFSET (9)
#define TEST_CTRL_OFFSET 10

#define RST_MASK (1)
#define CLR_MASK (1<<1)
#define INSERT_ERR_MASK (1)

void format_date_time(char *date, char *time, char *str);

struct gth_test_dev {
    struct device *dev;
    struct platform_device *pdev;

    const char *compatible;

    u32 __iomem *preg;
	
    struct resource *gth_test_reg_res;
};



// gth_test ip core access
static inline u32 read_gth_test_reg(struct gth_test_dev *gth_test, int offset) {
	return *((u32*)(gth_test->preg+offset));
}

static inline void write_gth_test_reg(struct gth_test_dev *gth_test, int offset, u32 data) {
	*((u32*)(gth_test->preg+offset)) = data;
}

static inline int gth_test_bitget(struct gth_test_dev *gth_test, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_gth_test_reg(gth_test, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void gth_test_bitset(struct gth_test_dev *gth_test, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_gth_test_reg(gth_test, offset);
	reg |= bitmask; 
	write_gth_test_reg(gth_test, offset, reg);
}

static inline void gth_test_bitclr(struct gth_test_dev *gth_test, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_gth_test_reg(gth_test, offset);
	reg &= ~bitmask;
	write_gth_test_reg(gth_test, offset, reg);
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


// RESET
static ssize_t rst_show(struct device *dev, struct device_attribute *attr, char *buf){
        struct gth_test_dev *gth_test = dev_get_drvdata(dev);
        u32 reg;
        reg = read_gth_test_reg(gth_test, CTRL_OFFSET);
        return sprintf(buf, "%u\n", reg);
}
static ssize_t rst_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count){
        struct gth_test_dev *gth_test = dev_get_drvdata(dev);
        unsigned int val;
        if (kstrtouint(buf, 0, &val)) {
            dev_info(gth_test->dev, "Error in sysfs/gth_test_reset");
            return count;
        }

        if (val) {
            gth_test_bitset(gth_test, CTRL_OFFSET, RST_MASK);
        } else {
            gth_test_bitclr(gth_test, CTRL_OFFSET, RST_MASK);
        }
        return count;
}
static  DEVICE_ATTR_RW(rst);

// CLEAR
static ssize_t clr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct gth_test_dev *gth_test = dev_get_drvdata(dev);
    gth_test_bitclr(gth_test, CTRL_OFFSET, CLR_MASK);
    gth_test_bitset(gth_test, CTRL_OFFSET, CLR_MASK);
    gth_test_bitclr(gth_test, CTRL_OFFSET, CLR_MASK);
    return count;
}
static	DEVICE_ATTR_WO(clr);

// insert_err
static ssize_t insert_err_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct gth_test_dev *gth_test = dev_get_drvdata(dev);
    gth_test_bitclr(gth_test, TEST_CTRL_OFFSET, INSERT_ERR_MASK);
    gth_test_bitset(gth_test, TEST_CTRL_OFFSET, INSERT_ERR_MASK);
    gth_test_bitclr(gth_test, TEST_CTRL_OFFSET, INSERT_ERR_MASK);
    return count;
}
static	DEVICE_ATTR_WO(insert_err);


/* ===== read only ===== */

// SYNC
static ssize_t sync_show(struct device *dev, struct device_attribute *attr, char *buf){
        struct gth_test_dev *gth_test = dev_get_drvdata(dev);
        u32 reg;
        reg = read_gth_test_reg(gth_test, SYNC_OFFSET);
        return sprintf(buf, "%u\n", reg);
}
static  DEVICE_ATTR_RO(sync);

// LANE0_CHECK
static ssize_t lane0_check_show(struct device *dev, struct device_attribute *attr, char *buf){
        struct gth_test_dev *gth_test = dev_get_drvdata(dev);
        u32 reg;
        reg = read_gth_test_reg(gth_test, LANE0_CHECK_OFFSET);
        return sprintf(buf, "%u\n", reg);
}
static  DEVICE_ATTR_RO(lane0_check);

// LANE0_ERRS
static ssize_t lane0_errs_show(struct device *dev, struct device_attribute *attr, char *buf){
        struct gth_test_dev *gth_test = dev_get_drvdata(dev);
        u32 reg;
        reg = read_gth_test_reg(gth_test, LANE0_ERRS_OFFSET);
        return sprintf(buf, "%u\n", reg);
}
static  DEVICE_ATTR_RO(lane0_errs);

// LANE1_CHECK
static ssize_t lane1_check_show(struct device *dev, struct device_attribute *attr, char *buf){
        struct gth_test_dev *gth_test = dev_get_drvdata(dev);
        u32 reg;
        reg = read_gth_test_reg(gth_test, LANE1_CHECK_OFFSET);
        return sprintf(buf, "%u\n", reg);
}
static  DEVICE_ATTR_RO(lane1_check);

// LANE1_ERRS
static ssize_t lane1_errs_show(struct device *dev, struct device_attribute *attr, char *buf){
        struct gth_test_dev *gth_test = dev_get_drvdata(dev);
        u32 reg;
        reg = read_gth_test_reg(gth_test, LANE1_ERRS_OFFSET);
        return sprintf(buf, "%u\n", reg);
}
static  DEVICE_ATTR_RO(lane1_errs);

// LANE2_CHECK
static ssize_t lane2_check_show(struct device *dev, struct device_attribute *attr, char *buf){
        struct gth_test_dev *gth_test = dev_get_drvdata(dev);
        u32 reg;
        reg = read_gth_test_reg(gth_test, LANE2_CHECK_OFFSET);
        return sprintf(buf, "%u\n", reg);
}
static  DEVICE_ATTR_RO(lane2_check);

// LANE2_ERRS
static ssize_t lane2_errs_show(struct device *dev, struct device_attribute *attr, char *buf){
        struct gth_test_dev *gth_test = dev_get_drvdata(dev);
        u32 reg;
        reg = read_gth_test_reg(gth_test, LANE2_ERRS_OFFSET);
        return sprintf(buf, "%u\n", reg);
}
static  DEVICE_ATTR_RO(lane2_errs);

// LANE3_CHECK
static ssize_t lane3_check_show(struct device *dev, struct device_attribute *attr, char *buf){
        struct gth_test_dev *gth_test = dev_get_drvdata(dev);
        u32 reg;
        reg = read_gth_test_reg(gth_test, LANE3_CHECK_OFFSET);
        return sprintf(buf, "%u\n", reg);
}
static  DEVICE_ATTR_RO(lane3_check);

// LANE3_ERRS
static ssize_t lane3_errs_show(struct device *dev, struct device_attribute *attr, char *buf){
        struct gth_test_dev *gth_test = dev_get_drvdata(dev);
        u32 reg;
        reg = read_gth_test_reg(gth_test, LANE3_ERRS_OFFSET);
        return sprintf(buf, "%u\n", reg);
}
static  DEVICE_ATTR_RO(lane3_errs);



static struct attribute *gth_test_attrs[] = {
    &dev_attr_build_time.attr,
    &dev_attr_rst.attr,
    &dev_attr_clr.attr,
    &dev_attr_insert_err.attr,
    &dev_attr_sync.attr,
    &dev_attr_lane0_check.attr,
    &dev_attr_lane0_errs.attr,
    &dev_attr_lane1_check.attr,
    &dev_attr_lane1_errs.attr,
    &dev_attr_lane2_check.attr,
    &dev_attr_lane2_errs.attr,
    &dev_attr_lane3_check.attr,
    &dev_attr_lane3_errs.attr,
	NULL,
};

static struct attribute_group gth_test_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = gth_test_attrs,
};

static int gth_test_parse_of(struct gth_test_dev *gth_test) {
	struct device *dev = gth_test->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &gth_test->compatible);
	if (ret < 0) {
		dev_err(dev, "gth_test not in DT");
		return ret;
	}

	// reg -- axi lite
	res = platform_get_resource(gth_test->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	gth_test->gth_test_reg_res = res;
		
	dev_info(dev, "Done parsing DT");

	return 0;
}

static int gth_test_allocate_resources(struct gth_test_dev *gth_test) {
    int ret = -EINVAL;
    int addr_size;

    u32 start;
    u32 end;

    // map gth_test device memory
    start = (u32)gth_test->gth_test_reg_res->start;
    end = (u32)gth_test->gth_test_reg_res->end;
    addr_size = end - start + 1;
    dev_info(gth_test->dev, "Mapping GTH_TEST 0x%x, size 0x%x", start, addr_size);
	
    gth_test->preg =
	(u32*)devm_ioremap(gth_test->dev, gth_test->gth_test_reg_res->start, addr_size);
    if (gth_test->preg == NULL) {
        dev_err(gth_test->dev, "Error mapping gth_test base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    return 0;

FAIL:
    return ret;
}

static int gth_test_probe(struct platform_device *pdev) {
	struct gth_test_dev *gth_test;
	struct device *dev;
	int ret;

	// gth_test
	gth_test = devm_kzalloc(&pdev->dev, sizeof(*gth_test), GFP_KERNEL);
	if (!gth_test)
		return -ENOMEM;

	gth_test->dev = &pdev->dev;
	gth_test->pdev = pdev;
	dev = gth_test->dev;

	dev_set_drvdata(&pdev->dev, gth_test);
	
	ret = gth_test_parse_of(gth_test);
	if (ret < 0) {
		return ret;
    }

	ret = gth_test_allocate_resources(gth_test);
	if (ret < 0) {
		return ret;
    }

	if (sysfs_create_group(&pdev->dev.kobj, &gth_test_attr_group)) {
		dev_err(gth_test->dev, "failed to create sysfs entries\n");
    }

	return 0;
}

static void gth_test_remove(struct platform_device *pdev) {
	struct gth_test_dev *gth_test = dev_get_drvdata(&pdev->dev);

	dev_info(gth_test->dev, "unmapping io memory");
	devm_iounmap(gth_test->dev, gth_test->preg);

	sysfs_remove_group(&pdev->dev.kobj, &gth_test_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(gth_test->dev, "remove complete");
}

static const struct of_device_id gth_test_of_id_table[] = {
	{ .compatible = "gth_test-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, gth_test_of_id_table);

static struct platform_driver gth_test_driver = {
	.driver = {
		.name = "gth_test",
		.of_match_table = gth_test_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = gth_test_probe,
	.remove = gth_test_remove,
};

module_platform_driver(gth_test_driver);

MODULE_AUTHOR("Fiona O'Connell");
MODULE_DESCRIPTION("gth_test: 1.0: mlsc channel-specific control and status");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("gth_test");

