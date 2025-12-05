#include <linux/module.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/circ_buf.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/ctype.h>

#define BERT_DEV_NAME "bert"

// 1: initial
#define DRIVER_VERSION 1

/*** internal test / bert generator ***/
// clockgen accumulator width
// hard-coded in vhdl (32 bits)
#define ACCUM 0x100000000

/////////////////////////////////////
///////////// registers /////////////
/////////////////////////////////////

/*** reg0: ctrl ****************/
#define CTRL_OFFSET (0)
#define RST_BIT (1)
#define CLR_BIT (1<<1)
#define BERT_IN_INIT_BIT (1<<2)
#define BERT_IN_RESYNC_RESTART_BIT (1<<3)
#define BERT_IN_LIMIT_SEL_BIT (1<<4)
#define INSERT_ERR_BIT (1<<6)
#define BERT_OUT_C_POLARITY_BIT (1<<7)
#define BERT_OUT_D_POLARITY_BIT (1<<8)

#define TP_SHIFT 16
#define TP_MASK (0x0FFFF)
#define PATTERN_SEL_SHIFT 12
#define PATTERN_SEL_MASK (0x07)

/*** reg1: status ****************/
#define STATUS_OFFSET (1)
#define SYNC_BIT (1)
#define SYNC_LOST_BIT (1<<1)
#define INV_BIT (1<<2)
#define BERT_IN_RESYNCED_BIT (1<<3)
#define BERT_IN_C_POLARITY_BIT (1<<4)

/*** reg2-3: bert_in_bits **********/
#define BERT_IN_BITS_LSB_OFFSET (2)
#define BERT_IN_BITS_MSB_OFFSET (3)
  
/*** reg4-5: bert_in_errs ********/
#define BERT_IN_ERRS_LSB_OFFSET (4)
#define BERT_IN_ERRS_MSB_OFFSET (5)

/*** reg6: bert_out_clockgen ********/
#define CLOCKGEN_STEP_OFFSET (6)

/*** reg18-19: bert_out_bits **********/
#define BERT_OUT_BITS_LSB_OFFSET (18)
#define BERT_OUT_BITS_MSB_OFFSET (19)
  
/*** reg31: version ***/
#define VERSION_OFFSET (31)

void format_date_time(char *date, char *time, char *str);

struct  bert_dev {
  struct device *dev;
  struct platform_device *pdev;
  
  const char *compatible;
  
  u32 __iomem *preg;
  
  struct resource *reg_res;
  
  u32 bert_number;

  // bert parameters
  u32 tp;
  char pattern_sel[80];
  u32 refclk_hz;
  u32 bitrate;
  int bert_out_c_polarity;
  int bert_out_d_polarity;
};

/////////////////////////////////////////////////////////////////////////////////////////
// register and bit accessors
/////////////////////////////////////////////////////////////////////////////////////////
// bert
static inline u32 read_reg(struct bert_dev *bert,
                                     int offset) {
    return ioread32(bert->preg+offset);
}
static inline void write_reg(struct bert_dev *bert,
                                       int offset, u32 data) {
    iowrite32(data, bert->preg+offset);
}
static inline int bitget(struct bert_dev *bert,
                                   int offset, u32 bitmask) {
    u32 reg;
	
    reg = read_reg(bert, offset);
    return (reg & bitmask) ? 1 : 0;
}
static inline void bitset(struct bert_dev *bert,
                                    int offset, u32 bitmask) {
    u32 reg;
    u32 reg_set;
	
    reg = read_reg(bert, offset);
    reg_set = reg | bitmask;
    write_reg(bert, offset, reg_set);
}
static inline void bitclr(struct bert_dev *bert,
                                    int offset, u32 bitmask) {
    u32 reg;
    u32 reg_clr;
	
    reg = read_reg(bert, offset);
    reg_clr = reg & ~bitmask;
    write_reg(bert, offset, reg_clr);
}
static inline void bitwrite(struct bert_dev *bert,
                                      int offset, u32 bitmask, int value) {
    if (value)
        bitset(bert, offset, bitmask);
    else
        bitclr(bert, offset, bitmask);
}
static inline void bittoggle(struct bert_dev *bert,
                                       int offset, u32 bitmask) {
    bitset(bert, offset, bitmask);
    bitclr(bert, offset, bitmask);
}

/////////////////////////////////////////////////////////////////////////////////////////
// misc helper functions
/////////////////////////////////////////////////////////////////////////////////////////

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

static void set_bitrate(struct bert_dev *bert) {
    u64 req_hz = bert->bitrate;
    u32 refclk = bert->refclk_hz;
    u64 accum = (u64)ACCUM;
    u64 step;
    u64 dividend;

    dividend = req_hz * accum;
    step = div_u64(dividend, refclk);
    write_reg(bert, CLOCKGEN_STEP_OFFSET, (u32)step);
}

void set_pattern_sel(struct bert_dev *bert) {
    u32 sel;
    u32 reg;
    if (!strncmp(bert->pattern_sel, "tp", 2) || !strncmp(bert->pattern_sel, "TP", 2)) {
        dev_info(bert->dev, "setting bert pattern in selection to tp");
        sel = 1;
    }
    else if (!strncmp(bert->pattern_sel, "pn6", 3) || !strncmp(bert->pattern_sel, "PN6", 3)) {
        dev_info(bert->dev, "setting bert pattern in selection to pn6");
        sel = 2;
    }
    else if (!strncmp(bert->pattern_sel, "pn11", 4) || !strncmp(bert->pattern_sel, "PN11", 4)) {
        dev_info(bert->dev, "setting bert pattern in selection to pn11");
        sel = 3;
    }
    else if (!strncmp(bert->pattern_sel, "pn15", 4) || !strncmp(bert->pattern_sel, "PN15", 4)) {
        dev_info(bert->dev, "setting bert pattern in selection to pn15");
        sel = 4;
    }
    else if (!strncmp(bert->pattern_sel, "pn23", 4) || !strncmp(bert->pattern_sel, "PN23", 4)) {
        dev_info(bert->dev, "setting bert pattern in selection to pn23");
        sel = 5;
    }
    else {
        dev_info(bert->dev, "invalid pattern selection %s", bert->pattern_sel);
        sel = 0;
    }
    if (sel) {
        reg = read_reg(bert, CTRL_OFFSET);
        sel &= PATTERN_SEL_MASK;
        sel <<= PATTERN_SEL_SHIFT;
        reg &= ~(PATTERN_SEL_MASK << PATTERN_SEL_SHIFT);
        reg |= sel;
        write_reg(bert, CTRL_OFFSET, reg);
    }
}
void set_tp(struct bert_dev *bert) {
    u32 tp = bert->tp;
    u32 reg = read_reg(bert, CTRL_OFFSET);
    tp <<= TP_SHIFT;
    reg &= ~(0xFFFF<<TP_SHIFT);
    reg |= tp;
    write_reg(bert, CTRL_OFFSET, reg);
}

/////////////////////////////////////////////////////////////////////////////////////////
// sysfs
/////////////////////////////////////////////////////////////////////////////////////////
static ssize_t bitrate_show(struct device *dev, struct device_attribute *attr,
                                          char *buf) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    return sprintf(buf, "%u Hz\n", bert->bitrate);
}
static ssize_t bitrate_store(struct device *dev, struct device_attribute *attr,
                                           const char *buf, size_t count) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    unsigned int bitrate;
    if (kstrtouint(buf, 0, &bitrate)) {
        dev_info(bert->dev, "Error in sysfs/bitrate");
        return count;
    }
    bert->bitrate = bitrate;
    set_bitrate(bert);
    return count;
}
static	DEVICE_ATTR_RW(bitrate);

static ssize_t tp_show(struct device *dev, struct device_attribute *attr,
                                    char *buf) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    return sprintf(buf, "0x%x\n", bert->tp);
}
static ssize_t tp_store(struct device *dev, struct device_attribute *attr,
                                     const char *buf, size_t count) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(bert->dev, "Error in sysfs/bert_tp");
        return count;
    }
    bert->tp = val;
    set_tp(bert);
    return count;
}
static	DEVICE_ATTR_RW(tp);

static ssize_t pattern_sel_show(struct device *dev, struct device_attribute *attr,
                                             char *buf) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    return sprintf(buf, "%s\n", bert->pattern_sel);
}
static ssize_t pattern_sel_store(struct device *dev, struct device_attribute *attr,
                                              const char *buf, size_t count) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    int valid_sel=1;

    if (!strncmp(buf, "tp", 2) || !strncmp(buf, "TP", 2)) 
        strcpy(bert->pattern_sel, "tp\n");
    else if (!strncmp(buf, "pn6", 3) || !strncmp(buf, "PN6", 3)) 
        strcpy(bert->pattern_sel, "pn6\n");
    else if (!strncmp(buf, "pn11", 4) || !strncmp(buf, "PN11", 4)) 
        strcpy(bert->pattern_sel, "pn11\n");
    else if (!strncmp(buf, "pn15", 4) || !strncmp(buf, "PN15", 4)) 
        strcpy(bert->pattern_sel, "pn15\n");
    else if (!strncmp(buf, "pn23", 4) || !strncmp(buf, "PN23", 4)) 
        strcpy(bert->pattern_sel, "pn23\n");
    else {
        dev_info(bert->dev, "invalid pattern selection %s", buf);
        valid_sel = 0;
    }
    if (valid_sel)
        set_pattern_sel(bert);
    return count;
}
static	DEVICE_ATTR_RW(pattern_sel);

static ssize_t which_bert_show(struct device *dev, struct device_attribute *attr,
                                      char *buf) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", bert->bert_number);
}
static	DEVICE_ATTR_RO(which_bert);

static ssize_t bert_in_bits_show(struct device *dev, struct device_attribute *attr,
                                      char *buf) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    u64 bert_bits;
    bert_bits = read_reg(bert, BERT_IN_BITS_LSB_OFFSET);
    bert_bits |= ((u64)read_reg(bert, BERT_IN_BITS_MSB_OFFSET))<<32;
    return sprintf(buf, "%lld\n", bert_bits);
}
static	DEVICE_ATTR_RO(bert_in_bits);

static ssize_t sync_show(struct device *dev, struct device_attribute *attr,
                                      char *buf) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    u32 reg;
    reg = read_reg(bert, STATUS_OFFSET);
    return sprintf(buf, reg & SYNC_BIT ? "sync\n" : "unsynced\n");
}
static	DEVICE_ATTR_RO(sync);

static ssize_t bert_inv_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    u32 reg;
    reg = read_reg(bert, STATUS_OFFSET);
    return sprintf(buf, reg & INV_BIT ? "inverted\n" : "noninverted\n");
}
static	DEVICE_ATTR_RO(bert_inv);

static ssize_t bert_out_bits_show(struct device *dev, struct device_attribute *attr,
                                       char *buf) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    u64 bert_bits;
    bert_bits = read_reg(bert, BERT_OUT_BITS_LSB_OFFSET);
    bert_bits |= ((u64)read_reg(bert, BERT_OUT_BITS_MSB_OFFSET))<<32;
    return sprintf(buf, "%lld\n", bert_bits);
}
static	DEVICE_ATTR_RO(bert_out_bits);

static ssize_t bert_in_errs_show(struct device *dev, struct device_attribute *attr,
                                      char *buf) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    u64 bert_errs;
    bert_errs = read_reg(bert, BERT_IN_ERRS_LSB_OFFSET);
    bert_errs |= ((u64)read_reg(bert, BERT_IN_ERRS_MSB_OFFSET))<<32;
    return sprintf(buf, "%lld\n", bert_errs);
}
static	DEVICE_ATTR_RO(bert_in_errs);

static ssize_t bert_ber_show(struct device *dev, struct device_attribute *attr,
                                  char *buf) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    u64 bert_errs;
    u64 bert_bits;
    u64 bert_ber;
    bert_bits = read_reg(bert, BERT_IN_BITS_LSB_OFFSET);
    bert_bits |= ((u64)read_reg(bert, BERT_IN_BITS_MSB_OFFSET))<<32;
    bert_errs = read_reg(bert, BERT_IN_ERRS_LSB_OFFSET);
    bert_errs |= ((u64)read_reg(bert, BERT_IN_ERRS_MSB_OFFSET))<<32;
    bert_ber = div64_u64(bert_bits, bert_errs);
    return sprintf(buf, "%lld\n", bert_ber);
}
static	DEVICE_ATTR_RO(bert_ber);

static ssize_t clr_store(struct device *dev, struct device_attribute *attr,
                                      const char *buf, size_t count) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    bittoggle(bert, CTRL_OFFSET, CLR_BIT);
    return count;
}
static DEVICE_ATTR_WO(clr);

static ssize_t insert_err_store(struct device *dev, struct device_attribute *attr,
                                             const char *buf, size_t count) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    bittoggle(bert, CTRL_OFFSET, INSERT_ERR_BIT);
    return count;
}
static DEVICE_ATTR_WO(insert_err);

static ssize_t version_show(struct device *dev, struct device_attribute *attr,
                                    char *buf) {
    struct bert_dev *bert = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_reg(bert, VERSION_OFFSET));
}
static	DEVICE_ATTR_RO(version);


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


static struct attribute *bert_attrs[] = {
    &dev_attr_bitrate.attr,
    &dev_attr_tp.attr,
    &dev_attr_pattern_sel.attr,
    &dev_attr_which_bert.attr,
    &dev_attr_bert_in_bits.attr,
    &dev_attr_sync.attr,
    &dev_attr_bert_inv.attr,
    &dev_attr_bert_out_bits.attr,
    &dev_attr_bert_in_errs.attr,
    &dev_attr_bert_ber.attr,
    &dev_attr_clr.attr,
    &dev_attr_insert_err.attr,
    &dev_attr_version.attr,
    &dev_attr_build_time.attr,
    NULL,
};

static struct attribute_group bert_attr_group = {
    .name = NULL, /* put in device directory */
    .attrs = bert_attrs,
};

/////////////////////////////////////////////////////////////////////////////////////////
// probe, remove, parse_of, etc
/////////////////////////////////////////////////////////////////////////////////////////
static int bert_parse_of(struct bert_dev *bert)
{
    struct device *dev = bert->dev;
    struct device_node *node = dev->of_node;
    struct resource *res;

    int ret;

    // of node
    ret = of_property_read_string(node, "compatible", &bert->compatible);
    if (ret < 0) {
        dev_err(dev, "bert not in DT");
        return ret;
    }

    // bert number
    ret = of_property_read_u32(node, "bert_number", &bert->bert_number);
    if (ret) {
        dev_err(bert->dev, "error reading bert from dts");
        return ret;
    }
    dev_info(bert->dev, "bert%d\n", bert->bert_number);

    // reg -- axi bert
    res = platform_get_resource(bert->pdev, IORESOURCE_MEM, 0);
    if (res == NULL) {
        dev_err(dev, "error getting bert base reg addr from dts");
        return -EINVAL;
    }
    bert->reg_res = res;

    // refclk_hz
    ret = of_property_read_u32(node, "refclk_hz", &bert->refclk_hz);
    if (ret < 0) {
        dev_err(dev, "error parsing refclk_hz");
        return ret;
    }

    return 0;
}

static int bert_allocate_resources(struct bert_dev *bert) {
    int ret = -EINVAL;
    int addr_size;
    u32 start;
    u32 end;

    // initialize members
    bert->preg = NULL;
    bert->bert_out_c_polarity = 0;
    bert->bert_out_d_polarity = 0;

    // map bert device memory
    start = (u32)bert->reg_res->start;
    end = (u32)bert->reg_res->end;
    addr_size = end - start + 1;
    bert->preg =
        (u32*)devm_ioremap(bert->dev,
                           start, addr_size);
    if (bert->preg == NULL) {
        dev_err(bert->dev, "Error mapping bert base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    // init bert
    write_reg(bert, CTRL_OFFSET, RST_BIT);
    write_reg(bert, CTRL_OFFSET, 0);

    return 0;

FAIL:
    return ret;
}

static int bert_probe(struct platform_device *pdev) {
    struct bert_dev *bert;
    struct device *dev;
    int ret;

    // bert
    bert = devm_kzalloc(&pdev->dev, sizeof(*bert), GFP_KERNEL);
    if (!bert)
        return -ENOMEM;

    bert->dev = &pdev->dev;
    bert->pdev = pdev;
    dev = bert->dev;

    dev_set_drvdata(&pdev->dev, bert);
	
    ret = bert_parse_of(bert);
    if (ret < 0)
        return ret;

    ret = bert_allocate_resources(bert);
    if (ret < 0)
        return ret;

    if (sysfs_create_group(&pdev->dev.kobj, &bert_attr_group))
        dev_err(bert->dev, "failed to create sysfs entries\n");

    return 0;
}

static void bert_remove(struct platform_device *pdev) {
    struct bert_dev *bert = dev_get_drvdata(&pdev->dev);
    dev_info(bert->dev, "asserting reset");

    dev_info(bert->dev, "unmapping io memory");
    devm_iounmap(bert->dev, bert->preg);

    // free ring buf
    sysfs_remove_group(&pdev->dev.kobj, &bert_attr_group);

    dev_set_drvdata(&pdev->dev, NULL);
    dev_info(bert->dev, "remove complete");
}

static const struct of_device_id bert_of_id_table[] = {
    { .compatible = "bert-1.0" },
    {}
};

MODULE_DEVICE_TABLE(of, bert_of_id_table);

static struct platform_driver bert_driver = {
    .driver = {
        .name = "bert",
        .of_match_table = bert_of_id_table,
        .owner = THIS_MODULE,
    },
    .probe = bert_probe,
    .remove = bert_remove,
};

module_platform_driver(bert_driver);

MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("bert: 1.0: BERT control");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("bert");

