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

#define MLSCIF_CHANNEL_DEV_NAME "mlscif_channel"

#define BSS_DELAY_OFFSET (0)
#define DQD_STAT_OFFSET (4)
#define DQD_FRAME_COUNT_OFFSET (5)
#define DQD_LOCK_COUNT_OFFSET (6)
#define BERT_STAT_OFFSET (23)
#define BERT_MS_COUNT_OFFSET (24)
#define BERT_BIT_COUNT_L_OFFSET (25)
#define BERT_BIT_COUNT_H_OFFSET (26)
#define BERT_ERROR_COUNT_L_OFFSET (27)
#define BERT_ERROR_COUNT_H_OFFSET (28)
#define EBERT_ERROR_COUNT_F_OFFSET (29)
#define EBERT_ERROR_COUNT_L_OFFSET (30)
#define EBERT_ERROR_COUNT_H_OFFSET (31) 

void format_date_time(char *date, char *time, char *str);

struct mlscif_channel_dev {
    struct device *dev;
    struct platform_device *pdev;

    const char *compatible;

    u32 __iomem *preg;
	
    struct resource *mlscif_channel_reg_res;
};

// mlscif_channel ip core access
static inline u32 read_mlscif_channel_reg(struct mlscif_channel_dev *mlscif_channel, int offset) {
	return *((u32*)(mlscif_channel->preg+offset));
}

static inline void write_mlscif_channel_reg(struct mlscif_channel_dev *mlscif_channel, int offset, u32 data) {
	*((u32*)(mlscif_channel->preg+offset)) = data;
}

static inline int mlscif_channel_bitget(struct mlscif_channel_dev *mlscif_channel, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void mlscif_channel_bitset(struct mlscif_channel_dev *mlscif_channel, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, offset);
	reg |= bitmask; 
	write_mlscif_channel_reg(mlscif_channel, offset, reg);
}

static inline void mlscif_channel_bitclr(struct mlscif_channel_dev *mlscif_channel, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, offset);
	reg &= ~bitmask;
	write_mlscif_channel_reg(mlscif_channel, offset, reg);
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

// BSS_DELAY
static ssize_t bss_delay_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, BSS_DELAY_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bss_delay);

// DQD_STAT
static ssize_t dqd_stat_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, DQD_STAT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(dqd_stat);

// DQD_FRAME_COUNT
static ssize_t dqd_frame_count_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, DQD_FRAME_COUNT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(dqd_frame_count);

// DQD_LOCK_COUNT
static ssize_t dqd_lock_count_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, DQD_LOCK_COUNT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(dqd_lock_count);

// BERT_STAT
static ssize_t bert_stat_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, BERT_STAT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_stat);

// BERT_MS_COUNT
static ssize_t bert_ms_count_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, BERT_MS_COUNT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_ms_count);


// BERT_BIT_COUNT_L
static ssize_t bert_bit_count_l_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, BERT_BIT_COUNT_L_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_bit_count_l);

// BERT_BIT_COUNT_H
static ssize_t bert_bit_count_h_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, BERT_BIT_COUNT_H_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_bit_count_h);

// BERT_ERROR_COUNT_L
static ssize_t bert_error_count_l_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, BERT_ERROR_COUNT_L_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_error_count_l);

// BERT_ERROR_COUNT_H
static ssize_t bert_error_count_h_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, BERT_ERROR_COUNT_H_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_error_count_h);

// EBERT_ERROR_COUNT_F
static ssize_t ebert_error_count_f_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, EBERT_ERROR_COUNT_F_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(ebert_error_count_f);

// EBERT_ERROR_COUNT_L
static ssize_t ebert_error_count_l_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, EBERT_ERROR_COUNT_L_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(ebert_error_count_l);

// EBERT_ERROR_COUNT_H
static ssize_t ebert_error_count_h_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_channel_reg(mlscif_channel, EBERT_ERROR_COUNT_H_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(ebert_error_count_h);

static struct attribute *mlscif_channel_attrs[] = {
	&dev_attr_build_time.attr,
	&dev_attr_bss_delay.attr,
    &dev_attr_dqd_stat.attr,
    &dev_attr_dqd_frame_count.attr,
    &dev_attr_dqd_lock_count.attr,
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

static struct attribute_group mlscif_channel_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = mlscif_channel_attrs,
};

static int mlscif_channel_parse_of(struct mlscif_channel_dev *mlscif_channel) {
	struct device *dev = mlscif_channel->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &mlscif_channel->compatible);
	if (ret < 0) {
		dev_err(dev, "mlscif_channel not in DT");
		return ret;
	}

	// reg -- axi lite
	res = platform_get_resource(mlscif_channel->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	mlscif_channel->mlscif_channel_reg_res = res;
		
	dev_info(dev, "Done parsing DT");

	return 0;
}

static int mlscif_channel_allocate_resources(struct mlscif_channel_dev *mlscif_channel) {
    int ret = -EINVAL;
    int addr_size;

    u32 start;
    u32 end;

    // map mlscif_channel device memory
    start = (u32)mlscif_channel->mlscif_channel_reg_res->start;
    end = (u32)mlscif_channel->mlscif_channel_reg_res->end;
    addr_size = end - start + 1;
    dev_info(mlscif_channel->dev, "Mapping MLSCIF_CHANNEL 0x%x, size 0x%x", start, addr_size);
	
    mlscif_channel->preg =
	(u32*)devm_ioremap(mlscif_channel->dev, mlscif_channel->mlscif_channel_reg_res->start, addr_size);
    if (mlscif_channel->preg == NULL) {
        dev_err(mlscif_channel->dev, "Error mapping mlscif_channel base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    return 0;

FAIL:
    return ret;
}

static int mlscif_channel_probe(struct platform_device *pdev) {
	struct mlscif_channel_dev *mlscif_channel;
	struct device *dev;
	int ret;

	// mlscif_channel
	mlscif_channel = devm_kzalloc(&pdev->dev, sizeof(*mlscif_channel), GFP_KERNEL);
	if (!mlscif_channel)
		return -ENOMEM;

	mlscif_channel->dev = &pdev->dev;
	mlscif_channel->pdev = pdev;
	dev = mlscif_channel->dev;

	dev_set_drvdata(&pdev->dev, mlscif_channel);
	
	ret = mlscif_channel_parse_of(mlscif_channel);
	if (ret < 0) {
		return ret;
    }

	ret = mlscif_channel_allocate_resources(mlscif_channel);
	if (ret < 0) {
		return ret;
    }

	if (sysfs_create_group(&pdev->dev.kobj, &mlscif_channel_attr_group)) {
		dev_err(mlscif_channel->dev, "failed to create sysfs entries\n");
    }

	return 0;
}

static void mlscif_channel_remove(struct platform_device *pdev) {
	struct mlscif_channel_dev *mlscif_channel = dev_get_drvdata(&pdev->dev);

	dev_info(mlscif_channel->dev, "unmapping io memory");
	devm_iounmap(mlscif_channel->dev, mlscif_channel->preg);

	sysfs_remove_group(&pdev->dev.kobj, &mlscif_channel_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(mlscif_channel->dev, "remove complete");
}

static const struct of_device_id mlscif_channel_of_id_table[] = {
	{ .compatible = "mlscif_channel-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, mlscif_channel_of_id_table);

static struct platform_driver mlscif_channel_driver = {
	.driver = {
		.name = "mlscif_channel",
		.of_match_table = mlscif_channel_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = mlscif_channel_probe,
	.remove = mlscif_channel_remove,
};

module_platform_driver(mlscif_channel_driver);

MODULE_AUTHOR("Fiona O'Connell");
MODULE_DESCRIPTION("mlscif_channel: 1.0: mlsc channel-specific control and status");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("mlscif_channel");

