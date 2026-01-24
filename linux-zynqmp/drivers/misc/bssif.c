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

#define BSSIF_DEV_NAME "bssif"

#define CTRL_OFFSET (0)
#define BIT_RST_MASK (1)

#define CMN_TOP_BUILD_OFFSET (0)
#define CMN_TOP_TIME_OFFSET (4)
#define CMN_DQE_CTRL_OFFSET (8)


// channel base offsets
#define SRC_BSS_DELAY (0x80)
#define SRC_DQD_STAT_OFFSET (0x90)
#define SRC_DQD_FRAME_COUNT_OFFSET (0x94)
#define SRC_DQD_LOCK_COUNT_OFFSET (0x98)
#define SRC_BERT_STAT_OFFSET (0xa0)
#define SRC_BERT_MS_COUNT_OFFSET (0xa4)
#define SRC_BERT_BIT_COUNT_L_OFFSET (0xa8)
#define SRC_BERT_BIT_COUNT_H_OFFSET (0xac)
#define SRC_BERT_ERROR_COUNT_L_OFFSET (0xb0)
#define SRC_BERT_ERROR_COUNT_H_OFFSET (0xb4)
#define SRC_EBERT_ERROR_COUNT_L_OFFSET (0xb8)
#define SRC_EBERT_ERROR_COUNT_H_OFFSET (0xbc)

void format_date_time(char *date, char *time, char *str);

struct bssif_dev {
    struct device *dev;
    struct platform_device *pdev;

    const char *compatible;

    u32 __iomem *preg;
	
    struct resource *bssif_reg_res;
};

// bssif ip core access
static inline u32 read_bssif_reg(struct bssif_dev *bssif, int offset) {
	return *((u32*)(bssif->preg+offset));
}

static inline void write_bssif_reg(struct bssif_dev *bssif, int offset, u32 data) {
	*((u32*)(bssif->preg+offset)) = data;
}

static inline int bssif_bitget(struct bssif_dev *bssif, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssif_reg(bssif, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void bssif_bitset(struct bssif_dev *bssif, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssif_reg(bssif, offset);
	reg |= bitmask; 
	write_bssif_reg(bssif, offset, reg);
}

static inline void bssif_bitclr(struct bssif_dev *bssif, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssif_reg(bssif, offset);
	reg &= ~bitmask;
	write_bssif_reg(bssif, offset, reg);
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
	struct bssif_dev *bssif = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssif_reg(bssif, CMN_TOP_BUILD_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(top_build);

static ssize_t dqe_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssif_dev *bssif = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssif_reg(bssif, CMN_DQE_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dqe_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssif_dev *bssif = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssif->dev, "Error in sysfs/dqe_ctrl");
		return count;
	}
    write_bssif_reg(bssif, CMN_DQE_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dqe_ctrl);

static ssize_t bert_bit_cnt_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssif_dev *bssif = dev_get_drvdata(dev);
    u32 reg_l;
    u32 reg_h;

    reg_l = read_bssif_reg(bssif, SRC_BERT_BIT_COUNT_L_OFFSET);
    reg_h = read_bssif_reg(bssif, SRC_BERT_BIT_COUNT_H_OFFSET);
    return sprintf(buf, "0x%llx\n", ((u64)reg_h << 32) | (u64)reg_l);
}
static	DEVICE_ATTR_RO(bert_bit_cnt);

static struct attribute *bssif_attrs[] = {
	&dev_attr_build_time.attr,
	&dev_attr_top_build.attr,
	&dev_attr_dqe_ctrl.attr,
	&dev_attr_bert_bit_cnt.attr,
	NULL,
};

static struct attribute_group bssif_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = bssif_attrs,
};

static int bssif_parse_of(struct bssif_dev *bssif) {
	struct device *dev = bssif->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &bssif->compatible);
	if (ret < 0) {
		dev_err(dev, "bssif not in DT");
		return ret;
	}

	// reg -- axi lite
	res = platform_get_resource(bssif->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	bssif->bssif_reg_res = res;
		
	dev_info(dev, "Done parsing DT");

	return 0;
}

static int bssif_allocate_resources(struct bssif_dev *bssif) {
    int ret = -EINVAL;
    int addr_size;

    u32 start;
    u32 end;

    // map bssif device memory
    start = (u32)bssif->bssif_reg_res->start;
    end = (u32)bssif->bssif_reg_res->end;
    addr_size = end - start + 1;
    dev_info(bssif->dev, "Mapping BSSIF 0x%x, size 0x%x", start, addr_size);
	
    bssif->preg =
	(u32*)devm_ioremap(bssif->dev, bssif->bssif_reg_res->start, addr_size);
    if (bssif->preg == NULL) {
        dev_err(bssif->dev, "Error mapping bssif base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    return 0;

FAIL:
    return ret;
}

static int bssif_probe(struct platform_device *pdev) {
	struct bssif_dev *bssif;
	struct device *dev;
	int ret;

	// bssif
	bssif = devm_kzalloc(&pdev->dev, sizeof(*bssif), GFP_KERNEL);
	if (!bssif)
		return -ENOMEM;

	bssif->dev = &pdev->dev;
	bssif->pdev = pdev;
	dev = bssif->dev;

	dev_set_drvdata(&pdev->dev, bssif);
	
	ret = bssif_parse_of(bssif);
	if (ret < 0) {
		return ret;
    }

	ret = bssif_allocate_resources(bssif);
	if (ret < 0) {
		return ret;
    }

	if (sysfs_create_group(&pdev->dev.kobj, &bssif_attr_group)) {
		dev_err(bssif->dev, "failed to create sysfs entries\n");
    }

	return 0;
}

static void bssif_remove(struct platform_device *pdev) {
	struct bssif_dev *bssif = dev_get_drvdata(&pdev->dev);

	dev_info(bssif->dev, "unmapping io memory");
	devm_iounmap(bssif->dev, bssif->preg);

	sysfs_remove_group(&pdev->dev.kobj, &bssif_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(bssif->dev, "remove complete");
}

static const struct of_device_id bssif_of_id_table[] = {
	{ .compatible = "bssif-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, bssif_of_id_table);

static struct platform_driver bssif_driver = {
	.driver = {
		.name = "bssif",
		.of_match_table = bssif_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = bssif_probe,
	.remove = bssif_remove,
};

module_platform_driver(bssif_driver);

MODULE_AUTHOR("Fiona O'Connell");
MODULE_DESCRIPTION("bssif: 1.0: bss ip control and status");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("bssif");

