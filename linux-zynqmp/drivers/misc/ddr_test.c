// driver for both ddr_test_wr (fpga writes to cpu) and ddr_test_rd (fpga reads from cpu)
// for now we are assuming a 16-bit sync word for framed data

// toggle signals (like status_clr) use a hardware mechanism.
// when the driver writes the bit high to low, the hardware
// pulses the signal
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/circ_buf.h>
#include <linux/ctype.h>
#include <linux/kthread.h> 
#include <linux/sched.h>   

#define DDR_TEST_DEV_NAME "ddr_test"

// 1: initial
// 2: added kernel polling thread
#define DRIVER_VERSION 2

#define BUF_SIZE_BYTES (1024*1024)

/////////////////////////////////////
///////////// registers /////////////
/////////////////////////////////////
/*** reg0: ctrl ****************/
#define CTRL_OFFSET (0)
#define NRST_MASK (1)
#define STATS_CLR_MASK (1<<1)
#define INSERT_ERR_MASK (1<<2)
/*** reg1: status ****************/
#define STATUS_OFFSET (1)
#define RESP_ERR_MASK (1)
/*** reg2: base address ***/
#define BASE_ADDR_OFFSET (2)
/*** reg3: delaycntto ***/
// write side
#define DELAY_CNTTO_OFFSET (3)
/*** reg4: interframe clks ***/
// write side
#define INTERFRAME_CLKS_CNTTO_OFFSET (4)
/*** reg7: err_cnt (read side)***/
#define ERR_CNT_OFFSET (7)
/*** reg8: continuity_err_cnt (read side)***/
#define CONTINUITY_ERR_CNT_OFFSET (8)
/*** reg9: max_clkcnt_wait (read side)***/
#define MAX_CLKCNT_WAIT_OFFSET (9)
/*** reg10: max_frame_cnt_diff (read side)***/
#define MAX_FRAME_CNT_DIFF_OFFSET (10)
/*** reg15: version ***/
#define VERSION_OFFSET (15)

void format_date_time(char *date, char *time, char *str);

#define ROM1_ID 0xABCD123401020301ULL
#define ROM2_ID 0x567811F2DE77764FULL

struct  ddr_test_dev {
    struct device *dev;
    struct platform_device *pdev;
    struct miscdevice mdev;

    const char *compatible;

    u32 __iomem *preg_in;
    u32 __iomem *preg_out;
  
    struct resource *ddr_test_wr_reg_res;
    struct resource *ddr_test_rd_reg_res;
  
    u32 channel_number;
    char *name;

    volatile u32 running;

    // Kernel thread task struct
    struct task_struct *polling_thread;

    u32 interframe_clks;
	
    struct circ_buf ring_in;
    dma_addr_t phys_addr_in;
    struct circ_buf ring_out;
    dma_addr_t phys_addr_out;

    u64 in_cnt;
    u64 out_cnt;

    u64 total_write_bytes;
    u64 total_read_bytes;
};

/////////////////////////////////////////////////////////////////////////////////////////
// register and bit accessors
/////////////////////////////////////////////////////////////////////////////////////////
// ddr_test_wr
static inline u32 read_ddr_test_wr_reg(struct ddr_test_dev *ddr_test,
									   int offset) {
    u32 reg;
    reg = ioread32(ddr_test->preg_in+offset);
    return reg;
}
static inline void write_ddr_test_wr_reg(struct ddr_test_dev *ddr_test,
										 int offset, u32 data) {
    iowrite32(data, ddr_test->preg_in+offset);
}
static inline int ddr_test_wr_bitget(struct ddr_test_dev *ddr_test,
									 int offset, u32 bitmask) {
    u32 reg;
	
    reg = read_ddr_test_wr_reg(ddr_test, offset);
    return (reg & bitmask) ? 1 : 0;
}
static inline void ddr_test_wr_bitset(struct ddr_test_dev *ddr_test,
									  int offset, u32 bitmask) {
    u32 reg;
    u32 reg_set;

    reg = read_ddr_test_wr_reg(ddr_test, offset);
    reg_set = reg | bitmask;
    write_ddr_test_wr_reg(ddr_test, offset, reg_set);
}
static inline void ddr_test_wr_bitclr(struct ddr_test_dev *ddr_test,
									  int offset, u32 bitmask) {
    u32 reg;
    u32 reg_clr;

    reg = read_ddr_test_wr_reg(ddr_test, offset);
    reg_clr = reg & ~bitmask;
    write_ddr_test_wr_reg(ddr_test, offset, reg_clr);
}
static inline void ddr_test_wr_bitwrite(struct ddr_test_dev *ddr_test,
										int offset, u32 bitmask, int value) {
    if (value)
		ddr_test_wr_bitset(ddr_test, offset, bitmask);
    else
		ddr_test_wr_bitclr(ddr_test, offset, bitmask);
}
static inline void ddr_test_wr_bittoggle(struct ddr_test_dev *ddr_test,
										 int offset, u32 bitmask) {
    ddr_test_wr_bitset(ddr_test, offset, bitmask);
    ddr_test_wr_bitclr(ddr_test, offset, bitmask);
}
// ddr_test_rd
static inline u32 read_ddr_test_rd_reg(struct ddr_test_dev *ddr_test,
										int offset) {
    return ioread32(ddr_test->preg_out+offset);
}
static inline void write_ddr_test_rd_reg(struct ddr_test_dev *ddr_test,
										  int offset, u32 data) {
    iowrite32(data, ddr_test->preg_out+offset);
}
static inline int ddr_test_rd_bitget(struct ddr_test_dev *ddr_test,
									  int offset, u32 bitmask) {
    u32 reg;
	
    reg = read_ddr_test_rd_reg(ddr_test, offset);
    return (reg & bitmask) ? 1 : 0;
}
static inline void ddr_test_rd_bitset(struct ddr_test_dev *ddr_test,
									   int offset, u32 bitmask) {
    u32 reg;
    u32 reg_set;
	
    reg = read_ddr_test_rd_reg(ddr_test, offset);
    reg_set = reg | bitmask;
    write_ddr_test_rd_reg(ddr_test, offset, reg_set);
}
static inline void ddr_test_rd_bitclr(struct ddr_test_dev *ddr_test,
									   int offset, u32 bitmask) {
    u32 reg;
    u32 reg_clr;
	
    reg = read_ddr_test_rd_reg(ddr_test, offset);
    reg_clr = reg & ~bitmask;
    write_ddr_test_rd_reg(ddr_test, offset, reg_clr);
}
static inline void ddr_test_rd_bitwrite(struct ddr_test_dev *ddr_test,
										 int offset, u32 bitmask, int value) {
    if (value)
		ddr_test_rd_bitset(ddr_test, offset, bitmask);
    else
		ddr_test_rd_bitclr(ddr_test, offset, bitmask);
}
static inline void ddr_test_rd_bittoggle(struct ddr_test_dev *ddr_test,
										  int offset, u32 bitmask) {
    ddr_test_rd_bitset(ddr_test, offset, bitmask);
    ddr_test_rd_bitclr(ddr_test, offset, bitmask);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Polling Kernel Thread
/////////////////////////////////////////////////////////////////////////////////////////

/**
 * polling_thread_func - The main function for the kernel thread.
 * @data: A pointer to the ddr_test_dev struct.
 */
static int polling_thread_func(void *data)
{
    struct ddr_test_dev *ddr_test = data;
	u8 *pr = (ddr_test->ring_in.buf) + 15; // MSB of FPGA data to CPU
	u8 *pw = (ddr_test->ring_out.buf) + 15; // MSB as seen by the FPGA 

    dev_info(ddr_test->dev, "Polling thread started\n");
    // Loop until kthread_stop is called.
    while (!kthread_should_stop()) {
        // Only poll and copy if the 'run' sysfs attribute has been set to 1.
        if (ddr_test->running) {
			// check for data available
			if ((*pr == (ROM1_ID>>56)) || (*pr == (ROM2_ID>>56))) {
				if (!(*pw)) { // space avaialble for write
                    // copy all but the first line. leave that zeroed for now
					memcpy(ddr_test->ring_out.buf+16, ddr_test->ring_in.buf+16, 4080);
					wmb();
                    // copy the first line to indicate data ready to the FPGA
					memcpy(ddr_test->ring_out.buf, ddr_test->ring_in.buf, 16);
					// write zero to the ring_in side so the FPGA knows that page is available again
					wmb();
					memset(ddr_test->ring_in.buf, 0, 16);
					// Update statistics
					ddr_test->total_read_bytes += 4096;
					ddr_test->total_write_bytes += 4096;
				}
			}
            usleep_range(250, 300);
		}
        else { // not running
            msleep(10);
        }
				
        cond_resched();
    }

    dev_info(ddr_test->dev, "Polling thread stopping\n");
    return 0;
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

static void ddr_test_reset(struct ddr_test_dev *ddr_test, int state) {
    if (state) {
        ddr_test_wr_bitclr(ddr_test, CTRL_OFFSET, NRST_MASK);
        ddr_test_rd_bitclr(ddr_test, CTRL_OFFSET, NRST_MASK);
    }
    else {
        memset(ddr_test->ring_in.buf, 0, BUF_SIZE_BYTES);
        memset(ddr_test->ring_out.buf, 0, BUF_SIZE_BYTES);
        ddr_test_wr_bitset(ddr_test, CTRL_OFFSET, NRST_MASK);
        ddr_test_rd_bitset(ddr_test, CTRL_OFFSET, NRST_MASK);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
// ddr_test file i/o
/////////////////////////////////////////////////////////////////////////////////////////
static inline struct ddr_test_dev *file_to_ddr_test_dev(struct file *file) {
    struct miscdevice *miscdev = file->private_data;

    return container_of(miscdev, struct ddr_test_dev, mdev);
}

static int ddr_test_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int ddr_test_close(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t ddr_test_write(struct file *file, const char __user *buf,
							  size_t len, loff_t *ppos)
{
	return len;
}

static ssize_t ddr_test_read(struct file *file, char __user *buf,
							 size_t len, loff_t *ppos)
{
    return len;
}

static const struct file_operations ddr_test_fops = {
    .owner = THIS_MODULE,
    .write = ddr_test_write,
    .read = ddr_test_read,
    .open = ddr_test_open,
    .release = ddr_test_close,
    .llseek = noop_llseek,
};

/////////////////////////////////////////////////////////////////////////////////////////
// sysfs
/////////////////////////////////////////////////////////////////////////////////////////
static ssize_t run_show(struct device *dev, struct device_attribute *attr,
						char *buf) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    return sprintf(buf, "%s\n", ddr_test->running ? "running" : "not running");
}
static ssize_t run_store(struct device *dev, struct device_attribute *attr,
						 const char *buf, size_t count) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    unsigned int val;
	int running;
    if (kstrtouint(buf, 0, &val)) {
		dev_info(ddr_test->dev, "Error in sysfs/run");
		return count;
    }
    running = val ? 1 : 0;
    if (running) {
		if (ddr_test->running) {
			dev_info(ddr_test->dev, "already running");
			return count;
		}
		else {
			ddr_test->running = 1;
			ddr_test_reset(ddr_test, 0);
			return count;
		}
	}
	else { // running = 0
	    if (ddr_test->running == 0) {
			dev_info(ddr_test->dev, "already not running");
			return count;
		}
		else {
			ddr_test->running = 0;
			ddr_test_reset(ddr_test, 1);
			return count;
		}
    }
    return count;
}
static DEVICE_ATTR_RW(run);

static ssize_t stats_clr_store(struct device *dev, struct device_attribute *attr,
						 const char *buf, size_t count) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    ddr_test_rd_bittoggle(ddr_test, CTRL_OFFSET, STATS_CLR_MASK);
    return count;
}
static DEVICE_ATTR_WO(stats_clr);

static ssize_t insert_err_store(struct device *dev, struct device_attribute *attr,
						 const char *buf, size_t count) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    ddr_test_wr_bittoggle(ddr_test, CTRL_OFFSET, INSERT_ERR_MASK);
    return count;
}
static DEVICE_ATTR_WO(insert_err);

static ssize_t max_clkcnt_wait_show(struct device *dev, struct device_attribute *attr,
									  char *buf) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    u32 reg;
    reg = read_ddr_test_rd_reg(ddr_test, MAX_CLKCNT_WAIT_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(max_clkcnt_wait);

static ssize_t max_frame_cnt_diff_show(struct device *dev, struct device_attribute *attr,
									  char *buf) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    u32 reg;
    reg = read_ddr_test_rd_reg(ddr_test, MAX_FRAME_CNT_DIFF_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(max_frame_cnt_diff);

static ssize_t err_cnt_show(struct device *dev, struct device_attribute *attr,
									  char *buf) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    u32 reg;
    reg = read_ddr_test_rd_reg(ddr_test, ERR_CNT_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(err_cnt);

static ssize_t continuity_err_cnt_show(struct device *dev, struct device_attribute *attr,
									  char *buf) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    u32 reg;
    reg = read_ddr_test_rd_reg(ddr_test, CONTINUITY_ERR_CNT_OFFSET);
    return sprintf(buf, "%u\n", reg);
}
static	DEVICE_ATTR_RO(continuity_err_cnt);

static ssize_t total_write_bytes_show(struct device *dev, struct device_attribute *attr,
									  char *buf) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    u64 bytes = ddr_test->total_write_bytes;
    ddr_test->total_write_bytes = 0;
    return sprintf(buf, "%llu\n", bytes);
}
static	DEVICE_ATTR_RO(total_write_bytes);

static ssize_t total_write_bits_show(struct device *dev, struct device_attribute *attr,
									  char *buf) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    u64 bytes = ddr_test->total_write_bytes;
    return sprintf(buf, "%llu\n", bytes<<3);
}
static	DEVICE_ATTR_RO(total_write_bits);

static ssize_t total_read_bytes_show(struct device *dev, struct device_attribute *attr,
									 char *buf) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    u64 bytes = ddr_test->total_read_bytes;
    ddr_test->total_read_bytes = 0;
    return sprintf(buf, "%llu\n", bytes);
}
static	DEVICE_ATTR_RO(total_read_bytes);

static ssize_t total_read_bits_show(struct device *dev, struct device_attribute *attr,
									 char *buf) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    u64 bytes = ddr_test->total_read_bytes;
    return sprintf(buf, "%llu\n", bytes<<3);
}
static	DEVICE_ATTR_RO(total_read_bits);

static ssize_t interframe_clks_show(struct device *dev, struct device_attribute *attr,
						char *buf) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", ddr_test->interframe_clks);
}
static ssize_t interframe_clks_store(struct device *dev, struct device_attribute *attr,
						 const char *buf, size_t count) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
		dev_info(ddr_test->dev, "Error in sysfs/interframe_clks");
		return count;
    }
    ddr_test->interframe_clks = val;
    write_ddr_test_wr_reg(ddr_test, INTERFRAME_CLKS_CNTTO_OFFSET, val);
    return count;
}
static DEVICE_ATTR_RW(interframe_clks);

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

static struct attribute *ddr_test_attrs[] = {
    &dev_attr_run.attr,
    &dev_attr_stats_clr.attr,
    &dev_attr_insert_err.attr,
    &dev_attr_max_clkcnt_wait.attr,
    &dev_attr_max_frame_cnt_diff.attr,
    &dev_attr_err_cnt.attr,
    &dev_attr_continuity_err_cnt.attr,
    &dev_attr_total_write_bytes.attr,
    &dev_attr_total_write_bits.attr,
    &dev_attr_total_read_bytes.attr,
    &dev_attr_total_read_bits.attr,
    &dev_attr_interframe_clks.attr,
    &dev_attr_build_time.attr,
    NULL,
};

static struct attribute_group ddr_test_attr_group = {
    .name = NULL, /* put in device directory */
    .attrs = ddr_test_attrs,
};

/////////////////////////////////////////////////////////////////////////////////////////
// probe, remove, parse_of, etc
/////////////////////////////////////////////////////////////////////////////////////////
static int ddr_test_parse_of(struct ddr_test_dev *ddr_test)
{
    struct device *dev = ddr_test->dev;
    struct device_node *node = dev->of_node;
    struct resource *res;

    int ret;

    // of node
    ret = of_property_read_string(node, "compatible", &ddr_test->compatible);
    if (ret < 0) {
		dev_err(dev, "ddr_test not in DT");
		return ret;
    }

    // channel number
    ret = of_property_read_u32(node, "channel_number", &ddr_test->channel_number);
    if (ret) {
		dev_err(ddr_test->dev, "error reading channel from dts");
		return ret;
    }
    dev_info(ddr_test->dev, "channel%d\n", ddr_test->channel_number);

    // reg -- axi ddr_test_wr
    res = platform_get_resource_byname(ddr_test->pdev, IORESOURCE_MEM, "ddr_test_wr");
    if (res == NULL) {
		dev_err(dev, "error getting ddr_test_wr base reg addr from dts");
		return -EINVAL;
    }
    ddr_test->ddr_test_wr_reg_res = res;

    // reg -- axi ddr_test_rd
    res = platform_get_resource_byname(ddr_test->pdev, IORESOURCE_MEM, "ddr_test_rd");
    if (res == NULL) {
		dev_err(dev, "error getting ddr_test_rd base reg addr from dts");
		return -EINVAL;
    }
    ddr_test->ddr_test_rd_reg_res = res;

    return 0;
}

static int ddr_test_allocate_resources(struct ddr_test_dev *ddr_test) {
    int ret = -EINVAL;
    int addr_size;
    u32 start;
    u32 end;
 
    char channel_number[4];

    // initialize members
    ddr_test->running = 0;
    ddr_test->preg_in = NULL;
    ddr_test->preg_out = NULL;
    ddr_test->total_read_bytes = 0;
    ddr_test->total_write_bytes = 0;

    ddr_test->interframe_clks = 180000;

    // map ddr_test device memory
    dev_info(ddr_test->dev, "map ddr_test in reg space");
    start = (u32)ddr_test->ddr_test_wr_reg_res->start;
    end = (u32)ddr_test->ddr_test_wr_reg_res->end;
    addr_size = end - start + 1;
    ddr_test->preg_in =
		(u32*)devm_ioremap(ddr_test->dev,
						   start, addr_size);
    if (ddr_test->preg_in == NULL) {
		dev_err(ddr_test->dev, "Error mapping ddr_test_wr base memory");
		ret = -ENOMEM;
		goto FAIL;
    }
    dev_info(ddr_test->dev, "map ddr_test out reg space");
    start = (u32)ddr_test->ddr_test_rd_reg_res->start;
    end = (u32)ddr_test->ddr_test_rd_reg_res->end;
    addr_size = end - start + 1;
    ddr_test->preg_out =
		(u32*)devm_ioremap(ddr_test->dev,
						   start, addr_size);
    if (ddr_test->preg_out == NULL) {
		dev_err(ddr_test->dev, "Error mapping ddr_test_rd base memory");
		ret = -ENOMEM;
		goto FAIL;
    }

    // allocate memory for data exchange to/from the FPGA
    dev_info(ddr_test->dev, "allocate ring_out buffer");
    ddr_test->ring_out.buf =
		dma_alloc_coherent(ddr_test->dev,
						   BUF_SIZE_BYTES,
						   &ddr_test->phys_addr_out, GFP_KERNEL);
    dev_info(ddr_test->dev, "phys_addr_out = 0x%p", (u32*)ddr_test->phys_addr_out);
    if (!ddr_test->ring_out.buf) {
		dev_err(ddr_test->dev, "Failed to allocate memory for output\r\n");
		goto FAIL;
    }
    write_ddr_test_rd_reg(ddr_test, BASE_ADDR_OFFSET, ddr_test->phys_addr_out);
	
    dev_info(ddr_test->dev, "allocate ring_in buffer");
    ddr_test->ring_in.buf =
		dma_alloc_coherent(ddr_test->dev,
						   BUF_SIZE_BYTES,
						   &ddr_test->phys_addr_in, GFP_KERNEL);
    dev_info(ddr_test->dev, "phys_addr_in = 0x%p", (u32*)ddr_test->phys_addr_in);
    if (!ddr_test->ring_in.buf) {
		dev_err(ddr_test->dev, "Failed to allocate memory for input\r\n");
		goto FREE_OUT_BUF;
    }
    write_ddr_test_wr_reg(ddr_test, BASE_ADDR_OFFSET, ddr_test->phys_addr_in);

	// set default delay for write-side not ready
    write_ddr_test_wr_reg(ddr_test, DELAY_CNTTO_OFFSET, 5000);

    // form channel name
    dev_info(ddr_test->dev, "channel name");
    if (ddr_test->channel_number >= 0 && ddr_test->channel_number < 10) 
		sprintf(channel_number, "%1d", ddr_test->channel_number);
    else if (ddr_test->channel_number >= 10 && ddr_test->channel_number < 100) 
		sprintf(channel_number, "%2d", ddr_test->channel_number);
    else {
		dev_err(ddr_test->dev, "ddr_test number too large (%d)",
				ddr_test->channel_number);
		goto FREE_IN_BUF;
    }
    ddr_test->name = devm_kmalloc(ddr_test->dev,
								  sizeof(DDR_TEST_DEV_NAME) +
								  sizeof(channel_number),
								  GFP_KERNEL);
    if (ddr_test->name == NULL) {
		dev_err(ddr_test->dev, "cannot allocate memory for ddr_test name");
		goto FREE_IN_BUF;
    }
    strcpy(ddr_test->name, DDR_TEST_DEV_NAME);
    strcat(ddr_test->name, channel_number);
    dev_info(ddr_test->dev, "channel name = %s", ddr_test->name);

    ddr_test_reset(ddr_test, 1);
    return 0;
	
    // free previously mapped memory
FREE_IN_BUF:
    dma_free_coherent(ddr_test->dev, BUF_SIZE_BYTES,
					  ddr_test->ring_in.buf,
					  ddr_test->phys_addr_in);
FREE_OUT_BUF:
    devm_iounmap(ddr_test->dev, ddr_test->preg_in);
    devm_iounmap(ddr_test->dev, ddr_test->preg_out);
    dma_free_coherent(ddr_test->dev, BUF_SIZE_BYTES,
					  ddr_test->ring_out.buf,
					  ddr_test->phys_addr_out);
FAIL:
    return ret;
}

static int ddr_test_probe(struct platform_device *pdev) {
    struct ddr_test_dev *ddr_test;
    struct device *dev;
    int ret;

    // ddr_test
    ddr_test = devm_kzalloc(&pdev->dev, sizeof(*ddr_test), GFP_KERNEL);
    if (!ddr_test)
		return -ENOMEM;

    ddr_test->dev = &pdev->dev;
    ddr_test->pdev = pdev;
    dev = ddr_test->dev;

    dev_set_drvdata(&pdev->dev, ddr_test);

    dev_info(ddr_test->dev, "calling parse_of");
    ret = ddr_test_parse_of(ddr_test);
    if (ret < 0)
		return ret;

    dev_info(ddr_test->dev, "calling allocate");
    ret = ddr_test_allocate_resources(ddr_test);
    if (ret < 0)
		return ret;

    if (sysfs_create_group(&pdev->dev.kobj, &ddr_test_attr_group))
		dev_err(ddr_test->dev, "failed to create sysfs entries\n");

    ddr_test->mdev.minor = MISC_DYNAMIC_MINOR;
    ddr_test->mdev.name = ddr_test->name;
    ddr_test->mdev.fops = &ddr_test_fops;

    ret = misc_register(&ddr_test->mdev);
    if (ret) {
		dev_err(ddr_test->dev, "failed to register as misc_device\n");
        goto unregister_sysfs;
    }

    // Create and start the kernel thread
    dev_info(ddr_test->dev, "Creating and waking up polling thread");
    ddr_test->polling_thread = kthread_run(polling_thread_func, ddr_test, "ddr_test_poll_%s", ddr_test->name);
    if (IS_ERR(ddr_test->polling_thread)) {
        ret = PTR_ERR(ddr_test->polling_thread);
        dev_err(ddr_test->dev, "Failed to create polling thread: %d\n", ret);
        ddr_test->polling_thread = NULL;
        goto unregister_misc;
    }

    dev_info(ddr_test->dev, "ddr_test_probe complete");
	return 0;

unregister_misc:
    misc_deregister(&ddr_test->mdev);
unregister_sysfs:
    sysfs_remove_group(&pdev->dev.kobj, &ddr_test_attr_group);
    return ret;
}

static void ddr_test_remove(struct platform_device *pdev) {
    struct ddr_test_dev *ddr_test = dev_get_drvdata(&pdev->dev);

    // Stop the kernel thread before freeing any resources it might be using.
    // kthread_stop() signals the thread to exit and waits for it to finish.
    if (ddr_test->polling_thread) {
        dev_info(ddr_test->dev, "Stopping polling thread");
        kthread_stop(ddr_test->polling_thread);
        ddr_test->polling_thread = NULL;
    }

	dev_info(ddr_test->dev, "asserting reset");
    ddr_test_reset(ddr_test, 1);

    dev_info(ddr_test->dev, "unmapping io memory");
    devm_iounmap(ddr_test->dev, ddr_test->preg_in);
    devm_iounmap(ddr_test->dev, ddr_test->preg_out);

    dev_info(ddr_test->dev, "freeing dma buffers");
    dma_free_coherent(ddr_test->dev, BUF_SIZE_BYTES,
					  ddr_test->ring_in.buf,
					  ddr_test->phys_addr_in);
    dma_free_coherent(ddr_test->dev, BUF_SIZE_BYTES,
					  ddr_test->ring_out.buf,
					  ddr_test->phys_addr_out);

    // free ring buf
    sysfs_remove_group(&pdev->dev.kobj, &ddr_test_attr_group);

    misc_deregister(&ddr_test->mdev);

    dev_set_drvdata(&pdev->dev, NULL);
    dev_info(ddr_test->dev, "remove complete");
}

static const struct of_device_id ddr_test_of_id_table[] = {
    { .compatible = "ddr_test-1.0" },
    {}
};

MODULE_DEVICE_TABLE(of, ddr_test_of_id_table);

static struct platform_driver ddr_test_driver = {
    .driver = {
		.name = "ddr_test",
		.of_match_table = ddr_test_of_id_table,
		.owner = THIS_MODULE,
    },
    .probe = ddr_test_probe,
    .remove = ddr_test_remove,
};

module_platform_driver(ddr_test_driver);

MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("ddr_test: 1.0: DDR_TEST control using kthread");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("ddr_test");

