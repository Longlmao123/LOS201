/*

 * Muc dich: Khung dang ky thiet bi cam bien cho ASM2.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

#include "sms_sensor.h"

/* Cac callback read va ioctl se duoc bo sung sau. */
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

static DEFINE_MUTEX(sms_lock);
static unsigned int sms_interval_ms = SMS_DEFAULT_INTERVAL_MS;
static u64 sms_last_sample_ms;
static bool sms_has_sample;
static u64 sms_read_count;
static int sms_last_temperature10;
static int sms_last_humidity10;
static u64 sms_start_seconds;
static struct proc_dir_entry *sms_proc_entry;
/*
 * Doc mot dong CSV; buffer userspace can it nhat 64 byte.
 * Tra -EAGAIN neu chua du khoang thoi gian giua hai mau.
 */
static ssize_t sms_read(struct file *file, char __user *buf,
                        size_t count, loff_t *ppos)
{
    char line[64];
    u64 now_ms;
    int temperature10;
    int humidity10;
    int len;
    ssize_t ret;

    (void)file;
    (void)ppos;

    if (count == 0)
        return 0;

    if (count < sizeof(line))
        return -EINVAL;

    if (mutex_lock_interruptible(&sms_lock))
        return -ERESTARTSYS;

    now_ms = ktime_to_ms(ktime_get());

    if (sms_has_sample &&
        now_ms - sms_last_sample_ms < sms_interval_ms) {
        ret = -EAGAIN;
        goto out;
    }

    temperature10 = 150 + get_random_u32() % 301;
    humidity10 = 300 + get_random_u32() % 601;

    len = scnprintf(line, sizeof(line),
                    "%llu,%d.%d,%d.%d\n",
                    (unsigned long long)now_ms,
                    temperature10 / 10, temperature10 % 10,
                    humidity10 / 10, humidity10 % 10);

    if (copy_to_user(buf, line, len)) {
        ret = -EFAULT;
        goto out;
    }

    sms_last_sample_ms = now_ms;
    sms_has_sample = true;
 sms_read_count++;
    sms_last_temperature10 = temperature10;
    sms_last_humidity10 = humidity10;
    ret = len;

out:
    mutex_unlock(&sms_lock);
    return ret;
}
/*
 * Nhan interval tu userspace, don vi millisecond.
 * Chap nhan 1..60000 ms theo gioi han thiet ke.
 */
static long sms_ioctl(struct file *file,
                      unsigned int cmd, unsigned long arg)
{
    __u32 new_interval;

    (void)file;

    if (cmd != SMS_SET_INTERVAL)
        return -ENOTTY;

    if (copy_from_user(&new_interval, (void __user *)arg,
                       sizeof(new_interval)))
        return -EFAULT;

    if (new_interval == 0 || new_interval > 60000U)
        return -EINVAL;

    if (mutex_lock_interruptible(&sms_lock))
        return -ERESTARTSYS;

    sms_interval_ms = new_interval;

    mutex_unlock(&sms_lock);

    pr_info("sms_sensor: interval set to %u ms\n",
            new_interval);
    return 0;
}

static const struct file_operations sms_fops = {
    .owner = THIS_MODULE,
 .read = sms_read,
.unlocked_ioctl = sms_ioctl,
    .llseek = no_llseek,
};

/* Kernel tu cap minor number cho thiet bi. */
static struct miscdevice sms_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "sms_sensor",
    .fops = &sms_fops,
    .mode = 0600,
};

/* Dang ky thiet bi khi insmod. */
/* Doc thong ke, khong tao them sample cam bien. */
static int sms_stats_show(struct seq_file *m, void *v)
{
    (void)v;

    if (mutex_lock_interruptible(&sms_lock))
        return -ERESTARTSYS;

    seq_printf(m, "read_count=%llu\n",
               (unsigned long long)sms_read_count);

    seq_printf(m, "has_sample=%u\n",
               sms_has_sample ? 1U : 0U);

    seq_printf(m, "last_temperature=%d.%d\n",
               sms_last_temperature10 / 10,
               sms_last_temperature10 % 10);

    seq_printf(m, "last_humidity=%d.%d\n",
               sms_last_humidity10 / 10,
               sms_last_humidity10 % 10);

    seq_printf(m, "interval_ms=%u\n", sms_interval_ms);

    seq_printf(m, "uptime_seconds=%llu\n",
               (unsigned long long)
               (ktime_get_seconds() - sms_start_seconds));

    mutex_unlock(&sms_lock);
    return 0;
}
static int __init sms_init(void)
{
    int ret;

    sms_start_seconds = ktime_get_seconds();

    ret = misc_register(&sms_device);
    if (ret) {
        pr_err("sms_sensor: registration failed: %d\n", ret);
        return ret;
    }

    sms_proc_entry = proc_create_single(
        "sms_stats", 0444, NULL, sms_stats_show);

    if (!sms_proc_entry) {
        misc_deregister(&sms_device);
        pr_err("sms_sensor: procfs creation failed\n");
        return -ENOMEM;
    }

    pr_info("sms_sensor: device and procfs registered\n");
    return 0;
}
/* Huy dang ky khi rmmod. */
static void __exit sms_exit(void)
{
    proc_remove(sms_proc_entry);
    misc_deregister(&sms_device);
    pr_info("sms_sensor: device and procfs unregistered\n");
}
module_init(sms_init);

module_exit(sms_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pham Tran Gia Long - SE201095");
MODULE_DESCRIPTION("Assignment 02 - Sensor monitoring driver");
MODULE_VERSION("0.1");


