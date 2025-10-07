#ifndef NXP_SIMTEMP_H
#define NXP_SIMTEMP_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/overflow.h>
#include <linux/random.h>

#define DEVICE_NAME "simtemp"
/* Flag masks */
#define NEW_SAMPLE_ON 0x1
#define THRESHOLD_CROSSED_ON 0x2
#define NEW_SAMPLE_OFF 0xFFFFFFFE
#define THRESHOLD_CROSSED_OFF 0xFFFFFFFD
/* Constants for temperature simulation */
#define TEMP_STEP_MC 100
#define MEAN_MC 25000
#define NOISE_AMPLITUDE_MC 1500
#define SUM_AVERAGE 1530
#define TEMP_RANGE 40000
/* Temperature simulation Modes */
#define NORMAL_MODE 0
#define NOISY_MODE 1
#define RAMP_MODE 2

/* variable to be exposed to user */
struct simtemp_info{
    __u64 timestamp_ns;
    __s32 temp_mC;
    __u32 flags;
}__attribute__((packed));
static struct simtemp_info simtemp_sample;

/* Struct used to store dts variables */
struct simtemp_config{
    int sampling_ms;
    int threshold_mC;
    int mode;
}; 
static struct simtemp_config simtemp_dt = {
    .sampling_ms = 100,
    .threshold_mC = 45000,
    .mode = RAMP_MODE
};

/********************* Read, epoll and queue read ************************************/
static wait_queue_head_t simtemp_wait_queue;

/* Read function for the character device */
static ssize_t simtemp_sample_read(struct file *file, char __user *buffer, size_t len, loff_t *offset);

/* Poll fucntion for epoll support */
static unsigned int simtemp_sample_poll(struct file *file, struct poll_table_struct *poll_table);
/********************** Read epoll end ***********************************************/

/******************* Miscellaneous Character Device ***********************************/
/* File operations perform by module */
static const struct file_operations simtemp_fops = {
    .owner = THIS_MODULE,
    .read = simtemp_sample_read,
    .poll = simtemp_sample_poll
};

/* Miscellaneous device description */
static struct miscdevice simtemp_misc_device = {
    .minor = MISC_DYNAMIC_MINOR, // let kernel assign minor number
    .name = DEVICE_NAME,
    .mode = 0666,
    .fops = &simtemp_fops
};
/************************misc device end *********************************************/

/******************************* sysfs ***********************************************/
/* Show function for simtemp_dt.sampling_ms */
static ssize_t simtemp_sampling_show(struct device *dev, struct device_attribute *attr, char *buf);

/* Store function for simtemp_dt.sampling_ms */
static ssize_t simtemp_sampling_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

/* Show function for simtemp_dt.threshold_mC */
static ssize_t simtemp_threshold_show(struct device *dev, struct device_attribute *attr, char *buf);

/* Store function for simtemp_dt.threshold_mC */
static ssize_t simtemp_threshold_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

/* Show function for simtemp_dt.mode */
static ssize_t simtemp_mode_show(struct device *dev, struct device_attribute *attr, char *buf);

/* Store function for simtemp_dt.mode */
static ssize_t simtemp_show_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

/* Attribute definition */
static DEVICE_ATTR_RW(simtemp_sampling);
static DEVICE_ATTR_RW(simtemp_threshold);
static DEVICE_ATTR_RW(simtemp_mode);
/******************************* sysfs end *******************************************/

/******************************* HRTimer *********************************************/
/* Timer struct */
static struct hrtimer sample_hrtimer;

/* Callback function for re-triggering timer */
enum hrtimer_restart sample_hrtimer_callback(struct hrtimer *timer);
/******************************* HRTimer end *****************************************/

/************************** Temperature simulation functions *************************/
/* 1. Ramp simulation function */
__s32 generate_ramp_temp(__s32);
/* 2. Normal simulation function. 
** Simulates temp following a normal distribution arround a mean. */
__s32 generate_normal_temp(void);
/* 3. Noisy simulation function 
** noise_amplitude_mc: The maximum devation from the base trend in milli celsius */
__s32 generate_noisy_temp(__s32);
/********************** Temperature simulation functions end *************************/

#endif 
