// driver for both channel_in and channel_out
// for now we are assuming a 16-bit sync word for framed data

// toggle signals (like status_clr) use a hardware mechanism.
// when the driver writes the bit high to low, the hardware
// pulses the signal
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

#define CHANNEL_DEV_NAME "channel"

// 1: initial
// 2: removed enable (enable and rst are locked (en <= not rst))
// 3: adding line test
// 4: rate scaling in channel out
// 5: changes for axim based PL DDR master system
#define DRIVER_VERSION 5

/////////// system PARAMS //////////////////
#define MAX_FRAME_SIZE_BYTES 0x0FFF
#define TIMESTAMP_SIZE_BYTES 8
#define STATUS_BYTES 2

#define BUF_SIZE_BYTES (1024*1024)
#define CHANNEL_PAGE_SIZE (4096)
#define LAST_PAGE 255

/*** internal test / bert generator ***/
// clockgen accumulator width
// hard-coded in vhdl (32 bits)
#define ACCUM 0x100000000ULL

// max system bitrate
#define MAX_BITRATE 51000000

/////////////////////////////////////
///////////// registers /////////////
/////////////////////////////////////
/*** reg0: ctrl ****************/
#define CTRL_OFFSET (0)
// common
#define BIT_RST 1
#define BIT_CHANNEL_EN (1<<1)
#define BIT_STATUS_CLR (1<<8)
// channel_in/out
#define BIT_AXIM_NRST (1<<16)
// channel out
#define BIT_SEQ_ERR_CLR_MASK (1<<17)
// channel in/out
#define BIT_SHUTDOWN_MASK (1<<18)
// pcmif
#define BIT_PCMIF_RST (1)
#define BIT_PCMIF_CHOUT_SRC_SEL (1<<1)
#define BIT_PCMIF_CHIN_SRC_SEL (1<<2)
#define PCMIF_CHIN_SEL_TEST_DATA 1
#define PCMIF_CHIN_SEL_PCMIN 0
#define PCMIF_CHIN_C_POLARITY_AUTO (1<<3)
#define PCMIF_CHIN_C_POLARITY_FIXED (1<<4)
#define PCMIF_CHIN_D_POLARITY (1<<5)
#define PCMIF_CHOUT_C_POLARITY (1<<6)
#define PCMIF_CHOUT_D_POLARITY (1<<7)
#define PCMIF_STATS_CLR (1<<8)
#define PCMIF_ALARM_CLR (1<<9)
#define PCMIF_KILL_OUTPUT (1<<10)
#define PCMIF_NTS_FILT (1<<11)

/*** reg1: status ****************/
#define STATUS_OFFSET (1)
#define PCMIF_CHIN_C_POLARITY_DETECTED (1)
// channel_in
#define PCMIN_FRAME_SYNC (1<<2)
#define PCMIN_ALARM_OVERFLOW (1<<3)
// channel_in/out
#define AXIM_WRITE_RESP_ERROR_SHIFT (8)
#define AXIM_READ_RESP_ERROR_SHIFT (10)
#define AXIM_WRITE_RESP_ERROR_MASK (0x03)
#define AXIM_READ_RESP_ERROR_MASK (0x03)
// pcmif
#define CHIN_C_POLARITY_DETECTED (1)
#define CHOUT_FRAME_SYNC (1<<1)
#define CHOUT_ALARM_UNDERFLOW (1<<2)
#define CHOUT_ALARM_OVERFLOW (1<<3)

/*** reg2: sync pattern **********/
#define SYNC_PATTERN_OFFSET (2)

/*** reg3: id word  ********/
// channel_out
#define HDR_SIZE_BYTES_OFFSET (3)
#define PCMIF_BERT_IN_BITS_LSB_OFFSET (3)
  
/*** reg4: payload_size_bytes ********/
#define PAYLOAD_SIZE_BYTES_OFFSET (4)
#define PAYLOAD_SIZE_BYTES_MASK 0x3FF
#define PCMIF_BERT_IN_BITS_MSB_OFFSET (4)

/*** reg5: PCMIF_BERT_IN_ERRS_LSB ********/
#define PCMIF_BERT_IN_ERRS_LSB_OFFSET (5)

// channel_in and out
#define FRAME_SIZE_OFFSET (5)

/*** reg6: pcmin_test_clockgen_nominal ********/
#define PCMIN_TEST_NOMINAL_OFFSET (6)
#define PCMIF_BERT_IN_ERRS_MSB_OFFSET (6)

/*** reg7: bert ********/
#define DEV_BERT_CTRL_OFFSET (7)
#define BERT_TP_SHIFT 16
#define DEV_BERT_PATTERN_SEL_SHIFT 4
#define BERT_PATTERN_SEL_MASK 0x07
#define DEV_BERT_CLR (1<<1)
// pcmif reg7
#define PCMIF_BERT_IN_STATUS_OFFSET (8)
#define BERT_SYNC (1)
#define BERT_SYNC_LOST (1<<1)
#define BERT_INV (1<<2)
#define BERT_RESYNCED (1<<3)
#define BERT_NOCLK (1<<4)

/*** reg8: pcmif_pattern ***/
#define PCMIF_PATTERN_OFFSET (8)
#define PCMIF_PATTERN_SEL_MASK (0x07)
#define PCMIF_SYNC_PATTERN_MSB_MASK (1<<4)
#define PCMIF_TP_SHIFT (16)

/*** reg9: pcmif_sync_pattern ***/
#define PCMIF_SYNC_PATTERN_OFFSET (9)

/*** reg10: sync_size, sync_pattern msb (in/out) ***/
#define SYNC_SIZE_OFFSET (10)

/*** reg11  ***/
// pcmif frame size
#define PCMIF_FRAME_SIZE_OFFSET (11)
#define PCMIF_HDR_SIZE_SHIFT (16)
// channel_in / channel_out
#define AXIM_STATS_OFFSET (11)
#define AXIM_PAGE_NUM_MASK (0x0FF)

/*** reg12 ***/
#define PCMIF_BERT_IN_CTRL_OFFSET (12)
#define BERT_IN_CLR (1)
#define BERT_IN_SRC_SEL (1<<1)
#define BERT_IN_SRC_PCMOUT (1)
#define BERT_IN_SRC_PCMIN (0)
#define BERT_IN_INIT (1<<2)
#define BERT_IN_RESYNC_RESTART (1<<3)
#define BERT_IN_LIMIT_SEL (1<<4)
#define BERT_IN_INSERT_ERR (1<<6)

/*** reg13: ***/
// pcmin_los_cnt (loss of sync cnt) 
#define PCMIN_LOS_CNT_OFFSET (13)
// pcmif
#define DCOUNT_MIN_OFFSET (13)
/*** reg14 ***/
// channel_out dev_bert_bits lsb 
#define DEV_BERT_BITS_LSB_OFFSET (14)
// pcmif
#define DCOUNT_MAX_OFFSET (14)
/*** reg15: dev_bert_bits msb  ***/
#define DEV_BERT_BITS_MSB_OFFSET (15)
/*** reg16: bert_errs lsb (channel_out) ***/
/*** pcmin rate estimate ***/
#define DEV_BERT_ERRS_LSB_OFFSET (16)
#define PCMIF_BERT_OUT_CTRL_OFFSET (16)
#define BERT_OUT_CLR (1)
#define BERT_OUT_INSERT_ERR (1<<6)
/*** reg17: bert_errs msb (channel_out) ***/
#define DEV_BERT_ERRS_MSB_OFFSET (17)
#define PCMIF_BERT_OUT_CLOCKGEN_OFFSET (17)
/*** reg18: ***/
#define PCMIF_BERT_OUT_BITS_LSB_OFFSET (18)
/** reg19 **/
// channel_out
#define PCMIF_BERT_OUT_BITS_MSB_OFFSET (19)
#define PCMOUT_LOS_CNT_OFFSET (19)
// channel_in
#define FRONTEND_OVERFLOW_CNT_OFFSET (19)
/** reg20 **/
#define PCMIN_D_CNT_OFFSET (20)
#define PCMIF_NCO_STEP_MIN_OFFSET (20)
/** reg21 **/
#define PCMIF_NCO_STEP_MAX_OFFSET (21)
/** reg22 **/
#define SEQ_ERR_CNT_OFFSET (22)
// channel out
/** reg23 **/
#define PCMOUT_BITRATE_ESTIMATE_OFFSET (23)
/** reg24 **/
#define DELAY_CNTTO_OFFSET (24)
/** reg25 **/
// channel_out
#define BITRATE_EWMA_ALPHA_OFFSET (25)
// pcmif
#define DCOUNT_EWMA_ALPHA_OFFSET (25)
/** reg26 **/
// pcmif
#define DCOUNT_MIN_SETPOINT_OFFSET (26)
/** reg27 **/
// pcmif
#define DCOUNT_MIN_RANGE_OFFSET (27)
/** reg28 **/
// pcmif
#define DCOUNT_MIN_BASE_ADJ_OFFSET (28)
/** reg29 **/
// pcmif
#define BITRATE_ADJ_OFFSET (29)
/*** reg30: version ***/
#define AXIM_DDR_BASE_ADDR_OFFSET (30)
/*** reg31: version ***/
#define VERSION_OFFSET (31)

#define BITRATE_EWMA_ALPHA_DEFAULT 0x0010
#define DCOUNT_EWMA_ALPHA_DEFAULT 0x0010

void format_date_time(char *date, char *time, char *str);

// axim to cpu, 1 byte sync, 6 byte sequence number, 0xfe, timestamp ctrl
// cpu to axim, 1 byte sync, 7 byte sequence number, 32-bit bitrate, 32-bit nco step
#define INTERNAL_HDR_SIZE_BYTES 16
#define INTERNAL_SYNC_BYTE 0xEB
#define TIMESTAMP_HDR 0xFE

struct  channel_dev {
   struct device *dev;
   struct platform_device *pdev;
   struct miscdevice mdev;

   const char *compatible;
  
   u32 __iomem *preg_in;
   u32 __iomem *preg_out;
   u32 __iomem *preg_pcmif;
  
   struct resource *channel_in_reg_res;
   struct resource *channel_out_reg_res;
   struct resource *pcmif_reg_res;
  
   u32 channel_number;
   char *name;

   u32 channel_active;
  
   // initial values
   u32 pcmin_data_src_bert_npcmin;
   u32 bert_in_src_pcmout_npcmin;
   u32 pcmout_c_polarity;
   u32 pcmout_d_polarity;
  
   // system parameters
   u32 pcm_en;

   u32 pcmout_delay_cntto;
   u32 pcmin_delay_cntto;

   // frame parameters including internal tests
   u64 sync_pattern;
   u32 sync_size;
   u32 frame_size_bytes; // total configured frame size (non-internal)
   u32 axim_frame_size_bytes; // total bytes including internal header, aligned to 128bits
   u32 payload_size_bytes;
   u32 tp;
   char pattern_sel[80];
   u32 refclk_hz;
   int running;
   u64 read_wait_cnt;
   u64 write_wait_cnt;
   u64 last_seq_num;
   u32 seq_mismatch_cnt;
   u32 seq_mismatch_expected;
   u32 seq_mismatch_received;

   // timestamps and bitrates
   u32 bert_out_bitrate;
   u64 pcmout_bitrate_timestamps;
   s32 pcmout_bitrate_step_offset;
   u64 timestamp_pkt;
   u64 timestamp_local;

   s32 bitrate_offset;

   u32 bitrate_ewma_alpha;
   u32 bitrate_ewma_one_minus_alpha;

   u32 dcount_ewma_alpha;
   u32 dcount_ewma_one_minus_alpha;

   u32 abs_min_bitrate;
   u32 abs_max_bitrate;

   u32 abs_min_dcount;
   u32 abs_max_dcount;

   u32 pl_to_driver_page_diff;
   u32 driver_to_pl_page_diff;

    int detect_ts_rate_changes;
    u32 ts_rate_change_cnt;

   // buffers
   struct circ_buf ring_in;
   dma_addr_t phys_addr_in;
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
// channel_in
static inline u32 read_channel_in_reg(struct channel_dev *channel,
                                      int offset) {
   u32 reg;
   reg = ioread32(channel->preg_in+offset);
   return reg;
}
static inline void write_channel_in_reg(struct channel_dev *channel,
                                        int offset, u32 data) {
   iowrite32(data, channel->preg_in+offset);
}
static inline int channel_in_bitget(struct channel_dev *channel,
                                    int offset, u32 bitmask) {
   u32 reg;
	
   reg = read_channel_in_reg(channel, offset);
   return (reg & bitmask) ? 1 : 0;
}
static inline void channel_in_bitset(struct channel_dev *channel,
                                     int offset, u32 bitmask) {
   u32 reg;
   u32 reg_set;

   reg = read_channel_in_reg(channel, offset);
   reg_set = reg | bitmask;
   write_channel_in_reg(channel, offset, reg_set);
}
static inline void channel_in_bitclr(struct channel_dev *channel,
                                     int offset, u32 bitmask) {
   u32 reg;
   u32 reg_clr;

   reg = read_channel_in_reg(channel, offset);
   reg_clr = reg & ~bitmask;
   write_channel_in_reg(channel, offset, reg_clr);
}
static inline void channel_in_bitwrite(struct channel_dev *channel,
                                       int offset, u32 bitmask, int value) {
   if (value)
      channel_in_bitset(channel, offset, bitmask);
   else
      channel_in_bitclr(channel, offset, bitmask);
}
static inline void channel_in_bittoggle(struct channel_dev *channel,
                                        int offset, u32 bitmask) {
   channel_in_bitset(channel, offset, bitmask);
   channel_in_bitclr(channel, offset, bitmask);
}
// channel_out
static inline u32 read_channel_out_reg(struct channel_dev *channel,
                                       int offset) {
   return ioread32(channel->preg_out+offset);
}
static inline void write_channel_out_reg(struct channel_dev *channel,
                                         int offset, u32 data) {
   iowrite32(data, channel->preg_out+offset);
}
static inline int channel_out_bitget(struct channel_dev *channel,
                                     int offset, u32 bitmask) {
   u32 reg;
	
   reg = read_channel_out_reg(channel, offset);
   return (reg & bitmask) ? 1 : 0;
}
static inline void channel_out_bitset(struct channel_dev *channel,
                                      int offset, u32 bitmask) {
   u32 reg;
   u32 reg_set;
	
   reg = read_channel_out_reg(channel, offset);
   reg_set = reg | bitmask;
   write_channel_out_reg(channel, offset, reg_set);
}
static inline void channel_out_bitclr(struct channel_dev *channel,
                                      int offset, u32 bitmask) {
   u32 reg;
   u32 reg_clr;
	
   reg = read_channel_out_reg(channel, offset);
   reg_clr = reg & ~bitmask;
   write_channel_out_reg(channel, offset, reg_clr);
}
static inline void channel_out_bitwrite(struct channel_dev *channel,
                                        int offset, u32 bitmask, int value) {
   if (value)
      channel_out_bitset(channel, offset, bitmask);
   else
      channel_out_bitclr(channel, offset, bitmask);
}
static inline void channel_out_bittoggle(struct channel_dev *channel,
                                         int offset, u32 bitmask) {
   channel_out_bitset(channel, offset, bitmask);
   channel_out_bitclr(channel, offset, bitmask);
}
// pcmif
static inline u32 read_pcmif_reg(struct channel_dev *channel,
                                     int offset) {
   return ioread32(channel->preg_pcmif+offset);
}
static inline void write_pcmif_reg(struct channel_dev *channel,
                                       int offset, u32 data) {
   iowrite32(data, channel->preg_pcmif+offset);
}
static inline int pcmif_bitget(struct channel_dev *channel,
                                   int offset, u32 bitmask) {
   u32 reg;
	
   reg = read_pcmif_reg(channel, offset);
   return (reg & bitmask) ? 1 : 0;
}
static inline void pcmif_bitset(struct channel_dev *channel,
                                    int offset, u32 bitmask) {
   u32 reg;
   u32 reg_set;
	
   reg = read_pcmif_reg(channel, offset);
   reg_set = reg | bitmask;
   write_pcmif_reg(channel, offset, reg_set);
}
static inline void pcmif_bitclr(struct channel_dev *channel,
                                    int offset, u32 bitmask) {
   u32 reg;
   u32 reg_clr;
	
   reg = read_pcmif_reg(channel, offset);
   reg_clr = reg & ~bitmask;
   write_pcmif_reg(channel, offset, reg_clr);
}
static inline void pcmif_bitwrite(struct channel_dev *channel,
                                      int offset, u32 bitmask, int value) {
   if (value)
      pcmif_bitset(channel, offset, bitmask);
   else
      pcmif_bitclr(channel, offset, bitmask);
}
static inline void pcmif_bittoggle(struct channel_dev *channel,
                                       int offset, u32 bitmask) {
   pcmif_bitset(channel, offset, bitmask);
   pcmif_bitclr(channel, offset, bitmask);
}

/////////////////////////////////////////////////////////////////////////////////////////
// misc helper functions
////////////////////////////////////////////////////////////////////////////////////////
/* check the given page to see if there is data available to read into the read buffer */
static inline int read_page_available(struct channel_dev *channel) {
   u32 page = channel->current_read_page;
   u8 *pr = (channel->ring_in.buf) + (page * 4096); // MSB of FPGA data to CPU
   if (*pr == INTERNAL_SYNC_BYTE)
      return 1;
   else
      return 0;
}
/* check to see if the page is available to write */
static inline int write_page_available(struct channel_dev *channel) {
   u32 page = channel->current_write_page;
   u8 *pr = (channel->ring_out.buf) + (page * 4096); // MSB of FPGA data to CPU
   if (*pr == 0) 
      return 1;
   else 
      return 0;
}
static inline u64 read_get_seq(struct channel_dev *channel) {
   u8 *p = channel->ring_in.buf + (CHANNEL_PAGE_SIZE * channel->current_read_page);
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

static void set_bert_out_bitrate(struct channel_dev *channel) {
   u64 req_hz = channel->bert_out_bitrate;
   u32 refclk = channel->refclk_hz;
   u64 accum = (u64)ACCUM;
   u64 step;
   u64 dividend;

   dividend = req_hz * accum;
   step = div_u64(dividend, refclk);
   write_pcmif_reg(channel, PCMIF_BERT_OUT_CLOCKGEN_OFFSET, (u32)step);
}

static void set_pattern_sel(struct channel_dev *channel) {
   u32 sel;
   u32 reg;
   if (!strncmp(channel->pattern_sel, "tp", 2) || !strncmp(channel->pattern_sel, "TP", 2)) {
      dev_info(channel->dev, "setting bert pattern in selection to tp");
      sel = 1;
   }
   else if (!strncmp(channel->pattern_sel, "pn6", 3) || !strncmp(channel->pattern_sel, "PN6", 3)) {
      dev_info(channel->dev, "setting bert pattern in selection to pn6");
      sel = 2;
   }
   else if (!strncmp(channel->pattern_sel, "pn11", 4) || !strncmp(channel->pattern_sel, "PN11", 4)) {
      dev_info(channel->dev, "setting bert pattern in selection to pn11");
      sel = 3;
   }
   else if (!strncmp(channel->pattern_sel, "pn15", 4) || !strncmp(channel->pattern_sel, "PN15", 4)) {
      dev_info(channel->dev, "setting bert pattern in selection to pn15");
      sel = 4;
   }
   else if (!strncmp(channel->pattern_sel, "pn23", 4) || !strncmp(channel->pattern_sel, "PN23", 4)) {
      dev_info(channel->dev, "setting bert pattern in selection to pn23");
      sel = 5;
   }
   else if (!strncmp(channel->pattern_sel, "pn31", 4) || !strncmp(channel->pattern_sel, "PN31", 4)) {
      //dev_info(channel->dev, "setting bert pattern in selection to pn31");
      dev_info(channel->dev, "pn31 not yet implemented");
      sel = 6;
   }
   else if (!strncmp(channel->pattern_sel, "cnt", 3) || !strncmp(channel->pattern_sel, "CNT", 3)) {
      dev_info(channel->dev, "setting bert pattern in selection to count");
      sel = 7;
   }
   else {
      dev_info(channel->dev, "invalid pattern selection %s", channel->pattern_sel);
      sel = 0;
   }
   if (sel) {
      // channel_out dev bert
      reg = read_channel_out_reg(channel, DEV_BERT_CTRL_OFFSET);
      reg &= ~(BERT_PATTERN_SEL_MASK << DEV_BERT_PATTERN_SEL_SHIFT);
      reg |= ((sel & BERT_PATTERN_SEL_MASK)<<DEV_BERT_PATTERN_SEL_SHIFT);
      write_channel_out_reg(channel, DEV_BERT_CTRL_OFFSET, reg);
      // pcmif
      reg = read_pcmif_reg(channel, PCMIF_PATTERN_OFFSET);
      reg &= ~(BERT_PATTERN_SEL_MASK);
      reg |= sel;
      write_pcmif_reg(channel, PCMIF_PATTERN_OFFSET, reg);
   }
}
static void set_tp(struct channel_dev *channel) {
   u32 tp = channel->tp;
   u32 reg;
   // dev bert in channel_out
   reg = read_channel_out_reg(channel, DEV_BERT_CTRL_OFFSET);
   tp <<= BERT_TP_SHIFT;
   reg &= ~(0xFFFF<<BERT_TP_SHIFT);
   reg |= tp;
   write_channel_out_reg(channel, DEV_BERT_CTRL_OFFSET, reg);
   // pcmif 
   tp = channel->tp;
   reg = read_pcmif_reg(channel, PCMIF_PATTERN_OFFSET);
   tp <<= PCMIF_TP_SHIFT;
   reg &= ~(0xFFFF<<PCMIF_TP_SHIFT);
   reg |= tp;
   write_pcmif_reg(channel, PCMIF_PATTERN_OFFSET, reg);
}

static void set_sync(struct channel_dev *channel) {
   int msb;
   u32 reg;
   write_channel_in_reg(channel, SYNC_PATTERN_OFFSET, channel->sync_pattern);
   write_channel_out_reg(channel, SYNC_PATTERN_OFFSET, channel->sync_pattern);
   write_pcmif_reg(channel, PCMIF_SYNC_PATTERN_OFFSET, channel->sync_pattern);
   msb = (channel->sync_pattern >> 32) & 0x01;
   reg = (msb << 6) | channel->sync_size;
   write_channel_in_reg(channel, SYNC_SIZE_OFFSET, reg);
   write_channel_out_reg(channel, SYNC_SIZE_OFFSET, reg);
   write_pcmif_reg(channel, SYNC_SIZE_OFFSET, channel->sync_size);
   if (msb)
      pcmif_bitset(channel, PCMIF_PATTERN_OFFSET, PCMIF_SYNC_PATTERN_MSB_MASK);
   else
      pcmif_bitclr(channel, PCMIF_PATTERN_OFFSET, PCMIF_SYNC_PATTERN_MSB_MASK);
   dev_info(channel->dev, "sync_pattern = 0x%llu, sync_size = %u",
            channel->sync_pattern, channel->sync_size);
}
static void set_frame_size_bytes(struct channel_dev *channel) {
   u32 hdr_size_bytes; // everything but payload
   u32 axim_frame_size_bytes; // total bytes including internal headers
   u32 axim_frame_size_lines;
   u32 reg;
   u32 frame_size_bytes = channel->frame_size_bytes;
   u32 payload_size_bytes = channel->payload_size_bytes;
   u32 hdr_size_bits, payload_size_bits;
   hdr_size_bytes = frame_size_bytes - payload_size_bytes;
   write_channel_out_reg(channel, HDR_SIZE_BYTES_OFFSET, hdr_size_bytes);
   // FRAME_SIZE_OFFSET PROVIDES number of bytes per frame not including the internal header
   //  and the number of total lines (128-bit lines), including the internal header
   //  this is used in the FPGA logic
   axim_frame_size_bytes = channel->frame_size_bytes + INTERNAL_HDR_SIZE_BYTES;
   if ((axim_frame_size_bytes & 0x0F) != 0) {
      axim_frame_size_bytes &= 0xFFFFFFF0;
      axim_frame_size_bytes += 0x010;
   }
   channel->axim_frame_size_bytes = axim_frame_size_bytes;
   axim_frame_size_lines= axim_frame_size_bytes>>4;
   reg = (axim_frame_size_lines<<16) | channel->frame_size_bytes;
   write_channel_in_reg(channel, FRAME_SIZE_OFFSET, reg);
   write_channel_out_reg(channel, FRAME_SIZE_OFFSET, reg);
   // pcmif
   payload_size_bits = payload_size_bytes<<3;
   hdr_size_bits = hdr_size_bytes<<3;
   write_pcmif_reg(channel, PCMIF_FRAME_SIZE_OFFSET, (hdr_size_bits<<PCMIF_HDR_SIZE_SHIFT) | payload_size_bits);
   dev_info(channel->dev, "frame_size_bytes = %u, axim_frame_size_bytes = %u",
            channel->frame_size_bytes, channel->axim_frame_size_bytes);
}
static void set_payload_size_bytes(struct channel_dev *channel) {
   u32 hdr_size_bytes;
   hdr_size_bytes = channel->frame_size_bytes - channel->payload_size_bytes;
   write_channel_in_reg(channel, PAYLOAD_SIZE_BYTES_OFFSET, channel->payload_size_bytes);
   write_channel_out_reg(channel, PAYLOAD_SIZE_BYTES_OFFSET, channel->payload_size_bytes);
   set_frame_size_bytes(channel);
}
static void channel_in_reset(struct channel_dev *channel, int state) {
   if (state) {
      channel_in_bitset(channel, CTRL_OFFSET, BIT_SHUTDOWN_MASK);
      msleep(5);
      channel->total_read_bytes = 0;
      channel->seq_mismatch_cnt = 0;
      channel_in_bitclr(channel, CTRL_OFFSET, BIT_CHANNEL_EN);
      channel_in_bitset(channel, CTRL_OFFSET, BIT_RST);
      channel_in_bitclr(channel, CTRL_OFFSET, BIT_AXIM_NRST);
      channel->read_wait_cnt = 0;
      channel->current_read_page = 0;
      channel->pl_to_driver_page_diff = 0;
      memset(channel->ring_in.buf, 0, BUF_SIZE_BYTES);
   }
   else {
      channel_in_bitclr(channel, CTRL_OFFSET, BIT_SHUTDOWN_MASK);
      channel_in_bitset(channel, CTRL_OFFSET, BIT_CHANNEL_EN);
      // reset is cleared on first read or write
   }
}

static void channel_out_reset(struct channel_dev *channel, int state) {
   u32 one_minus_alpha = 0x010000 - BITRATE_EWMA_ALPHA_DEFAULT;
   if (state) {
      channel_out_bitset(channel, CTRL_OFFSET, BIT_SHUTDOWN_MASK);
      msleep(5);
      channel->total_write_bytes = 0;
      channel->write_seq = 0;
      channel->pcmout_bitrate_timestamps = 0;
      channel->pcmout_bitrate_step_offset = 0;
      channel_out_bitclr(channel, CTRL_OFFSET, BIT_CHANNEL_EN);
      channel_out_bitset(channel, CTRL_OFFSET, BIT_RST);
      channel_out_bitclr(channel, CTRL_OFFSET, BIT_AXIM_NRST);
      channel->timestamp_pkt=0;
      channel->timestamp_local=0;
      channel->write_wait_cnt = 0;
      channel->abs_max_bitrate = 0;
      channel->abs_min_bitrate = 0xFFFFFFFF;
      channel->current_write_page = 0;
      channel->driver_to_pl_page_diff = 0;
      channel->ts_rate_change_cnt = 0;
      channel->detect_ts_rate_changes = 1;
      memset(channel->ring_out.buf, 0, BUF_SIZE_BYTES);
      channel->bitrate_ewma_alpha = BITRATE_EWMA_ALPHA_DEFAULT;
      channel->bitrate_ewma_one_minus_alpha = one_minus_alpha;
      write_channel_out_reg(channel, BITRATE_EWMA_ALPHA_OFFSET,
                            (one_minus_alpha<<16) | BITRATE_EWMA_ALPHA_DEFAULT);
      one_minus_alpha = 0x010000 - DCOUNT_EWMA_ALPHA_DEFAULT;
      write_pcmif_reg(channel, DCOUNT_EWMA_ALPHA_OFFSET,
                            (one_minus_alpha<<16) | DCOUNT_EWMA_ALPHA_DEFAULT);
      channel->dcount_ewma_alpha = DCOUNT_EWMA_ALPHA_DEFAULT;
      channel->dcount_ewma_one_minus_alpha = one_minus_alpha;
   }
   else {
      channel_out_bitclr(channel, CTRL_OFFSET, BIT_SHUTDOWN_MASK);
      channel_out_bitset(channel, CTRL_OFFSET, BIT_CHANNEL_EN);
      // reset is cleared on first read or write
   }
}

static inline int did_bitrate_shift(struct channel_dev *channel, u64 cur, u64 prev)
{
   const u32 percent = 1; // 1%
   const u32 h = 128 + percent;
   const u32 l = 128 - percent;
   const u64 cur_x_128 = cur * 128;
   const u64 prev_x_h = prev * h;
   const u64 prev_x_l = prev * l;

   int increase = (cur_x_128 > prev_x_h) ? 1 : 0;
   int decrease = (cur_x_128 < prev_x_l) ? 1 : 0;

   if (increase || decrease) {
      return 1;
   }
   return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// channel file i/o
/////////////////////////////////////////////////////////////////////////////////////////
static inline struct channel_dev *file_to_channel_dev(struct file *file) {
   struct miscdevice *miscdev = file->private_data;

   return container_of(miscdev, struct channel_dev, mdev);
}

static int channel_open(struct inode *inode, struct file *file)
{
   return 0;
}

static int channel_close(struct inode *inode, struct file *file)
{
   return 0;
}

// expects 64 bit padding (reserved) followed by data 
static ssize_t channel_write(struct file *file, const char __user *buf,
                             size_t len, loff_t *ppos)
{
   struct channel_dev *channel = file_to_channel_dev(file);
   int r;
   u32 current_write_page = channel->current_write_page;
   u64 seq = channel->write_seq;
   u32 frame_size_bytes = channel->frame_size_bytes;
   // rate calculations
   u32 refclk = channel->refclk_hz;
   u64 accum = (u64)ACCUM;
   u64 step = 0;
   u64 dividend;
   u64 timestamp_pkt=0;
   u64 timestamp_pkt_1 = channel->timestamp_pkt;
   u64 timestamp_local=0;
   u64 timestamp_local_1 = channel->timestamp_local;
   u64 timestamp_local_diff;
   u64 timestamp_pkt_diff;
   u32 bits_per_frame = (channel->frame_size_bytes)<<3;
   u64 bits_per_frame_scaled;
   u64 bitrate = 0;
   char timestamp_tmp[8];
   static u64 call_cnt = 0;

   volatile u8 *p = channel->ring_out.buf + (CHANNEL_PAGE_SIZE * current_write_page);
      
   if (!channel->running && channel->pcm_en) {
       channel->running = 1;
       dev_info(channel->dev, "starting pcm processing");
       channel_in_bitclr(channel, CTRL_OFFSET, BIT_RST);
       channel_in_bitset(channel, CTRL_OFFSET, BIT_AXIM_NRST);
       channel_out_bitclr(channel, CTRL_OFFSET, BIT_RST);
       channel_out_bitset(channel, CTRL_OFFSET, BIT_AXIM_NRST);
       return 0;
   }
       
   if (!write_page_available(channel)) {
      channel->write_wait_cnt++;
      return 0;
   }   
   else {
      //////////////////////////////////////////////////
      // timestamps
      //////////////////////////////////////////////////
      r = copy_from_user(&timestamp_tmp, buf, 8);

      timestamp_pkt = ((u64)timestamp_tmp[0]<<56) | ((u64)timestamp_tmp[1]<<48) |
         ((u64)timestamp_tmp[2]<<40) | ((u64)timestamp_tmp[3]<<32) |
         ((u64)timestamp_tmp[4]<<24) | ((u64)timestamp_tmp[5]<<16) |
         ((u64)timestamp_tmp[6]<<8) | (u64)timestamp_tmp[7];
      if (r)
         dev_info(channel->dev, "write: r = %d", r);

      if (timestamp_pkt_1) { // not first time
         timestamp_pkt_diff = timestamp_pkt - timestamp_pkt_1;
         timestamp_local_diff = timestamp_local - timestamp_local_1 ;
         bits_per_frame_scaled = (u64)bits_per_frame * 1000000000;
         bitrate = bits_per_frame_scaled / timestamp_pkt_diff;
         bitrate = (u64)((s64)bitrate + (s64)channel->bitrate_offset);
         dividend = bitrate * accum;
         step = div_u64(dividend, refclk);
      }
      channel->timestamp_local = timestamp_local;
      channel->timestamp_pkt = timestamp_pkt;

      //////////////////////////////////////////////////
      // bitrate change detection and adjusting
      //////////////////////////////////////////////////
      if (call_cnt > 1) {
	  if (bitrate < MAX_BITRATE && bitrate > 0 && channel->detect_ts_rate_changes) {
	      if (did_bitrate_shift(channel, bitrate, channel->pcmout_bitrate_timestamps)) {
		  channel->ts_rate_change_cnt++;
	      }
	  }
      }
      call_cnt++;
      channel->pcmout_bitrate_timestamps = bitrate;
      
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
      // bitrate / step
      *p++ = (bitrate>>24);
      *p++ = (bitrate>>16);
      *p++ = (bitrate>>8);
      *p++ = bitrate;
      *p++ = (step>>24);
      *p++ = (step>>16);
      *p++ = (step>>8);
      *p++ = step;

      // frame_size_bytes, starting after the padding
      r = copy_from_user((void *)p, (buf+8), frame_size_bytes);
      if (r)
         dev_info(channel->dev, "write: r = %d", r);
      // write sync to indicate page ready
      wmb();
      p = channel->ring_out.buf + (CHANNEL_PAGE_SIZE * current_write_page);
      *p = 0xEB;

      //////////////////////////////////////////////////
      // sysvar updates
      //////////////////////////////////////////////////
      channel->total_write_bytes += len;
      current_write_page++;
      if (current_write_page > LAST_PAGE) {
         current_write_page = 0;
      }
      channel->write_seq++;
      channel->current_write_page = current_write_page;
   }
   return len;
}

// returns a single frame as
// 0xEB
// 48 bit sequence number
// 0xFE
// 64-bit timestamp
static ssize_t channel_read(struct file *file, char __user *buf,
                            size_t len, loff_t *ppos)
{
   struct channel_dev *channel = file_to_channel_dev(file);
   int r;
   u32 axim_frame_size_bytes = channel->axim_frame_size_bytes;
   u8 current_read_page = channel->current_read_page;
   u64 seq;
   u64 seq_1 = channel->last_seq_num;
   volatile u8 *p = channel->ring_in.buf + (CHANNEL_PAGE_SIZE * current_read_page);

   if (!channel->running && channel->pcm_en) {
       channel->running = 1;
       dev_info(channel->dev, "starting pcm processing");
       channel_in_bitclr(channel, CTRL_OFFSET, BIT_RST);
       channel_in_bitset(channel, CTRL_OFFSET, BIT_AXIM_NRST);
       channel_out_bitclr(channel, CTRL_OFFSET, BIT_RST);
       channel_out_bitset(channel, CTRL_OFFSET, BIT_AXIM_NRST);
       return 0;
   }
       
   if (!read_page_available(channel)) {
      channel->read_wait_cnt++;
      return 0;
   }

   seq = read_get_seq(channel);
   if (seq != seq_1 + 1) {
      channel->seq_mismatch_cnt++;
      channel->seq_mismatch_expected = seq_1 + 1;
      channel->seq_mismatch_received = seq;
   }
   channel->last_seq_num = seq;
   r = copy_to_user(buf, (const void*)p, axim_frame_size_bytes);
   if (r)
      dev_info(channel->dev, "read: r = %d", r);
   wmb();
   // zero out this page
   *p = 0x00;

   // update stats and pointers
   channel->total_read_bytes += axim_frame_size_bytes;
   current_read_page++;
   if (current_read_page > LAST_PAGE) { // wrap
      current_read_page = 0;
   }
   channel->current_read_page = current_read_page;
   return axim_frame_size_bytes;
}

static const struct file_operations channel_fops = {
   .owner = THIS_MODULE,
   .write = channel_write,
   .read = channel_read,
   .open = channel_open,
   .release = channel_close,
   .llseek = noop_llseek,
};

/////////////////////////////////////////////////////////////////////////////////////////
// sysfs
/////////////////////////////////////////////////////////////////////////////////////////
static ssize_t pcm_en_show(struct device *dev, struct device_attribute *attr,
                             char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%s\n", channel->pcm_en ? "pcm enabled" : "pcm disabled");
}
static ssize_t pcm_en_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   int pcm_en;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/pcm_en");
      return count;
   }
   pcm_en = val ? 1 : 0;
   if (pcm_en) {
      if (channel->pcm_en) {
         dev_info(channel->dev, "already enabled");
         return count;
      }
      else {
         channel->running = 0;
         channel->pcm_en = 1;
         channel_in_reset(channel, 0);
         channel_out_reset(channel, 0);
         dev_info(channel->dev, "pcm enabled");
         return count;

      }
   }
   else {
      if (!channel->pcm_en) {
         dev_info(channel->dev, "already disabled");
         return count;
      }
      else {
         channel->running = 0;
         channel->pcm_en = 0;
         channel_in_reset(channel, 1);
         channel_out_reset(channel, 1);
         dev_info(channel->dev, "pcm disabled");
         return count;
      }
   }
   return count;
}
static DEVICE_ATTR_RW(pcm_en);

static ssize_t pcmin_frame_sync_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   if (channel_in_bitget(channel, STATUS_OFFSET, PCMIN_FRAME_SYNC))
      return sprintf(buf, "sync\n");
   else
      return sprintf(buf, "unsynced\n");
}
static DEVICE_ATTR_RO(pcmin_frame_sync);

static ssize_t axim_resp_error_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 reg;
   int channel_in_read_resp_error;
   int channel_in_write_resp_error;
   int channel_out_read_resp_error;
   int channel_out_write_resp_error;
   reg = read_channel_in_reg(channel, STATUS_OFFSET);
   channel_in_read_resp_error = (reg>>AXIM_READ_RESP_ERROR_SHIFT) & AXIM_READ_RESP_ERROR_MASK;
   channel_in_write_resp_error = (reg>>AXIM_WRITE_RESP_ERROR_SHIFT) & AXIM_WRITE_RESP_ERROR_MASK;
   reg = read_channel_out_reg(channel, STATUS_OFFSET);
   channel_out_read_resp_error = (reg>>AXIM_READ_RESP_ERROR_SHIFT) & AXIM_READ_RESP_ERROR_MASK;
   channel_out_write_resp_error = (reg>>AXIM_WRITE_RESP_ERROR_SHIFT) & AXIM_WRITE_RESP_ERROR_MASK;
   return sprintf(buf, "channel_in read, write error: channel_out read, write error\n\t%d, %d, %d, %d\n",
                  channel_in_read_resp_error, channel_in_write_resp_error,
                  channel_out_read_resp_error, channel_out_read_resp_error);
}
static DEVICE_ATTR_RO(axim_resp_error);

static ssize_t seq_mismatch_clr_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   channel->seq_mismatch_cnt = 0;
   channel_out_bittoggle(channel, CTRL_OFFSET, BIT_SEQ_ERR_CLR_MASK);
   return count;
}
static DEVICE_ATTR_WO(seq_mismatch_clr);

static ssize_t seq_mismatch_cnt_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 reg;
   reg = read_channel_out_reg(channel, SEQ_ERR_CNT_OFFSET);
   return sprintf(buf, "driver: %u\tchannel_out: %u\texpected/received: %u, %u\n",
                  channel->seq_mismatch_cnt, reg, channel->seq_mismatch_expected,
                  channel->seq_mismatch_received);
}
static	DEVICE_ATTR_RO(seq_mismatch_cnt);

static ssize_t total_write_bytes_show(struct device *dev, struct device_attribute *attr,
                                      char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u64 bytes = channel->total_write_bytes;
   channel->total_write_bytes = 0;
   return sprintf(buf, "%llu\n", bytes);
}
static	DEVICE_ATTR_RO(total_write_bytes);

static ssize_t total_read_bytes_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u64 bytes = channel->total_read_bytes;
   channel->total_read_bytes = 0;
   return sprintf(buf, "%llu\n", bytes);
}
static	DEVICE_ATTR_RO(total_read_bytes);

static ssize_t pcmout_bitrate_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "FPGA: %u Hz\ndriver (timestamps): %u\n",
                  read_channel_out_reg(channel, PCMOUT_BITRATE_ESTIMATE_OFFSET),
                  (u32)channel->pcmout_bitrate_timestamps);
}
static	DEVICE_ATTR_RO(pcmout_bitrate);

static ssize_t pcmout_delay_cntto_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%u\n", channel->pcmout_delay_cntto);
}
static ssize_t pcmout_delay_cntto_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 cntto;
   if (kstrtouint(buf, 0, &cntto)) {
      dev_info(channel->dev, "Error in sysfs/pcmout_delay_cntto");
      return count;
   }
   channel->pcmout_delay_cntto = cntto;
   write_channel_out_reg(channel, DELAY_CNTTO_OFFSET, cntto);
   return count;
}
static DEVICE_ATTR_RW(pcmout_delay_cntto);

static ssize_t pcmin_delay_cntto_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%u\n", channel->pcmin_delay_cntto);
}
static ssize_t pcmin_delay_cntto_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 cntto;
   if (kstrtouint(buf, 0, &cntto)) {
      dev_info(channel->dev, "Error in sysfs/pcmin_delay_cntto");
      return count;
   }
   channel->pcmin_delay_cntto = cntto;
   write_channel_in_reg(channel, DELAY_CNTTO_OFFSET, cntto);
   return count;
}
static DEVICE_ATTR_RW(pcmin_delay_cntto);

static ssize_t pcmin_data_src_bert_npcmin_show(struct device *dev, struct device_attribute *attr,
                                               char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%s\n",
                  channel->pcmin_data_src_bert_npcmin ? "source: BERT" : "source: PCMIN"); 
}
static ssize_t pcmin_data_src_bert_npcmin_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/pcmin_data_src_bert_npcmin");
      return count;
   }
   channel->pcmin_data_src_bert_npcmin = val ? 1 : 0;
   pcmif_bitwrite(channel, CTRL_OFFSET, BIT_PCMIF_CHIN_SRC_SEL, val);
   return count;
}
static DEVICE_ATTR_RW(pcmin_data_src_bert_npcmin);

static ssize_t bert_in_src_pcmout_npcmin_show(struct device *dev, struct device_attribute *attr,
                                               char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%s\n",
                  channel->bert_in_src_pcmout_npcmin ? "source: PCMOUT" : "source: PCMIN"); 
}
static ssize_t bert_in_src_pcmout_npcmin_store(struct device *dev, struct device_attribute *attr,
                                                const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/bert_in_src_pcmout_npcmin");
      return count;
   }
   channel->bert_in_src_pcmout_npcmin = val ? 1 : 0;
   pcmif_bitwrite(channel, PCMIF_BERT_IN_CTRL_OFFSET, BERT_IN_SRC_SEL, val);
   return count;
}
static DEVICE_ATTR_RW(bert_in_src_pcmout_npcmin);

static ssize_t pcmin_d_polarity_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   int reg;
   reg = pcmif_bitget(channel, CTRL_OFFSET, PCMIF_CHIN_D_POLARITY);
   return sprintf(buf, "%s\n", reg ? "at the FPGA pins: data is inverted" :
                  "at the FPGA pins: data is non-inverted");
}
static ssize_t pcmin_d_polarity_store(struct device *dev, struct device_attribute *attr,
                                      const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/pcmin_d_polarity");
      return count;
   }
   channel_in_bitwrite(channel, CTRL_OFFSET, PCMIF_CHIN_D_POLARITY, val);
   return count;
}
static DEVICE_ATTR_RW(pcmin_d_polarity);

static ssize_t pcmin_c_polarity_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   int polarity = pcmif_bitget(channel, STATUS_OFFSET,  PCMIF_CHIN_C_POLARITY_DETECTED);
   int polarity_auto = pcmif_bitget(channel, CTRL_OFFSET,  PCMIF_CHIN_C_POLARITY_AUTO);
   int polarity_fixed = pcmif_bitget(channel, CTRL_OFFSET,  PCMIF_CHIN_C_POLARITY_FIXED);
   if (polarity_auto)
      return sprintf(buf, "%s\n", polarity ? "at the FPGA pins: data changes on rising edge" :
                     "at the FPGA pins: data changes on falling edge");
   else // fixed polarity
      return sprintf(buf, "%s\n", polarity_fixed ? "at the FPGA pins: data changes on rising edge" :
                     "at the FPGA pins data changes on falling edge");
}
static ssize_t pcmin_c_polarity_store(struct device *dev, struct device_attribute *attr,
                                      const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/pcmin_c_polarity");
      return count;
   }
   pcmif_bitwrite(channel, CTRL_OFFSET, PCMIF_CHIN_C_POLARITY_FIXED, val);
   return count;
}
static DEVICE_ATTR_RW(pcmin_c_polarity);

static ssize_t pcmin_c_polarity_auto_show(struct device *dev, struct device_attribute *attr,
                                          char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   int polarity_auto = pcmif_bitget(channel, CTRL_OFFSET,  PCMIF_CHIN_C_POLARITY_AUTO);
   return sprintf(buf, "%s\n", polarity_auto ? "enabled" : "disabled");
}
static ssize_t pcmin_c_polarity_auto_store(struct device *dev, struct device_attribute *attr,
                                           const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/pcmin_c_polarity_auto");
      return count;
   }
   pcmif_bitwrite(channel, CTRL_OFFSET, PCMIF_CHIN_C_POLARITY_AUTO, val);
   return count;
}
static DEVICE_ATTR_RW(pcmin_c_polarity_auto);

static ssize_t pcmin_c_rate_estimate_show(struct device *dev, struct device_attribute *attr,
                                          char *buf) {
   //struct channel_dev *channel = dev_get_drvdata(dev);
   //u32 rate_hz;
   //rate_hz = read_channel_in_reg(channel, PCMIN_C_RATE_ESTIMATE_OFFSET);
   return sprintf(buf, "not yet implemented\n");
}
static	DEVICE_ATTR_RO(pcmin_c_rate_estimate);

static ssize_t pcmout_d_polarity_show(struct device *dev, struct device_attribute *attr,
                                      char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   int reg;
   reg = pcmif_bitget(channel, CTRL_OFFSET, PCMIF_CHOUT_D_POLARITY);
   return sprintf(buf, "%s\n", reg ? "at the FPGA pins: data is inverted" :
                  "at the FPGA pins: data is non-inverted");
}
static ssize_t pcmout_d_polarity_store(struct device *dev, struct device_attribute *attr,
                                       const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/pcmout_d_polarity");
      return count;
   }
   pcmif_bitwrite(channel, CTRL_OFFSET, PCMIF_CHOUT_D_POLARITY, val);
   return count;
}
static DEVICE_ATTR_RW(pcmout_d_polarity);

static ssize_t pcmout_c_polarity_show(struct device *dev, struct device_attribute *attr,
                                      char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   int reg;
   reg = pcmif_bitget(channel, CTRL_OFFSET, PCMIF_CHOUT_C_POLARITY);
   return sprintf(buf, "%s\n", reg ? "at the FPGA pins: data changes on rising edge" :
                  "at the FPGA pins: data changes on falling edge");
}
static ssize_t pcmout_c_polarity_store(struct device *dev, struct device_attribute *attr,
                                       const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/pcmout_c_polarity");
      return count;
   }
   pcmif_bitwrite(channel, CTRL_OFFSET, PCMIF_CHOUT_C_POLARITY, val);
   return count;
}
static DEVICE_ATTR_RW(pcmout_c_polarity);

static ssize_t pcmout_bit_cnt_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   //struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "not yet implemented\n");
   //%d\n", read_channel_reg(channel, PCMOUT_BIT_CNT_OFFSET));
}
static	DEVICE_ATTR_RO(pcmout_bit_cnt);

static ssize_t pcmout_bit_cnt_total_show(struct device *dev, struct device_attribute *attr,
                                         char *buf) {
   //struct channel_dev *channel = dev_get_drvdata(dev);
   u32 low=0;
   u32 high=0;
   u64 total=0;
   //low = read_channel_reg(channel, PCMOUT_BIT_CNT_TOTAL_LOW_OFFSET);
   //high = read_channel_reg(channel, PCMOUT_BIT_CNT_TOTAL_HIGH_OFFSET);
   total = (u64)high << 32;
   total |= low;
   return sprintf(buf, "%llu\n", total);
}
static	DEVICE_ATTR_RO(pcmout_bit_cnt_total);

static ssize_t pcmout_bit_cnt_clr_store(struct device *dev, struct device_attribute *attr,
                                        const char *buf, size_t count) {
   //struct channel_dev *channel = dev_get_drvdata(dev);
   //channel_out_bittoggle(channel, CTRL_OFFSET, PCMOUT_DIN_CNT_CLR);
   return count;
}
static DEVICE_ATTR_WO(pcmout_bit_cnt_clr);

static ssize_t pcmout_bit_cnt_total_clr_store(struct device *dev, struct device_attribute *attr,
                                              const char *buf, size_t count) {
   //struct channel_dev *channel = dev_get_drvdata(dev);
   //	channel_out_bittoggle(channel, CTRL_OFFSET, PCMOUT_DIN_CNT_TOTAL_CLR);
   return count;
}
static DEVICE_ATTR_WO(pcmout_bit_cnt_total_clr);

static ssize_t status_clr_store(struct device *dev, struct device_attribute *attr,
                                const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   channel_in_bittoggle(channel, CTRL_OFFSET, BIT_STATUS_CLR);
   channel_out_bittoggle(channel, CTRL_OFFSET, BIT_STATUS_CLR);
   return count;
}
static DEVICE_ATTR_WO(status_clr);

static ssize_t sync_pattern_show(struct device *dev, struct device_attribute *attr,
                                 char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "0x%llx\n", channel->sync_pattern);
}
static ssize_t sync_pattern_store(struct device *dev, struct device_attribute *attr,
                                  const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u64 pattern;
   if (kstrtou64(buf, 0, &pattern)) {
      dev_info(channel->dev, "Error in sysfs/sync_pattern");
      return count;
   }
   channel->sync_pattern = pattern;
   set_sync(channel);
   return count;
}
static DEVICE_ATTR_RW(sync_pattern);

static ssize_t sync_size_bits_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%d\n", channel->sync_size);
}
static ssize_t sync_size_bits_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 sync_size;
   if (kstrtouint(buf, 0, &sync_size)) {
      dev_info(channel->dev, "Error in sysfs/sync_size_bits");
      return count;
   }
   channel->sync_size = sync_size;
   set_sync(channel);
   return count;
}
static DEVICE_ATTR_RW(sync_size_bits);

static ssize_t frame_size_bytes_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%d\n", channel->frame_size_bytes);
}
static ssize_t frame_size_bytes_store(struct device *dev, struct device_attribute *attr,
                                      const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int num_bytes;
   if (kstrtouint(buf, 0, &num_bytes)) {
      dev_info(channel->dev, "Error in sysfs/frame_size_bytes");
      return count;
   }
   if (num_bytes > MAX_FRAME_SIZE_BYTES) {
      dev_info(channel->dev, "Error in sysfs/frame_size_bytes");
      return count;
   }
   channel->frame_size_bytes = num_bytes;
   set_frame_size_bytes(channel);
   return count;
}
static DEVICE_ATTR_RW(frame_size_bytes);

static ssize_t payload_size_bytes_show(struct device *dev, struct device_attribute *attr,
                                       char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%d\n", channel->payload_size_bytes);
}
static ssize_t payload_size_bytes_store(struct device *dev, struct device_attribute *attr,
                                        const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/payload_size_bytes");
      return count;
   }
   channel->payload_size_bytes = val;
   set_payload_size_bytes(channel);
   return count;
}
static	DEVICE_ATTR_RW(payload_size_bytes);

static ssize_t bert_tp_show(struct device *dev, struct device_attribute *attr,
                                char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "0x%x\n", channel->tp);
}
static ssize_t bert_tp_store(struct device *dev, struct device_attribute *attr,
                                 const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/bert_tp");
      return count;
   }
   channel->tp = val;
   set_tp(channel);
   return count;
}
static	DEVICE_ATTR_RW(bert_tp);

static ssize_t bert_pattern_sel_show(struct device *dev, struct device_attribute *attr,
                                         char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%s\n", channel->pattern_sel);
}
static ssize_t bert_pattern_sel_store(struct device *dev, struct device_attribute *attr,
                                          const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   int valid_sel=1;

   if (!strncmp(buf, "tp", 2) || !strncmp(buf, "TP", 2)) 
      strcpy(channel->pattern_sel, "tp\n");
   else if (!strncmp(buf, "pn6", 3) || !strncmp(buf, "PN6", 3)) 
      strcpy(channel->pattern_sel, "pn6\n");
   else if (!strncmp(buf, "pn11", 4) || !strncmp(buf, "PN11", 4)) 
      strcpy(channel->pattern_sel, "pn11\n");
   else if (!strncmp(buf, "pn15", 4) || !strncmp(buf, "PN15", 4)) 
      strcpy(channel->pattern_sel, "pn15\n");
   else if (!strncmp(buf, "pn23", 4) || !strncmp(buf, "PN23", 4)) 
      strcpy(channel->pattern_sel, "pn23\n");
   else if (!strncmp(buf, "pn31", 4) || !strncmp(buf, "PN31", 4)) 
      strcpy(channel->pattern_sel, "pn23\n");
   else if (!strncmp(buf, "cnt", 3) || !strncmp(buf, "CNT", 3)) 
      strcpy(channel->pattern_sel, "cnt\n");
   else {
      dev_info(channel->dev, "invalid pattern selection %s", buf);
      valid_sel = 0;
   }
   if (valid_sel)
      set_pattern_sel(channel);
   return count;
}
static	DEVICE_ATTR_RW(bert_pattern_sel);

static ssize_t bert_out_bits_show(struct device *dev, struct device_attribute *attr,
                                    char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u64 bert_bits;
   bert_bits = read_pcmif_reg(channel, PCMIF_BERT_OUT_BITS_LSB_OFFSET);
   bert_bits |= ((u64)read_pcmif_reg(channel, PCMIF_BERT_OUT_BITS_MSB_OFFSET))<<32;
   return sprintf(buf, "%lld\n", bert_bits);
}
static DEVICE_ATTR_RO(bert_out_bits);

static ssize_t bert_in_bits_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u64 bert_bits;
   bert_bits = read_pcmif_reg(channel, PCMIF_BERT_IN_BITS_LSB_OFFSET);
   bert_bits |= ((u64)read_pcmif_reg(channel, PCMIF_BERT_IN_BITS_MSB_OFFSET))<<32;
   return sprintf(buf, "%lld\n", bert_bits);
}
static DEVICE_ATTR_RO(bert_in_bits);

static ssize_t bert_in_errs_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u64 bert_errs;
   bert_errs = read_pcmif_reg(channel, PCMIF_BERT_IN_ERRS_LSB_OFFSET);
   bert_errs |= ((u64)read_pcmif_reg(channel, PCMIF_BERT_IN_ERRS_MSB_OFFSET))<<32;
   return sprintf(buf, "%lld\n", bert_errs);
}
static	DEVICE_ATTR_RO(bert_in_errs);

static ssize_t bert_in_sync_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 reg;
   reg = read_pcmif_reg(channel, PCMIF_BERT_IN_STATUS_OFFSET);
   return sprintf(buf, reg & BERT_SYNC ? "sync\n" : "unsync\n");
}
static	DEVICE_ATTR_RO(bert_in_sync);

static ssize_t bert_in_inv_show(struct device *dev, struct device_attribute *attr,
                                    char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 reg;
   reg = read_pcmif_reg(channel, PCMIF_BERT_IN_STATUS_OFFSET);
   return sprintf(buf, reg & BERT_INV ? "inverted\n" : "non-inverted\n");
}
static	DEVICE_ATTR_RO(bert_in_inv);

static ssize_t dev_bert_bits_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u64 bert_bits;
   bert_bits = read_channel_out_reg(channel, DEV_BERT_BITS_LSB_OFFSET);
   bert_bits |= ((u64)read_channel_out_reg(channel, DEV_BERT_BITS_MSB_OFFSET))<<32;
   return sprintf(buf, "%lld\n", bert_bits);
}
static	DEVICE_ATTR_RO(dev_bert_bits);

static ssize_t dev_bert_errs_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u64 bert_errs;
   bert_errs = read_channel_out_reg(channel, DEV_BERT_ERRS_LSB_OFFSET);
   bert_errs |= ((u64)read_channel_out_reg(channel, DEV_BERT_ERRS_MSB_OFFSET))<<32;
   return sprintf(buf, "%lld\n", bert_errs);
}
static	DEVICE_ATTR_RO(dev_bert_errs);

static ssize_t dev_bert_sync_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 reg;
   reg = read_channel_out_reg(channel, STATUS_OFFSET);
   return sprintf(buf, reg & BERT_SYNC ? "sync\n" : "unsync\n");
}
static	DEVICE_ATTR_RO(dev_bert_sync);

static ssize_t dev_bert_inv_show(struct device *dev, struct device_attribute *attr,
                                    char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 reg;
   reg = read_channel_out_reg(channel, STATUS_OFFSET);
   return sprintf(buf, reg & BERT_INV ? "inverted\n" : "non-inverted\n");
}
static	DEVICE_ATTR_RO(dev_bert_inv);

static ssize_t bert_clr_store(struct device *dev, struct device_attribute *attr,
                                  const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   channel_out_bittoggle(channel, DEV_BERT_CTRL_OFFSET, DEV_BERT_CLR);
   pcmif_bittoggle(channel, PCMIF_BERT_IN_CTRL_OFFSET, BERT_IN_CLR);
   return count;
}
static DEVICE_ATTR_WO(bert_clr);

static ssize_t bert_in_insert_err_store(struct device *dev, struct device_attribute *attr,
                                      const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   pcmif_bittoggle(channel, PCMIF_BERT_IN_CTRL_OFFSET, BERT_IN_INSERT_ERR);
   return count;
}
static DEVICE_ATTR_WO(bert_in_insert_err);

static ssize_t bert_out_insert_err_store(struct device *dev, struct device_attribute *attr,
                                       const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   channel_out_bittoggle(channel, PCMIF_BERT_OUT_CTRL_OFFSET, BERT_OUT_INSERT_ERR);
   return count;
}
static DEVICE_ATTR_WO(bert_out_insert_err);

static ssize_t bert_out_bitrate_show(struct device *dev, struct device_attribute *attr,
                                          char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%u Hz\n", channel->bert_out_bitrate);
}
static ssize_t bert_out_bitrate_store(struct device *dev, struct device_attribute *attr,
                                           const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int bitrate;
   if (kstrtouint(buf, 0, &bitrate)) {
      dev_info(channel->dev, "Error in sysfs/bert_out_bitrate");
      return count;
   }
   channel->bert_out_bitrate = bitrate;
   set_bert_out_bitrate(channel);
   return count;
}
static	DEVICE_ATTR_RW(bert_out_bitrate);

static ssize_t ip_core_version_show(struct device *dev, struct device_attribute *attr,
                                    char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "channel_in: %u, channel_out: %u\n",
                  read_channel_in_reg(channel, VERSION_OFFSET),
                  read_channel_out_reg(channel, VERSION_OFFSET));
}
static	DEVICE_ATTR_RO(ip_core_version);


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


static ssize_t pcmin_d_cnt_show(struct device *dev, struct device_attribute *attr,
                                char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 rate_hz;
   rate_hz = read_channel_in_reg(channel, PCMIN_D_CNT_OFFSET);
   return sprintf(buf, "%u\n", rate_hz);
}
static	DEVICE_ATTR_RO(pcmin_d_cnt);

static ssize_t pcmin_los_cnt_show(struct device *dev, struct device_attribute *attr,
                                  char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 los_cnt;
   los_cnt = read_channel_in_reg(channel, PCMIN_LOS_CNT_OFFSET);
   return sprintf(buf, "%u\n", los_cnt);
}
static	DEVICE_ATTR_RO(pcmin_los_cnt);

static ssize_t pcmout_los_cnt_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%d\n", read_channel_out_reg(channel, PCMOUT_LOS_CNT_OFFSET));
}
static	DEVICE_ATTR_RO(pcmout_los_cnt);

static ssize_t frontend_overflow_cnt_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%d\n", read_channel_in_reg(channel, FRONTEND_OVERFLOW_CNT_OFFSET));
}
static	DEVICE_ATTR_RO(frontend_overflow_cnt);

static ssize_t pcmout_bitrate_step_offset_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%d\n", channel->pcmout_bitrate_step_offset);
}
static ssize_t pcmout_bitrate_step_offset_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   s32 offset;
   if (kstrtos32(buf, 0, &offset)) {
      dev_info(channel->dev, "Error in sysfs/pcmout_bitrate_step_offset");
      return count;
   }
   channel->pcmout_bitrate_step_offset = offset;;
   return count;
}
static DEVICE_ATTR_RW(pcmout_bitrate_step_offset);

static ssize_t bitrate_offset_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%d\n", channel->bitrate_offset);
}
static ssize_t bitrate_offset_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 offset;
   if (kstrtoint(buf, 0, &offset)) {
      dev_info(channel->dev, "Error in sysfs/bitrate_offset");
      return count;
   }
   channel->bitrate_offset = offset;
   return count;
}
static DEVICE_ATTR_RW(bitrate_offset);

static ssize_t bitrate_ewma_alpha_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "alpha = 0x%x\n1-alpha = 0x%x\n",
                  channel->bitrate_ewma_alpha, channel->bitrate_ewma_one_minus_alpha);
}
static ssize_t bitrate_ewma_alpha_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 bitrate_ewma_alpha;
   u32 bitrate_ewma_one_minus_alpha;
   if (kstrtouint(buf, 0, &bitrate_ewma_alpha)) {
      dev_info(channel->dev, "Error in sysfs/bitrate_ewma_alpha");
      return count;
   }
   if (bitrate_ewma_alpha > 0x0FFFF)
      bitrate_ewma_alpha = 0x0FFFF;
   bitrate_ewma_one_minus_alpha = 0x010000 - bitrate_ewma_alpha;
   channel->bitrate_ewma_alpha = bitrate_ewma_alpha;
   channel->bitrate_ewma_one_minus_alpha = bitrate_ewma_one_minus_alpha;
   write_channel_out_reg(channel, BITRATE_EWMA_ALPHA_OFFSET, (bitrate_ewma_one_minus_alpha<<16) | bitrate_ewma_alpha);
   return count;
}
static DEVICE_ATTR_RW(bitrate_ewma_alpha);

static ssize_t dcount_ewma_alpha_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "alpha = 0x%x\n1-alpha = 0x%x\n",
                  channel->dcount_ewma_alpha, channel->dcount_ewma_one_minus_alpha);
}
static ssize_t dcount_ewma_alpha_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 dcount_ewma_alpha;
   u32 dcount_ewma_one_minus_alpha;
   if (kstrtouint(buf, 0, &dcount_ewma_alpha)) {
      dev_info(channel->dev, "Error in sysfs/dcount_ewma_alpha");
      return count;
   }
   if (dcount_ewma_alpha > 0x0FFFF)
      dcount_ewma_alpha = 0x0FFFF;
   dcount_ewma_one_minus_alpha = 0x010000 - dcount_ewma_alpha;
   channel->dcount_ewma_alpha = dcount_ewma_alpha;
   channel->dcount_ewma_one_minus_alpha = dcount_ewma_one_minus_alpha;
   write_channel_out_reg(channel, DCOUNT_EWMA_ALPHA_OFFSET, (dcount_ewma_one_minus_alpha<<16) | dcount_ewma_alpha);
   return count;
}
static DEVICE_ATTR_RW(dcount_ewma_alpha);

static ssize_t br_dcount_stats_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 min, max;
   u64 br_min;
   u64 br_max;
   u64 min_rate, max_rate;
   u64 refclk = (u64)channel->refclk_hz;
   u64 accum = (u64)ACCUM;
   u64 dividend;
   // dcount stats
   min = read_pcmif_reg(channel, DCOUNT_MIN_OFFSET);
   max = read_pcmif_reg(channel, DCOUNT_MAX_OFFSET);
   // bitrate stats
   br_min = (u64)read_pcmif_reg(channel, PCMIF_NCO_STEP_MIN_OFFSET);
   br_max = (u64)read_pcmif_reg(channel, PCMIF_NCO_STEP_MAX_OFFSET);
   dividend = br_min * refclk;
   min_rate = div64_u64(dividend, accum);
   dividend = br_max * refclk;
   max_rate = div64_u64(dividend, accum);
   // update absolutes
   if (channel->abs_min_dcount > min)
      channel->abs_min_dcount = min;
   if (channel->abs_max_dcount < max)
      channel->abs_max_dcount = max;
   if (channel->abs_min_bitrate > min_rate)
      channel->abs_min_bitrate = min_rate;
   if (channel->abs_max_bitrate < max_rate)
      channel->abs_max_bitrate = max_rate;
   // clear stats
   pcmif_bittoggle(channel, CTRL_OFFSET, PCMIF_STATS_CLR);
   return sprintf(buf, "%u, %u (bits), %llu, %llu (Hz)\nabs: %u, %u (bits), %u, %u (Hz) diff = %u Hz\n",
                  min, max, min_rate, max_rate, channel->abs_min_dcount, channel-> abs_max_dcount,
                  channel->abs_min_bitrate, channel->abs_max_bitrate, channel->abs_max_bitrate - channel->abs_min_bitrate);
}
static ssize_t br_dcount_stats_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   channel->abs_min_dcount = 0x0FFFFF;
   channel->abs_max_dcount = 0;
   channel->abs_min_bitrate = 0xFFFFFFFF;
   channel->abs_max_bitrate = 0;
   return count;
}
static DEVICE_ATTR_RW(br_dcount_stats);

static ssize_t chout_alarms_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 reg;
   int alarm_underflow, alarm_overflow;
   reg = read_pcmif_reg(channel, STATUS_OFFSET);
   alarm_underflow = reg & CHOUT_ALARM_UNDERFLOW ? 1:0;
   alarm_overflow = reg & CHOUT_ALARM_OVERFLOW ? 1:0;
   pcmif_bittoggle(channel, CTRL_OFFSET, PCMIF_ALARM_CLR);
   return sprintf(buf, "under/overflow = %d, %d\n", alarm_underflow, alarm_overflow);
}
static DEVICE_ATTR_RO(chout_alarms);

static ssize_t pcmout_bitrate_adj_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u64 rate;
   u32 refclk = channel->refclk_hz;
   u64 accum = (u64)ACCUM;
   u64 dividend;
   s32 bitrate_adj;
   bitrate_adj = read_pcmif_reg(channel, BITRATE_ADJ_OFFSET);
   dividend = bitrate_adj * (u64)refclk;
   rate = div64_s64(dividend, accum);
   return sprintf(buf, "%d Hz\n", (s32)rate); 
}
static	DEVICE_ATTR_RO(pcmout_bitrate_adj);

static ssize_t dcount_min_setpoint_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 reg;
   reg = read_pcmif_reg(channel, DCOUNT_MIN_SETPOINT_OFFSET);
   return sprintf(buf, "%u\n", reg);
}
static DEVICE_ATTR_RO(dcount_min_setpoint);

static ssize_t dcount_min_range_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 reg;
   reg = read_pcmif_reg(channel, DCOUNT_MIN_RANGE_OFFSET);
   return sprintf(buf, "%u\n", reg);
}
static DEVICE_ATTR_RO(dcount_min_range);

static ssize_t dcount_min_base_adj_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 reg;
   reg = read_pcmif_reg(channel, DCOUNT_MIN_BASE_ADJ_OFFSET);
   return sprintf(buf, "%u\n", reg);
}
static DEVICE_ATTR_RO(dcount_min_base_adj);

static ssize_t buf_stats_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 channel_in_page_num;
   u32 channel_out_page_num;
   s32 diff;
   channel_in_page_num = read_channel_in_reg(channel, AXIM_STATS_OFFSET);
   channel_in_page_num &= AXIM_PAGE_NUM_MASK;
   channel_out_page_num = read_channel_out_reg(channel, AXIM_STATS_OFFSET);
   channel_out_page_num &= AXIM_PAGE_NUM_MASK;
   diff = (s32)channel_in_page_num - (s32)channel->current_read_page;
   if (diff > 0 && diff > channel->pl_to_driver_page_diff)
      channel->pl_to_driver_page_diff = diff;
   diff = (s32)channel->current_read_page - (s32)channel_out_page_num;
   if (diff > 0 && diff > channel->driver_to_pl_page_diff)
      channel->driver_to_pl_page_diff = diff;
   return sprintf(buf, "pl/driver pages: %u, %u -- driver/pl pages: %u, %u -- abs diff: %u, %u\n",
                  channel_in_page_num, channel->current_read_page,
                  channel->current_write_page, channel_out_page_num,
                  channel->pl_to_driver_page_diff, channel->driver_to_pl_page_diff);
}
static ssize_t buf_stats_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   channel->pl_to_driver_page_diff = 0;
   channel->driver_to_pl_page_diff = 0;
   return count;
}
static	DEVICE_ATTR_RW(buf_stats);

static ssize_t kill_output_show(struct device *dev, struct device_attribute *attr,
                             char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   u32 reg;
   reg = read_pcmif_reg(channel, CTRL_OFFSET);
   return sprintf(buf, "%s\n", reg & PCMIF_KILL_OUTPUT ? "active" : "inactive");
}
static ssize_t kill_output_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   int kill_output;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/kill_output");
      return count;
   }
   kill_output = val ? 1 : 0;
   if (kill_output) 
      pcmif_bitset(channel, CTRL_OFFSET, PCMIF_KILL_OUTPUT);
   else 
      pcmif_bitclr(channel, CTRL_OFFSET, PCMIF_KILL_OUTPUT);
   return count;
}
static DEVICE_ATTR_RW(kill_output);

static ssize_t detect_ts_rate_changes_show(struct device *dev, struct device_attribute *attr,
                             char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   return sprintf(buf, "%s\n", channel->detect_ts_rate_changes ? "detecting" : "not detecting");
}
static ssize_t detect_ts_rate_changes_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/detect_ts_rate_changes");
      return count;
   }
   channel->detect_ts_rate_changes = val ? 1 : 0;
   return count;
}
static DEVICE_ATTR_RW(detect_ts_rate_changes);

static ssize_t nts_filt_show(struct device *dev, struct device_attribute *attr,
                             char *buf) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   int nts_filt;
   nts_filt = pcmif_bitget(channel, CTRL_OFFSET, PCMIF_NTS_FILT);
   return sprintf(buf, "%s\n", nts_filt ? "filtered" : "timestamp");
}
static ssize_t nts_filt_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count) {
   struct channel_dev *channel = dev_get_drvdata(dev);
   unsigned int val;
   int nts_filt;
   if (kstrtouint(buf, 0, &val)) {
      dev_info(channel->dev, "Error in sysfs/nts_filt");
      return count;
   }
   nts_filt = val ? 1 : 0;
   if (nts_filt) {
      channel_out_bitset(channel, CTRL_OFFSET, PCMIF_NTS_FILT);
      return count;
   }
   else {
      channel_out_bitclr(channel, CTRL_OFFSET, PCMIF_NTS_FILT);
      return count;
   }
   return count;
}
static DEVICE_ATTR_RW(nts_filt);

static struct attribute *channel_attrs[] = {
   &dev_attr_pcm_en.attr,
   &dev_attr_pcmin_frame_sync.attr,
   &dev_attr_axim_resp_error.attr,
   &dev_attr_seq_mismatch_clr.attr,
   &dev_attr_seq_mismatch_cnt.attr,
   &dev_attr_total_write_bytes.attr,
   &dev_attr_total_read_bytes.attr,
   &dev_attr_pcmout_bitrate.attr,
   &dev_attr_pcmout_delay_cntto.attr,
   &dev_attr_pcmin_delay_cntto.attr,
   &dev_attr_pcmin_data_src_bert_npcmin.attr,
   &dev_attr_bert_in_src_pcmout_npcmin.attr,
   &dev_attr_pcmin_d_polarity.attr,
   &dev_attr_pcmin_c_polarity.attr,
   &dev_attr_pcmin_c_polarity_auto.attr,
   &dev_attr_pcmin_c_rate_estimate.attr,
   &dev_attr_pcmout_d_polarity.attr,
   &dev_attr_pcmout_c_polarity.attr,
   &dev_attr_pcmout_bit_cnt.attr,
   &dev_attr_pcmout_bit_cnt_total.attr,
   &dev_attr_pcmout_bit_cnt_clr.attr,
   &dev_attr_pcmout_bit_cnt_total_clr.attr,
   &dev_attr_status_clr.attr,
   &dev_attr_sync_pattern.attr,
   &dev_attr_sync_size_bits.attr,
   &dev_attr_frame_size_bytes.attr,
   &dev_attr_payload_size_bytes.attr,
   &dev_attr_bert_tp.attr,
   &dev_attr_bert_pattern_sel.attr,
   &dev_attr_bert_out_bits.attr,
   &dev_attr_bert_in_bits.attr,
   &dev_attr_bert_in_errs.attr,
   &dev_attr_bert_in_sync.attr,
   &dev_attr_bert_in_inv.attr,
   &dev_attr_dev_bert_bits.attr,
   &dev_attr_dev_bert_errs.attr,
   &dev_attr_dev_bert_sync.attr,
   &dev_attr_dev_bert_inv.attr,
   &dev_attr_bert_clr.attr,
   &dev_attr_bert_in_insert_err.attr,
   &dev_attr_bert_out_insert_err.attr,
   &dev_attr_bert_out_bitrate.attr,
   &dev_attr_ip_core_version.attr,
   &dev_attr_build_time.attr,
   &dev_attr_pcmin_d_cnt.attr,	
   &dev_attr_pcmin_los_cnt.attr,	
   &dev_attr_pcmout_los_cnt.attr,
   &dev_attr_frontend_overflow_cnt.attr,
   &dev_attr_pcmout_bitrate_step_offset.attr,
   &dev_attr_bitrate_offset.attr,
   &dev_attr_bitrate_ewma_alpha.attr,
   &dev_attr_dcount_ewma_alpha.attr,
   &dev_attr_br_dcount_stats.attr,
   &dev_attr_chout_alarms.attr,
   &dev_attr_pcmout_bitrate_adj.attr,
   &dev_attr_dcount_min_setpoint.attr,
   &dev_attr_dcount_min_range.attr,
   &dev_attr_dcount_min_base_adj.attr,
   &dev_attr_buf_stats.attr,
   &dev_attr_kill_output.attr,
   &dev_attr_detect_ts_rate_changes.attr,
   &dev_attr_nts_filt.attr,
   NULL,
};

static struct attribute_group channel_attr_group = {
   .name = NULL, /* put in device directory */
   .attrs = channel_attrs,
};

/////////////////////////////////////////////////////////////////////////////////////////
// probe, remove, parse_of, etc
/////////////////////////////////////////////////////////////////////////////////////////
static int channel_parse_of(struct channel_dev *channel)
{
   struct device *dev = channel->dev;
   struct device_node *node = dev->of_node;
   struct resource *res;

   int ret;

   // of node
   ret = of_property_read_string(node, "compatible", &channel->compatible);
   if (ret < 0) {
      dev_err(dev, "channel not in DT");
      return ret;
   }

   // channel number
   ret = of_property_read_u32(node, "channel_number", &channel->channel_number);
   if (ret) {
      dev_err(channel->dev, "error reading channel from dts");
      return ret;
   }
   dev_info(channel->dev, "channel%d\n", channel->channel_number);

   // reg -- axi channel_in
   res = platform_get_resource_byname(channel->pdev, IORESOURCE_MEM, "channel_in");
   if (res == NULL) {
      dev_err(dev, "error getting channel_in base reg addr from dts");
      return -EINVAL;
   }
   channel->channel_in_reg_res = res;

   // reg -- axi channel_out
   res = platform_get_resource_byname(channel->pdev, IORESOURCE_MEM, "channel_out");
   if (res == NULL) {
      dev_err(dev, "error getting channel_out base reg addr from dts");
      return -EINVAL;
   }
   channel->channel_out_reg_res = res;

   // reg -- axi pcmif
   res = platform_get_resource_byname(channel->pdev, IORESOURCE_MEM, "pcmif");
   if (res == NULL) {
      dev_err(dev, "error getting pcmif base reg addr from dts");
      return -EINVAL;
   }
   channel->pcmif_reg_res = res;

   // refclk_hz
   ret = of_property_read_u32(node, "refclk_hz", &channel->refclk_hz);
   if (ret < 0) {
      dev_err(dev, "error parsing refclk_hz");
      return ret;
   }

   return 0;
}

static int channel_allocate_resources(struct channel_dev *channel) {
   int ret = -EINVAL;
   int addr_size;
   u32 start;
   u32 end;
 
   char channel_number[4];

   // initialize members
   channel->channel_active = 0;
   channel->preg_in = NULL;
   channel->preg_out = NULL;
   channel->preg_pcmif = NULL;
   channel->pcm_en = 0;
   channel->pcmout_c_polarity = 0;
   channel->pcmout_d_polarity = 0;
   channel->write_seq = 0;
   channel->total_read_bytes = 0;
   channel->total_write_bytes = 0;
   channel->sync_pattern = 0;
   channel->sync_size = 0;
   channel->current_read_page = 0;
   channel->current_write_page = 0;
   channel->pcmin_delay_cntto = 10000;
   channel->pcmout_delay_cntto = 10000;
   channel->running = 0;

   // map channel device memory
   dev_info(channel->dev, "map channel in reg space");
   start = (u32)channel->channel_in_reg_res->start;
   end = (u32)channel->channel_in_reg_res->end;
   addr_size = end - start + 1;
   channel->preg_in =
      (u32*)devm_ioremap(channel->dev,
                         start, addr_size);
   if (channel->preg_in == NULL) {
      dev_err(channel->dev, "Error mapping channel_in base memory");
      ret = -ENOMEM;
      goto FAIL;
   }
   dev_info(channel->dev, "map channel out reg space");
   start = (u32)channel->channel_out_reg_res->start;
   end = (u32)channel->channel_out_reg_res->end;
   addr_size = end - start + 1;
   channel->preg_out =
      (u32*)devm_ioremap(channel->dev,
                         start, addr_size);
   if (channel->preg_out == NULL) {
      dev_err(channel->dev, "Error mapping channel_out base memory");
      ret = -ENOMEM;
      goto FAIL;
   }
   dev_info(channel->dev, "map pcmif reg space");
   start = (u32)channel->pcmif_reg_res->start;
   end = (u32)channel->pcmif_reg_res->end;
   addr_size = end - start + 1;
   channel->preg_pcmif =
      (u32*)devm_ioremap(channel->dev,
                         start, addr_size);
   if (channel->preg_pcmif == NULL) {
      dev_err(channel->dev, "Error mapping pcmif base memory");
      ret = -ENOMEM;
      goto FAIL;
   }

   // allocate memory for data exchange to/from the FPGA
   dev_info(channel->dev, "allocate ring_out buffer");
   channel->ring_out.buf =
      dma_alloc_coherent(channel->dev,
                         BUF_SIZE_BYTES,
                         &channel->phys_addr_out, GFP_KERNEL);
   dev_info(channel->dev, "phys_addr_out = 0x%p", (u32*)channel->phys_addr_out);
   if (!channel->ring_out.buf) {
      dev_err(channel->dev, "Failed to allocate memory for output\r\n");
      goto FAIL;
   }
   write_channel_out_reg(channel, AXIM_DDR_BASE_ADDR_OFFSET, channel->phys_addr_out);
	
   dev_info(channel->dev, "allocate ring_in buffer");
   channel->ring_in.buf =
      dma_alloc_coherent(channel->dev,
                         BUF_SIZE_BYTES,
                         &channel->phys_addr_in, GFP_KERNEL);
   dev_info(channel->dev, "phys_addr_in = 0x%p", (u32*)channel->phys_addr_in);
   if (!channel->ring_in.buf) {
      dev_err(channel->dev, "Failed to allocate memory for input\r\n");
      goto UNMAP_MEM;
   }
   write_channel_in_reg(channel, AXIM_DDR_BASE_ADDR_OFFSET, channel->phys_addr_in);

   // form channel name
   dev_info(channel->dev, "channel name");
   if (channel->channel_number >= 0 && channel->channel_number < 10) 
      sprintf(channel_number, "%1d", channel->channel_number);
   else if (channel->channel_number >= 10 && channel->channel_number < 100) 
      sprintf(channel_number, "%2d", channel->channel_number);
   else {
      dev_err(channel->dev, "channel number too large (%d)",
              channel->channel_number);
      goto UNMAP_MEM;
   }
   channel->name = devm_kmalloc(channel->dev,
                                sizeof(CHANNEL_DEV_NAME) +
                                sizeof(channel_number),
                                GFP_KERNEL);
   if (channel->name == NULL) {
      dev_err(channel->dev, "cannot allocate memory for channel name");
      goto UNMAP_MEM;
   }
   strcpy(channel->name, CHANNEL_DEV_NAME);
   strcat(channel->name, channel_number);
   dev_info(channel->dev, "Channel name = %s", channel->name);

   // init the ctrl reg to channel_rst (making everything else 0)
   write_channel_in_reg(channel, CTRL_OFFSET, BIT_RST);
   write_channel_out_reg(channel, CTRL_OFFSET, BIT_RST);
   write_pcmif_reg(channel, CTRL_OFFSET, BIT_RST);

   // initialize anything that required the fpga logic to be mapped above
   pcmif_bitwrite(channel, CTRL_OFFSET, PCMIF_CHOUT_D_POLARITY, 
                        channel->pcmout_d_polarity);

   // axim delay cntto
   write_channel_in_reg(channel, DELAY_CNTTO_OFFSET, channel->pcmin_delay_cntto);
   write_channel_out_reg(channel, DELAY_CNTTO_OFFSET, channel->pcmin_delay_cntto);
   
   set_sync(channel);
   set_payload_size_bytes(channel);
   set_frame_size_bytes(channel);

   channel_in_reset(channel, 1);
   channel_out_reset(channel, 1);
   // for now, leave pcmif enabled
   pcmif_bitclr(channel, CTRL_OFFSET, BIT_RST);

   return 0;

 UNMAP_MEM:
   // free previously mapped memory
   devm_iounmap(channel->dev, channel->preg_in);
   devm_iounmap(channel->dev, channel->preg_out);
   devm_iounmap(channel->dev, channel->preg_pcmif);
   dma_free_coherent(channel->dev, BUF_SIZE_BYTES,
                     channel->ring_in.buf,
                     channel->phys_addr_in);
   dma_free_coherent(channel->dev, BUF_SIZE_BYTES,
                     channel->ring_out.buf,
                     channel->phys_addr_out);
 FAIL:
   return ret;
}

static int channel_probe(struct platform_device *pdev) {
   struct channel_dev *channel;
   struct device *dev;
   int ret;

   // channel
   channel = devm_kzalloc(&pdev->dev, sizeof(*channel), GFP_KERNEL);
   if (!channel)
      return -ENOMEM;

   channel->dev = &pdev->dev;
   channel->pdev = pdev;
   dev = channel->dev;

   dev_set_drvdata(&pdev->dev, channel);

   dev_info(channel->dev, "calling parse_of");
   ret = channel_parse_of(channel);
   if (ret < 0)
      return ret;

   dev_info(channel->dev, "calling allocate");
   ret = channel_allocate_resources(channel);
   if (ret < 0)
      return ret;

   if (sysfs_create_group(&pdev->dev.kobj, &channel_attr_group))
      dev_err(channel->dev, "failed to create sysfs entries\n");

   channel->mdev.minor = MISC_DYNAMIC_MINOR;
   channel->mdev.name = channel->name;
   channel->mdev.fops = &channel_fops;

   ret = misc_register(&channel->mdev);
   if (ret) {
      dev_err(channel->dev, "failed to register as misc_device\n");
      return ret;
   }

   dev_info(channel->dev, "channel_probe complete");
   return 0;

    misc_deregister(&channel->mdev);
    return ret;
}

static void channel_remove(struct platform_device *pdev) {
   struct channel_dev *channel = dev_get_drvdata(&pdev->dev);

   channel_in_bitset(channel, CTRL_OFFSET, BIT_SHUTDOWN_MASK);
   channel_out_bitset(channel, CTRL_OFFSET, BIT_SHUTDOWN_MASK);
   dev_info(channel->dev, "asserting reset");
   channel_in_reset(channel, 1);
   channel_out_reset(channel, 1);

   dev_info(channel->dev, "unmapping io memory");
   devm_iounmap(channel->dev, channel->preg_in);
   devm_iounmap(channel->dev, channel->preg_out);
   dma_free_coherent(channel->dev, BUF_SIZE_BYTES,
                     channel->ring_in.buf,
                     channel->phys_addr_in);
   dma_free_coherent(channel->dev, BUF_SIZE_BYTES,
                     channel->ring_out.buf,
                     channel->phys_addr_out);

   // free ring buf
   sysfs_remove_group(&pdev->dev.kobj, &channel_attr_group);

   misc_deregister(&channel->mdev);

   dev_set_drvdata(&pdev->dev, NULL);
   dev_info(channel->dev, "remove complete");
}

static const struct of_device_id channel_of_id_table[] = {
   { .compatible = "channel-1.0" },
   {}
};

MODULE_DEVICE_TABLE(of, channel_of_id_table);

static struct platform_driver channel_driver = {
   .driver = {
      .name = "channel",
      .of_match_table = channel_of_id_table,
      .owner = THIS_MODULE,
   },
   .probe = channel_probe,
   .remove = channel_remove,
};

module_platform_driver(channel_driver);

MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("channel: 1.0: CHANNEL control");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("channel");

