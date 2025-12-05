#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/math64.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/wait.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/circ_buf.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/ctype.h>

#define DMA_CENTRAL_DEV_NAME "dma_central"

// 1 init
// 2 make all dma xfers sizes mod DMA_SIZE_ALIGN
// 3 min xfer size is 1024, xfer size is multiples of 256, buffer size is 4MB
// 4 512 / 512
#define DRIVER_VERSION 4

#define DMA_CENTRAL_BUF_SIZE_BYTES (1024*1024)
#define MAX_DMA_XFER_SIZE 32768
// 64 bytes == 512
#define DMA_MIN_XFER_SIZE 512
#define DMA_ALIGN_BYTES 512
#define DMA_ALIGN (~(DMA_ALIGN_BYTES-1))
#define CHANNEL_OFFSET 0x10000
#define MAX_CHANNELS 4

/**** reg0: ctrl **************/
#define CTRL_OFFSET (0)
#define BIT_INT_RETRY_INIT3 (1<<11)
#define BIT_INT_RETRY_INIT2 (1<<10)
#define BIT_INT_RETRY_INIT1 (1<<9)
#define BIT_INT_RETRY_INIT0 (1<<8)
#define BIT_CH3_DIN_OVERFLOW (1<<7)
#define BIT_CH2_DIN_OVERFLOW (1<<6)
#define BIT_CH1_DIN_OVERFLOW (1<<5)
#define BIT_CH0_DIN_OVERFLOW (1<<4)
#define BIT_INT_CHANGE_DETECT_CLR (1<<3)
#define BIT_DMA_IN_PROGRESS (1<<2)
#define BIT_INT_DMA_CLR (1<<1)
#define BIT_RST 1

/**** reg1: status ************/
#define STATUS_OFFSET (1)
#define BIT_INTERRUPT (1)
#define BIT_INTERRUPT_EN_CHANGE (1<<1)
#define BIT_INTERRUPT_DMA (1<<2)
#define BIT_CH0_DIN_EN (1<<4)
#define BIT_CH1_DIN_EN (1<<5)
#define BIT_CH2_DIN_EN (1<<6)
#define BIT_CH3_DIN_EN (1<<7)
#define BIT_CH0_DOUT_EN (1<<8)
#define BIT_CH1_DOUT_EN (1<<9)
#define BIT_CH2_DOUT_EN (1<<10)
#define BIT_CH3_DOUT_EN (1<<11)
// unused 15..12
#define BIT_TX0_RDY (1<<16)
#define BIT_TX1_RDY (1<<17)
#define BIT_TX2_RDY (1<<18)
#define BIT_TX3_RDY (1<<19)
#define BIT_RX0_RDY (1<<20)
#define BIT_RX1_RDY (1<<21)
#define BIT_RX2_RDY (1<<22)
#define BIT_RX3_RDY (1<<23)

/**** reg3: int retry count *********/
#define INT_RETRY_CNTFROM_OFFSET (3)

/**** reg4-7: dcounts for ch0-ch3 *********/
#define PCMOUT_DMABUF_BASE_OFFSET (4)
#define PCMOUT_DMABUF_WREN_MASK (1<<31)

/**** reg8: stats *****/
#define STATS_CTRL_OFFSET (8)
#define BIT_STATS_CLR (1<<1)
#define BIT_TRIG2 (1<<1)
#define BIT_TRIG1 (1<<2)
#define BIT_TRIG0 (1<<3)

/**** reg16-19: rx_tx_dcount ************/
#define RXTX_DCOUNT_BASE_OFFSET (16)

/**** reg20-23: max/min rx_tx_dcount ************/
#define MAXMIN_RXTX_DCOUNT_BASE_OFFSET (20)

/**** reg24: rx/tx en change counts ************/
#define RX_TX_EN_CHANGE_CNTS_OFFSET (24)
#define CH3_IN_EN_CNT_MASK 0xF0000000
#define CH3_IN_EN_CNT_SHIFT 28
#define CH3_OUT_EN_CNT_MASK 0x0F000000
#define CH3_OUT_EN_CNT_SHIFT 24
#define CH2_IN_EN_CNT_MASK 0x0F00000
#define CH2_IN_EN_CNT_SHIFT 20
#define CH2_OUT_EN_CNT_MASK 0x0F0000
#define CH2_OUT_EN_CNT_SHIFT 16
#define CH1_IN_EN_CNT_MASK 0x0F000
#define CH1_IN_EN_CNT_SHIFT 12
#define CH1_OUT_EN_CNT_MASK 0x0F00
#define CH1_OUT_EN_CNT_SHIFT 8
#define CH0_IN_EN_CNT_MASK 0xF0
#define CH0_IN_EN_CNT_SHIFT 4
#define CH0_OUT_EN_CNT_MASK 0x0F
#define CH0_OUT_EN_CNT_SHIFT 0

/**** reg31: ip core version ************/
#define VERSION_OFFSET (31)

u32 dma_central_buf_size_bytes = DMA_CENTRAL_BUF_SIZE_BYTES;

int dma_central_ch0_ready = 0;
int dma_central_ch1_ready = 0;
int dma_central_ch2_ready = 0;
int dma_central_ch3_ready = 0;
int dma_central_ch4_ready = 0;
int dma_central_ch5_ready = 0;
int dma_central_ch6_ready = 0;
int dma_central_ch7_ready = 0;
int dma_central_ch8_ready = 0;
int dma_central_ch9_ready = 0;
int dma_central_ch10_ready = 0;
int dma_central_ch11_ready = 0;

u32 ch0_read_ready = 0;
u32 ch1_read_ready = 0;
u32 ch2_read_ready = 0;
u32 ch3_read_ready = 0;
u32 ch4_read_ready = 0;
u32 ch5_read_ready = 0;
u32 ch6_read_ready = 0;
u32 ch7_read_ready = 0;
u32 ch8_read_ready = 0;
u32 ch9_read_ready = 0;
u32 ch10_read_ready = 0;
u32 ch11_read_ready = 0;

struct circ_buf *dma_central_ch0_buf_in = NULL;
struct circ_buf *dma_central_ch1_buf_in = NULL;
struct circ_buf *dma_central_ch2_buf_in = NULL;
struct circ_buf *dma_central_ch3_buf_in = NULL;
struct circ_buf *dma_central_ch4_buf_in = NULL;
struct circ_buf *dma_central_ch5_buf_in = NULL;
struct circ_buf *dma_central_ch6_buf_in = NULL;
struct circ_buf *dma_central_ch7_buf_in = NULL;
struct circ_buf *dma_central_ch8_buf_in = NULL;
struct circ_buf *dma_central_ch9_buf_in = NULL;
struct circ_buf *dma_central_ch10_buf_in = NULL;
struct circ_buf *dma_central_ch11_buf_in = NULL;

struct circ_buf *dma_central_ch0_buf_out = NULL;
struct circ_buf *dma_central_ch1_buf_out = NULL;
struct circ_buf *dma_central_ch2_buf_out = NULL;
struct circ_buf *dma_central_ch3_buf_out = NULL;
struct circ_buf *dma_central_ch4_buf_out = NULL;
struct circ_buf *dma_central_ch5_buf_out = NULL;
struct circ_buf *dma_central_ch6_buf_out = NULL;
struct circ_buf *dma_central_ch7_buf_out = NULL;
struct circ_buf *dma_central_ch8_buf_out = NULL;
struct circ_buf *dma_central_ch9_buf_out = NULL;
struct circ_buf *dma_central_ch10_buf_out = NULL;
struct circ_buf *dma_central_ch11_buf_out = NULL;

DECLARE_WAIT_QUEUE_HEAD(ch0_waitq);
DECLARE_WAIT_QUEUE_HEAD(ch1_waitq);
DECLARE_WAIT_QUEUE_HEAD(ch2_waitq);
DECLARE_WAIT_QUEUE_HEAD(ch3_waitq);
DECLARE_WAIT_QUEUE_HEAD(ch4_waitq);
DECLARE_WAIT_QUEUE_HEAD(ch5_waitq);
DECLARE_WAIT_QUEUE_HEAD(ch6_waitq);
DECLARE_WAIT_QUEUE_HEAD(ch7_waitq);
DECLARE_WAIT_QUEUE_HEAD(ch8_waitq);
DECLARE_WAIT_QUEUE_HEAD(ch9_waitq);
DECLARE_WAIT_QUEUE_HEAD(ch10_waitq);
DECLARE_WAIT_QUEUE_HEAD(ch11_waitq);

void format_date_time(char *date, char *time, char *str);

struct buf {
    struct list_head list;
    struct circ_buf ring;
    dma_addr_t dma_handle;
};

struct statistics {
    u32 dmain_xfer_max[MAX_CHANNELS];
    u32 dmain_xfer_min[MAX_CHANNELS];
    u32 dmaout_xfer_max[MAX_CHANNELS];
    u32 dmaout_xfer_min[MAX_CHANNELS];
    u32 irq_cnt;
    u32 dma_cnt;
    u64 total_bytes_in[MAX_CHANNELS];
    u64 total_bytes_out[MAX_CHANNELS];
};

struct  dma_central_dev {
    struct device *dev;
    struct platform_device *pdev;

    const char *compatible;

    u32 __iomem *preg;
    u64 __iomem *paxi;
    int paxi_buffer_size;
    dma_addr_t axi_phys_addr;

    struct resource *dma_central_reg_res;
    struct resource *dma_central_axi_res;

    u32 start_channel;
    u32 num_channels;
    u32 rx_dcount[MAX_CHANNELS];
    u32 tx_dcount[MAX_CHANNELS];
    u32 rx_en_change_cnt[MAX_CHANNELS];
    u32 tx_en_change_cnt[MAX_CHANNELS];
    
    // interrupt
    u32 irqnum;
    u32 int_retry_cnt;

    // dma
    struct dma_chan *dma;
    struct dma_slave_config dma_config;
    struct completion dma_ok;
    dma_addr_t dma_handle;
    dma_cookie_t                    cookie;
    int active;

    // test/debug modes
    u32 dma_out_overflow_cnt;

    // stats
    struct statistics stats;
  
    // dma buffers / dma_central shared vars
    struct list_head buf_in_list;
    struct list_head buf_out_list;
};

/////////////////////////////////////////////////////////////////////////////////////////
// register and bit accessors
/////////////////////////////////////////////////////////////////////////////////////////
static inline u32 read_dma_central_reg(struct dma_central_dev *dma_central,
                                       int offset) {
    return ioread32(dma_central->preg+offset);
}
static inline void write_dma_central_reg(struct dma_central_dev *dma_central,
                                         int offset, u32 data) {
    iowrite32(data, dma_central->preg+offset);
}
static inline int dma_central_bitget(struct dma_central_dev *dma_central,
                                     int offset, u32 bitmask) {
    return (ioread32(dma_central->preg+offset) & bitmask) ? 1 : 0;
}
static inline void dma_central_bitset(struct dma_central_dev *dma_central,
                                      int offset, u32 bitmask) {
    iowrite32(ioread32(dma_central->preg+offset) | bitmask, dma_central->preg+offset);
}
static inline void dma_central_bitclr(struct dma_central_dev *dma_central,
                                      int offset, u32 bitmask) {
    iowrite32(ioread32(dma_central->preg+offset) & (~bitmask), dma_central->preg+offset);
}
static inline void dma_central_bitwrite(struct dma_central_dev *dma_central,
                                        int offset, u32 bitmask, int value) {
    if (value)
        iowrite32(ioread32(dma_central->preg+offset) | bitmask, dma_central->preg+offset);
    else
        iowrite32(ioread32(dma_central->preg+offset) & (~bitmask), dma_central->preg+offset);
}
static inline void dma_central_bittoggle(struct dma_central_dev *dma_central,
                                         int offset, u32 bitmask) {
    iowrite32(ioread32(dma_central->preg+offset) & (~bitmask), dma_central->preg+offset);
    iowrite32(ioread32(dma_central->preg+offset) | bitmask, dma_central->preg+offset);
    iowrite32(ioread32(dma_central->preg+offset) & (~bitmask), dma_central->preg+offset);
}

/////////////////////////////////////////////////////////////////////////////////////////
// dma and isr -- for test the callback will copy the received data into the
//    corresponding tx buffer and dma the data back to the fpga fabric for test
//    using the internal bert
/////////////////////////////////////////////////////////////////////////////////////////
static void dma_callback(void *completion) {
    struct dma_central_dev *dma_central =
        container_of(completion, struct dma_central_dev, dma_ok);
    struct dma_tx_state state;
    enum dma_status     status;

    struct buf *b_in;
    struct buf *b_out;
    unsigned long     head_in;
    unsigned long     tail_out;
    int bufnum;
    unsigned long buf_size = DMA_CENTRAL_BUF_SIZE_BYTES;
    u32 dcount;

    dma_central->stats.dma_cnt++;
    status = dmaengine_tx_status(dma_central->dma, dma_central->cookie, &state);
    if (status != DMA_COMPLETE) {
        // wait for all xfers to complete
        return;
    }
    //dma_central_bitset(dma_central, CTRL_OFFSET, TRIG1);
    dma_central_bitclr(dma_central, CTRL_OFFSET, BIT_DMA_IN_PROGRESS);

    // move pcmin head and pcmout_tail
    bufnum = 0;
    list_for_each_entry(b_in, &dma_central->buf_in_list, list) {
        dcount = dma_central->rx_dcount[bufnum];
        if (dcount) {
            dma_central->stats.total_bytes_in[bufnum] += dcount;
            head_in = b_in->ring.head;
            head_in = (head_in + dcount) & (buf_size - 1);
            smp_store_release(&b_in->ring.head, head_in);
            // wake up the channel
            switch (bufnum+dma_central->start_channel) {
            case 0: ch0_read_ready = 1; wake_up_interruptible_all(&ch0_waitq); break;
            case 1: ch1_read_ready = 1; wake_up_interruptible_all(&ch1_waitq); break;
            case 2: ch2_read_ready = 1; wake_up_interruptible_all(&ch2_waitq); break;
            case 3: ch3_read_ready = 1; wake_up_interruptible_all(&ch3_waitq); break;
            case 4: ch4_read_ready = 1; wake_up_interruptible_all(&ch4_waitq); break;
            case 5: ch5_read_ready = 1; wake_up_interruptible_all(&ch5_waitq); break;
            case 6: ch6_read_ready = 1; wake_up_interruptible_all(&ch6_waitq); break;
            case 7: ch7_read_ready = 1; wake_up_interruptible_all(&ch7_waitq); break;
            case 8: ch8_read_ready = 1; wake_up_interruptible_all(&ch8_waitq); break;
            case 9: ch9_read_ready = 1; wake_up_interruptible_all(&ch9_waitq); break;
            case 10: ch10_read_ready = 1; wake_up_interruptible_all(&ch10_waitq); break;
            case 11: ch11_read_ready = 1; wake_up_interruptible_all(&ch11_waitq); break;
            }
        }
        bufnum++;
    }
    bufnum = 0;
    list_for_each_entry(b_out, &dma_central->buf_out_list, list) {
        dcount = dma_central->tx_dcount[bufnum];
        if (dcount) {
            dma_central->stats.total_bytes_out[bufnum] += dcount;
            tail_out = b_out->ring.tail;
            tail_out = (tail_out + dcount) & (buf_size - 1);
            smp_store_release(&b_out->ring.tail, tail_out);
        }
        bufnum++;
    }
    complete(completion);
    //dma_central_bitclr(dma_central, CTRL_OFFSET, TRIG1);
    dma_central_bitclr(dma_central, CTRL_OFFSET, BIT_INT_DMA_CLR);
}

static irqreturn_t dma_central_isr(int irq, void *dev_id) {
    struct dma_central_dev *dma_central = (struct dma_central_dev*)dev_id;
    u32 status;

    enum dma_ctrl_flags dma_flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
    struct dma_async_tx_descriptor *dma_desc = NULL;
    dma_addr_t src=0;
    dma_addr_t dest=0;
    u32 space;
    u32 count;
    unsigned long buf_size = DMA_CENTRAL_BUF_SIZE_BYTES;
    int bufnum;
    u32 reg;
    u32 num_dma=0;
    struct buf *b;
    u32 offset=CHANNEL_OFFSET;
    u32 dcount;
    unsigned long head;
    unsigned long tail;
    u32 max_xfer_size_bytes = MAX_DMA_XFER_SIZE;
    u32 dma_profile = 0;
    u32 rx_enables[MAX_CHANNELS];
    u32 tx_enables[MAX_CHANNELS];
    int tx_rdy[MAX_CHANNELS] = {0,0,0,0};
    int rx0_rdy, rx1_rdy, rx2_rdy, rx3_rdy;
    u32 rx_en_change_cnt[MAX_CHANNELS];
    u32 tx_en_change_cnt[MAX_CHANNELS];
    int tx_en_change[MAX_CHANNELS] = {0,0,0,0};
    int rx_en_change[MAX_CHANNELS] = {0,0,0,0};
    int idx;
    
    status = ioread32(dma_central->preg+STATUS_OFFSET);
    if (!(status & BIT_INTERRUPT)) {
        dev_info(dma_central->dev, "IRQ_NONE");
        return IRQ_NONE;
    }

    if (status & BIT_INTERRUPT_DMA) {
        iowrite32(ioread32(dma_central->preg+CTRL_OFFSET) | BIT_INT_DMA_CLR, dma_central->preg+CTRL_OFFSET);
        //dma_central_bitset(dma_central, CTRL_OFFSET, BIT_INT_DMA_CLR);
    }
    dma_central->stats.irq_cnt++;

    /* handle changes in pcmin/out enable status */
    reg = ioread32(dma_central->preg+RX_TX_EN_CHANGE_CNTS_OFFSET);
    rx_en_change_cnt[3] = (reg & CH3_IN_EN_CNT_MASK)>>CH3_IN_EN_CNT_SHIFT;
    tx_en_change_cnt[3] = (reg & CH3_OUT_EN_CNT_MASK)>>CH3_OUT_EN_CNT_SHIFT;
    rx_en_change_cnt[2] = (reg & CH2_IN_EN_CNT_MASK)>>CH2_IN_EN_CNT_SHIFT;
    tx_en_change_cnt[2] = (reg & CH2_OUT_EN_CNT_MASK)>>CH2_OUT_EN_CNT_SHIFT;
    rx_en_change_cnt[1] = (reg & CH1_IN_EN_CNT_MASK)>>CH1_IN_EN_CNT_SHIFT;
    tx_en_change_cnt[1] = (reg & CH1_OUT_EN_CNT_MASK)>>CH1_OUT_EN_CNT_SHIFT;
    rx_en_change_cnt[0] = (reg & CH0_IN_EN_CNT_MASK)>>CH0_IN_EN_CNT_SHIFT;
    tx_en_change_cnt[0] = (reg & CH0_OUT_EN_CNT_MASK)>>CH0_OUT_EN_CNT_SHIFT;

    // detect any change in an rx (pcmin) enable status
    for (idx=0; idx<MAX_CHANNELS; idx++) {
        if (dma_central->rx_en_change_cnt[idx] != rx_en_change_cnt[idx]) {
            rx_en_change[idx] = 1;
            dma_central->rx_en_change_cnt[idx] = rx_en_change_cnt[idx];
        }
    }
    // detect any change in an tx (pcmout) enable status
    for (idx=0; idx<MAX_CHANNELS; idx++) {
        if (dma_central->tx_en_change_cnt[idx] != tx_en_change_cnt[idx]) {
            tx_en_change[idx] = 1;
            dma_central->tx_en_change_cnt[idx] = tx_en_change_cnt[idx];
        }
    }

    if (rx_en_change[0] || rx_en_change[1] || rx_en_change[2] || rx_en_change[3]) {
        bufnum=0;
        list_for_each_entry(b, &dma_central->buf_in_list, list) {
            if (rx_en_change[bufnum]) {
                smp_store_release(&b->ring.head, 0);
                smp_store_release(&b->ring.tail, 0);
            }
            bufnum++;
        }
    }
    if (tx_en_change[0] || tx_en_change[1] || tx_en_change[2] || tx_en_change[3]) {
        bufnum=0;
        list_for_each_entry(b, &dma_central->buf_out_list, list) {
            if (tx_en_change[bufnum]) {
                smp_store_release(&b->ring.head, 0);
                smp_store_release(&b->ring.tail, 0);
            }
            bufnum++;
        }
    }

    // get all dcounts
    for (bufnum=0; bufnum < dma_central->num_channels; bufnum++) {
        reg = read_dma_central_reg(dma_central, (RXTX_DCOUNT_BASE_OFFSET + bufnum));
        dma_central->rx_dcount[bufnum] = (reg>>16);
        dma_central->tx_dcount[bufnum] = reg & 0x0FFFF;
        if (dma_central->rx_dcount[bufnum] > MAX_DMA_XFER_SIZE)
            dma_central->rx_dcount[bufnum] = MAX_DMA_XFER_SIZE;
    }
    // get current enable status
    rx_enables[0] = status & BIT_CH0_DIN_EN ? 1:0;
    rx_enables[1] = status & BIT_CH1_DIN_EN ? 1:0;
    rx_enables[2] = status & BIT_CH2_DIN_EN ? 1:0;
    rx_enables[3] = status & BIT_CH3_DIN_EN ? 1:0;
    tx_enables[0] = status & BIT_CH0_DOUT_EN ? 1:0;
    tx_enables[1] = status & BIT_CH1_DOUT_EN ? 1:0;
    tx_enables[2] = status & BIT_CH2_DOUT_EN ? 1:0;
    tx_enables[3] = status & BIT_CH3_DOUT_EN ? 1:0;

    /*********************** DMA : data in channels *******************************************/
    bufnum=0;
    list_for_each_entry(b, &dma_central->buf_in_list, list) {
        if (rx_enables[bufnum]) {
            dcount = dma_central->rx_dcount[bufnum];
            // if we don't have enough space, consider it a buffer overrun 
            head = b->ring.head;
            tail = b->ring.tail;
            space = CIRC_SPACE(head, tail, buf_size);
            if (space < DMA_MIN_XFER_SIZE) {
                //dev_info(dma_central->dev, "buffer overrun / pcmin, channel %d", bufnum);
                b->ring.head = 0;
                b->ring.tail = 0;
                // channel in will edge detect this signal
                switch (bufnum) {
                case 0:
                    dma_central_bittoggle(dma_central, CTRL_OFFSET, BIT_CH0_DIN_OVERFLOW);
                    break;
                case 1:
                    dma_central_bittoggle(dma_central, CTRL_OFFSET, BIT_CH1_DIN_OVERFLOW);
                    break;
                case 2:
                    dma_central_bittoggle(dma_central, CTRL_OFFSET, BIT_CH2_DIN_OVERFLOW);
                    break;
                case 3:
                    dma_central_bittoggle(dma_central, CTRL_OFFSET, BIT_CH3_DIN_OVERFLOW);
                    break;
                }
                // no transfer scheduled
                dma_central->rx_dcount[bufnum] = 0;
            }
            else if (dcount >= DMA_MIN_XFER_SIZE) {
                space = CIRC_SPACE_TO_END(head, tail, buf_size);
                dcount = min(dcount,space);
                // all xfer sizes aligned with DMA_ALIGN
                dcount &= DMA_ALIGN;
                dest = b->dma_handle + head;
                src =  (dma_addr_t)dma_central->axi_phys_addr + (offset * bufnum);
                dma_desc = dmaengine_prep_dma_memcpy(dma_central->dma,
                                                     dest, src,
                                                     dcount,
                                                     dma_flags);
                // store actual xfer count for callback
                dma_central->rx_dcount[bufnum] = dcount;
                if (dma_desc == NULL) {
                    dev_err(dma_central->dev, "isr: Error in dmaengine_prep_dma_memcpy");
                    dma_central->rx_dcount[bufnum] = 0;
                }
                else {
                    dma_desc->callback = dma_callback;
                    dma_desc->callback_param = &dma_central->dma_ok;
                    dma_central->cookie = dmaengine_submit(dma_desc);
                    if (dma_submit_error(dma_central->cookie)) {
                        dev_err(dma_central->dev, "Failed to submit DMA transfer");
                    }
                    else {
                        num_dma++;
                        dma_profile |= (1<<bufnum);
                    }
                    // stats
                    if (dcount > dma_central->stats.dmain_xfer_max[bufnum])
                        dma_central->stats.dmain_xfer_max[bufnum] = dcount;
                    if (dma_central->stats.dmain_xfer_min[bufnum] == 0)
                        dma_central->stats.dmain_xfer_min[bufnum] = dcount;
                    else if (dcount < dma_central->stats.dmain_xfer_min[bufnum])
                        dma_central->stats.dmain_xfer_min[bufnum] = dcount;
                    // end stats
                    tx_rdy[bufnum] = 1;
                }
            }
            else {
                dma_central->rx_dcount[bufnum] = 0;
            }        
        }
        else {
            dma_central->rx_dcount[bufnum] = 0;
        }        
        bufnum++;
    }
    /*********************** DMA : data out channels *******************************************/
    bufnum=0;
    list_for_each_entry(b, &dma_central->buf_out_list, list) {
        if (tx_enables[bufnum]) {
            tail = b->ring.tail;
            head = b->ring.head;
            count = CIRC_CNT(head, tail, buf_size);
            dcount = dma_central->tx_dcount[bufnum];
            // for dma we only care about cnt_to_end
            count = CIRC_CNT_TO_END(head, tail, buf_size);
            if (dcount >= DMA_MIN_XFER_SIZE && count >= DMA_MIN_XFER_SIZE) {
                dcount = min(dcount, max_xfer_size_bytes);
                dcount = min(dcount, count);
                dcount &= DMA_ALIGN;
                src = b->dma_handle + tail;
                dest =  (dma_addr_t)dma_central->axi_phys_addr + (offset * bufnum);
                dma_desc = dmaengine_prep_dma_memcpy(dma_central->dma,
                                                     dest, src,
                                                     dcount,
                                                     dma_flags);
                // store actual xfer count for callback
                dma_central->tx_dcount[bufnum] = dcount;
                if (dma_desc == NULL) {
                    dma_central->tx_dcount[bufnum] = 0;
                    dev_err(dma_central->dev, "isr: Error in dmaengine_prep_dma_memcpy");
                }
                else {
                    dma_desc->callback = dma_callback;
                    dma_desc->callback_param = &dma_central->dma_ok;
                    dma_central->cookie = dmaengine_submit(dma_desc);
                    if (dma_submit_error(dma_central->cookie)) {
                        dev_err(dma_central->dev, "Failed to submit DMA transfer");
                    }
                    else {
                        // write out count. two writes ensure rising edge on wren
                        count = CIRC_CNT(head, tail, buf_size);
                        write_dma_central_reg(dma_central, PCMOUT_DMABUF_BASE_OFFSET+bufnum,
                                              count);
                        write_dma_central_reg(dma_central, PCMOUT_DMABUF_BASE_OFFSET+bufnum,
                                              count | PCMOUT_DMABUF_WREN_MASK);
                        num_dma++;
                        dma_profile |= (1<<(bufnum+4));
                    }
                    // stats
                    if (dcount > dma_central->stats.dmaout_xfer_max[bufnum])
                        dma_central->stats.dmaout_xfer_max[bufnum] = dcount;
                    if (dma_central->stats.dmaout_xfer_min[bufnum] == 0)
                        dma_central->stats.dmaout_xfer_min[bufnum] = dcount;
                    else if (dcount < dma_central->stats.dmaout_xfer_min[bufnum])
                        dma_central->stats.dmaout_xfer_min[bufnum] = dcount;
                    // end stats
                }
            }
            else {
                dma_central->tx_dcount[bufnum] = 0;
                /*
                // if the driver output buffer is too full, clear it (TODO: set an alarm)
                space = CIRC_SPACE(head, tail, buf_size);
                if (space < DMA_MIN_XFER_SIZE) {
                    dma_central->dma_out_overflow_cnt++;
                    smp_store_release(&b->ring.head, 0);
                    smp_store_release(&b->ring.tail, 0);
                }
                */
            }
        }
        else {
            dma_central->tx_dcount[bufnum] = 0;
        }        
        bufnum++;
    }
    
    // int retry the interrupting channel if nothing is actually ready
    // rx ready is based on dma_central saying there is data available
    rx0_rdy = status & BIT_RX0_RDY;
    rx1_rdy = status & BIT_RX1_RDY;
    rx2_rdy = status & BIT_RX2_RDY;
    rx3_rdy = status & BIT_RX3_RDY;

    if (num_dma == 0) {
        if (!(rx0_rdy | tx_rdy[0])) {
            dma_central_bittoggle(dma_central, CTRL_OFFSET, BIT_INT_RETRY_INIT0);
        }
        if (!(rx1_rdy | tx_rdy[1])) {
            dma_central_bittoggle(dma_central, CTRL_OFFSET, BIT_INT_RETRY_INIT1);
        }
        if (!(rx2_rdy | tx_rdy[2])) {
            dma_central_bittoggle(dma_central, CTRL_OFFSET, BIT_INT_RETRY_INIT2);
        }
        if (!(rx3_rdy | tx_rdy[3])) {
            dma_central_bittoggle(dma_central, CTRL_OFFSET, BIT_INT_RETRY_INIT3);
        }
        iowrite32(ioread32(dma_central->preg+CTRL_OFFSET) & (~BIT_INT_DMA_CLR), dma_central->preg+CTRL_OFFSET);
        //dma_central_bitclr(dma_central, CTRL_OFFSET, BIT_INT_DMA_CLR);
    }
    // issue pending
    else {
        dma_async_issue_pending(dma_central->dma);
        dma_central_bitset(dma_central, CTRL_OFFSET, BIT_DMA_IN_PROGRESS);
    }
    //dma_central_bitclr(dma_central, CTRL_OFFSET, TRIG0);
    return IRQ_HANDLED;
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

static void dma_central_start(struct dma_central_dev *dma_central) {
    struct dma_slave_config *dma_config = &dma_central->dma_config;
    struct buf *b;

    dma_central->active = 1;
    dma_central_bitclr(dma_central, CTRL_OFFSET, BIT_RST);

    list_for_each_entry(b, &dma_central->buf_in_list, list) {
        b->ring.head = 0;
        b->ring.tail = 0;
    }
    list_for_each_entry(b, &dma_central->buf_out_list, list) {
        b->ring.head = 0;
        b->ring.tail = 0;
    }
    // allocate channel
    dma_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_16_BYTES;
    dma_central->dma = dma_request_chan(dma_central->dev, "dma");
    dma_config->direction = DMA_MEM_TO_MEM;
    if (IS_ERR(dma_central->dma)) 
        dev_err(dma_central->dev, "Failed to alloc dma chan\n");
    dmaengine_slave_config(dma_central->dma, dma_config);
    
    dma_central_bitclr(dma_central, CTRL_OFFSET, BIT_INT_DMA_CLR);
    dma_central_bitclr(dma_central, CTRL_OFFSET, BIT_INT_CHANGE_DETECT_CLR);
    switch(dma_central->start_channel) {
    case 0:
        dma_central_ch0_ready = 1;
        dma_central_ch1_ready = 1;
        dma_central_ch2_ready = 1;
        dma_central_ch3_ready = 1;
        ch0_read_ready = 0;
        ch1_read_ready = 0;
        ch2_read_ready = 0;
        ch3_read_ready = 0;
        break;
    case 4:
        dma_central_ch4_ready = 1;
        dma_central_ch5_ready = 1;
        dma_central_ch6_ready = 1;
        dma_central_ch7_ready = 1;
        ch4_read_ready = 0;
        ch5_read_ready = 0;
        ch6_read_ready = 0;
        ch7_read_ready = 0;
        break;
    case 8:
        dma_central_ch8_ready = 1;
        dma_central_ch9_ready = 1;
        dma_central_ch10_ready = 1;
        dma_central_ch11_ready = 1;
        ch8_read_ready = 0;
        ch9_read_ready = 0;
        ch10_read_ready = 0;
        ch11_read_ready = 0;
        break;
    }
}

static void dma_central_stop(struct dma_central_dev *dma_central) {
    dma_central_bitset(dma_central, CTRL_OFFSET, BIT_INT_DMA_CLR);
    dma_central_bitset(dma_central, CTRL_OFFSET, BIT_INT_CHANGE_DETECT_CLR);
    msleep(500);
    dma_central_bitset(dma_central, CTRL_OFFSET, BIT_RST);
    dma_central->active = 0;
    switch(dma_central->start_channel) {
    case 0:
        dma_central_ch0_ready = 0;
        dma_central_ch1_ready = 0;
        dma_central_ch2_ready = 0;
        dma_central_ch3_ready = 0;
        break;
    case 4:
        dma_central_ch4_ready = 0;
        dma_central_ch5_ready = 0;
        dma_central_ch6_ready = 0;
        dma_central_ch7_ready = 0;
        break;
    case 8:
        dma_central_ch8_ready = 0;
        dma_central_ch9_ready = 0;
        dma_central_ch10_ready = 0;
        dma_central_ch11_ready = 0;
        break;
    }
    dma_release_channel(dma_central->dma);
}

/////////////////////////////////////////////////////////////////////////////////////////
// sysfs
/////////////////////////////////////////////////////////////////////////////////////////
/*
  static ssize_t dma_start_show(struct device *dev, struct device_attribute *attr,
  char *buf) {
  struct dma_central_dev *dma_central = dev_get_drvdata(dev);
  return sprintf(buf, "%s\n", dma_central->active ? "dma active" : "dma stopped");
  }
  static ssize_t dma_start_store(struct device *dev, struct device_attribute *attr,
  const char *buf, size_t count) {
  struct dma_central_dev *dma_central = dev_get_drvdata(dev);
  if (!dma_central->active) 
  dma_central_start(dma_central);
  else 
  dev_info(dma_central->dev, "dma already active");
  return count;
  }
  static DEVICE_ATTR_RW(dma_start);

  static ssize_t dma_stop_show(struct device *dev, struct device_attribute *attr,
  char *buf) {
  struct dma_central_dev *dma_central = dev_get_drvdata(dev);
  return sprintf(buf, "%s\n", dma_central->active ? "dma active" : "dma stopped");
  }
  static ssize_t dma_stop_store(struct device *dev, struct device_attribute *attr,
  const char *buf, size_t count) {
  struct dma_central_dev *dma_central = dev_get_drvdata(dev);
  if (dma_central->active) 
  dma_central_stop(dma_central);
  else 
  dev_info(dma_central->dev, "dma already stopped");
  return count;
  }
  static DEVICE_ATTR_RW(dma_stop);
*/

static ssize_t stats_show(struct device *dev, struct device_attribute *attr,
                          char *buf) {
    struct dma_central_dev *dma_central = dev_get_drvdata(dev);
    return sprintf(buf, "statistics:\n\t"
                   "irq cnt: %u\n\t"
                   "dma cnt: %u\n\t"
                   "dmain max[0]: %u\n\t"
                   "dmain min[0]: %u\n\t"
                   "dmaout max[0]: %u\n\t"
                   "dmaout min[0]: %u\n\t"
                   "dmain max[1]: %u\n\t"
                   "dmain min[1]: %u\n\t"
                   "dmaout max[1]: %u\n\t"
                   "dmaout min[1]: %u\n\t"
                   "dmain max[2]: %u\n\t"
                   "dmain min[2]: %u\n\t"
                   "dmaout max[2]: %u\n\t"
                   "dmaout min[2]: %u\n\t"
                   "dmain max[3]: %u\n\t"
                   "dmain min[3]: %u\n\t"
                   "dmaout max[3]: %u\n\t"
                   "dmaout min[3]: %u\n",
                   dma_central->stats.irq_cnt,
                   dma_central->stats.dma_cnt,
                   dma_central->stats.dmain_xfer_max[0],
                   dma_central->stats.dmain_xfer_min[0],
                   dma_central->stats.dmaout_xfer_max[0],
                   dma_central->stats.dmaout_xfer_min[0],
                   dma_central->stats.dmain_xfer_max[1],
                   dma_central->stats.dmain_xfer_min[1],
                   dma_central->stats.dmaout_xfer_max[1],
                   dma_central->stats.dmaout_xfer_min[1],
                   dma_central->stats.dmain_xfer_max[2],
                   dma_central->stats.dmain_xfer_min[2],
                   dma_central->stats.dmaout_xfer_max[2],
                   dma_central->stats.dmaout_xfer_min[2],
                   dma_central->stats.dmain_xfer_max[3],
                   dma_central->stats.dmain_xfer_min[3],
                   dma_central->stats.dmaout_xfer_max[3],
                   dma_central->stats.dmaout_xfer_min[3]);
}
static	DEVICE_ATTR_RO(stats);

static ssize_t dma_out_overflow_cnt_show(struct device *dev, struct device_attribute *attr,
                               char *buf) {
    struct dma_central_dev *dma_central = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", dma_central->dma_out_overflow_cnt);
}
static	DEVICE_ATTR_RO(dma_out_overflow_cnt);

static ssize_t dma_bufout_show(struct device *dev, struct device_attribute *attr,
                               char *buf) {
    struct dma_central_dev *dma_central = dev_get_drvdata(dev);
    struct buf *b;
    int count[MAX_CHANNELS] = {0,0,0,0};
    int bufnum=0;
    unsigned long buf_size = DMA_CENTRAL_BUF_SIZE_BYTES;
    list_for_each_entry(b, &dma_central->buf_out_list, list) {
        count[bufnum++] = CIRC_CNT(b->ring.head, b->ring.tail, buf_size);
    }
    switch (bufnum) {
    case 4: return sprintf(buf, "%d, %d, %d, %d\n",
                           count[0], count[1], count[2], count[3]); break;
    case 3: return sprintf(buf, "%d, %d, %d\n",
                           count[0], count[1], count[2]); break;
    case 2: return sprintf(buf, "%d, %d\n", count[0], count[1]); break;
    case 1: return sprintf(buf, "%d\n", count[0]); break;
    }
    return sprintf(buf, "error in dma_bufout_show\n");
}
static	DEVICE_ATTR_RO(dma_bufout);

static ssize_t dma_bufin_show(struct device *dev, struct device_attribute *attr,
                              char *buf) {
    struct dma_central_dev *dma_central = dev_get_drvdata(dev);
    struct buf *b;
    int count[MAX_CHANNELS] = {0,0,0,0};
    int bufnum=0;
    unsigned long buf_size = DMA_CENTRAL_BUF_SIZE_BYTES;
    list_for_each_entry(b, &dma_central->buf_in_list, list) {
        count[bufnum++] = CIRC_CNT(b->ring.head, b->ring.tail, buf_size);
    }
    switch (bufnum) {
    case 4: return sprintf(buf, "%d, %d, %d, %d\n",
                           count[0], count[1], count[2], count[3]); break;
    case 3: return sprintf(buf, "%d, %d, %d\n",
                           count[0], count[1], count[2]); break;
    case 2: return sprintf(buf, "%d, %d\n", count[0], count[1]); break;
    case 1: return sprintf(buf, "%d\n", count[0]); break;
    }
    return sprintf(buf, "error in dma_bufin_show\n");
}
static	DEVICE_ATTR_RO(dma_bufin);

static ssize_t buffers_show(struct device *dev, struct device_attribute *attr,
                            char *buf) {
    struct dma_central_dev *dma_central = dev_get_drvdata(dev);
    struct buf *b;
    unsigned long buf_size = DMA_CENTRAL_BUF_SIZE_BYTES;
    int bufin_sizes[MAX_CHANNELS];
    int bufout_sizes[MAX_CHANNELS];
    int bufnum=0;
    list_for_each_entry(b, &dma_central->buf_in_list, list) {
        bufin_sizes[bufnum++] = CIRC_CNT(b->ring.head, b->ring.tail, buf_size);
    }
    bufnum=0;
    list_for_each_entry(b, &dma_central->buf_out_list, list) {
        bufout_sizes[bufnum++] = CIRC_CNT(b->ring.head, b->ring.tail, buf_size);
    }
    switch (bufnum) {
    case 4: return sprintf(buf, "(in, out) :\n\t(%d, %d)\n\t(%d, %d)\n\t" \
                           "(%d, %d)\n\t(%d, %d)\n",
                           bufin_sizes[0], bufout_sizes[0],
                           bufin_sizes[1], bufout_sizes[1],
                           bufin_sizes[2], bufout_sizes[2],
                           bufin_sizes[3], bufout_sizes[3]); break;
    case 3: return sprintf(buf, "(in, out) :\n\t(%d, %d)\n\t(%d, %d)\n\t" \
                           "(%d, %d)\n",
                           bufin_sizes[0], bufout_sizes[0],
                           bufin_sizes[1], bufout_sizes[1],
                           bufin_sizes[2], bufout_sizes[2]); break;
    case 2: return sprintf(buf, "(in, out) :\n\t(%d, %d)\n\t(%d, %d)\n",
                           bufin_sizes[0], bufout_sizes[0],
                           bufin_sizes[1], bufout_sizes[1]); break;
    case 1: return sprintf(buf, "(in, out) :\n\t(%d, %d)\n", 
                           bufin_sizes[0], bufout_sizes[0]); break;
 
    }
    return sprintf(buf, "error in buffers_show\n");
}
static	DEVICE_ATTR_RO(buffers);

static ssize_t max_rx_dcounts_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
    struct dma_central_dev *dma_central = dev_get_drvdata(dev);
    unsigned max_rx[4] = {0,0,0,0};
    unsigned reg;
    int channel;

    for (channel=0; channel<dma_central->num_channels; channel++) {
        reg = read_dma_central_reg(dma_central, MAXMIN_RXTX_DCOUNT_BASE_OFFSET + channel);
        max_rx[channel] = reg >> 16;
    }
    return sprintf(buf, "sizes = %u, %u, %u, %u\n",
                   max_rx[0], max_rx[1], max_rx[2], max_rx[3]);
}
static	DEVICE_ATTR_RO(max_rx_dcounts);

static ssize_t min_tx_dcounts_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
    struct dma_central_dev *dma_central = dev_get_drvdata(dev);
    unsigned min_tx[4]={32768, 32768, 32768, 32768};
    unsigned reg;
    int channel;

    for (channel=0; channel<dma_central->num_channels; channel++) {
        reg = read_dma_central_reg(dma_central, MAXMIN_RXTX_DCOUNT_BASE_OFFSET + channel);
        min_tx[channel] = reg & 0x0FFFF;
    }
    return sprintf(buf, "sizes = %u, %u, %u, %u\n",
                   min_tx[0], min_tx[1], min_tx[2], min_tx[3]);
}
static	DEVICE_ATTR_RO(min_tx_dcounts);

static ssize_t stats_clr_store(struct device *dev, struct device_attribute *attr,
                               const char *buf, size_t count) {
    struct dma_central_dev *dma_central = dev_get_drvdata(dev);
    int x;
    dma_central_bittoggle(dma_central, STATS_CTRL_OFFSET, BIT_STATS_CLR);
    // reset statistics

    dma_central->stats.irq_cnt = 0;
    dma_central->stats.dma_cnt = 0;
    for (x=0; x<dma_central->num_channels; x++) {
        dma_central->stats.dmain_xfer_max[x] = 0;
        dma_central->stats.dmain_xfer_min[x] = 0;
        dma_central->stats.dmaout_xfer_max[x] = 0;
        dma_central->stats.dmaout_xfer_min[x] = 0;
        dma_central->stats.total_bytes_in[x] = 0;
        dma_central->stats.total_bytes_out[x] = 0;
    }
    return count;
}
static DEVICE_ATTR_WO(stats_clr);

static ssize_t total_bytes_out_show(struct device *dev, struct device_attribute *attr,
                                    char *buf) {
    struct dma_central_dev *dma_central = dev_get_drvdata(dev);
    u32 total_bytes[4];
    int channel;

    for (channel=0; channel<4; channel++) {
        total_bytes[channel] = dma_central->stats.total_bytes_out[channel];
    }
    return sprintf(buf, "%u, %u, %u, %u\n",
                   total_bytes[0], total_bytes[1], total_bytes[2], total_bytes[3]);
}
static	DEVICE_ATTR_RO(total_bytes_out);

static ssize_t total_bytes_in_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
    struct dma_central_dev *dma_central = dev_get_drvdata(dev);
    u32 total_bytes[4];
    int channel;

    for (channel=0; channel<4; channel++) {
        total_bytes[channel] = dma_central->stats.total_bytes_in[channel];
    }
    return sprintf(buf, "%u, %u, %u, %u\n",
                   total_bytes[0], total_bytes[1], total_bytes[2], total_bytes[3]);
}
static	DEVICE_ATTR_RO(total_bytes_in);


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



static struct attribute *dma_central_attrs[] = {
    //&dev_attr_dma_start.attr,
    //&dev_attr_dma_stop.attr,
    &dev_attr_stats.attr,
    &dev_attr_dma_out_overflow_cnt.attr,
    &dev_attr_dma_bufout.attr,
    &dev_attr_dma_bufin.attr,
    &dev_attr_buffers.attr,
    &dev_attr_max_rx_dcounts.attr,
    &dev_attr_min_tx_dcounts.attr,
    &dev_attr_stats_clr.attr,
    &dev_attr_total_bytes_out.attr,
    &dev_attr_total_bytes_in.attr,
    &dev_attr_build_time.attr,
    NULL,
};

static struct attribute_group dma_central_attr_group = {
    .name = NULL, /* put in device directory */
    .attrs = dma_central_attrs,
};

/////////////////////////////////////////////////////////////////////////////////////////
// probe, remove, parse_of, etc
/////////////////////////////////////////////////////////////////////////////////////////
static int dma_central_parse_of(struct dma_central_dev *dma_central)
{
    struct device *dev = dma_central->dev;
    struct device_node *node = dev->of_node;

    struct resource *res;

    int irqnum;
    int ret;

    // of node
    ret = of_property_read_string(node, "compatible", &dma_central->compatible);
    if (ret < 0) {
        dev_err(dev, "dma_central not in DT");
        return ret;
    }

    // start_channel
    ret = of_property_read_u32(node, "start_channel", &dma_central->start_channel);
    if (ret) {
        dev_err(dma_central->dev, "error reading start_channel from dts");
        return ret;
    }
    dev_info(dma_central->dev, "dma_central start_channel = %d\n", dma_central->start_channel);

    // num_channels
    ret = of_property_read_u32(node, "num_channels", &dma_central->num_channels);
    if (ret) {
        dev_err(dma_central->dev, "error reading num_channels from dts");
        return ret;
    }
    dev_info(dma_central->dev, "dma_central num channels = %d\n", dma_central->num_channels);

    // irq
    irqnum = platform_get_irq(dma_central->pdev, 0);
    if (irqnum < 0) {
        dev_err(dma_central->dev, "Could  not get IRQ");
        return irqnum;
    }
    dma_central->irqnum = irqnum;
    dev_info(dev, "IRQ number from DT = %d", dma_central->irqnum);

    // reg -- axi lite
    res = platform_get_resource(dma_central->pdev, IORESOURCE_MEM, 0);
    if (res == NULL) {
        dev_err(dev, "error getting reg addr from dts");
        return -EINVAL;
    }
    dma_central->dma_central_reg_res = res;
		
    // reg -- axi full
    res = platform_get_resource(dma_central->pdev, IORESOURCE_MEM, 1);
    if (res == NULL) {
        dev_err(dev, "error getting reg addr from dts");
        return -EINVAL;
    }
    dma_central->dma_central_axi_res = res;
		
    // int_retry_cnt
    ret = of_property_read_u32(node, "int_retry_cnt", &dma_central->int_retry_cnt);
    if (ret) {
        dev_err(dma_central->dev, "error reading int_retry_cnt from dts");
        return ret;
    }
    dev_info(dma_central->dev, "int_retry_cnt = %d\n", dma_central->int_retry_cnt);

    dev_info(dev, "Done parsing DT");

    return 0;
}

static int dma_central_allocate_resources(struct dma_central_dev *dma_central) {
    int ret = -EINVAL;
    int addr_size;
    u32 start;
    u32 end;
    struct buf *new_buf;
    int x;

    // reset statistics
    dma_central->stats.irq_cnt = 0;
    dma_central->stats.dma_cnt = 0;
    dma_central->dma_out_overflow_cnt = 0;
    for (x=0; x<dma_central->num_channels; x++) {
        dma_central->stats.dmain_xfer_max[x] = 0;
        dma_central->stats.dmain_xfer_min[x] = 0;
        dma_central->stats.dmaout_xfer_max[x] = 0;
        dma_central->stats.dmaout_xfer_min[x] = 0;
        dma_central->rx_en_change_cnt[x] = 0;
        dma_central->tx_en_change_cnt[x] = 0;
    }

    // reset other members
    dma_central->active = 0;
    dma_central->preg = NULL;
    init_completion(&dma_central->dma_ok);

    // map dma_central device memory
    start = (u32)dma_central->dma_central_reg_res->start;
    end = (u32)dma_central->dma_central_reg_res->end;
    addr_size = end - start + 1;
    dma_central->preg =
        (u32*)devm_ioremap(dma_central->dev,
                           dma_central->dma_central_reg_res->start,
                           addr_size);
    if (dma_central->preg == NULL) {
        dev_err(dma_central->dev, "Error mapping dma_central base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    // map dma_central device memory (axi full)
    start = (u32)dma_central->dma_central_axi_res->start;
    end = (u32)dma_central->dma_central_axi_res->end;
    addr_size = end - start + 1;
    dma_central->paxi_buffer_size = addr_size;
    dma_central->axi_phys_addr = (dma_addr_t)start;
    dev_info(dma_central->dev, "axi_phys_addr = 0x%x", (u32)dma_central->axi_phys_addr);
    dma_central->paxi =
        (u64*)ioremap(dma_central->dma_central_axi_res->start,
                      addr_size);
    if (dma_central->paxi == NULL) {
        dev_err(dma_central->dev, "Error mapping dma_central axi base memory");
        ret = -ENOMEM;
        goto UNMAP_MEM;
    }

    // init the ctrl reg to channel_rst (making everything else 0)
    write_dma_central_reg(dma_central, CTRL_OFFSET, BIT_RST);
    dma_central_bitset(dma_central, CTRL_OFFSET, BIT_INT_DMA_CLR);
    dma_central_bitset(dma_central, CTRL_OFFSET, BIT_INT_CHANGE_DETECT_CLR);

    // dma buffers -- dma in
    INIT_LIST_HEAD(&dma_central->buf_in_list);
    for (x=0; x < dma_central->num_channels; x++) {
        new_buf = devm_kmalloc(dma_central->dev, sizeof(struct buf), GFP_KERNEL);
        new_buf->ring.buf = dma_alloc_coherent(dma_central->dev,
                                               DMA_CENTRAL_BUF_SIZE_BYTES,
                                               &new_buf->dma_handle, GFP_KERNEL);
        if (!new_buf->ring.buf) {
            dev_err(dma_central->dev, "Failed to allocate memory for DMA (channel %d)\r\n", x);
            goto UNMAP_AXI_FULL_MEM;
        }
        // add to list
        list_add_tail(&new_buf->list, &dma_central->buf_in_list);
        // shared buffer
        switch (x+dma_central->start_channel) {
        case 0: dma_central_ch0_buf_in = &(new_buf->ring); break;
        case 1: dma_central_ch1_buf_in = &(new_buf->ring); break;
        case 2: dma_central_ch2_buf_in = &(new_buf->ring); break;
        case 3: dma_central_ch3_buf_in = &(new_buf->ring); break;
        case 4: dma_central_ch4_buf_in = &(new_buf->ring); break;
        case 5: dma_central_ch5_buf_in = &(new_buf->ring); break;
        case 6: dma_central_ch6_buf_in = &(new_buf->ring); break;
        case 7: dma_central_ch7_buf_in = &(new_buf->ring); break;
        case 8: dma_central_ch8_buf_in = &(new_buf->ring); break;
        case 9: dma_central_ch9_buf_in = &(new_buf->ring); break;
        case 10: dma_central_ch10_buf_in = &(new_buf->ring); break;
        case 11: dma_central_ch11_buf_in = &(new_buf->ring); break;
        }
    }
    // dma out
    INIT_LIST_HEAD(&dma_central->buf_out_list);
    for (x=0; x < dma_central->num_channels; x++) {
        new_buf = devm_kmalloc(dma_central->dev, sizeof(struct buf), GFP_KERNEL);
        new_buf->ring.buf = dma_alloc_coherent(dma_central->dev,
                                               DMA_CENTRAL_BUF_SIZE_BYTES,
                                               &new_buf->dma_handle, GFP_KERNEL);
        if (!new_buf->ring.buf) {
            dev_err(dma_central->dev, "Failed to allocate memory for DMA (channel %d)\r\n", x);
            goto UNMAP_AXI_FULL_MEM;
        }
        // add to list
        list_add_tail(&new_buf->list, &dma_central->buf_out_list);
        // shared buffer
        switch (x+dma_central->start_channel) {
        case 0: dma_central_ch0_buf_out = &(new_buf->ring); break;
        case 1: dma_central_ch1_buf_out = &(new_buf->ring); break;
        case 2: dma_central_ch2_buf_out = &(new_buf->ring); break;
        case 3: dma_central_ch3_buf_out = &(new_buf->ring); break;
        case 4: dma_central_ch4_buf_out = &(new_buf->ring); break;
        case 5: dma_central_ch5_buf_out = &(new_buf->ring); break;
        case 6: dma_central_ch6_buf_out = &(new_buf->ring); break;
        case 7: dma_central_ch7_buf_out = &(new_buf->ring); break;
        case 8: dma_central_ch8_buf_out = &(new_buf->ring); break;
        case 9: dma_central_ch9_buf_out = &(new_buf->ring); break;
        case 10: dma_central_ch10_buf_out = &(new_buf->ring); break;
        case 11: dma_central_ch11_buf_out = &(new_buf->ring); break;
        }
    }
    dev_info(dma_central->dev, "allocated %d in and %d out dma_buffers", dma_central->num_channels,
             dma_central->num_channels);

    // set int_retry_cnt
    write_dma_central_reg(dma_central, INT_RETRY_CNTFROM_OFFSET, dma_central->int_retry_cnt);

    // init interrupt
    ret = request_irq(dma_central->irqnum, dma_central_isr,
                      IRQF_SHARED | IRQF_TRIGGER_RISING, DMA_CENTRAL_DEV_NAME, dma_central);
    if (ret)
    {
        dev_info(dma_central->dev, "Failed to reserve irq %d\n", dma_central->irqnum);
        ret = -ENODEV;
        goto UNMAP_AXI_FULL_MEM;
    }
    dev_info(dma_central->dev,"Successfully loading ISR handler -- starting dma\n");
    dma_central_start(dma_central);

    return 0;

UNMAP_AXI_FULL_MEM:
    // free previously mapped memory
    iounmap(dma_central->paxi);
UNMAP_MEM:
    // free previously mapped memory
    devm_iounmap(dma_central->dev, dma_central->preg);
FAIL:
    return ret;
}

static int dma_central_probe(struct platform_device *pdev) {
    struct dma_central_dev *dma_central;
    struct device *dev;
    int ret;

    // dma_central
    dma_central = devm_kzalloc(&pdev->dev, sizeof(*dma_central), GFP_KERNEL);
    if (!dma_central)
        return -ENOMEM;

    dma_central->dev = &pdev->dev;
    dma_central->pdev = pdev;
    dev = dma_central->dev;

    dev_set_drvdata(&pdev->dev, dma_central);
	
    ret = dma_central_parse_of(dma_central);
    if (ret < 0)
        return ret;

    ret = dma_central_allocate_resources(dma_central);
    if (ret < 0)
        return ret;

    if (sysfs_create_group(&pdev->dev.kobj, &dma_central_attr_group))
        dev_err(dma_central->dev, "failed to create sysfs entries\n");

    dev_info(dma_central->dev, "dma_central_probe complete");

    return 0;
}

static void dma_central_remove(struct platform_device *pdev) {
    struct dma_central_dev *dma_central = dev_get_drvdata(&pdev->dev);
    struct buf *b;
    
    dev_info(dma_central->dev, "asserting reset");
    dma_central_stop(dma_central);

    dev_info(dma_central->dev, "freeing irq");
    free_irq(dma_central->irqnum, dma_central);

    dev_info(dma_central->dev, "unmapping io memory");
    devm_iounmap(dma_central->dev, dma_central->preg);
    iounmap(dma_central->paxi);

    dev_info(dma_central->dev, "freeing buffers");
    list_for_each_entry(b, &dma_central->buf_in_list, list) {
        dma_free_coherent(dma_central->dev, DMA_CENTRAL_BUF_SIZE_BYTES,
                          b->ring.buf, b->dma_handle);
    }
    list_for_each_entry(b, &dma_central->buf_out_list, list) {
        dma_free_coherent(dma_central->dev, DMA_CENTRAL_BUF_SIZE_BYTES,
                          b->ring.buf, b->dma_handle);
    }

    sysfs_remove_group(&pdev->dev.kobj, &dma_central_attr_group);

    dev_set_drvdata(&pdev->dev, NULL);
    dev_info(dma_central->dev, "remove complete");
}

static const struct of_device_id dma_central_of_id_table[] = {
    { .compatible = "dma_central-1.0" },
    {}
};

MODULE_DEVICE_TABLE(of, dma_central_of_id_table);

static struct platform_driver dma_central_driver = {
    .driver = {
        .name = "dma_central",
        .of_match_table = dma_central_of_id_table,
        .owner = THIS_MODULE,
    },
    .probe = dma_central_probe,
    .remove = dma_central_remove,
};

module_platform_driver(dma_central_driver);

EXPORT_SYMBOL(dma_central_buf_size_bytes);
EXPORT_SYMBOL(dma_central_ch0_ready);
EXPORT_SYMBOL(dma_central_ch1_ready);
EXPORT_SYMBOL(dma_central_ch2_ready);
EXPORT_SYMBOL(dma_central_ch3_ready);
EXPORT_SYMBOL(dma_central_ch4_ready);
EXPORT_SYMBOL(dma_central_ch5_ready);
EXPORT_SYMBOL(dma_central_ch6_ready);
EXPORT_SYMBOL(dma_central_ch7_ready);
EXPORT_SYMBOL(dma_central_ch8_ready);
EXPORT_SYMBOL(dma_central_ch9_ready);
EXPORT_SYMBOL(dma_central_ch10_ready);
EXPORT_SYMBOL(dma_central_ch11_ready);
EXPORT_SYMBOL(dma_central_ch0_buf_in);
EXPORT_SYMBOL(dma_central_ch1_buf_in);
EXPORT_SYMBOL(dma_central_ch2_buf_in);
EXPORT_SYMBOL(dma_central_ch3_buf_in);
EXPORT_SYMBOL(dma_central_ch4_buf_in);
EXPORT_SYMBOL(dma_central_ch5_buf_in);
EXPORT_SYMBOL(dma_central_ch6_buf_in);
EXPORT_SYMBOL(dma_central_ch7_buf_in);
EXPORT_SYMBOL(dma_central_ch8_buf_in);
EXPORT_SYMBOL(dma_central_ch9_buf_in);
EXPORT_SYMBOL(dma_central_ch10_buf_in);
EXPORT_SYMBOL(dma_central_ch11_buf_in);
EXPORT_SYMBOL(dma_central_ch0_buf_out);
EXPORT_SYMBOL(dma_central_ch1_buf_out);
EXPORT_SYMBOL(dma_central_ch2_buf_out);
EXPORT_SYMBOL(dma_central_ch3_buf_out);
EXPORT_SYMBOL(dma_central_ch4_buf_out);
EXPORT_SYMBOL(dma_central_ch5_buf_out);
EXPORT_SYMBOL(dma_central_ch6_buf_out);
EXPORT_SYMBOL(dma_central_ch7_buf_out);
EXPORT_SYMBOL(dma_central_ch8_buf_out);
EXPORT_SYMBOL(dma_central_ch9_buf_out);
EXPORT_SYMBOL(dma_central_ch10_buf_out);
EXPORT_SYMBOL(dma_central_ch11_buf_out);
EXPORT_SYMBOL(ch0_waitq);
EXPORT_SYMBOL(ch1_waitq);
EXPORT_SYMBOL(ch2_waitq);
EXPORT_SYMBOL(ch3_waitq);
EXPORT_SYMBOL(ch4_waitq);
EXPORT_SYMBOL(ch5_waitq);
EXPORT_SYMBOL(ch6_waitq);
EXPORT_SYMBOL(ch7_waitq);
EXPORT_SYMBOL(ch8_waitq);
EXPORT_SYMBOL(ch9_waitq);
EXPORT_SYMBOL(ch10_waitq);
EXPORT_SYMBOL(ch11_waitq);
EXPORT_SYMBOL(ch0_read_ready);
EXPORT_SYMBOL(ch1_read_ready);
EXPORT_SYMBOL(ch2_read_ready);
EXPORT_SYMBOL(ch3_read_ready);
EXPORT_SYMBOL(ch4_read_ready);
EXPORT_SYMBOL(ch5_read_ready);
EXPORT_SYMBOL(ch6_read_ready);
EXPORT_SYMBOL(ch7_read_ready);
EXPORT_SYMBOL(ch8_read_ready);
EXPORT_SYMBOL(ch9_read_ready);
EXPORT_SYMBOL(ch10_read_ready);
EXPORT_SYMBOL(ch11_read_ready);


MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("dma_central: 1.0: DMA_CENTRAL control");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("dma_central");

