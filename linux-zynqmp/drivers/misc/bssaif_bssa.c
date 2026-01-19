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

#define BSSAIF_BSSA_DEV_NAME "bssaif_bssa"

#define DQE_CTRL_OFFSET (9)

#define DQD_CTRL_OFFSET (11)
#define DQD_STAT_OFFSET (12)
#define DQD_FRAME_COUNT_OFFSET (13)
#define DQD_LOCK_COUNT_OFFSET (14)

#define BERT_CTRL_OFFSET (16)
#define BERT_TAPS_OFFSET (17)
#define BERT_PATTERN_OFFSET (18)
#define BERT_MS_LIMIT_OFFSET (19)
#define BERT_BIT_LIMIT_L_OFFSET (20)
#define BERT_BIT_LIMIT_H_OFFSET (21)

#define BERT_STAT_OFFSET (23)
#define BERT_MS_COUNT_OFFSET (24)
#define BERT_BIT_COUNT_L_OFFSET (25)
#define BERT_BIT_COUNT_H_OFFSET (26)
#define BERT_ERROR_COUNT_L_OFFSET (27)
#define BERT_ERROR_COUNT_H_OFFSET (28)
#define EBERT_ERROR_COUNT_F_OFFSET (29)
#define EBERT_ERROR_COUNT_L_OFFSET (30)
#define EBERT_ERROR_COUNT_H_OFFSET (31)


struct bssaif_bssa_dev {
    struct device *dev;
    struct platform_device *pdev;

    const char *compatible;

    u32 __iomem *preg;
	
    struct resource *bssaif_bssa_reg_res;
};

// bssaif_bssa ip core access
static inline u32 read_bssaif_bssa_reg(struct bssaif_bssa_dev *bssaif_bssa, int offset) {
	return *((u32*)(bssaif_bssa->preg+offset));
}

static inline void write_bssaif_bssa_reg(struct bssaif_bssa_dev *bssaif_bssa, int offset, u32 data) {
	*((u32*)(bssaif_bssa->preg+offset)) = data;
}

static inline int bssaif_bssa_bitget(struct bssaif_bssa_dev *bssaif_bssa, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void bssaif_bssa_bitset(struct bssaif_bssa_dev *bssaif_bssa, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, offset);
	reg |= bitmask; 
	write_bssaif_bssa_reg(bssaif_bssa, offset, reg);
}

static inline void bssaif_bssa_bitclr(struct bssaif_bssa_dev *bssaif_bssa, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, offset);
	reg &= ~bitmask;
	write_bssaif_bssa_reg(bssaif_bssa, offset, reg);
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

// DQE_CTRL
static ssize_t dqe_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssaif_bssa_reg(bssaif_bssa, DQE_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dqe_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_bssa->dev, "Error in sysfs/ctrl");
		return count;
	}
    write_bssaif_bssa_reg(bssaif_bssa, DQE_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dqe_ctrl);

// DQD_CTRL
static ssize_t dqd_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssaif_bssa_reg(bssaif_bssa, DQD_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dqd_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_bssa->dev, "Error in sysfs/ctrl");
		return count;
	}
    write_bssaif_bssa_reg(bssaif_bssa, DQD_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dqd_ctrl);

// DQD_STAT
static ssize_t dqd_stat_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, DQD_STAT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(dqd_stat);

// DQD_FRAME_COUNT
static ssize_t dqd_frame_count_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, DQD_FRAME_COUNT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(dqd_frame_count);

// DQD_LOCK_COUNT
static ssize_t dqd_lock_count_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, DQD_LOCK_COUNT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(dqd_lock_count);

// BERT_CTRL
static ssize_t bert_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_bssa->dev, "Error in sysfs/bert_ctrl");
		return count;
	}
    write_bssaif_bssa_reg(bssaif_bssa, BERT_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_ctrl);

// BERT_TAPS
static ssize_t bert_taps_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_TAPS_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_taps_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_bssa->dev, "Error in sysfs/bert_taps");
		return count;
	}
    write_bssaif_bssa_reg(bssaif_bssa, BERT_TAPS_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_taps);

// BERT_PATTERN
static ssize_t bert_pattern_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_PATTERN_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_pattern_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_bssa->dev, "Error in sysfs/bert_pattern");
		return count;
	}
    write_bssaif_bssa_reg(bssaif_bssa, BERT_PATTERN_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_pattern);

// BERT_MS_LIMIT
static ssize_t bert_ms_limit_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_MS_LIMIT_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_ms_limit_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_bssa->dev, "Error in sysfs/bert_ms_limit");
		return count;
	}
    write_bssaif_bssa_reg(bssaif_bssa, BERT_MS_LIMIT_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_ms_limit);

// BERT_BIT_LIMIT_L
static ssize_t bert_bit_limit_l_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_BIT_LIMIT_L_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_bit_limit_l_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_bssa->dev, "Error in sysfs/bert_bit_limit_l");
		return count;
	}
    write_bssaif_bssa_reg(bssaif_bssa, BERT_BIT_LIMIT_L_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_bit_limit_l);


// BERT_BIT_LIMIT_H
static ssize_t bert_bit_limit_h_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_BIT_LIMIT_H_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_bit_limit_h_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(bssaif_bssa->dev, "Error in sysfs/bert_bit_limit_h");
		return count;
	}
    write_bssaif_bssa_reg(bssaif_bssa, BERT_BIT_LIMIT_H_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_bit_limit_h);

// BERT_STAT
static ssize_t bert_stat_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_STAT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_stat);

// BERT_MS_COUNT
static ssize_t bert_ms_count_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_MS_COUNT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_ms_count);

// BERT_BIT_COUNT_L
static ssize_t bert_bit_count_l_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_BIT_COUNT_L_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_bit_count_l);

// BERT_BIT_COUNT_H
static ssize_t bert_bit_count_h_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_BIT_COUNT_H_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_bit_count_h);

// BERT_ERROR_COUNT_L
static ssize_t bert_error_count_l_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_ERROR_COUNT_L_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_error_count_l);

// BERT_ERROR_COUNT_H
static ssize_t bert_error_count_h_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, BERT_ERROR_COUNT_H_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_error_count_h);

// EBERT_ERROR_COUNT_F
static ssize_t ebert_error_count_f_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, EBERT_ERROR_COUNT_F_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(ebert_error_count_f);

// EBERT_ERROR_COUNT_L
static ssize_t ebert_error_count_l_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, EBERT_ERROR_COUNT_L_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(ebert_error_count_l);

// EBERT_ERROR_COUNT_H
static ssize_t ebert_error_count_h_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(dev);
	u32 reg;
	reg = read_bssaif_bssa_reg(bssaif_bssa, EBERT_ERROR_COUNT_H_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(ebert_error_count_h);

static struct attribute *bssaif_bssa_attrs[] = {
    &dev_attr_build_time.attr,
    &dev_attr_dqe_ctrl.attr,
    &dev_attr_dqd_ctrl.attr,
    &dev_attr_dqd_stat.attr,
    &dev_attr_dqd_frame_count.attr,
    &dev_attr_dqd_lock_count.attr,
    &dev_attr_bert_ctrl.attr,
    &dev_attr_bert_taps.attr,
    &dev_attr_bert_pattern.attr,
    &dev_attr_bert_ms_limit.attr,
    &dev_attr_bert_bit_limit_l.attr,
    &dev_attr_bert_bit_limit_h.attr,
    &dev_attr_bert_stat.attr,
    &dev_attr_bert_ms_count.attr,
    &dev_attr_bert_bit_count_l.attr,
    &dev_attr_bert_bit_count_h.attr,
    &dev_attr_bert_error_count_l.attr,
    &dev_attr_bert_error_count_h.attr,
    &dev_attr_ebert_error_count_f.attr,
    &dev_attr_ebert_error_count_l.attr,
    &dev_attr_ebert_error_count_h.attr,
    NULL,
};

static struct attribute_group bssaif_bssa_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = bssaif_bssa_attrs,
};

static int bssaif_bssa_parse_of(struct bssaif_bssa_dev *bssaif_bssa) {
	struct device *dev = bssaif_bssa->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &bssaif_bssa->compatible);
	if (ret < 0) {
		dev_err(dev, "bssaif_bssa not in DT");
		return ret;
	}

	// reg -- axi lite
	res = platform_get_resource(bssaif_bssa->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	bssaif_bssa->bssaif_bssa_reg_res = res;
		
	dev_info(dev, "Done parsing DT");

	return 0;
}

static int bssaif_bssa_allocate_resources(struct bssaif_bssa_dev *bssaif_bssa) {
    int ret = -EINVAL;
    int addr_size;

    u32 start;
    u32 end;

    // map bssaif_bssa device memory
    start = (u32)bssaif_bssa->bssaif_bssa_reg_res->start;
    end = (u32)bssaif_bssa->bssaif_bssa_reg_res->end;
    addr_size = end - start + 1;
    dev_info(bssaif_bssa->dev, "Mapping BSSAIF_BSSA 0x%x, size 0x%x", start, addr_size);
	
    bssaif_bssa->preg =
	(u32*)devm_ioremap(bssaif_bssa->dev, bssaif_bssa->bssaif_bssa_reg_res->start, addr_size);
    if (bssaif_bssa->preg == NULL) {
        dev_err(bssaif_bssa->dev, "Error mapping bssaif_bssa base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    return 0;

FAIL:
    return ret;
}

static int bssaif_bssa_probe(struct platform_device *pdev) {
	struct bssaif_bssa_dev *bssaif_bssa;
	struct device *dev;
	int ret;

	// bssaif_bssa
	bssaif_bssa = devm_kzalloc(&pdev->dev, sizeof(*bssaif_bssa), GFP_KERNEL);
	if (!bssaif_bssa)
		return -ENOMEM;

	bssaif_bssa->dev = &pdev->dev;
	bssaif_bssa->pdev = pdev;
	dev = bssaif_bssa->dev;

	dev_set_drvdata(&pdev->dev, bssaif_bssa);
	
	ret = bssaif_bssa_parse_of(bssaif_bssa);
	if (ret < 0) {
		return ret;
    }

	ret = bssaif_bssa_allocate_resources(bssaif_bssa);
	if (ret < 0) {
		return ret;
    }

	if (sysfs_create_group(&pdev->dev.kobj, &bssaif_bssa_attr_group)) {
		dev_err(bssaif_bssa->dev, "failed to create sysfs entries\n");
    }

	return 0;
}

static int bssaif_bssa_remove(struct platform_device *pdev) {
	struct bssaif_bssa_dev *bssaif_bssa = dev_get_drvdata(&pdev->dev);

	dev_info(bssaif_bssa->dev, "unmapping io memory");
	devm_iounmap(bssaif_bssa->dev, bssaif_bssa->preg);

	sysfs_remove_group(&pdev->dev.kobj, &bssaif_bssa_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(bssaif_bssa->dev, "remove complete");
	
	return 0;
}

static const struct of_device_id bssaif_bssa_of_id_table[] = {
	{ .compatible = "bssaif_bssa-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, bssaif_bssa_of_id_table);

static struct platform_driver bssaif_bssa_driver = {
	.driver = {
		.name = "bssaif_bssa",
		.of_match_table = bssaif_bssa_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = bssaif_bssa_probe,
	.remove = bssaif_bssa_remove,
};

module_platform_driver(bssaif_bssa_driver);

MODULE_AUTHOR("Fiona O'Connell");
MODULE_DESCRIPTION("bssaif_bssa: 1.0: bssaif-bssa module control and status");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("bssaif_bssa");

