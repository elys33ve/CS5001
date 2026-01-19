// TODO: when support for low rates (<50kbps) is added, reduce the timer callback time to give the loop a chance to
// lock before monitoring lock_history
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
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/math64.h>

// 1: initial
#define DRIVER_VERSION 1

// hard-coded in vhdl (31 bits)
#define ACCUM 0x80000000

// max bitrate
#define MAX_BITRATE 50000000

// max timer_ms
#define MAX_TIMER_CALLBACK_MS 1000

// bit_sync nco size in bits
#define NCO_STEP_SIZE 31

// default rate compare threshhold divisor
#define DEFAULT_THRESH_DIV 5

/////////////////////////////////////
///////////// registers /////////////
/////////////////////////////////////
/* reg0 - ctrl */
#define CTRL_OFFSET (0)
#define BIT_RST (1)
#define BIT_EN (1<<1)
#define BIT_INT_CLR (1<<2)
#define BIT_ACQUISITION (1<<3)

/* reg1 - status */
#define STATUS_OFFSET (1)
#define BIT_LOCK (1)

/* reg2 - min_num5 */
#define NCO_STEP_OFFSET (2)
/* reg3-4 - KI_BASE */
#define KI_BASE_MSW_OFFSET (3)
#define KI_BASE_LSW_OFFSET (4)
/* reg5-6 - KP_BASE */
#define KP_BASE_MSW_OFFSET (5)
#define KP_BASE_LSW_OFFSET (6)
/* reg8 - REFCLK_HZ */
#define REFCLK_HZ_OFFSET (8)
/* reg9 - PHASE_ERROR_DIFF_THRESH */
#define PHASE_ERROR_DIFF_THRESH_OFFSET (9)

/* reg12 - phase_error_abs_diff */
#define PHASE_ERROR_ABS_DIFF_OFFSET (12)

/* reg14 - i_term */
#define I_TERM_OFFSET (14)
/* reg15 - nco_mean */
#define NCO_MEAN_OFFSET (15)

/* reg17 - min_cnt1 */
#define MIN_CNT1_OFFSET (17)
/* reg18 - min_num1 */
#define MIN_NUM1_OFFSET (18)
/* reg19 - min_cnt2 */
#define MIN_CNT2_OFFSET (19)
/* reg20 - min_num2 */
#define MIN_NUM2_OFFSET (20)
/* reg21 - min_cnt3 */
#define MIN_CNT3_OFFSET (21)
/* reg22 - min_num3 */
#define MIN_NUM3_OFFSET (22)
/* reg23 - min_cnt4 */
#define MIN_CNT4_OFFSET (23)
/* reg24 - min_num4 */
#define MIN_NUM4_OFFSET (24)
/* reg27 - nco_control_out */
#define NCO_CONTROL_OFFSET (27)
/* reg28 - rate estimate */
#define RATE_ESTIMATE_OFFSET (28)
#define RATE_ESTIMATE_MASK 0x3FFFFFF
#define SEQ_IDX_SHIFT 26
/* reg31 - version */
#define VERSION_OFFSET (31)
void format_date_time(char *date, char *time, char *str);

struct  bit_sync_dev {
    struct device *dev;
    struct platform_device *pdev;

    const char *compatible;
    u32 __iomem *preg;
    struct resource *reg_res;

    struct timer_list timer;

    u32 timer_callback_ms;

    u32 channel_number;
    u32 refclk_hz;

    u32 bitrate_nominal;

   int enabled;

    // loop
    u64 ki_base;
    u64 kp_base;
    u64 ki;
    u64 kp;
    
    // rate estimate and monitoring
    u64 cnt1, cnt2, cnt3, cnt4, cnt5;
    u64 num1, num2, num3, num4, num5;
    int acquisition;
    int acquisition_init;
    int force_tracking;
    u32 rate_estimate[4];
    u32 rate_estimate_idx;
    u32 rate_estimate_seq_idx_prev;
    
    u32 acquisition_thresh;
    u32 ld_perr_thresh;
    int lock_history[4];
    int lock_history_idx;
    u32 thresh_div;

    // stats
    u64 timer_callback_cnt;
    u64 num_bitrate_updates;
    u32 diff_estimate_nominal_max;
    u32 diff_estimate_actual_max;
};

// use the sequence id's on the rate estimates to determine if the
// rate_estimate history should be updated
static inline u32 rate_history_update(struct bit_sync_dev *bit_sync, u32 current_est, u32 seq_idx) {
   int idx;
   u32 rate1, rate2, rate3, rate4;
   idx = bit_sync->rate_estimate_idx - 1;
   if (idx < 0)
      idx = 3;
   rate1 = bit_sync->rate_estimate[idx--];
   if (idx < 0)
      idx = 3;
   rate2 = bit_sync->rate_estimate[idx--];
   if (idx < 0)
      idx = 3;
   rate3 = bit_sync->rate_estimate[idx--];
   if (idx < 0)
      idx = 3;
   rate4 = bit_sync->rate_estimate[idx--];
   //dev_info(bit_sync->dev, "callback: %u, %u, %u, %u Hz\n", rate1, rate2, rate3, rate4);
    
   if (bit_sync->rate_estimate_seq_idx_prev != seq_idx) {
      //dev_info(bit_sync->dev, "seq prev, current = %u, %u",
      //          bit_sync->rate_estimate_seq_idx_prev, seq_idx);
      bit_sync->rate_estimate[bit_sync->rate_estimate_idx++] = current_est;
      if (bit_sync->rate_estimate_idx == 4)
	 bit_sync->rate_estimate_idx = 0;
      bit_sync->rate_estimate_seq_idx_prev = seq_idx;
      return 1;
   }
   return 0;
}

    
// evaluate if we should move from acquisition to tracking
static inline u32 acquisition_rate_eval(struct bit_sync_dev *bit_sync) {
    u32 rate1, rate2, rate3;
    int idx;
    u64 rate_sum;
    u64 rate_avg;
    u64 rate_high_thresh;
    u64 rate_low_thresh;
    
    idx = bit_sync->rate_estimate_idx - 1;
    if (idx < 0)
        idx = 3;
    rate1 = bit_sync->rate_estimate[idx--];
    if (rate1 == 0)
        return 0;
    if (idx < 0)
        idx = 3;
    rate2 = bit_sync->rate_estimate[idx--];
    if (rate2 == 0)
        return 0;
    if (idx < 0)
        idx = 3;
    rate3 = bit_sync->rate_estimate[idx--];
    if (rate3 == 0)
        return 0;
    rate_sum = rate1 + rate2 + rate3;
    rate_avg = rate_sum / 3;
    rate_low_thresh = (rate_avg * 97) / 100;
    rate_high_thresh = (rate_avg * 103) / 100;
    if (rate_avg < rate_low_thresh) {
        return 0;
    }
    if (rate_avg > rate_high_thresh) {
        return 0;
    }
    return 1; // move to tracking
}

// bit_sync ip core access
static inline u32 read_bit_sync_reg(struct bit_sync_dev *bit_sync,
                                    int offset) {
    return ioread32(bit_sync->preg+offset);
}

static inline void write_bit_sync_reg(struct bit_sync_dev *bit_sync,
                                      int offset, u32 data) {
    iowrite32(data, bit_sync->preg+offset);
}

static inline int bit_sync_bitget(struct bit_sync_dev *bit_sync,
                                  u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_bit_sync_reg(bit_sync, offset);
	return (reg & bitmask) ? 1 : 0;
}

static inline void bit_sync_bitset(struct bit_sync_dev *bit_sync,
                                   u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_bit_sync_reg(bit_sync, offset);
	reg |= bitmask;
	write_bit_sync_reg(bit_sync, offset, reg);
}

static inline void bit_sync_bitclr(struct bit_sync_dev *bit_sync,
                                   u32 offset, u32 bitmask) {
	u32 reg;
	
	reg = read_bit_sync_reg(bit_sync, offset);
	reg &= ~bitmask;
	write_bit_sync_reg(bit_sync, offset, reg);
}
static inline void bit_sync_bitwrite(struct bit_sync_dev *bit_sync,
                                     int offset, u32 bitmask, int value) {
    if (value)
        bit_sync_bitset(bit_sync, offset, bitmask);
    else
        bit_sync_bitclr(bit_sync, offset, bitmask);
}

/////////////////////////////////////////////////////////////////////////////////////////
// helpers
/////////////////////////////////////////////////////////////////////////////////////////
static u32 get_bitrate_actual(struct bit_sync_dev *bit_sync) {
    u32 nco_control_out;
    u32 rate;
    u64 step_refclk;
    nco_control_out = read_bit_sync_reg(bit_sync, NCO_CONTROL_OFFSET);
    step_refclk = (u64)bit_sync->refclk_hz * (u64)nco_control_out;
    rate = (u32)(step_refclk>>NCO_STEP_SIZE); // div accum
    return rate;
}

static void set_bitrate(struct bit_sync_dev *bit_sync, u32 bitrate) {
    u64 req_hz = (u64)bitrate;
    u32 refclk = bit_sync->refclk_hz;
    u64 accum = (u64)ACCUM;
    u64 step;
    u64 dividend;

    dividend = req_hz * accum;
    step = div_u64(dividend, refclk);
    write_bit_sync_reg(bit_sync, NCO_STEP_OFFSET, (u32)step);
}

static void set_loop_parms(struct bit_sync_dev *bit_sync) {
    u32 rate = bit_sync->bitrate_nominal;
    u32 ratio;
    u64 ki = bit_sync->ki_base;
    u64 kp = bit_sync->kp_base;

    ratio = MAX_BITRATE / rate;
    if (ratio > 2) {
       kp = (kp<<1) / ratio;
       ki = (ki>>2) / ratio;
    }
    write_bit_sync_reg(bit_sync, KP_BASE_MSW_OFFSET, (u32)(kp>>32));
    write_bit_sync_reg(bit_sync, KP_BASE_LSW_OFFSET, (u32)(kp & 0x0FFFFFFFF));
    write_bit_sync_reg(bit_sync, KI_BASE_MSW_OFFSET, (u32)(ki>>32));
    write_bit_sync_reg(bit_sync, KI_BASE_LSW_OFFSET, (u32)(ki & 0x0FFFFFFFF));
    bit_sync->kp = kp;
    bit_sync->ki = ki;
    dev_info(bit_sync->dev, "ratio = %u, kp = 0x%llx, ki = 0x%llx", ratio, bit_sync->kp, bit_sync->ki);
};

// get the rough bitrate
static void acquisition_start(struct bit_sync_dev *bit_sync) {
    int x;
    bit_sync->bitrate_nominal=0;
    bit_sync->acquisition=1;
    bit_sync->diff_estimate_nominal_max=0;
    bit_sync->diff_estimate_actual_max=0;
    bit_sync_bitset(bit_sync, CTRL_OFFSET, BIT_RST);
    bit_sync_bitset(bit_sync, CTRL_OFFSET, BIT_ACQUISITION);
    bit_sync_bitclr(bit_sync, CTRL_OFFSET, BIT_RST);
    bit_sync->lock_history_idx = 0;
    for (x=0; x<4; x++) 
        bit_sync->lock_history[x] = 1;
    for (x=0; x<4; x++) 
        bit_sync->rate_estimate[x] = 0;
    bit_sync->rate_estimate_seq_idx_prev = 0xFF;
    bit_sync->rate_estimate_idx = 0;
}

// let the loop track
static void tracking_start(struct bit_sync_dev *bit_sync) {
    dev_info(bit_sync->dev, "tracking start (nominal = %u Hz)", bit_sync->bitrate_nominal);
    bit_sync->num_bitrate_updates++;
    set_loop_parms(bit_sync);
    set_bitrate(bit_sync, bit_sync->bitrate_nominal);
    bit_sync_bitclr(bit_sync, CTRL_OFFSET, BIT_ACQUISITION);
    bit_sync->acquisition = 0;
    bit_sync->acquisition_init=1;
}

static int rate_eval(struct bit_sync_dev *bit_sync) {
    // use the current rate vs estimated 
    u32 diff_est_nominal;
    u32 diff_est_actual;
    u32 acquisition_thresh;
    u32 nominal = bit_sync->bitrate_nominal;
    u32 estimate;
    u32 actual = get_bitrate_actual(bit_sync);
    int retval = 0;
    int idx;

    // get the current estimate
    idx = bit_sync->rate_estimate_idx - 1;
    if (idx < 0)
        idx = 3;
    estimate = bit_sync->rate_estimate[idx];
    // find max of the two nominal, estimate and actual. set the acquisition_thresh from that
    if (estimate > nominal) {
       if (actual > estimate)
	  acquisition_thresh = actual/bit_sync->thresh_div;
       else
	  acquisition_thresh = estimate/bit_sync->thresh_div;
    }
    else {
       if (actual > nominal)
	  acquisition_thresh = actual/bit_sync->thresh_div;
       else
	  acquisition_thresh = nominal/bit_sync->thresh_div;
    }
    bit_sync->acquisition_thresh = acquisition_thresh;
    // diff values
    diff_est_nominal = abs(nominal - estimate);
    diff_est_actual = abs(actual - estimate);
    // stats
    if (diff_est_nominal > bit_sync->diff_estimate_nominal_max)
        bit_sync->diff_estimate_nominal_max = diff_est_nominal;
    if (diff_est_actual > bit_sync->diff_estimate_actual_max)
        bit_sync->diff_estimate_actual_max = diff_est_actual;
    // test estimate vs actual
    if (diff_est_actual > acquisition_thresh && bit_sync->force_tracking == 0) {
       retval = 1;
    }
    return retval;
}

/////////////////////////////////////////////////////////////////////////////////////////
// timer (currently used by the bit_sync operation)
/////////////////////////////////////////////////////////////////////////////////////////
static void timer_callback(struct timer_list *timer_list) {
    struct bit_sync_dev *bit_sync = container_of(timer_list, struct bit_sync_dev, timer);
    s32 rate_estimate=0;
    int rate_estimate_seq_idx;
    u32 status;
    int lock_history_sum = 0;
    u32 reg;

    bit_sync->timer_callback_cnt++;
    // enabled?
    if (bit_sync->enabled == 0) {
       mod_timer(&bit_sync->timer, jiffies + msecs_to_jiffies(bit_sync->timer_callback_ms));
       return;
    }

    // status
    status = read_bit_sync_reg(bit_sync, STATUS_OFFSET);
    if (bit_sync->acquisition == 0) {
        if (bit_sync->acquisition_init == 0) {
            bit_sync->lock_history[bit_sync->lock_history_idx++] = status & BIT_LOCK;
            if (bit_sync->lock_history_idx == 4)
                bit_sync->lock_history_idx = 0;
        }
        else { 
            bit_sync->acquisition_init = 0;
        }
    }
    // get estimate
    reg = read_bit_sync_reg(bit_sync, RATE_ESTIMATE_OFFSET);
    rate_estimate_seq_idx = reg>>SEQ_IDX_SHIFT;
    rate_estimate = reg & RATE_ESTIMATE_MASK;
    if (bit_sync->acquisition) {
        if (rate_history_update(bit_sync, rate_estimate, rate_estimate_seq_idx)) {
            if (acquisition_rate_eval(bit_sync)) {
                bit_sync->bitrate_nominal = rate_estimate;
                tracking_start(bit_sync);
            }
        }
    }
    else { // not acquisition, update estimate values
        if (rate_history_update(bit_sync, rate_estimate, rate_estimate_seq_idx)) {
            if (rate_eval(bit_sync)) { // nonzero return means return to acquisition
	       dev_info(bit_sync->dev, "acquisition start (rate mismatch)");
	       acquisition_start(bit_sync);
            }
        }
        // check the lock history to determine if we should go back to acquistion.
        // There is a delay after acquisition to ensure the loop has time to lock
        else if (bit_sync->acquisition == 0 && bit_sync->acquisition_init == 0) { 
            lock_history_sum = bit_sync->lock_history[0] | bit_sync->lock_history[1] |
                bit_sync->lock_history[2] | bit_sync->lock_history[3];
            if (lock_history_sum == 0) {
	       dev_info(bit_sync->dev, "acquisition start (lock history)");
                acquisition_start(bit_sync);
            }
        }
    }
    mod_timer(&bit_sync->timer, jiffies + msecs_to_jiffies(bit_sync->timer_callback_ms)); 
}

/////////////////////////////////////////////////////////////////////////////////////////
// sysfs
/////////////////////////////////////////////////////////////////////////////////////////
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

static ssize_t reset_show(struct device *dev, struct device_attribute *attr,
                          char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_bit_sync_reg(bit_sync, CTRL_OFFSET);
    return sprintf(buf, reg & BIT_RST ? "reset\n" : "not reset\n");
}
static ssize_t reset_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(bit_sync->dev, "Error in sysfs/reset");
        return count;
    }
    bit_sync->acquisition = 1;
    bit_sync_bitset(bit_sync, CTRL_OFFSET, BIT_ACQUISITION);
    if (val)
        bit_sync_bitset(bit_sync, CTRL_OFFSET, BIT_RST);
    else
        bit_sync_bitclr(bit_sync, CTRL_OFFSET, BIT_RST);
    return count;
}
static	DEVICE_ATTR_RW(reset);

static ssize_t en_show(struct device *dev, struct device_attribute *attr,
                       char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    unsigned reg;
    reg = read_bit_sync_reg(bit_sync, CTRL_OFFSET);
    return sprintf(buf, reg & BIT_EN ? "enabled\n" : "disabled\n");
}
static ssize_t en_store(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    unsigned int val;
    int en_prev = bit_sync->enabled;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(bit_sync->dev, "Error in sysfs/en");
        return count;
    }
    bit_sync->enabled = val ? 1 : 0;
    bit_sync_bitwrite(bit_sync, CTRL_OFFSET, BIT_EN, bit_sync->enabled);
    if (en_prev == 0 && bit_sync->enabled == 1)
       acquisition_start(bit_sync);
    return count;
}
static	DEVICE_ATTR_RW(en);

static ssize_t estimator_stats_show(struct device *dev, struct device_attribute *attr,
                               char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    u32 nco_control_out;
    u32 rate;
    u64 step_refclk;
    u32 cnt1, num1, cnt2, num2, cnt3, num3, cnt4, num4;
    u32 rate_estimate;
    u32 rate_estimate_idx;
    u32 reg;
    nco_control_out = read_bit_sync_reg(bit_sync, NCO_CONTROL_OFFSET);
    reg = read_bit_sync_reg(bit_sync, RATE_ESTIMATE_OFFSET);
    rate_estimate_idx = reg>>SEQ_IDX_SHIFT;
    rate_estimate = reg & RATE_ESTIMATE_MASK;
    cnt1 = read_bit_sync_reg(bit_sync, MIN_CNT1_OFFSET);
    num1 = read_bit_sync_reg(bit_sync, MIN_NUM1_OFFSET);
    cnt2 = read_bit_sync_reg(bit_sync, MIN_CNT2_OFFSET);
    num2 = read_bit_sync_reg(bit_sync, MIN_NUM2_OFFSET);
    cnt3 = read_bit_sync_reg(bit_sync, MIN_CNT3_OFFSET);
    num3 = read_bit_sync_reg(bit_sync, MIN_NUM3_OFFSET);
    cnt4 = read_bit_sync_reg(bit_sync, MIN_CNT4_OFFSET);
    num4 = read_bit_sync_reg(bit_sync, MIN_NUM4_OFFSET);
    step_refclk = (u64)bit_sync->refclk_hz * (u64)nco_control_out;
    rate = (u32)(step_refclk>>NCO_STEP_SIZE); // div accum 
    return sprintf(buf, "\n---------------\n%u, %u\n%u, %u\n%u, %u\n%u, %u\nnumber of callbacks = %llu\nrate_estimate = %u Hz, rate_actual = %u Hz, rate_estimate_idx = %u\n",
                   cnt1, num1, cnt2, num2,
                   cnt3, num3, cnt4, num4,
                   bit_sync->timer_callback_cnt,
                   rate_estimate, rate, rate_estimate_idx);
}
static	DEVICE_ATTR_RO(estimator_stats);

static ssize_t force_tracking_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, bit_sync->force_tracking ? "enabled\n" : "disabled\n");
}
static ssize_t force_tracking_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val)) {
        dev_info(bit_sync->dev, "Error in sysfs/force_tracking");
        return count;
    }
    bit_sync->force_tracking = val ? 1 : 0;
    return count;
}
static	DEVICE_ATTR_RW(force_tracking);

static ssize_t force_acquisition_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    dev_info(bit_sync->dev, "Starting acquisition...");
    acquisition_start(bit_sync);
    return count;
}
static	DEVICE_ATTR_WO(force_acquisition);

static ssize_t num_bitrate_updates_show(struct device *dev, struct device_attribute *attr,
                                        char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "%llu\n", bit_sync->num_bitrate_updates);
}
static	DEVICE_ATTR_RO(num_bitrate_updates);

static ssize_t ki_base_show(struct device *dev, struct device_attribute *attr,
                       char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "ki base = 0x%llx\n", bit_sync->ki_base);
}
static ssize_t ki_base_store(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    u64 val;
    if (kstrtoull(buf, 0, &val)) {
        dev_info(bit_sync->dev, "Error in sysfs/ki_base");
        return count;
    }
    bit_sync->ki_base = val;
    set_loop_parms(bit_sync);
    return count;
}
static	DEVICE_ATTR_RW(ki_base);

static ssize_t kp_base_show(struct device *dev, struct device_attribute *attr,
                       char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "kp base = 0x%llx\n", bit_sync->kp_base);
}
static ssize_t kp_base_store(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    u64 val;
    if (kstrtoull(buf, 0, &val)) {
        dev_info(bit_sync->dev, "Error in sysfs/kp_base");
        return count;
    }
    bit_sync->kp_base = val;
    set_loop_parms(bit_sync);
    return count;
}
static	DEVICE_ATTR_RW(kp_base);

static ssize_t ki_actual_show(struct device *dev, struct device_attribute *attr,
                       char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "ki actual value = 0x%llx\n", bit_sync->ki);
}
static ssize_t ki_actual_store(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    u64 val;
    if (kstrtoull(buf, 0, &val)) {
        dev_info(bit_sync->dev, "Error in sysfs/ki_actual");
        return count;
    }
    bit_sync->ki = val;
    write_bit_sync_reg(bit_sync, KI_BASE_MSW_OFFSET, (u32)(bit_sync->ki>>32));
    write_bit_sync_reg(bit_sync, KI_BASE_LSW_OFFSET, (u32)(bit_sync->ki & 0x0FFFFFFFF));
    dev_info(bit_sync->dev, "ki = 0x%llx", bit_sync->ki);
    return count;
}
static	DEVICE_ATTR_RW(ki_actual);

static ssize_t kp_actual_show(struct device *dev, struct device_attribute *attr,
                       char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "kp actual value = 0x%llx\n", bit_sync->kp);
}
static ssize_t kp_actual_store(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    u64 val;
    if (kstrtoull(buf, 0, &val)) {
        dev_info(bit_sync->dev, "Error in sysfs/kp_actual");
        return count;
    }
    bit_sync->kp = val;
    write_bit_sync_reg(bit_sync, KP_BASE_MSW_OFFSET, (u32)(bit_sync->kp>>32));
    write_bit_sync_reg(bit_sync, KP_BASE_LSW_OFFSET, (u32)(bit_sync->kp & 0x0FFFFFFFF));
    dev_info(bit_sync->dev, "kp = 0x%llx", bit_sync->kp);
    return count;
}
static	DEVICE_ATTR_RW(kp_actual);

static ssize_t bitrate_actual_show(struct device *dev, struct device_attribute *attr,
                                   char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "%u Hz\n", get_bitrate_actual(bit_sync));
}
static	DEVICE_ATTR_RO(bitrate_actual);

static ssize_t bitrate_estimate_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    int idx;
    u32 rate1, rate2, rate3, rate4;
    idx = bit_sync->rate_estimate_idx - 1;
    if (idx < 0)
        idx = 3;
    rate1 = bit_sync->rate_estimate[idx--];
    if (idx < 0)
        idx = 3;
    rate2 = bit_sync->rate_estimate[idx--];
    if (idx < 0)
        idx = 3;
    rate3 = bit_sync->rate_estimate[idx--];
    if (idx < 0)
        idx = 3;
    rate4 = bit_sync->rate_estimate[idx--];
    return sprintf(buf, "%u, %u, %u, %u Hz\n", rate1, rate2, rate3, rate4);
}
static	DEVICE_ATTR_RO(bitrate_estimate);

static ssize_t stats_clr_store(struct device *dev, struct device_attribute *attr,
                               const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    bit_sync->num_bitrate_updates = 0;
    bit_sync->diff_estimate_nominal_max=0;
    bit_sync->diff_estimate_actual_max=0;
    return count;
}
static	DEVICE_ATTR_WO(stats_clr);

static ssize_t timer_callback_ms_show(struct device *dev, struct device_attribute *attr,
                                      char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", bit_sync->timer_callback_ms);
}
static ssize_t timer_callback_ms_store(struct device *dev, struct device_attribute *attr,
                                       const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    u64 val;
    if (kstrtoull(buf, 0, &val)) {
        dev_info(bit_sync->dev, "Error in sysfs/timer_callback_ms");
        return count;
    }
    bit_sync->timer_callback_ms = val;
    set_loop_parms(bit_sync);
    return count;
}
static	DEVICE_ATTR_RW(timer_callback_ms);

static ssize_t acquisition_thresh_show(struct device *dev, struct device_attribute *attr,
                       char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", bit_sync->acquisition_thresh);
}
static	DEVICE_ATTR_RO(acquisition_thresh);

static ssize_t diff_estimate_nominal_max_show(struct device *dev, struct device_attribute *attr,
                       char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", bit_sync->diff_estimate_nominal_max);
}
static	DEVICE_ATTR_RO(diff_estimate_nominal_max);

static ssize_t diff_estimate_actual_max_show(struct device *dev, struct device_attribute *attr,
                       char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", bit_sync->diff_estimate_actual_max);
}
static	DEVICE_ATTR_RO(diff_estimate_actual_max);

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
                               char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    if (bit_sync->acquisition) 
        return sprintf(buf, "acquisition\n");
    else
        return sprintf(buf, "tracking\n");
}
static	DEVICE_ATTR_RO(state);

static ssize_t clear_integrator_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    bit_sync_bitset(bit_sync, CTRL_OFFSET, BIT_INT_CLR);
    bit_sync_bitclr(bit_sync, CTRL_OFFSET, BIT_INT_CLR);
    return count;
}
static	DEVICE_ATTR_WO(clear_integrator);

static ssize_t which_channel_show(struct device *dev, struct device_attribute *attr,
                                char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", bit_sync->channel_number);
}
static	DEVICE_ATTR_RO(which_channel);

static ssize_t version_show(struct device *dev, struct device_attribute *attr,
                               char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", read_bit_sync_reg(bit_sync, VERSION_OFFSET));
}
static	DEVICE_ATTR_RO(version);

static ssize_t nco_mean_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    u32 reg;
    u64 step_refclk;
    u32 rate;
    reg = read_bit_sync_reg(bit_sync, NCO_MEAN_OFFSET);
    step_refclk = (u64)bit_sync->refclk_hz * (u64)reg;
    rate = (u32)(step_refclk>>NCO_STEP_SIZE);
    return sprintf(buf, "0x%x, %u Hz\n", reg, rate); 
}
static	DEVICE_ATTR_RO(nco_mean);

static ssize_t i_term_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bit_sync_reg(bit_sync, I_TERM_OFFSET);
    return sprintf(buf, "%d\n", reg);
}
static	DEVICE_ATTR_RO(i_term);

static ssize_t phase_error_abs_diff_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bit_sync_reg(bit_sync, PHASE_ERROR_ABS_DIFF_OFFSET);
    return sprintf(buf, "%d, 0x%x\n", reg, reg);
}
static	DEVICE_ATTR_RO(phase_error_abs_diff);

static ssize_t ld_phase_error_diff_thresh_show(struct device *dev, struct device_attribute *attr,
                       char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", bit_sync->ld_perr_thresh);
}
static ssize_t ld_phase_error_diff_thresh_store(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    u32 val;
    if (kstrtoint(buf, 0, &val)) {
        dev_info(bit_sync->dev, "Error in sysfs/thresh_min");
        return count;
    }
    bit_sync->ld_perr_thresh = val;
    write_bit_sync_reg(bit_sync, PHASE_ERROR_DIFF_THRESH_OFFSET, val);
    return count;
}
static	DEVICE_ATTR_RW(ld_phase_error_diff_thresh);

static ssize_t lock_show(struct device *dev, struct device_attribute *attr,
                                     char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    u32 reg;
    reg = read_bit_sync_reg(bit_sync, STATUS_OFFSET);
    return sprintf(buf, "%s\n", reg & BIT_LOCK ? "locked" : "unlocked");
}
static	DEVICE_ATTR_RO(lock);

static ssize_t lock_history_show(struct device *dev, struct device_attribute *attr,
                                        char *buf) {
    struct bit_sync_dev *bit_sync = dev_get_drvdata(dev);
    return sprintf(buf, "%d %d %d %d\n", bit_sync->lock_history[0],
		   bit_sync->lock_history[1], bit_sync->lock_history[2],
		   bit_sync->lock_history[3]);
}
static	DEVICE_ATTR_RO(lock_history);

static struct attribute *bit_sync_attrs[] = {
	&dev_attr_build_time.attr,
	&dev_attr_reset.attr,
	&dev_attr_en.attr,
	&dev_attr_estimator_stats.attr,
	&dev_attr_force_tracking.attr,
	&dev_attr_force_acquisition.attr,
	&dev_attr_num_bitrate_updates.attr,
	&dev_attr_ki_base.attr,
	&dev_attr_kp_base.attr,
	&dev_attr_ki_actual.attr,
	&dev_attr_kp_actual.attr,
	&dev_attr_bitrate_actual.attr,
	&dev_attr_bitrate_estimate.attr,
	&dev_attr_stats_clr.attr,
	&dev_attr_timer_callback_ms.attr,
	&dev_attr_acquisition_thresh.attr,
	&dev_attr_diff_estimate_nominal_max.attr,
	&dev_attr_diff_estimate_actual_max.attr,
	&dev_attr_state.attr,
	&dev_attr_clear_integrator.attr,
	&dev_attr_which_channel.attr,
	&dev_attr_version.attr,
	&dev_attr_nco_mean.attr,
	&dev_attr_i_term.attr,
	&dev_attr_phase_error_abs_diff.attr,
	&dev_attr_ld_phase_error_diff_thresh.attr,
	&dev_attr_lock.attr,
	&dev_attr_lock_history.attr,
	NULL,
};

static struct attribute_group bit_sync_attr_group = {
	.name = NULL, /* put in device directory */
	.attrs = bit_sync_attrs,
};

/////////////////////////////////////////////////////////////////////////////////////////
// probe, remove, parse_of, etc
/////////////////////////////////////////////////////////////////////////////////////////
static int bit_sync_parse_of(struct bit_sync_dev *bit_sync)
{
	struct device *dev = bit_sync->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;
	int ret;
    u32 kp_base_msw;
    u32 kp_base_lsw;
    u32 ki_base_msw;
    u32 ki_base_lsw;

	// of node
	ret = of_property_read_string(node, "compatible", &bit_sync->compatible);
	if (ret < 0) {
		dev_err(dev, "bit_sync not in DT");
		return ret;
	}

    // channel number
    ret = of_property_read_u32(node, "channel_number", &bit_sync->channel_number);
    if (ret) {
        dev_err(bit_sync->dev, "error reading channel from dts");
        return ret;
    }
    dev_info(bit_sync->dev, "channel%d\n", bit_sync->channel_number);

	// reg
	res = platform_get_resource(bit_sync->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "error getting reg addr from dts");
		return -EINVAL;
	}
	bit_sync->reg_res = res;

    // refclk_hz
    ret = of_property_read_u32(node, "refclk_hz", &bit_sync->refclk_hz);
    if (ret < 0) {
        dev_err(dev, "error parsing refclk_hz");
        return ret;
    }
    else {
        dev_info(bit_sync->dev, "refclk_hz = %u", bit_sync->refclk_hz);
    }

    // lock detect phase error thresh
    ret = of_property_read_u32(node, "perr_thresh", &bit_sync->ld_perr_thresh);
    if (ret < 0) {
        dev_err(dev, "error parsing lock detect phase error thresh");
        return ret;
    }
    else {
        dev_info(bit_sync->dev, "lock detect phase error thresh = %u", bit_sync->ld_perr_thresh);
    }

    // kp_base, msw and lsw
    ret = of_property_read_u32(node, "kp_base_msw", &kp_base_msw);
    if (ret < 0) {
        dev_err(dev, "error parsing kp_base_msw");
        return ret;
    }
    ret = of_property_read_u32(node, "kp_base_lsw", &kp_base_lsw);
    if (ret < 0) {
        dev_err(dev, "error parsing kp_base_lsw");
        return ret;
    }
    bit_sync->kp_base = ((u64)kp_base_msw)<<32 | kp_base_lsw;
    dev_info(bit_sync->dev, "kp_base = 0x%llu", bit_sync->kp_base);
    
    // ki_base
    ret = of_property_read_u32(node, "ki_base_msw", &ki_base_msw);
    if (ret < 0) {
        dev_err(dev, "error parsing ki_base_msw");
        return ret;
    }
    ret = of_property_read_u32(node, "ki_base_lsw", &ki_base_lsw);
    if (ret < 0) {
        dev_err(dev, "error parsing ki_base_lsw");
        return ret;
    }
    bit_sync->ki_base = ((u64)ki_base_msw)<<32 | ki_base_lsw;
    dev_info(bit_sync->dev, "ki_base = 0x%llu", bit_sync->ki_base);

	dev_info(dev, "Done parsing DT");

	return 0;
}

static int bit_sync_allocate_resources(struct bit_sync_dev *bit_sync) {
    int ret = -EINVAL;
    int addr_size;

    u32 start;
    u32 end;

    u32 refclk_hz;

    // init
    bit_sync->timer_callback_cnt=0;
    bit_sync->cnt1=0;
    bit_sync->cnt2=0;
    bit_sync->cnt3=0;
    bit_sync->cnt4=0;
    bit_sync->cnt5=0;
    bit_sync->num1=0;
    bit_sync->num2=0;
    bit_sync->num3=0;
    bit_sync->num4=0;
    bit_sync->num5=0;
    bit_sync->num_bitrate_updates=0;
    bit_sync->force_tracking=0;
    bit_sync->diff_estimate_nominal_max=0;
    bit_sync->diff_estimate_actual_max=0;
    bit_sync->enabled=0;

    bit_sync->lock_history[0] = 0;
    bit_sync->lock_history[1] = 0;
    bit_sync->lock_history[2] = 0;
    bit_sync->lock_history[3] = 0;
    bit_sync->lock_history_idx = 0;

    bit_sync->thresh_div = DEFAULT_THRESH_DIV;
    
    // map bit_sync device memory
    start = (u32)bit_sync->reg_res->start;
    end = (u32)bit_sync->reg_res->end;
    addr_size = end - start + 1;
	
    bit_sync->preg =
        (u32*)devm_ioremap(bit_sync->dev,
                           bit_sync->reg_res->start,
                           addr_size);
    if (bit_sync->preg == NULL) {
        dev_err(bit_sync->dev, "Error mapping bit_sync base memory");
        ret = -ENOMEM;
        goto FAIL;
    }

    // enter acquisition
    acquisition_start(bit_sync);

    // lock detect phase error threshhold
    write_bit_sync_reg(bit_sync, PHASE_ERROR_DIFF_THRESH_OFFSET, bit_sync->ld_perr_thresh);

    // refclk_hz should be written before taking the system out of reset
    refclk_hz = bit_sync->refclk_hz;
    write_bit_sync_reg(bit_sync, CTRL_OFFSET, BIT_RST);
    write_bit_sync_reg(bit_sync, REFCLK_HZ_OFFSET, bit_sync->refclk_hz);
    dev_info(bit_sync->dev, "writing refclk_hz (%u)", bit_sync->refclk_hz);
    // leave the bit sync disabled (which matches the init driver value for enabled)
    return 0;
FAIL:
    return ret;
}

static int bit_sync_probe(struct platform_device *pdev) {
    struct bit_sync_dev *bit_sync;
    struct device *dev;
    int ret;

    // bit_sync
    bit_sync = devm_kzalloc(&pdev->dev, sizeof(*bit_sync), GFP_KERNEL);
    if (!bit_sync)
        return -ENOMEM;

    bit_sync->dev = &pdev->dev;
    bit_sync->pdev = pdev;
    dev = bit_sync->dev;

    dev_set_drvdata(&pdev->dev, bit_sync);
	
    ret = bit_sync_parse_of(bit_sync);
    if (ret < 0)
        return ret;

    ret = bit_sync_allocate_resources(bit_sync);
    if (ret < 0)
        return ret;

    if (sysfs_create_group(&pdev->dev.kobj, &bit_sync_attr_group))
        dev_err(bit_sync->dev, "failed to create sysfs entries\n");

    dev_info(bit_sync->dev, "setting up timer");
    timer_setup(&bit_sync->timer, timer_callback, 0);
    bit_sync->timer_callback_ms = 400;
    mod_timer(&bit_sync->timer, jiffies + msecs_to_jiffies(bit_sync->timer_callback_ms)); 

    return 0;
}

static void bit_sync_remove(struct platform_device *pdev) {
	struct bit_sync_dev *bit_sync = dev_get_drvdata(&pdev->dev);

	dev_info(bit_sync->dev, "unmapping io memory");
	devm_iounmap(bit_sync->dev, bit_sync->preg);

	sysfs_remove_group(&pdev->dev.kobj, &bit_sync_attr_group);

    del_timer_sync(&bit_sync->timer);

	dev_set_drvdata(&pdev->dev, NULL);
	dev_info(bit_sync->dev, "remove complete");
}

static const struct of_device_id bit_sync_of_id_table[] = {
	{ .compatible = "bit_sync-1.0" },
	{}
};

MODULE_DEVICE_TABLE(of, bit_sync_of_id_table);

static struct platform_driver bit_sync_driver = {
	.driver = {
		.name = "bit_sync",
		.of_match_table = bit_sync_of_id_table,
		.owner = THIS_MODULE,
	},
	.probe = bit_sync_probe,
	.remove = bit_sync_remove,
};

module_platform_driver(bit_sync_driver);

MODULE_AUTHOR("Tim O'Connell");
MODULE_DESCRIPTION("bit_sync: 1.0: CDR originally developed for the TMoIP BS option");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("bit_sync");

