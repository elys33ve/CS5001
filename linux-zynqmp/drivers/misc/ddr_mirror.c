#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/math64.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/wait.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/circ_buf.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>

#define DDR_MIRROR_DEV_NAME "ddr_mirror"

// 1 init
#define DRIVER_VERSION 1

#define LOCAL_BUF_SIZE_BYTES (64*1024*1024)
#define DDR_MIRROR_BUF_SIZE_BYTES (8*1024)
#define DMA_XFER_SIZE (4*1024)

/**** reg0: ctrl **************/
#define CTRL_OFFSET (0)
#define BIT_RST 1
#define BIT_DDR_MIRROR_EN (1<<1)

/**** reg1: status ************/
#define STATUS_OFFSET (1)
#define BIT_FIFO_EMPTY (1)

/**** reg2: dma control and stats *********/
#define DMA_CTRL_OFFSET (2)
#define BIT_DMA_IN_PROGRESS (1)

/**** reg3: int retry count *********/
#define INT_RETRY_CNTFROM_OFFSET (3)

/**** reg15: ip core version ************/
#define VERSION_OFFSET (15)

void format_date_time(char *date, char *time, char *str);

struct  ddr_mirror_dev {
    struct device *dev;
    struct platform_device *pdev;
    struct miscdevice mdev;

    const char *compatible;

    u32 __iomem *preg;
    u64 __iomem *paxi;
    int paxi_buffer_size;
    dma_addr_t axi_phys_addr;

    struct resource *ddr_mirror_reg_res;
    struct resource *ddr_mirror_axi_res;

	// file buffer
	u8* file_buf;
	u32 file_buf_head;
	u32 file_buf_tail;

    // dma
    struct dma_chan *dma;
    struct dma_slave_config dma_config;
    struct completion dma_ok;
    dma_addr_t dma_handle;
	u8* dma_buf;
    dma_cookie_t                    cookie;
    int active;
    int dma_in_progress;

    // stats
    u32 dma_cnt;
};

static void ddr_mirror_start(struct ddr_mirror_dev *ddr_mirror);
static void ddr_mirror_stop(struct ddr_mirror_dev *ddr_mirror);

/////////////////////////////////////////////////////////////////////////////////////////
// register and bit accessors
/////////////////////////////////////////////////////////////////////////////////////////
static inline u32 read_ddr_mirror_reg(struct ddr_mirror_dev *ddr_mirror,
                                       int offset) {
    return ioread32(ddr_mirror->preg+offset);
}
static inline void write_ddr_mirror_reg(struct ddr_mirror_dev *ddr_mirror,
                                         int offset, u32 data) {
    iowrite32(data, ddr_mirror->preg+offset);
}
static inline int ddr_mirror_bitget(struct ddr_mirror_dev *ddr_mirror,
                                     int offset, u32 bitmask) {
    u32 reg;
	
    reg = read_ddr_mirror_reg(ddr_mirror, offset);
    return (reg & bitmask) ? 1 : 0;
}
static inline void ddr_mirror_bitset(struct ddr_mirror_dev *ddr_mirror,
                                      int offset, u32 bitmask) {
    u32 reg;
    u32 reg_set;
	
    reg = read_ddr_mirror_reg(ddr_mirror, offset);
    reg_set = reg | bitmask;
    write_ddr_mirror_reg(ddr_mirror, offset, reg_set);
}
static inline void ddr_mirror_bitclr(struct ddr_mirror_dev *ddr_mirror,
                                      int offset, u32 bitmask) {
    u32 reg;
    u32 reg_clr;
	
    reg = read_ddr_mirror_reg(ddr_mirror, offset);
    reg_clr = reg & ~bitmask;
    write_ddr_mirror_reg(ddr_mirror, offset, reg_clr);
}
static inline void ddr_mirror_bitwrite(struct ddr_mirror_dev *ddr_mirror,
                                        int offset, u32 bitmask, int value) {
    if (value)
        ddr_mirror_bitset(ddr_mirror, offset, bitmask);
    else
        ddr_mirror_bitclr(ddr_mirror, offset, bitmask);
}
static inline void ddr_mirror_bittoggle(struct ddr_mirror_dev *ddr_mirror,
                                         int offset, u32 bitmask) {
    ddr_mirror_bitclr(ddr_mirror, offset, bitmask);
    ddr_mirror_bitset(ddr_mirror, offset, bitmask);
    ddr_mirror_bitclr(ddr_mirror, offset, bitmask);
}

/////////////////////////////////////////////////////////////////////////////////////////
// dma -- for test the callback will copy the received data into the
//    corresponding tx buffer and dma the data back to the fpga fabric for test
//    using the internal bert
/////////////////////////////////////////////////////////////////////////////////////////
static void dma_callback(void *completion) {
    struct ddr_mirror_dev *ddr_mirror =
        container_of(completion, struct ddr_mirror_dev, dma_ok);
    struct dma_tx_state state;
    enum dma_status     status;

    //dev_info(ddr_mirror->dev, "entering dma_callback");
    ddr_mirror->dma_cnt++;
    status = dmaengine_tx_status(ddr_mirror->dma, ddr_mirror->cookie, &state);
    if (status != DMA_COMPLETE) {
        // wait for all xfers to complete
        return;
    }
    //ddr_mirror_bitclr(ddr_mirror, DMA_CTRL_OFFSET, BIT_DMA_IN_PROGRESS);
    complete(completion);
	smp_store_release(&ddr_mirror->dma_in_progress, 0);
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

static void ddr_mirror_start(struct ddr_mirror_dev *ddr_mirror) {
    struct dma_slave_config *dma_config = &ddr_mirror->dma_config;

    ddr_mirror->active = 1;
    ddr_mirror_bitclr(ddr_mirror, CTRL_OFFSET, BIT_RST);

    // allocate channel
    dma_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_16_BYTES;
    ddr_mirror->dma = dma_request_chan(ddr_mirror->dev, "dma");
    dma_config->direction = DMA_MEM_TO_MEM;
    if (IS_ERR(ddr_mirror->dma)) 
        dev_err(ddr_mirror->dev, "Failed to alloc dma chan\n");
    dmaengine_slave_config(ddr_mirror->dma, dma_config);
    dev_info(ddr_mirror->dev, "dma start");
}

static void ddr_mirror_stop(struct ddr_mirror_dev *ddr_mirror) {
    msleep(500);
    ddr_mirror_bitset(ddr_mirror, CTRL_OFFSET, BIT_RST);
    ddr_mirror->active = 0;
    dma_release_channel(ddr_mirror->dma);
    dev_info(ddr_mirror->dev, "dma stopped");
}

static ssize_t write_chunk(struct ddr_mirror_dev *ddr_mirror) {
    enum dma_ctrl_flags dma_flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
    struct dma_async_tx_descriptor *dma_desc = NULL;
    dma_addr_t src=0;
    dma_addr_t dest=0;
    u8* ptr = ddr_mirror->file_buf + ddr_mirror->file_buf_tail;
    u8* dma_ptr = ddr_mirror->dma_buf;
    int timeout_cnt;

	memcpy(dma_ptr, ptr, DMA_XFER_SIZE);
    
    src = ddr_mirror->dma_handle;
    dest =  (dma_addr_t)ddr_mirror->axi_phys_addr;
    dma_desc = dmaengine_prep_dma_memcpy(ddr_mirror->dma,
                                         dest, src,
                                         DMA_XFER_SIZE,
                                         dma_flags);
    if (dma_desc == NULL) {
        dev_err(ddr_mirror->dev, "Error in dmaengine_prep_dma_memcpy");
    }
    else {
        dma_desc->callback = dma_callback;
        dma_desc->callback_param = &ddr_mirror->dma_ok;
        ddr_mirror->cookie = dmaengine_submit(dma_desc);
        if (dma_submit_error(ddr_mirror->cookie)) {
            dev_err(ddr_mirror->dev, "Failed to submit DMA transfer");
        }
	}

    timeout_cnt = 0;
    while (ddr_mirror->dma_in_progress) {
        udelay(500);
        timeout_cnt++;
        if (timeout_cnt > 5000) {
            dev_err(ddr_mirror->dev, "timeout waiting for dma complete (file_buf_tail = %u)",
                    ddr_mirror->file_buf_tail);
            return -1;
        }
    }
	smp_store_release(&ddr_mirror->dma_in_progress, 1);
    dma_async_issue_pending(ddr_mirror->dma);
    //ddr_mirror_bitset(ddr_mirror, DMA_CTRL_OFFSET, BIT_DMA_IN_PROGRESS);
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// file i/o
/////////////////////////////////////////////////////////////////////////////////////////
static inline struct ddr_mirror_dev *file_to_ddr_mirror_dev(struct file *file) {
    struct miscdevice *miscdev = file->private_data;

    return container_of(miscdev, struct ddr_mirror_dev, mdev);
}

static int ddr_mirror_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int ddr_mirror_close(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t ddr_mirror_write(struct file *file, const char __user *buf,
                             size_t len, loff_t *ppos)
{
    struct ddr_mirror_dev *ddr_mirror = file_to_ddr_mirror_dev(file);
	u8* ptr = ddr_mirror->file_buf + ddr_mirror->file_buf_head;
	int r;
//	int available;
	r = copy_from_user(ptr, buf, len);
	if (r)
		dev_info(ddr_mirror->dev, "write: r = %d", r);
	ddr_mirror->file_buf_head += len;

	// send the data we can
/*	available = ddr_mirror->file_buf_head - ddr_mirror->file_buf_tail;
	while (available > 4095) {
		r = write_chunk(ddr_mirror);
		if (r) {
			dev_err(ddr_mirror->dev, "write: error writing chunk");
			return 0;
		}
		available -= 4096;
		ddr_mirror->file_buf_tail += 4096;
	}
*/
    return len;
}

static ssize_t ddr_mirror_read(struct file *file, char __user *buf,
                            size_t len, loff_t *ppos)
{
//    struct ddr_mirror_dev *ddr_mirror = file_to_ddr_mirror_dev(file);
    return len;
}

static const struct file_operations ddr_mirror_fops = {
    .owner = THIS_MODULE,
    .write = ddr_mirror_write,
    .read = ddr_mirror_read,
    .open = ddr_mirror_open,
    .release = ddr_mirror_close,
    .llseek = noop_llseek,
};

/////////////////////////////////////////////////////////////////////////////////////////
// sysfs
/////////////////////////////////////////////////////////////////////////////////////////
static ssize_t restart_store(struct device *dev, struct device_attribute *attr,
                               const char *buf, size_t count) {
    struct ddr_mirror_dev *ddr_mirror = dev_get_drvdata(dev);
	ddr_mirror->dma_in_progress = 0;
    ddr_mirror->dma_cnt = 0;
    ddr_mirror->file_buf_head = 0;
    ddr_mirror->file_buf_tail = 0;
	memset(ddr_mirror->file_buf, 0, LOCAL_BUF_SIZE_BYTES);

    ddr_mirror_start(ddr_mirror);
    return count;
}
static DEVICE_ATTR_WO(restart);

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

static ssize_t write_once_store(struct device *dev, struct device_attribute *attr,
                               const char *buf, size_t count) {
    struct ddr_mirror_dev *ddr_mirror = dev_get_drvdata(dev);
    enum dma_ctrl_flags dma_flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
    struct dma_async_tx_descriptor *dma_desc = NULL;
    dma_addr_t src=0;
    dma_addr_t dest=0;
    int x;
    u8* ptr = ddr_mirror->dma_buf;
    int timeout_cnt;

    for (x=0; x<DMA_XFER_SIZE; x++) {
        *ptr++ = (x+1) & 0x0FF;
    }
    
    src = ddr_mirror->dma_handle;
    dest =  (dma_addr_t)ddr_mirror->axi_phys_addr;
    dma_desc = dmaengine_prep_dma_memcpy(ddr_mirror->dma,
                                         dest, src,
                                         DMA_XFER_SIZE,
                                         dma_flags);
    if (dma_desc == NULL) {
        dev_err(ddr_mirror->dev, "write_once: Error in dmaengine_prep_dma_memcpy");
    }
    else {
        dma_desc->callback = dma_callback;
        dma_desc->callback_param = &ddr_mirror->dma_ok;
        ddr_mirror->cookie = dmaengine_submit(dma_desc);
        if (dma_submit_error(ddr_mirror->cookie)) {
            dev_err(ddr_mirror->dev, "Failed to submit DMA transfer");
        }
	}

    timeout_cnt = 0;
    while (ddr_mirror->dma_in_progress) {
        udelay(500);
        timeout_cnt++;
        if (timeout_cnt > 5000) {
            dev_err(ddr_mirror->dev, "timeout waiting for dma");
            return count;
        }
    }
    dma_async_issue_pending(ddr_mirror->dma);

    return count;
}
static DEVICE_ATTR_WO(write_once);

static ssize_t send_store(struct device *dev, struct device_attribute *attr,
                               const char *buf, size_t count) {
    struct ddr_mirror_dev *ddr_mirror = dev_get_drvdata(dev);
	int ret;

    dev_info(ddr_mirror->dev, "setting ddr_mirror_en");
    write_ddr_mirror_reg(ddr_mirror, CTRL_OFFSET, BIT_DDR_MIRROR_EN);
                         
    dev_info(ddr_mirror->dev, "sending %u bytes", ddr_mirror->file_buf_head);

	while (ddr_mirror->file_buf_tail < (32*1024*1024)) {
		ret = write_chunk(ddr_mirror);
		if (ret) {
			dev_err(ddr_mirror->dev, "send: error writing chunk");
			return 0;
		}
		ddr_mirror->file_buf_tail += 4096;
        while (ddr_mirror->dma_in_progress) {
            udelay(100);
        }
	}
    ddr_mirror_stop(ddr_mirror);
    dev_info(ddr_mirror->dev, "deasserting ddr_mirror_en");
    write_ddr_mirror_reg(ddr_mirror, CTRL_OFFSET, 0);
    return count;
}
static DEVICE_ATTR_WO(send);

static struct attribute *ddr_mirror_attrs[] = {
    &dev_attr_restart.attr,
    &dev_attr_build_time.attr,
    &dev_attr_write_once.attr,
    &dev_attr_send.attr,
    NULL,
};

static struct attribute_group ddr_mirror_attr_group = {
    .name = NULL, /* put in device directory */
    .attrs = ddr_mirror_attrs,
};

/////////////////////////////////////////////////////////////////////////////////////////
// probe, remove, parse_of, etc
/////////////////////////////////////////////////////////////////////////////////////////
static int ddr_mirror_parse_of(struct ddr_mirror_dev *ddr_mirror)
{
    struct device *dev = ddr_mirror->dev;
    struct device_node *node = dev->of_node;

    struct resource *res;

    int ret;

    // of node
    ret = of_property_read_string(node, "compatible", &ddr_mirror->compatible);
    if (ret < 0) {
        dev_err(dev, "ddr_mirror not in DT");
        return ret;
    }

    // reg -- axi lite
    res = platform_get_resource(ddr_mirror->pdev, IORESOURCE_MEM, 0);
    if (res == NULL) {
        dev_err(dev, "error getting reg addr from dts");
        return -EINVAL;
    }
    ddr_mirror->ddr_mirror_reg_res = res;
		
    // reg -- axi full
    res = platform_get_resource(ddr_mirror->pdev, IORESOURCE_MEM, 1);
    if (res == NULL) {
        dev_err(dev, "error getting reg addr from dts");
        return -EINVAL;
    }
    ddr_mirror->ddr_mirror_axi_res = res;
		
    dev_info(dev, "Done parsing DT");

    return 0;
}

static int ddr_mirror_allocate_resources(struct ddr_mirror_dev *ddr_mirror) {
    int ret = -EINVAL;
    int addr_size;
    u32 start;
    u32 end;

    // reset statistics
    ddr_mirror->dma_cnt = 0;

    // reset other members
    ddr_mirror->active = 0;
    ddr_mirror->preg = NULL;
    init_completion(&ddr_mirror->dma_ok);
	ddr_mirror->file_buf_head = 0;
	ddr_mirror->file_buf_tail = 0;
	ddr_mirror->dma_in_progress = 0;

    // map ddr_mirror device memory
    start = (u32)ddr_mirror->ddr_mirror_reg_res->start;
    end = (u32)ddr_mirror->ddr_mirror_reg_res->end;
    addr_size = end - start + 1;
    ddr_mirror->preg =
        (u32*)devm_ioremap(ddr_mirror->dev,
                           ddr_mirror->ddr_mirror_reg_res->start,
                           addr_size);
    if (ddr_mirror->preg == NULL) {
        dev_err(ddr_mirror->dev, "Error mapping ddr_mirror base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    // map ddr_mirror device memory (axi full)
    start = (u32)ddr_mirror->ddr_mirror_axi_res->start;
    end = (u32)ddr_mirror->ddr_mirror_axi_res->end;
    addr_size = end - start + 1;
    ddr_mirror->paxi_buffer_size = addr_size;
    ddr_mirror->axi_phys_addr = (dma_addr_t)start;
    dev_info(ddr_mirror->dev, "axi_phys_addr = 0x%x", (u32)ddr_mirror->axi_phys_addr);
    ddr_mirror->paxi =
        (u64*)ioremap(ddr_mirror->ddr_mirror_axi_res->start,
                      addr_size);
    if (ddr_mirror->paxi == NULL) {
        dev_err(ddr_mirror->dev, "Error mapping ddr_mirror axi base memory");
        ret = -ENOMEM;
        goto UNMAP_MEM;
    }

	// allocate memory to store the file that will be transferred
    ddr_mirror->file_buf = vmalloc(LOCAL_BUF_SIZE_BYTES);
    if (ddr_mirror->file_buf == NULL) {
        dev_err(ddr_mirror->dev, "cannot allocate memory for file buffer");
        goto UNMAP_MEM;
    }
	memset(ddr_mirror->file_buf, 0, LOCAL_BUF_SIZE_BYTES);

    // init the ctrl reg to ddr_mirror_rst (making everything else 0)
    write_ddr_mirror_reg(ddr_mirror, CTRL_OFFSET, BIT_RST);

    // dma out
    ddr_mirror->dma_buf = dma_alloc_coherent(ddr_mirror->dev,
                       DDR_MIRROR_BUF_SIZE_BYTES,
                       &ddr_mirror->dma_handle, GFP_KERNEL);
    if (!ddr_mirror) {
        dev_err(ddr_mirror->dev, "Failed to allocate memory for DMA \r\n");
        goto UNMAP_AXI_FULL_MEM;
    }
    ddr_mirror_start(ddr_mirror);

    return 0;

UNMAP_AXI_FULL_MEM:
    // free previously mapped memory
    iounmap(ddr_mirror->paxi);
UNMAP_MEM:
    // free previously mapped memory
    devm_iounmap(ddr_mirror->dev, ddr_mirror->preg);
FAIL:
    return ret;
}

static int ddr_mirror_probe(struct platform_device *pdev) {
    struct ddr_mirror_dev *ddr_mirror;
    struct device *dev;
    int ret;

    // ddr_mirror
    ddr_mirror = devm_kzalloc(&pdev->dev, sizeof(*ddr_mirror), GFP_KERNEL);
    if (!ddr_mirror)
        return -ENOMEM;

    ddr_mirror->dev = &pdev->dev;
    ddr_mirror->pdev = pdev;
    dev = ddr_mirror->dev;

    dev_set_drvdata(&pdev->dev, ddr_mirror);
	
    ret = ddr_mirror_parse_of(ddr_mirror);
    if (ret < 0)
        return ret;

    ret = ddr_mirror_allocate_resources(ddr_mirror);
    if (ret < 0)
        return ret;

    if (sysfs_create_group(&pdev->dev.kobj, &ddr_mirror_attr_group))
        dev_err(ddr_mirror->dev, "failed to create sysfs entries\n");

    ddr_mirror->mdev.minor = MISC_DYNAMIC_MINOR;
    ddr_mirror->mdev.name = "ddr_mirror";
    ddr_mirror->mdev.fops = &ddr_mirror_fops;

    ret = misc_register(&ddr_mirror->mdev);
    if (ret) {
        dev_err(ddr_mirror->dev, "failed to register as misc_device\n");
        return ret;
    }

    dev_info(ddr_mirror->dev, "ddr_mirror_probe complete");

    return 0;
}

static void ddr_mirror_remove(struct platform_device *pdev) {
    struct ddr_mirror_dev *ddr_mirror = dev_get_drvdata(&pdev->dev);
    
    dev_info(ddr_mirror->dev, "asserting reset");
    ddr_mirror_stop(ddr_mirror);

    dev_info(ddr_mirror->dev, "unmapping io memory");
    devm_iounmap(ddr_mirror->dev, ddr_mirror->preg);
    iounmap(ddr_mirror->paxi);

    dev_info(ddr_mirror->dev, "freeing buffers");
    dma_free_coherent(ddr_mirror->dev, DDR_MIRROR_BUF_SIZE_BYTES,
                          ddr_mirror->dma_buf, ddr_mirror->dma_handle);
    sysfs_remove_group(&pdev->dev.kobj, &ddr_mirror_attr_group);

    misc_deregister(&ddr_mirror->mdev);

    vfree(ddr_mirror->file_buf);

    dev_set_drvdata(&pdev->dev, NULL);
    dev_info(ddr_mirror->dev, "remove complete");
}

static const struct of_device_id ddr_mirror_of_id_table[] = {
    { .compatible = "ddr_mirror-1.0" },
    {}
};

MODULE_DEVICE_TABLE(of, ddr_mirror_of_id_table);

static struct platform_driver ddr_mirror_driver = {
    .driver = {
        .name = "ddr_mirror",
        .of_match_table = ddr_mirror_of_id_table,
        .owner = THIS_MODULE,
    },
    .probe = ddr_mirror_probe,
    .remove = ddr_mirror_remove,
};

module_platform_driver(ddr_mirror_driver);

MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("ddr_mirror: 1.0: DDR_MIRROR control");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("ddr_mirror");

