#include "nxp_simtemp.h"

/* __exit function */
static void __exit nxp_simtemp_exit(void)
{
    /* remove device files */
    device_remove_file(simtemp_misc_device.this_device, &dev_attr_simtemp_sampling);
    device_remove_file(simtemp_misc_device.this_device, &dev_attr_simtemp_threshold);
    
    /* Deregister misc device */
    misc_deregister(&simtemp_misc_device);
    printk("nxp_simtemp: simtemp miscellaneous device unregistered\n");

    /* Kill hrtimer */
    hrtimer_cancel(&sample_hrtimer);
    printk("nxp_simtemp: hrtimer cancelled\n");
}

static int __init nxp_simtemp_init(void)
{
    int ret;
    ktime_t simtemp_sample_period;

    /* Wait queue initialization */
    init_waitqueue_head(&simtemp_wait_queue);

    /* misc device initialization */
    ret = misc_register(&simtemp_misc_device);
    if(ret){
        printk("nxp_simtemp: Failed to register misc device\n");
        return ret;
    }
    else{
        printk("nxp_simtemp: simtemp registered, minor: %d\n", simtemp_misc_device.minor);
    }

    /* Add attributes to /sys/class/misc/simtemp */
    ret = device_create_file(simtemp_misc_device.this_device, &dev_attr_simtemp_sampling);
    if(ret){
        device_remove_file(simtemp_misc_device.this_device, &dev_attr_simtemp_sampling);
        printk("nxp_simtemp: Failed to register misc device sampling_ms\n");
        return ret;
    }
    ret = device_create_file(simtemp_misc_device.this_device, &dev_attr_simtemp_threshold);
    if(ret){
        device_remove_file(simtemp_misc_device.this_device, &dev_attr_simtemp_threshold);
        printk("nxp_simtemp: Failed to register misc device threshold_mC\n");
        return ret;
    }

    /* hrtimer initialization */
    simtemp_sample_period =  ktime_set(0, simtemp_dt.sampling_ms * NSEC_PER_MSEC);
    hrtimer_init(&sample_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    sample_hrtimer.function = sample_hrtimer_callback;
    hrtimer_start(&sample_hrtimer, simtemp_sample_period, HRTIMER_MODE_REL);
    printk("nxp_simtemp: hrtimer created with %d period\n", simtemp_dt.sampling_ms);

    return 0;
}

enum hrtimer_restart sample_hrtimer_callback(struct hrtimer *timer)
{
    /* increment temperature */
    switch(simtemp_dt.mode)
    {
        case NORMAL_MODE:
        simtemp_sample.temp_mC = generate_normal_temp();
        break;
        
        case RAMP_MODE:
        simtemp_sample.temp_mC = generate_ramp_temp(simtemp_sample.temp_mC);
        break;

        case NOISY_MODE:
        simtemp_sample.temp_mC = generate_noisy_temp(simtemp_sample.temp_mC);
        break;
    }

    /* get time stamp */
    simtemp_sample.timestamp_ns = ktime_get_ns();

    /* check of threshold has been passed */
    if(simtemp_sample.temp_mC > simtemp_dt.threshold_mC){
        simtemp_sample.flags |= THRESHOLD_CROSSED_ON;
    }
    else{
        simtemp_sample.flags &= THRESHOLD_CROSSED_OFF;
    }

    /* set new sample bit */
    simtemp_sample.flags |= NEW_SAMPLE_ON;

    /* Wake up any waiting processes */
    wake_up(&simtemp_wait_queue);

    /* re-trigger hrtimer */
    ktime_t simtemp_sample_period = ktime_set(0, simtemp_dt.sampling_ms * NSEC_PER_MSEC);
    hrtimer_forward_now(timer, simtemp_sample_period);

    printk("nxp_simtemp: Temp: %d", simtemp_sample.temp_mC);

    return HRTIMER_RESTART;
}

/* Read function for the character device */
static ssize_t simtemp_sample_read(struct file *file, char __user *buffer, size_t len, loff_t *offset)
{
    int ret;
    bool new_sample_available = (simtemp_sample.flags & NEW_SAMPLE_ON) || (simtemp_sample.flags & THRESHOLD_CROSSED_ON);

    /* Block process if new sample is not availble */
    if(!new_sample_available)
    {
        if(file->f_flags & O_NONBLOCK){
            return -EAGAIN;
        }
        /* Block until a new sample is available */
        if(wait_event_interruptible(simtemp_wait_queue, new_sample_available)){
            return -ERESTARTSYS;
        }
    }

    /* Copy the data to user space */
    ret = copy_to_user(buffer, &simtemp_sample, sizeof(simtemp_sample));
    if(ret){
        return -EFAULT;
    }

    /* Reset new sample flag */
    simtemp_sample.flags &= NEW_SAMPLE_OFF;
    /* Always repeat reads to block again if desired */
    *offset = 0;

    return (sizeof(simtemp_sample));
}

/* Poll fucntion for epoll support */
static unsigned int simtemp_sample_poll(struct file *file, struct poll_table_struct *poll_table)
{
    unsigned int mask = 0;

    /* Add the wait queue to the poll table */
    poll_wait(file, &simtemp_wait_queue, poll_table);

    /* Check if new sample is available */
    if((simtemp_sample.flags & NEW_SAMPLE_ON) || (simtemp_sample.flags & THRESHOLD_CROSSED_ON))
    {
        /* Data is ready to read */
        mask |= POLLIN | POLLRDNORM;
    }

    return mask;
}

/* Show function for simtemp_dt.sampling_ms */
static ssize_t simtemp_sampling_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret = sprintf(buf, "%d\n", simtemp_dt.sampling_ms);
    return ret;
}

/* Store function for simtemp_dt.sampling_ms */
static ssize_t simtemp_sampling_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int val;
    if(kstrtoint(buf, 10, &val)){
        return -EINVAL;
    }
    simtemp_dt.sampling_ms = val;
    return count;
}

/* Show function for simtemp_dt.threshold_mC */
static ssize_t simtemp_threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret = sprintf(buf, "%d\n", simtemp_dt.threshold_mC);
    return ret;
}

/* Store function for simtemp_dt.threshold_mC */
static ssize_t simtemp_threshold_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int val;
    if(kstrtoint(buf, 10, &val)){
        return -EINVAL;
    }
    simtemp_dt.threshold_mC = val;
    return count;
}

/* Show function for simtemp_dt.mode */
static ssize_t simtemp_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret = sprintf(buf, "%d\n", simtemp_dt.mode);
    return ret;
}

/* Store function for simtemp_dt.mode */
static ssize_t simtemp_show_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int val;
    if(kstrtoint(buf, 10, &val)){
        return -EINVAL;
    }
    simtemp_dt.mode = val;
    return count;
}

/* 1. Ramp simulation function */
__s32 generate_ramp_temp(__s32 current_temp)
{
    __s32 change_mc;

    if(check_add_overflow(TEMP_STEP_MC, current_temp, &change_mc)){
        return (TEMP_STEP_MC > 0) ? S32_MAX : S32_MIN;
    }

    return change_mc;
}

/* 2. Normal simulation function. 
** Simulates temp following a normal distribution arround a mean. */
__s32 generate_normal_temp(void)
{
    int i;
    __s32 sum = 0;

    /* Central limit theorem approximation:
    ** sum of 6 uniform random variables gives a psedo-normal distribution */
    for(i = 0; i < 6; i++){
        u8 rnd;
        get_random_bytes(&rnd, 1);
        sum += rnd;
    }

    /* Normalize: mean around 768, scale to temperature range */
    /* Map to a typical normal temp range (e.g., 30°C to 70°C) */
    __s32 temp_mc = MEAN_MC + (sum * TEMP_RANGE / SUM_AVERAGE);

    return temp_mc;
}

/* 3. Noisy simulation function */
__s32 generate_noisy_temp(__s32 current_temp)
{
    __s32 ramp_component_mc = generate_ramp_temp(current_temp);
    __u32 noise_u32;
    __s32 result;

    get_random_bytes(&noise_u32, sizeof(noise_u32));

    /* Generate uniform noise in the range [-noise_amplitude, +noise_amplitude] */
    __s32 noise_component = (__s32)(((u64)noise_u32 / U32_MAX) * 2 * NOISE_AMPLITUDE_MC - NOISE_AMPLITUDE_MC);

    if(check_add_overflow(ramp_component_mc, noise_component, &result)){
        return (ramp_component_mc > 0) ? S32_MAX : S32_MIN;
    }

    return result;
}

module_init(nxp_simtemp_init);
module_exit(nxp_simtemp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Paredes: david.walls.54@gmail.com");
MODULE_DESCRIPTION("Simulates a hardware sensor in the Linux kernel and exposes it to user space");

/*EOF*/
