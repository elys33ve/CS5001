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

#define MLSCIF_SELECTOR_DEV_NAME "mlscif_selector"

#define BSS_CTRL_OFFSET (0)
#define BSS_THRESH_OFFSET (1)
#define BSS_SRC_EN_OFFSET (2)
#define BSS_MIN_DELAY_OFFSET (3)
#define BSS_MAX_DELAY_OFFSET (4)
#define BSS_MAX_SRC_DELAY_OFFSET (5)
#define BSS_SRC_GOOD_OFFSET (8)
#define BSS_SRC_PIN_OFFSET (9)
#define BSS_SRC_SEL_OFFSET (10)
#define BSS_DELAY_OK_OFFSET (11)
#define DQE_CTRL_OFFSET (12)
#define DQD_CTRL_OFFSET (14)
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

void format_date_time(char *date, char *time, char *str);

struct mlscif_selector_dev {
    struct device *dev;
    struct platform_device *pdev;

    const char *compatible;

    u32 __iomem *preg;
	
    struct resource *mlscif_selector_reg_res;
};

// mlscif_selector ip core access
static inline u32 read_mlscif_selector_reg(struct mlscif_selector_dev *mlscif_selector, int offset) {
	return *((u32*)(mlscif_selector->preg+offset));
}

static inline void write_mlscif_selector_reg(struct mlscif_selector_dev *mlscif_selector, int offset, u32 data) {
	*((u32*)(mlscif_selector->preg+offset)) = data;
}

static inline int mlscif_selector_bitget(struct mlscif_selector_dev *mlscif_selector, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void mlscif_selector_bitset(struct mlscif_selector_dev *mlscif_selector, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, offset);
	reg |= bitmask; 
	write_mlscif_selector_reg(mlscif_selector, offset, reg);
}

static inline void mlscif_selector_bitclr(struct mlscif_selector_dev *mlscif_selector, u32 offset, u32 bitmask) {
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, offset);
	reg &= ~bitmask;
	write_mlscif_selector_reg(mlscif_selector, offset, reg);
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

// BSS CTRL reg
static ssize_t bss_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BSS_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bss_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/ctrl");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BSS_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bss_ctrl);

// BSS THRESH reg
static ssize_t bss_thresh_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BSS_THRESH_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bss_thresh_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/ctrl");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BSS_THRESH_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bss_thresh);

// BSS_SRC_EN
static ssize_t bss_src_en_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BSS_SRC_EN_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bss_src_en_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/bss_src_en");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BSS_SRC_EN_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bss_src_en);

// BSS_MIN_DELAY
static ssize_t bss_min_delay_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BSS_MIN_DELAY_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bss_min_delay_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/bss_min_delay");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BSS_MIN_DELAY_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bss_min_delay);

// BSS_MAX_DELAY
static ssize_t bss_max_delay_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BSS_MAX_DELAY_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bss_max_delay_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/bss_max_delay");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BSS_MAX_DELAY_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bss_max_delay);

// BSS_MAX_SRC_DELAY
static ssize_t bss_max_src_delay_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BSS_MAX_SRC_DELAY_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bss_max_src_delay_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/bss_max_src_delay");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BSS_MAX_SRC_DELAY_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bss_max_src_delay);

// BSS_SRC_GOOD
static ssize_t bss_src_good_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, BSS_SRC_GOOD_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bss_src_good);

// BSS_SRC_PIN
static ssize_t bss_src_pin_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, BSS_SRC_PIN_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bss_src_pin);

// BSS_SRC_SEL
static ssize_t bss_src_sel_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, BSS_SRC_SEL_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bss_src_sel);

// BSS_DELAY_OK
static ssize_t bss_delay_ok_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, BSS_DELAY_OK_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bss_delay_ok);

// DQE_CTRL
static ssize_t dqe_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, DQE_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dqe_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/dqe_ctrl");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, DQE_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dqe_ctrl);

// DQD_CTRL
static ssize_t dqd_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, DQD_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t dqd_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/dqd_ctrl");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, DQD_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(dqd_ctrl);

// BERT_CTRL
static ssize_t bert_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BERT_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/bert_ctrl");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BERT_CTRL_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_ctrl);

// BERT_TAPS
static ssize_t bert_taps_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BERT_TAPS_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_taps_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/bert_taps");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BERT_TAPS_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_taps);

// BERT_PATTERN
static ssize_t bert_pattern_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BERT_PATTERN_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_pattern_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/bert_pattern");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BERT_PATTERN_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_pattern);

// BERT_MS_LIMIT
static ssize_t bert_ms_limit_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BERT_MS_LIMIT_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_ms_limit_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/bert_ms_limit");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BERT_MS_LIMIT_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_ms_limit);




// BERT_BIT_LIMIT_L
static ssize_t bert_bit_limit_l_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BERT_BIT_LIMIT_L_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_bit_limit_l_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/bert_bit_limit_l");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BERT_BIT_LIMIT_L_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_bit_limit_l);


// BERT_BIT_LIMIT_H
static ssize_t bert_bit_limit_h_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 reg;
    reg = read_mlscif_selector_reg(mlscif_selector, BERT_BIT_LIMIT_H_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t bert_bit_limit_h_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val)) {
		dev_info(mlscif_selector->dev, "Error in sysfs/bert_bit_limit_h");
		return count;
	}
    write_mlscif_selector_reg(mlscif_selector, BERT_BIT_LIMIT_H_OFFSET, val);
	return count;
}
static	DEVICE_ATTR_RW(bert_bit_limit_h);

/*
// BERT_BIT_LIMIT
static ssize_t bert_bit_limit_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u32 regl, regh;
    u64 reg;
    regl = read_mlscif_selector_reg(mlscif_selector, BERT_BIT_LIMIT_L_OFFSET);
    regh = read_mlscif_selector_reg(mlscif_selector, BERT_BIT_LIMIT_H_OFFSET);
    reg = (u64)regh << 32 | regl;
    return sprintf(buf, "%llu\n", reg);
}
static ssize_t bert_bit_limit_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
    u64 val;
    u32 regl, regh;
    if (kstrtou64(buf, 0, &val)) {
	dev_info(mlscif_selector->dev, "Error in sysfs/bert_bit_limit");
	return count;
    }
    regh = (val>>32);
    regl = (val & 0x0FFFFFFFF);
    write_mlscif_selector_reg(mlscif_selector, BERT_BIT_LIMIT_L_OFFSET, regl);
    write_mlscif_selector_reg(mlscif_selector, BERT_BIT_LIMIT_H_OFFSET, regh);
    return count;
}
static	DEVICE_ATTR_RW(bert_bit_limit);
*/


// BERT_STAT
static ssize_t bert_stat_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, BERT_STAT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_stat);

// BERT_MS_COUNT
static ssize_t bert_ms_count_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, BERT_MS_COUNT_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_ms_count);



// BERT_BIT_COUNT_L
static ssize_t bert_bit_count_l_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, BERT_BIT_COUNT_L_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_bit_count_l);

// BERT_BIT_COUNT_H
static ssize_t bert_bit_count_h_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, BERT_BIT_COUNT_H_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_bit_count_h);

// BERT_ERROR_COUNT_L
static ssize_t bert_error_count_l_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, BERT_ERROR_COUNT_L_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_error_count_l);

// BERT_ERROR_COUNT_H
static ssize_t bert_error_count_h_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, BERT_ERROR_COUNT_H_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(bert_error_count_h);

// EBERT_ERROR_COUNT_F
static ssize_t ebert_error_count_f_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, EBERT_ERROR_COUNT_F_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(ebert_error_count_f);

// EBERT_ERROR_COUNT_L
static ssize_t ebert_error_count_l_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, EBERT_ERROR_COUNT_L_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(ebert_error_count_l);

// EBERT_ERROR_COUNT_H
static ssize_t ebert_error_count_h_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(dev);
	u32 reg;
	reg = read_mlscif_selector_reg(mlscif_selector, EBERT_ERROR_COUNT_H_OFFSET);
	return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(ebert_error_count_h);

static struct attribute *mlscif_selector_attrs[] = {
    &dev_attr_build_time.attr,
    &dev_attr_bss_ctrl.attr,
    &dev_attr_bss_thresh.attr,
    &dev_attr_bss_src_en.attr,
    &dev_attr_bss_min_delay.attr,
    &dev_attr_bss_max_delay.attr,
    &dev_attr_bss_max_src_delay.attr,
    &dev_attr_bss_src_good.attr,
    &dev_attr_bss_src_pin.attr,
    &dev_attr_bss_src_sel.attr,
    &dev_attr_bss_delay_ok.attr,
    &dev_attr_dqe_ctrl.attr,
    &dev_attr_dqd_ctrl.attr,
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

static struct attribute_group mlscif_selector_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = mlscif_selector_attrs,
};

static int mlscif_selector_parse_of(struct mlscif_selector_dev *mlscif_selector) {
	struct device *dev = mlscif_selector->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &mlscif_selector->compatible);
	if (ret < 0) {
		dev_err(dev, "mlscif_selector not in DT");
		return ret;
	}

	// reg -- axi lite
	res = platform_get_resource(mlscif_selector->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	mlscif_selector->mlscif_selector_reg_res = res;
		
	dev_info(dev, "Done parsing DT");

	return 0;
}

static int mlscif_selector_allocate_resources(struct mlscif_selector_dev *mlscif_selector) {
    int ret = -EINVAL;
    int addr_size;

    u32 start;
    u32 end;

    // map mlscif_selector device memory
    start = (u32)mlscif_selector->mlscif_selector_reg_res->start;
    end = (u32)mlscif_selector->mlscif_selector_reg_res->end;
    addr_size = end - start + 1;
    dev_info(mlscif_selector->dev, "Mapping MLSCIF_SELECTOR 0x%x, size 0x%x", start, addr_size);
	
    mlscif_selector->preg =
	(u32*)devm_ioremap(mlscif_selector->dev, mlscif_selector->mlscif_selector_reg_res->start, addr_size);
    if (mlscif_selector->preg == NULL) {
        dev_err(mlscif_selector->dev, "Error mapping mlscif_selector base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    return 0;

FAIL:
    return ret;
}

static int mlscif_selector_probe(struct platform_device *pdev) {
	struct mlscif_selector_dev *mlscif_selector;
	struct device *dev;
	int ret;

	// mlscif_selector
	mlscif_selector = devm_kzalloc(&pdev->dev, sizeof(*mlscif_selector), GFP_KERNEL);
	if (!mlscif_selector)
		return -ENOMEM;

	mlscif_selector->dev = &pdev->dev;
	mlscif_selector->pdev = pdev;
	dev = mlscif_selector->dev;

	dev_set_drvdata(&pdev->dev, mlscif_selector);
	
	ret = mlscif_selector_parse_of(mlscif_selector);
	if (ret < 0) {
		return ret;
    }

	ret = mlscif_selector_allocate_resources(mlscif_selector);
	if (ret < 0) {
		return ret;
    }

	if (sysfs_create_group(&pdev->dev.kobj, &mlscif_selector_attr_group)) {
		dev_err(mlscif_selector->dev, "failed to create sysfs entries\n");
    }

	return 0;
}

static void mlscif_selector_remove(struct platform_device *pdev) {
	struct mlscif_selector_dev *mlscif_selector = dev_get_drvdata(&pdev->dev);

	dev_info(mlscif_selector->dev, "unmapping io memory");
	devm_iounmap(mlscif_selector->dev, mlscif_selector->preg);

	sysfs_remove_group(&pdev->dev.kobj, &mlscif_selector_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(mlscif_selector->dev, "remove complete");
}

static const struct of_device_id mlscif_selector_of_id_table[] = {
	{ .compatible = "mlscif_selector-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, mlscif_selector_of_id_table);

static struct platform_driver mlscif_selector_driver = {
	.driver = {
		.name = "mlscif_selector",
		.of_match_table = mlscif_selector_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = mlscif_selector_probe,
	.remove = mlscif_selector_remove,
};

module_platform_driver(mlscif_selector_driver);

MODULE_AUTHOR("Fiona O'Connell");
MODULE_DESCRIPTION("mlscif_selector: 1.0: mlsc selector-specfic control and status");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("mlscif_selector");

