#include <linux/module.h>
#include <linux/dma-mapping.h>
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
#include <linux/limits.h>
#include <linux/kthread.h> 
#include <linux/ktime.h> 
#include <linux/sched.h>   

#define HWACCEL_DEV_NAME "hwaccel"

/////////// system PARAMS //////////////////
#define MAX_FRAME_SIZE_BYTES 0x0FFF
#define STATUS_BYTES 2

#define BUF_SIZE_BYTES (1024*1024)
#define HWACCEL_PAGE_SIZE (4096)
#define LAST_PAGE 255

/*** internal test / bert generator ***/
// clockgen accumulator width
// hard-coded in vhdl (32 bits)
#define ACCUM 0x100000000ULL

// max system bitrate
#define MAX_BITRATE 51000000

// platforms -- must agree with tmoip_system.c
#define PLATFORM_TYPE_8_CHAN_3EG 0
#define PLATFORM_TYPE_TMOIP_3CH 1
#define PLATFORM_TYPE_TMOIP_12CH 2
#define PLATFORM_TYPE_AVNET_CC7EV 3
#define PLATFORM_TYPE_XILINX_ZCU102 4
#define PLATFORM_TYPE_TMOIP_MLSC 5
#define PLATFORM_TYPE_TMOIP_BSSA 6

/////////////////////////////////////
///////////// registers /////////////
/////////////////////////////////////
/*** reg0: ctrl ****************/
#define CTRL_OFFSET (0)
#define BIT_RST 1
#define BIT_EN (1<<1)
#define BIT_SHUTDOWN (1<<2)
#define BIT_AXIM_NRST (1<<3)

/*** reg1: status ****************/
#define STATUS_OFFSET (1)

/*** reg2: axim_in_base address ***/
#define AXIM_IN_DDR_BASE_ADDR_OFFSET (2)
/*** reg3: axim_out_base address ***/
#define AXIM_OUT_DDR_BASE_ADDR_OFFSET (3)

/*** reg4: frame_size ****************/
#define FRAME_SIZE_OFFSET (4)
#define LINES_PER_FRAME_SHIFT (16)
#define FRAME_SIZE_BYTES_MASK (0x0FFF)

/*** reg5: delay_cntto ****************/
#define DELAY_CNTTO_OFFSET (5)

/*** reg31: version ****************/
#define VERSION_OFFSET (31)

void format_date_time(char *date, char *time, char *str);

// axim to cpu, 1 byte sync, 6 byte sequence number, 0xfe, timestamp ctrl
// cpu to axim, 1 byte sync, 7 byte sequence number, 32-bit bitrate, 32-bit nco step
#define INTERNAL_HDR_SIZE_BYTES 16
#define INTERNAL_SYNC_BYTE 0xEB
#define TIMESTAMP_HDR 0xFE

struct  hwaccel_dev {
   struct device *dev;
   struct platform_device *pdev;
   struct miscdevice mdev;

   const char *compatible;
  
   u32 __iomem *preg;
  
   struct resource *reg_res;
  
   char *name;

   u32 hwaccel_active;
   u32 hwaccel_number;
  
   u32 en;
   u32 cntto;

   u64 write_call_cnt;

   // frame parameters including internal tests
   u32 frame_size_bytes; // total configured frame size (non-internal)
   u32 axim_frame_size_bytes; // total bytes including internal header, aligned to 128bits
   u64 read_wait_cnt;
   u64 write_wait_cnt;
   u64 last_seq_num;
   u32 seq_mismatch_cnt;
   u32 seq_mismatch_expected;
   u32 seq_mismatch_received;
    u32 delay_cntto;

   u32 pl_to_driver_page_diff;
   u32 driver_to_pl_page_diff;

   // buffers
   // ring_in for axim_in (cpu to fpga)
   struct circ_buf ring_in;
   dma_addr_t phys_addr_in;
   // ring_out for axim_out (fpga to cpu)
   struct circ_buf ring_out;
   dma_addr_t phys_addr_out;
	
   u32 current_read_page;
   u32 current_write_page;

   u64 write_seq;

   u64 total_write_bytes;
   u64 total_read_bytes;
};

/////////////////////////////////////////////////////////////////////////////////////////
// register and bit accessors
/////////////////////////////////////////////////////////////////////////////////////////
// hwaccel_in
static inline u32 read_reg(struct hwaccel_dev *hwaccel,
                                      int offset) {
   u32 reg;
   reg = ioread32(hwaccel->preg+offset);
   return reg;
}
static inline void write_reg(struct hwaccel_dev *hwaccel,
                                        int offset, u32 data) {
   iowrite32(data, hwaccel->preg+offset);
}
static inline int bitget(struct hwaccel_dev *hwaccel,
                                    int offset, u32 bitmask) {
   u32 reg;
	
   reg = read_reg(hwaccel, offset);
   return (reg & bitmask) ? 1 : 0;
}
static inline void bitset(struct hwaccel_dev *hwaccel,
                                     int offset, u32 bitmask) {
   u32 reg;
   u32 reg_set;

   reg = read_reg(hwaccel, offset);
   reg_set = reg | bitmask;
   write_reg(hwaccel, offset, reg_set);
}
static inline void bitclr(struct hwaccel_dev *hwaccel,
                                     int offset, u32 bitmask) {
   u32 reg;
   u32 reg_clr;

   reg = read_reg(hwaccel, offset);
   reg_clr = reg & ~bitmask;
   write_reg(hwaccel, offset, reg_clr);
}
static inline void bitwrite(struct hwaccel_dev *hwaccel,
                                       int offset, u32 bitmask, int value) {
   if (value)
      bitset(hwaccel, offset, bitmask);
   else
      bitclr(hwaccel, offset, bitmask);
}
static inline void bittoggle(struct hwaccel_dev *hwaccel,
                                        int offset, u32 bitmask) {
   bitset(hwaccel, offset, bitmask);
   bitclr(hwaccel, offset, bitmask);
}

/////////////////////////////////////////////////////////////////////////////////////////
// misc helper functions
////////////////////////////////////////////////////////////////////////////////////////
/* check the given page to see if there is data available to read into the read buffer */
static inline int read_page_available(struct hwaccel_dev *hwaccel) {
   u32 page = hwaccel->current_read_page;
   u8 *pr = (hwaccel->ring_out.buf) + (page * 4096); // MSB of FPGA data to CPU
   if (*pr == INTERNAL_SYNC_BYTE)
      return 1;
   else
      return 0;
}
/* check to see if the page is available to write */
static inline int write_page_available(struct hwaccel_dev *hwaccel) {
   u32 page = hwaccel->current_write_page;
   u8 *pr = (hwaccel->ring_in.buf) + (page * 4096); // MSB of FPGA data to CPU
   if (*pr == 0) 
      return 1;
   else 
      return 0;
}
static inline u64 read_get_seq(struct hwaccel_dev *hwaccel) {
   u8 *p = hwaccel->ring_out.buf + (HWACCEL_PAGE_SIZE * hwaccel->current_read_page);
   u64 seq;
   seq = ((u64)*(p+1)<<40) | ((u64)*(p+2)<<32) | ((u64)*(p+3)<<24) | ((u64)*(p+4)<<16) |
      ((u64)*(p+5)<<8) | (u64)*(p+6);
   return seq;
}

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

static void set_frame_size_bytes(struct hwaccel_dev *hwaccel) {
   u32 axim_frame_size_bytes; // total bytes including internal headers
   u32 axim_frame_size_lines;
   u32 reg;
   // FRAME_SIZE_OFFSET PROVIDES number of bytes per frame not including the internal header
   //  and the number of total lines (128-bit lines), including the internal header
   //  this is used in the FPGA logic
   axim_frame_size_bytes = hwaccel->frame_size_bytes + INTERNAL_HDR_SIZE_BYTES;
   if ((axim_frame_size_bytes & 0x0F) != 0) {
      axim_frame_size_bytes &= 0xFFFFFFF0;
      axim_frame_size_bytes += 0x010;
   }
   hwaccel->axim_frame_size_bytes = axim_frame_size_bytes;
   axim_frame_size_lines= axim_frame_size_bytes>>4;
   reg = (axim_frame_size_lines<<16) | hwaccel->frame_size_bytes;
   write_reg(hwaccel, FRAME_SIZE_OFFSET, reg);
   dev_info(hwaccel->dev, "frame_size_bytes = %u, axim_frame_size_bytes = %u",
            hwaccel->frame_size_bytes, hwaccel->axim_frame_size_bytes);
}
static void reset(struct hwaccel_dev *hwaccel, int state) {
   if (state) {
      bitset(hwaccel, CTRL_OFFSET, BIT_SHUTDOWN);
      msleep(5);
      hwaccel->total_read_bytes = 0;
      hwaccel->seq_mismatch_cnt = 0;
      bitclr(hwaccel, CTRL_OFFSET, BIT_EN);
      bitset(hwaccel, CTRL_OFFSET, BIT_RST);
      bitclr(hwaccel, CTRL_OFFSET, BIT_AXIM_NRST);
      hwaccel->read_wait_cnt = 0;
      hwaccel->current_read_page = 0;
      hwaccel->pl_to_driver_page_diff = 0;
      hwaccel->total_write_bytes = 0;
      hwaccel->write_seq = 0;
      hwaccel->write_wait_cnt = 0;
      hwaccel->current_write_page = 0;
      hwaccel->driver_to_pl_page_diff = 0;
      hwaccel->write_call_cnt = 0;

      memset(hwaccel->ring_out.buf, 0, BUF_SIZE_BYTES);
      memset(hwaccel->ring_in.buf, 0, BUF_SIZE_BYTES);
   }
   else {
      bitclr(hwaccel, CTRL_OFFSET, BIT_SHUTDOWN);
      bitset(hwaccel, CTRL_OFFSET, BIT_EN);
      bitclr(hwaccel, CTRL_OFFSET, BIT_RST);
      bitset(hwaccel, CTRL_OFFSET, BIT_AXIM_NRST);
   }
}

/////////////////////////////////////////////////////////////////////////////////////////
// hwaccel file i/o
/////////////////////////////////////////////////////////////////////////////////////////
static inline struct hwaccel_dev *file_to_hwaccel_dev(struct file *file) {
   struct miscdevice *miscdev = file->private_data;

   return container_of(miscdev, struct hwaccel_dev, mdev);
}

static int hwaccel_open(struct inode *inode, struct file *file)
{
   return 0;
}

static int hwaccel_close(struct inode *inode, struct file *file)
{
   return 0;
}

// expects 64 bit padding (reserved) followed by data 
static ssize_t hwaccel_write(struct file *file, const char __user *buf,
                             size_t len, loff_t *ppos)
{
   struct hwaccel_dev *hwaccel = file_to_hwaccel_dev(file);
   int r;
   u32 current_write_page = hwaccel->current_write_page;
   u64 seq = hwaccel->write_seq;
   u32 frame_size_bytes = hwaccel->frame_size_bytes;

   volatile u8 *p = hwaccel->ring_in.buf + (HWACCEL_PAGE_SIZE * current_write_page);
      
   if (!write_page_available(hwaccel)) {
      hwaccel->write_wait_cnt++;
      return 0;
   }   
   else {
      hwaccel->write_call_cnt++;
      
      //////////////////////////////////////////////////
      // form frame for the FPGA
      //////////////////////////////////////////////////
      p++; // write sync last
      *p++ = (frame_size_bytes >> 8) & 0x0FF;
      *p++ = frame_size_bytes & 0x0FF;
      *p++ = (seq >> 32) & 0x0FF;
      *p++ = (seq >> 24) & 0x0FF;
      *p++ = (seq >> 16) & 0x0FF;
      *p++ = (seq >> 8) & 0x0FF;
      *p++ = seq & 0x0FF;
      // reserved
      *p++ = 0;
      *p++ = 0;
      *p++ = 0;
      *p++ = 0;
      *p++ = 0;
      *p++ = 0;
      *p++ = 0;
      *p++ = 0;

      // frame_size_bytes, starting after the padding
      r = copy_from_user((void *)p, buf, frame_size_bytes);
      if (r)
         dev_info(hwaccel->dev, "write: r = %d", r);
      // write sync to indicate page ready
      wmb();
      p = hwaccel->ring_in.buf + (HWACCEL_PAGE_SIZE * current_write_page);
      *p = 0xEB;

      //////////////////////////////////////////////////
      // sysvar updates
      //////////////////////////////////////////////////
      hwaccel->total_write_bytes += len;
      current_write_page++;
      if (current_write_page > LAST_PAGE) {
         current_write_page = 0;
      }
      hwaccel->write_seq++;
      hwaccel->current_write_page = current_write_page;
   }
   return len;
}

// returns a single frame as
// 0xEB
// 48 bit sequence number
// 0xFE
// <reserved>
static ssize_t hwaccel_read(struct file *file, char __user *buf,
                            size_t len, loff_t *ppos)
{
   struct hwaccel_dev *hwaccel = file_to_hwaccel_dev(file);
   int r;
   u32 frame_size_bytes = hwaccel->frame_size_bytes;
   u8 current_read_page = hwaccel->current_read_page;
   u64 seq;
   u64 seq_1 = hwaccel->last_seq_num;
   volatile u8 *p = hwaccel->ring_out.buf + (HWACCEL_PAGE_SIZE * current_read_page);

   if (!read_page_available(hwaccel)) {
      hwaccel->read_wait_cnt++;
      return 0;
   }

   seq = read_get_seq(hwaccel);
   if (seq != seq_1 + 1) {
      hwaccel->seq_mismatch_cnt++;
      hwaccel->seq_mismatch_expected = seq_1 + 1;
      hwaccel->seq_mismatch_received = seq;
   }
   hwaccel->last_seq_num = seq;
   r = copy_to_user(buf, (const void*)p+16, frame_size_bytes);
   if (r)
      dev_info(hwaccel->dev, "read: r = %d", r);
   wmb();
   // zero out this page
   *p = 0x00;

   // update stats and pointers
   hwaccel->total_read_bytes += frame_size_bytes;
   current_read_page++;
   if (current_read_page > LAST_PAGE) { // wrap
      current_read_page = 0;
   }
   hwaccel->current_read_page = current_read_page;
   return frame_size_bytes;
}

static const struct file_operations hwaccel_fops = {
   .owner = THIS_MODULE,
   .write = hwaccel_write,
   .read = hwaccel_read,
   .open = hwaccel_open,
   .release = hwaccel_close,
   .llseek = noop_llseek,
};

/////////////////////////////////////////////////////////////////////////////////////////
// sysfs
/////////////////////////////////////////////////////////////////////////////////////////
static ssize_t en_show(struct device *dev, struct device_attribute *attr,
                             char *buf) {
   struct hwaccel_dev *hwaccel = dev_get_drvdata(dev);
   return sprintf(buf, "%s\n", hwaccel->en ? "enabled" : "disabled");
}
static ssize_t en_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count) {
   struct hwaccel_dev *hwaccel = dev_get_drvdata(dev);
   unsigned int val;
   int en;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(hwaccel->dev, "Error in sysfs/en");
      return count;
   }
   en = val ? 1 : 0;
   if (en) {
      if (hwaccel->en) {
         dev_info(hwaccel->dev, "already enabled");
         return count;
      }
      else {
         hwaccel->en = 1;
         reset(hwaccel, 0);
         dev_info(hwaccel->dev, "enabled");
         return count;

      }
   }
   else {
      if (!hwaccel->en) {
         dev_info(hwaccel->dev, "already disabled");
         return count;
      }
      else {
         hwaccel->en = 0;
         reset(hwaccel, 1);
         dev_info(hwaccel->dev, "disabled");
         return count;
      }
   }
   return count;
}
static DEVICE_ATTR_RW(en);

static ssize_t total_write_bytes_show(struct device *dev, struct device_attribute *attr,
                                      char *buf) {
   struct hwaccel_dev *hwaccel = dev_get_drvdata(dev);
   u64 bytes = hwaccel->total_write_bytes;
   hwaccel->total_write_bytes = 0;
   return sprintf(buf, "%llu\n", bytes);
}
static	DEVICE_ATTR_RO(total_write_bytes);

static ssize_t total_read_bytes_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct hwaccel_dev *hwaccel = dev_get_drvdata(dev);
   u64 bytes = hwaccel->total_read_bytes;
   hwaccel->total_read_bytes = 0;
   return sprintf(buf, "%llu\n", bytes);
}
static	DEVICE_ATTR_RO(total_read_bytes);

static ssize_t frame_size_bytes_show(struct device *dev, struct device_attribute *attr,
                                       char *buf) {
   struct hwaccel_dev *hwaccel = dev_get_drvdata(dev);
   return sprintf(buf, "%d\n", hwaccel->frame_size_bytes);
}
static ssize_t frame_size_bytes_store(struct device *dev, struct device_attribute *attr,
                                        const char *buf, size_t count) {
   struct hwaccel_dev *hwaccel = dev_get_drvdata(dev);
   unsigned int val;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(hwaccel->dev, "Error in sysfs/frame_size_bytes");
      return count;
   }
   hwaccel->frame_size_bytes = val;
   set_frame_size_bytes(hwaccel);
   return count;
}
static	DEVICE_ATTR_RW(frame_size_bytes);

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

static struct attribute *hwaccel_attrs[] = {
   &dev_attr_en.attr,
   &dev_attr_total_write_bytes.attr,
   &dev_attr_total_read_bytes.attr,
   &dev_attr_frame_size_bytes.attr,
   &dev_attr_build_time.attr,
   NULL,
};

static struct attribute_group hwaccel_attr_group = {
   .name = NULL, /* put in device directory */
   .attrs = hwaccel_attrs,
};

/////////////////////////////////////////////////////////////////////////////////////////
// probe, remove, parse_of, etc
/////////////////////////////////////////////////////////////////////////////////////////
static int hwaccel_parse_of(struct hwaccel_dev *hwaccel)
{
   struct device *dev = hwaccel->dev;
   struct device_node *node = dev->of_node;
   struct resource *res;

   int ret;

   // of node
   ret = of_property_read_string(node, "compatible", &hwaccel->compatible);
   if (ret < 0) {
      dev_err(dev, "hwaccel not in DT");
      return ret;
   }

   // hwaccel number
   ret = of_property_read_u32(node, "hwaccel_number", &hwaccel->hwaccel_number);
   if (ret) {
      dev_err(hwaccel->dev, "error reading hwaccel from dts");
      return ret;
   }
   dev_info(hwaccel->dev, "hwaccel%d\n", hwaccel->hwaccel_number);

   // reg -- axi hwaccel
   res = platform_get_resource(hwaccel->pdev, IORESOURCE_MEM, 0);
   if (res == NULL) {
      dev_err(dev, "error getting hwaccel_in base reg addr from dts");
      return -EINVAL;
   }
   hwaccel->reg_res = res;

   return 0;
}

static int hwaccel_allocate_resources(struct hwaccel_dev *hwaccel) {
   int ret = -EINVAL;
   int addr_size;
   u32 start;
   u32 end;
 
   char hwaccel_number[4];

   // initialize members
   hwaccel->preg = NULL;
   hwaccel->en = 0;
   hwaccel->write_seq = 0;
   hwaccel->total_read_bytes = 0;
   hwaccel->total_write_bytes = 0;
   hwaccel->current_read_page = 0;
   hwaccel->current_write_page = 0;
   hwaccel->delay_cntto = 10000;

   // map hwaccel device memory
   dev_info(hwaccel->dev, "map hwaccel in reg space");
   start = (u32)hwaccel->reg_res->start;
   end = (u32)hwaccel->reg_res->end;
   addr_size = end - start + 1;
   hwaccel->preg =
      (u32*)devm_ioremap(hwaccel->dev,
                         start, addr_size);
   if (hwaccel->preg == NULL) {
      dev_err(hwaccel->dev, "Error mapping hwaccel_in base memory");
      ret = -ENOMEM;
      goto FAIL;
   }

   // allocate memory for data exchange to/from the FPGA
   dev_info(hwaccel->dev, "allocate ring_out buffer");
   hwaccel->ring_out.buf =
      dma_alloc_coherent(hwaccel->dev,
                         BUF_SIZE_BYTES,
                         &hwaccel->phys_addr_out, GFP_KERNEL);
   dev_info(hwaccel->dev, "phys_addr_out = 0x%p", (u32*)hwaccel->phys_addr_out);
   if (!hwaccel->ring_out.buf) {
      dev_err(hwaccel->dev, "Failed to allocate memory for output\r\n");
      goto FAIL;
   }
	
   dev_info(hwaccel->dev, "allocate ring_in buffer");
   hwaccel->ring_in.buf =
      dma_alloc_coherent(hwaccel->dev,
                         BUF_SIZE_BYTES,
                         &hwaccel->phys_addr_in, GFP_KERNEL);
   dev_info(hwaccel->dev, "phys_addr_in = 0x%p", (u32*)hwaccel->phys_addr_in);
   if (!hwaccel->ring_in.buf) {
      dev_err(hwaccel->dev, "Failed to allocate memory for input\r\n");
      goto UNMAP_MEM;
   }
   write_reg(hwaccel, AXIM_IN_DDR_BASE_ADDR_OFFSET, hwaccel->phys_addr_in);
   write_reg(hwaccel, AXIM_OUT_DDR_BASE_ADDR_OFFSET, hwaccel->phys_addr_out);

   // form hwaccel name
   dev_info(hwaccel->dev, "hwaccel name");
   if (hwaccel->hwaccel_number >= 0 && hwaccel->hwaccel_number < 10) 
      sprintf(hwaccel_number, "%1d", hwaccel->hwaccel_number);
   else if (hwaccel->hwaccel_number >= 10 && hwaccel->hwaccel_number < 100) 
      sprintf(hwaccel_number, "%2d", hwaccel->hwaccel_number);
   else {
      dev_err(hwaccel->dev, "hwaccel number too large (%d)",
              hwaccel->hwaccel_number);
      goto UNMAP_MEM;
   }
   hwaccel->name = devm_kmalloc(hwaccel->dev,
                                sizeof(HWACCEL_DEV_NAME) +
                                sizeof(hwaccel_number),
                                GFP_KERNEL);
   if (hwaccel->name == NULL) {
      dev_err(hwaccel->dev, "cannot allocate memory for hwaccel name");
      goto UNMAP_MEM;
   }
   strcpy(hwaccel->name, HWACCEL_DEV_NAME);
   strcat(hwaccel->name, hwaccel_number);
   dev_info(hwaccel->dev, "Hwaccel name = %s", hwaccel->name);

   // init the ctrl reg to hwaccel_rst (making everything else 0)
   write_reg(hwaccel, CTRL_OFFSET, BIT_RST);

   // axim delay cntto
   write_reg(hwaccel, DELAY_CNTTO_OFFSET, hwaccel->delay_cntto);
   reset(hwaccel, 1);

   return 0;

 UNMAP_MEM:
   // free previously mapped memory
   devm_iounmap(hwaccel->dev, hwaccel->preg);
   dma_free_coherent(hwaccel->dev, BUF_SIZE_BYTES,
                     hwaccel->ring_in.buf,
                     hwaccel->phys_addr_in);
   dma_free_coherent(hwaccel->dev, BUF_SIZE_BYTES,
                     hwaccel->ring_out.buf,
                     hwaccel->phys_addr_out);
 FAIL:
   return ret;
}

static int hwaccel_probe(struct platform_device *pdev) {
   struct hwaccel_dev *hwaccel;
   struct device *dev;
   int ret;

   // hwaccel
   hwaccel = devm_kzalloc(&pdev->dev, sizeof(*hwaccel), GFP_KERNEL);
   if (!hwaccel)
      return -ENOMEM;

   hwaccel->dev = &pdev->dev;
   hwaccel->pdev = pdev;
   dev = hwaccel->dev;

   dev_set_drvdata(&pdev->dev, hwaccel);

   dev_info(hwaccel->dev, "calling parse_of");
   ret = hwaccel_parse_of(hwaccel);
   if (ret < 0)
      return ret;

   dev_info(hwaccel->dev, "calling allocate");
   ret = hwaccel_allocate_resources(hwaccel);
   if (ret < 0)
      return ret;

   if (sysfs_create_group(&pdev->dev.kobj, &hwaccel_attr_group))
      dev_err(hwaccel->dev, "failed to create sysfs entries\n");

   hwaccel->mdev.minor = MISC_DYNAMIC_MINOR;
   hwaccel->mdev.name = hwaccel->name;
   hwaccel->mdev.fops = &hwaccel_fops;

   ret = misc_register(&hwaccel->mdev);
   if (ret) {
      dev_err(hwaccel->dev, "failed to register as misc_device\n");
      return ret;
   }

   dev_info(hwaccel->dev, "hwaccel_probe complete");
   return 0;

    misc_deregister(&hwaccel->mdev);
    return ret;
}

static void hwaccel_remove(struct platform_device *pdev) {
   struct hwaccel_dev *hwaccel = dev_get_drvdata(&pdev->dev);

   bitset(hwaccel, CTRL_OFFSET, BIT_SHUTDOWN);
   dev_info(hwaccel->dev, "asserting reset");
   reset(hwaccel, 1);

   dev_info(hwaccel->dev, "unmapping io memory");
   devm_iounmap(hwaccel->dev, hwaccel->preg);
   dma_free_coherent(hwaccel->dev, BUF_SIZE_BYTES,
                     hwaccel->ring_in.buf,
                     hwaccel->phys_addr_in);
   dma_free_coherent(hwaccel->dev, BUF_SIZE_BYTES,
                     hwaccel->ring_out.buf,
                     hwaccel->phys_addr_out);

   // free ring buf
   sysfs_remove_group(&pdev->dev.kobj, &hwaccel_attr_group);

   misc_deregister(&hwaccel->mdev);

   dev_set_drvdata(&pdev->dev, NULL);
   dev_info(hwaccel->dev, "remove complete");
}

static const struct of_device_id hwaccel_of_id_table[] = {
   { .compatible = "hwaccel-1.0" },
   {}
};

MODULE_DEVICE_TABLE(of, hwaccel_of_id_table);

static struct platform_driver hwaccel_driver = {
   .driver = {
      .name = "hwaccel",
      .of_match_table = hwaccel_of_id_table,
      .owner = THIS_MODULE,
   },
   .probe = hwaccel_probe,
   .remove = hwaccel_remove,
};

module_platform_driver(hwaccel_driver);

MODULE_AUTHOR("Fiona O'Connell");
MODULE_DESCRIPTION("hwaccel: 1.0: HWACCEL control");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("hwaccel");

