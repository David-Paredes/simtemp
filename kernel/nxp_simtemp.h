#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

static struct hrtimer sample_hrtimer;

enum hrtimer_restart sample_hrtimer_callback(struct hrtimer *timer);

