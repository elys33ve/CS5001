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

#define SYSTEM_MLSC_DEV_NAME "system_mlsc"

// Registers
#define PCM_CTRL_OFFSET (0)
#define MLSC_SRC1_SEL_OFFSET (1)
#define MLSC_SRC2_SEL_OFFSET (2)
#define MLSC_SRC3_SEL_OFFSET (3)
#define IO_OUT1_SEL_OFFSET (4)
#define IO_OUT2_SEL_OFFSET (5)
#define PCM_OUT1_SEL_OFFSET (6)
#define PCM_OUT2_SEL_OFFSET (7)

#define PROD_ID_VAR_OFFSET (8)
#define HB_CNT_DIPSW_OFFSET (9)
#define FPGA_VERSION_OFFSET (15)

// pcm_nfrom_tmoip reg (also includes the inversion control)
#define PCM_NFROM_TMOIP_MASK (0x0FFF)
#define NTTL_RS422_MASK (0x0FFF)
#define NTTL_RS422_SHIFT (16)
#define PRODUCTION_TEST_SEL_SHIFT (28)
#define PRODUCTION_TEST_SEL_MASK (0x3)

// prod id/var
#define PROD_VAR_MASK (0x01F)
#define PROD_ID_MASK (0x01F)
#define PROD_ID_SHIFT (8)

// hb_cnt / dipsw
#define HB_CNT_MASK (0x0FF)
#define HB_CNT_SHIFT (4)
#define DIPSW_MASK (0x0F)

void format_date_time(char *date, char *time, char *str);

struct  system_mlsc_dev {
	struct device *dev;
	struct platform_device *pdev;

	const char *compatible;
	u32 __iomem *preg;
	struct resource *system_mlsc_reg_res;
};

// system_mlsc ip core access
static inline u32 read_system_mlsc_reg(struct system_mlsc_dev *system_mlsc,
					 int offset) {
    return ioread32(system_mlsc->preg+offset);
}

static inline void write_system_mlsc_reg(struct system_mlsc_dev *system_mlsc,
					   int offset, u32 data) {
    iowrite32(data, system_mlsc->preg+offset);
}

static inline int system_mlsc_bitget(struct system_mlsc_dev *system_mlsc,
				      u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_system_mlsc_reg(system_mlsc, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void system_mlsc_bitset(struct system_mlsc_dev *system_mlsc,
				       u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_system_mlsc_reg(system_mlsc, offset);
	reg |= bitmask;
	write_system_mlsc_reg(system_mlsc, offset, reg);
}

static inline void system_mlsc_bitclr(struct system_mlsc_dev *system_mlsc,
				       u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_system_mlsc_reg(system_mlsc, offset);
	reg &= ~bitmask;
	write_system_mlsc_reg(system_mlsc, offset, reg);
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

static ssize_t pcm_nfrom_tmoip_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, PCM_CTRL_OFFSET);
    return sprintf(buf, "0x%x\n", reg & PCM_NFROM_TMOIP_MASK);
}
static ssize_t pcm_nfrom_tmoip_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned int val;
    unsigned reg;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(system_mlsc->dev, "Error in sysfs/pcm_nfrom_tmoip");
        return count;
    }
    reg = read_system_mlsc_reg(system_mlsc, PCM_CTRL_OFFSET);
    reg &= ~PCM_NFROM_TMOIP_MASK;
    reg |= (val & PCM_NFROM_TMOIP_MASK);
    write_system_mlsc_reg(system_mlsc, PCM_CTRL_OFFSET, reg);
    return count;
}
static	DEVICE_ATTR_RW(pcm_nfrom_tmoip);

static ssize_t nttl_rs422_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, PCM_CTRL_OFFSET);
    reg >>= NTTL_RS422_SHIFT;
    reg &= NTTL_RS422_MASK;
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t nttl_rs422_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned int val;
    unsigned int reg;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(system_mlsc->dev, "Error in sysfs/nttl_rs422");
        return count;
    }
    reg = read_system_mlsc_reg(system_mlsc, PCM_CTRL_OFFSET);
    reg &= ~(NTTL_RS422_MASK<<NTTL_RS422_SHIFT);
    reg |= ((val & NTTL_RS422_MASK)<<NTTL_RS422_SHIFT);
    write_system_mlsc_reg(system_mlsc, PCM_CTRL_OFFSET, reg);
    return count;
}
static	DEVICE_ATTR_RW(nttl_rs422);

static ssize_t production_test_sel_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, PCM_CTRL_OFFSET);
    reg >>= PRODUCTION_TEST_SEL_SHIFT;
    reg &= PRODUCTION_TEST_SEL_MASK;
    if (reg == 0)
      return sprintf(buf, "normal operation (production test disabled)\n");
    else if (reg == 1)
      return sprintf(buf, "Board connectors output TMoIP SOM PCM outputs\n");
    else if (reg == 2)
      return sprintf(buf, "Board connectors inputs mapped to TMoIP SOM PCM inputs\n");
    else
      return sprintf(buf, "Error: Invalid setting\n");
}
static ssize_t production_test_sel_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned int val;
    unsigned int reg;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(system_mlsc->dev, "Error in sysfs/production_test_sel");
        return count;
    }
    reg = read_system_mlsc_reg(system_mlsc, PCM_CTRL_OFFSET);
    reg &= ~(PRODUCTION_TEST_SEL_MASK<<PRODUCTION_TEST_SEL_SHIFT);
    reg |= ((val & PRODUCTION_TEST_SEL_MASK)<<PRODUCTION_TEST_SEL_SHIFT);
    write_system_mlsc_reg(system_mlsc, PCM_CTRL_OFFSET, reg);
    return count;
}
static	DEVICE_ATTR_RW(production_test_sel);

static ssize_t mlsc_src1_sel_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, MLSC_SRC1_SEL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t mlsc_src1_sel_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(system_mlsc->dev, "Error in sysfs/mlsc_src1_sel");
        return count;
    }
    write_system_mlsc_reg(system_mlsc, MLSC_SRC1_SEL_OFFSET, val);
    return count;
}
static	DEVICE_ATTR_RW(mlsc_src1_sel);

static ssize_t mlsc_src2_sel_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, MLSC_SRC2_SEL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t mlsc_src2_sel_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(system_mlsc->dev, "Error in sysfs/mlsc_src2_sel");
        return count;
    }
    write_system_mlsc_reg(system_mlsc, MLSC_SRC2_SEL_OFFSET, val);
    return count;
}
static	DEVICE_ATTR_RW(mlsc_src2_sel);

static ssize_t mlsc_src3_sel_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, MLSC_SRC3_SEL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t mlsc_src3_sel_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(system_mlsc->dev, "Error in sysfs/mlsc_src3_sel");
        return count;
    }
    write_system_mlsc_reg(system_mlsc, MLSC_SRC3_SEL_OFFSET, val);
    return count;
}
static	DEVICE_ATTR_RW(mlsc_src3_sel);

static ssize_t io_out1_sel_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, IO_OUT1_SEL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t io_out1_sel_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(system_mlsc->dev, "Error in sysfs/io_out1_sel");
        return count;
    }
    write_system_mlsc_reg(system_mlsc, IO_OUT1_SEL_OFFSET, val);
    return count;
}
static	DEVICE_ATTR_RW(io_out1_sel);

static ssize_t io_out2_sel_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, IO_OUT2_SEL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t io_out2_sel_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(system_mlsc->dev, "Error in sysfs/io_out2_sel");
        return count;
    }
    write_system_mlsc_reg(system_mlsc, IO_OUT2_SEL_OFFSET, val);
    return count;
}
static	DEVICE_ATTR_RW(io_out2_sel);

static ssize_t pcm_out1_sel_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, PCM_OUT1_SEL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t pcm_out1_sel_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(system_mlsc->dev, "Error in sysfs/pcm_out1_sel");
        return count;
    }
    write_system_mlsc_reg(system_mlsc, PCM_OUT1_SEL_OFFSET, val);
    return count;
}
static	DEVICE_ATTR_RW(pcm_out1_sel);

static ssize_t pcm_out2_sel_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, PCM_OUT2_SEL_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static ssize_t pcm_out2_sel_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(system_mlsc->dev, "Error in sysfs/pcm_out2_sel");
        return count;
    }
    write_system_mlsc_reg(system_mlsc, PCM_OUT2_SEL_OFFSET, val);
    return count;
}
static	DEVICE_ATTR_RW(pcm_out2_sel);

static ssize_t hb_cnt_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, HB_CNT_DIPSW_OFFSET);
    return sprintf(buf, "%u\n", ((reg>>HB_CNT_SHIFT) & HB_CNT_MASK));
}
static	DEVICE_ATTR_RO(hb_cnt);

static ssize_t dipsw_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, HB_CNT_DIPSW_OFFSET);
    return sprintf(buf, "0x%x\n", (reg & DIPSW_MASK));
}
static	DEVICE_ATTR_RO(dipsw);

static ssize_t mlsc_fpga_version_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, FPGA_VERSION_OFFSET);
    return sprintf(buf, "0x%x\n", reg);
}
static	DEVICE_ATTR_RO(mlsc_fpga_version);

static ssize_t prod_id_var_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
    struct system_mlsc_dev *system_mlsc = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_system_mlsc_reg(system_mlsc, PROD_ID_VAR_OFFSET);
    return sprintf(buf, "0x%x, 0x%x\n", (reg>>PROD_ID_SHIFT), reg & PROD_VAR_MASK);
}
static	DEVICE_ATTR_RO(prod_id_var);

static struct attribute *system_mlsc_attrs[] = {
	&dev_attr_build_time.attr,
	&dev_attr_pcm_nfrom_tmoip.attr,
	&dev_attr_nttl_rs422.attr,
	&dev_attr_production_test_sel.attr,
	&dev_attr_mlsc_src1_sel.attr,
	&dev_attr_mlsc_src2_sel.attr,
	&dev_attr_mlsc_src3_sel.attr,
	&dev_attr_io_out1_sel.attr,
	&dev_attr_io_out2_sel.attr,
	&dev_attr_pcm_out1_sel.attr,
	&dev_attr_pcm_out2_sel.attr,
	&dev_attr_hb_cnt.attr,
	&dev_attr_dipsw.attr,
	&dev_attr_mlsc_fpga_version.attr,
	&dev_attr_prod_id_var.attr,
	NULL,
};

static struct attribute_group system_mlsc_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = system_mlsc_attrs,
};

static int system_mlsc_parse_of(struct system_mlsc_dev *system_mlsc)
{
	struct device *dev = system_mlsc->dev;
	struct device_node *node = dev->of_node;

	struct resource *res;

	int ret;

	// of node
	ret = of_property_read_string(node, "compatible", &system_mlsc->compatible);
	if (ret < 0) {
		dev_err(dev, "system_mlsc not in DT");
		return ret;
	}

	// reg
	res = platform_get_resource(system_mlsc->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	system_mlsc->system_mlsc_reg_res = res;

	dev_info(dev, "Done parsing DT");

	return 0;
}

static int system_mlsc_allocate_resources(struct system_mlsc_dev *system_mlsc) {
	int ret = -EINVAL;
	int addr_size;

	u32 start;
	u32 end;

        // map system_mlsc device memory
	start = (u32)system_mlsc->system_mlsc_reg_res->start;
	end = (u32)system_mlsc->system_mlsc_reg_res->end;
	addr_size = end - start + 1;
	
	system_mlsc->preg =
		(u32*)devm_ioremap(system_mlsc->dev,
				   system_mlsc->system_mlsc_reg_res->start,
				   addr_size);
	if (system_mlsc->preg == NULL) {
		dev_err(system_mlsc->dev, "Error mapping system_mlsc base memory");
		ret = -ENOMEM;
		goto FAIL;
	}
	return 0;

FAIL:
	return ret;
}

static int system_mlsc_probe(struct platform_device *pdev) {
	struct system_mlsc_dev *system_mlsc;
	struct device *dev;
	int ret;

	// system_mlsc
	system_mlsc = devm_kzalloc(&pdev->dev, sizeof(*system_mlsc), GFP_KERNEL);
	if (!system_mlsc)
		return -ENOMEM;

	system_mlsc->dev = &pdev->dev;
	system_mlsc->pdev = pdev;
	dev = system_mlsc->dev;

	dev_set_drvdata(&pdev->dev, system_mlsc);
	
	ret = system_mlsc_parse_of(system_mlsc);
	if (ret < 0)
		return ret;

	ret = system_mlsc_allocate_resources(system_mlsc);
	if (ret < 0)
		return ret;

	if (sysfs_create_group(&pdev->dev.kobj, &system_mlsc_attr_group))
		dev_err(system_mlsc->dev, "failed to create sysfs entries\n");

	return 0;
}

static void system_mlsc_remove(struct platform_device *pdev) {
	struct system_mlsc_dev *system_mlsc = dev_get_drvdata(&pdev->dev);

	dev_info(system_mlsc->dev, "unmapping io memory");
	devm_iounmap(system_mlsc->dev, system_mlsc->preg);

	sysfs_remove_group(&pdev->dev.kobj, &system_mlsc_attr_group);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(system_mlsc->dev, "remove complete");
}

static const struct of_device_id system_mlsc_of_id_table[] = {
	{ .compatible = "system_mlsc-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, system_mlsc_of_id_table);

static struct platform_driver system_mlsc_driver = {
	.driver = {
		.name = "system_mlsc",
		.of_match_table = system_mlsc_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = system_mlsc_probe,
	.remove = system_mlsc_remove,
};

module_platform_driver(system_mlsc_driver);

MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("system_mlsc: 1.0: two SOM board som interface using GTH");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("system_mlsc");

