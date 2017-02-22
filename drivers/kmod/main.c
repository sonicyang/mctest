#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <asm/delay.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/completion.h>
#include <linux/kthread.h>

#include <linux/miscdevice.h>

#define CREATE_TRACE_POINTS
#include "trace-mctest.h"

#ifdef CONFIG_ARM
#include <asm/neon.h>
#define kernel_floating_point_begin kernel_neon_begin
#define kernel_floating_point_end kernel_neon_end
#else
#include <asm/fpu/api.h>
#define kernel_floating_point_begin kernel_fpu_begin
#define kernel_floating_point_end kernel_fpu_end
#endif


/* FIXME: Workaround functions for C++ Linux Kernel module
 * When C++ program runs, it needs some assistants from
 * runtime library.  However, it is impossible in Linux
 * kernel.  Most of dependencies are removed except the
 * following functions.  Implement them as empty functions
 * to avoid errors in linking and execution.
 */
void *__dso_handle;

void __atan2_finite(void)
{
	return;
}

void __acos_finite(void)
{
	return;
}

void __cxa_pure_virtual(void)
{
	while (1);
}

int __aeabi_atexit(void *object, void (*destructor)(void *), void *dso_handle)
{
	return 0;
}

int __errno(void)
{
	return 0;
}

void __stack_chk_fail(void)
{
	return;
}

void __stack_chk_guard(void)
{
	return;
}

int __cxa_atexit(void (*func) (void *), void * arg, void * dso_handle)
{
	return 0;
}
/* End of Workaround functions for C++ Linux Kernel module */

static struct task_struct *motion_task;
static DECLARE_COMPLETION(trigger_start);

extern void motion_init(void);
extern void motion_iteration(void);

static unsigned long repeat = 0;

struct motion_statistic {
	ktime_t max;
	ktime_t min;
	ktime_t avg;
};

static inline
ktime_t ktime_max(ktime_t a, ktime_t b)
{
	return (ktime_compare(a, b) > 0) ? a : b;
}

static inline
ktime_t ktime_min(ktime_t a, ktime_t b)
{
	return (ktime_compare(a, b) < 0) ? a : b;
}

static struct motion_statistic motion_stat;

enum motion_status_t {
	MOTION_INIT = 0,
	MOTION_FINISH = 1,
	MOTION_IN_PROCESSING = 2
};

static atomic_t motion_status = ATOMIC_INIT(MOTION_INIT);

static ktime_t motion_process(void)
{
	ktime_t time, delta;

	preempt_disable();
	time = ktime_get();

	kernel_floating_point_begin();

	motion_iteration();

	kernel_floating_point_end();

	delta = ktime_sub(ktime_get(), time);
	preempt_enable();

	return delta;
}

static int
motion_work_handler(void *data)
{
	int i = 0;
	ktime_t delta;


	while (!kthread_should_stop()) {
		wait_for_completion_timeout(&trigger_start,
					    msecs_to_jiffies(100));

		if (repeat == 0)
			continue;

		/* FIXME: Warm start only works here for each mctest trigger. */
		motion_process();

		for (i = 0 ; i < repeat ; i++) {
			delta = motion_process();

			motion_stat.max = ktime_max(motion_stat.max, delta);
			motion_stat.min = ktime_min(motion_stat.min, delta);
			motion_stat.avg = ktime_add(motion_stat.avg, delta);
			trace_mctest_exec_time(i, ktime_to_ns(delta));

			udelay(10);
		}

		motion_stat.avg.tv64 = ktime_divns(motion_stat.avg, repeat);

		atomic_set(&motion_status, MOTION_FINISH);

		repeat = 0;
	}

	return 0;
}

static void motion_stat_init(void)
{
	atomic_set(&motion_status, MOTION_INIT);
	motion_stat.max.tv64 = 0;
	motion_stat.min.tv64 = KTIME_MAX;
	motion_stat.avg.tv64 = 0;
}

static bool motion_work_start(void)
{
	if (atomic_read(&motion_status) != MOTION_INIT)
		return false;

	atomic_set(&motion_status, MOTION_IN_PROCESSING);
	complete(&trigger_start);

	return true;
}

static ssize_t
result_show(struct device *dev, struct device_attribute *attr,
	    char *buf)
{
	/* show the result of motion trigger */
	if (buf == NULL)
		return -ENOMEM;

	if (atomic_read(&motion_status) == MOTION_IN_PROCESSING)
		return -EAGAIN;

	/* If the output value is nagative, that means this value is overflow  */
	return scnprintf(buf, PAGE_SIZE, " avg = %lldns\n max = %lldns\n min = %lldns\n",
			 ktime_to_ns(motion_stat.avg),
			 ktime_to_ns(motion_stat.max),
			 ktime_to_ns(motion_stat.min));
}

static DEVICE_ATTR_RO(result);

static ssize_t
trigger_store(struct device *dev, struct device_attribute *attr,
	      const char *buf, size_t len)
{
	int ret;

	ret = kstrtoul(buf, 0, &repeat);
	if (ret)
		goto out;

	ret = len;
	motion_stat_init();

	if (repeat == 0)
		goto out;

	if (motion_work_start() == false)
		ret = -EAGAIN;
out:
	return ret;
}


static DEVICE_ATTR_WO(trigger);

static struct attribute *mc_attrs[] = {
	&dev_attr_result.attr,
	&dev_attr_trigger.attr,
	NULL
};

static const struct attribute_group mc_group = {
	.name = "experiment",
	.attrs = mc_attrs,
};

static const struct attribute_group *mc_groups[] = {
	&mc_group,
	NULL
};

static struct miscdevice mc_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "motion-ctrl",
	.groups = mc_groups,
};

static unsigned int mctest_cpu = NR_CPUS;
module_param(mctest_cpu, uint, 0000);
MODULE_PARM_DESC(mctest_cpu, "the cpu to run kernel module mctest");

#define IS_VALID_CPU(cpu) \
	((cpu) < NR_CPUS) && (cpu_online((cpu)))

static int __init mc_init(void)
{
#if !defined(CONFIG_CONSTRUCTORS) || (defined(CONFIG_ARM) && \
				      !defined(CONFIG_KERNEL_MODE_NEON))
	printk(KERN_ERR"Kernel built without necessary configuration!!\n"
	       "Enable the following kernel configurations:\n"
	       "All platform:\n"
	       "    CONFIG_CONSTRUCTORS\n"
	       "ARM:\n"
	       "    CONFIG_KERNEL_MODE_NEON\n");
	WARN_ON(1);
	return -EPERM;
#endif

	struct sched_param param = { .sched_priority = 90 };
	int node;

	kernel_floating_point_begin();
	motion_init();
	kernel_floating_point_end();

	node = IS_VALID_CPU(mctest_cpu) ? cpu_to_node(mctest_cpu) : NUMA_NO_NODE;

	motion_task = kthread_create_on_node(motion_work_handler,
				NULL, node, "motion");

	if (IS_ERR(motion_task))
		return PTR_ERR(motion_task);

	if (IS_VALID_CPU(mctest_cpu))
		kthread_bind(motion_task, mctest_cpu);

	sched_setscheduler(motion_task, SCHED_FIFO, &param);
	wake_up_process(motion_task);

	return misc_register(&mc_miscdev);
}

static void __exit mc_exit(void)
{
	kthread_stop(motion_task);
	misc_deregister(&mc_miscdev);
}

module_init(mc_init);
module_exit(mc_exit);

MODULE_LICENSE("GPL");
