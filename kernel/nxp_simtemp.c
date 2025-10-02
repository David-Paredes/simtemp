#include "nxp_simtemp.h"

static int __init nxp_simtemp_init(void)
{
    hrtimer_init(&sample_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hrtimer_set_expires(&sample_hrtimer, ktime_set(0, 100 * NSEC_PER_MSEC));
    sample_hrtimer.function = sample_hrtimer_callback;
    hrtimer_start(&sample_hrtimer, ktime_set(0, 100 * NSEC_PER_MSEC), HRTIMER_MODE_REL);
    printk("hello- Hello, Kernel!\n");
    return 0;
}

static void __exit nxp_simtemp_exit(void)
{
    hrtimer_cancel(&sample_hrtimer);
    printk("hello - Goodbye, Kernel!\n");
}

enum hrtimer_restart sample_hrtimer_callback(struct hrtimer *timer)
{
    ktime_t period = ktime_set(0, 100 * NSEC_PER_MSEC);
    hrtimer_forward(timer, hrtimer_get_expires(timer), period);
    printk("timer callback called!");
    return HRTIMER_RESTART;
}

module_init(nxp_simtemp_init);
module_exit(nxp_simtemp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Paredes: david.walls.54@gmail.com");
MODULE_DESCRIPTION("Simulates a hardware sensor in the Linux kernel and exposes it to user space");

