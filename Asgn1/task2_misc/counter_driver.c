// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/limits.h>

static atomic_t counter = ATOMIC_INIT(0);
static atomic_t total_reads = ATOMIC_INIT(0);
static atomic_t total_writes = ATOMIC_INIT(0);
static DEFINE_MUTEX(counter_lock);

/* Moi lan open co trang thai doc rieng de xu ly EOF va doc tung phan. */
struct counter_file_state {
    char text[24];
    size_t length;
    bool started;
};

/*
 * counter_open: Cap phat trang thai snapshot rieng cho moi lan mo.
 * Parameters: inode: inode thiet bi, khong su dung; file: nhan state qua private_data.
 * Return: 0 khi thanh cong hoac -ENOMEM neu cap phat that bai.
 */
static int counter_open(struct inode *inode, struct file *file)
{
    struct counter_file_state *state;

    state = kzalloc(sizeof(*state), GFP_KERNEL);
    if (!state)
        return -ENOMEM;

    file->private_data = state;
    return 0;
}

/*
 * counter_release: Giai phong state khi struct file het tham chieu.
 * Parameters: inode: inode thiet bi, khong su dung; file: chua state can giai phong.
 * Return: 0 khi hoan tat.
 */
static int counter_release(struct inode *inode, struct file *file)
{
    kfree(file->private_data);
    return 0;
}

/*
 * Lan doc thanh cong dau tien lay gia tri hien tai va tang counter.
 * Cac lan doc tiep theo lay phan con lai cua cung chuoi, sau do EOF.
 */
/*
 * counter_read: Doc mot snapshot so thap phan; tang counter o lan doc duong dau tien cua open.
 * Parameters: file: state rieng; buffer: dich userspace; count: so byte yeu cau; ppos: vi tri trong snapshot.
 * Return: So byte da doc, 0 tai EOF/count bang 0, hoac ma loi am. total_reads dem cac read tra byte duong.
 */
static ssize_t counter_read(struct file *file, char __user *buffer,
                            size_t count, loff_t *ppos)
{
    struct counter_file_state *state = file->private_data;
    ssize_t ret;
    int value;

    if (count == 0)
        return 0;

    mutex_lock(&counter_lock);

    if (!state->started) {
        value = atomic_read(&counter);

        /* Tranh tran so nguyen co dau khi tang counter. */
        if (value == INT_MAX) {
            ret = -EOVERFLOW;
            goto out;
        }

        state->length = scnprintf(state->text, sizeof(state->text),
                                  "%d\n", value);
    }

    ret = simple_read_from_buffer(buffer, count, ppos,
                                  state->text, state->length);

    if (ret > 0) {
        if (!state->started) {
            atomic_inc(&counter);
            state->started = true;
        }

        atomic_inc(&total_reads);
        pr_info("counter_driver: read %zd bytes, counter=%d\n",
                ret, atomic_read(&counter));
    }

out:
    mutex_unlock(&counter_lock);
    return ret;
}

/* Ghi so nguyen khong am; ghi 0 de reset counter. */
/*
 * counter_write: Parse so nguyen khong am va dat lai counter; ghi 0 la reset.
 * Parameters: file, ppos: khong su dung; buffer: chuoi ASCII userspace; count: so byte du lieu.
 * Return: count khi ghi hop le; 0 neu count bang 0; -EINVAL hoac -EFAULT khi loi.
 */
static ssize_t counter_write(struct file *file,
                             const char __user *buffer,
                             size_t count, loff_t *ppos)
{
    char text[32];
    int value;
    int ret;

    if (count == 0)
        return 0;

    if (count >= sizeof(text))
        return -EINVAL;

    if (copy_from_user(text, buffer, count))
        return -EFAULT;

    /* Khong chap nhan ky tu NUL nam giua du lieu userspace. */
    if (memchr(text, '\0', count))
        return -EINVAL;

    text[count] = '\0';

    ret = kstrtoint(text, 10, &value);
    if (ret || value < 0)
        return -EINVAL;

    mutex_lock(&counter_lock);

    atomic_set(&counter, value);
    atomic_inc(&total_writes);

    pr_info("counter_driver: write value=%d, total_writes=%d\n",
            value, atomic_read(&total_writes));

    mutex_unlock(&counter_lock);
    return count;
}

static const struct file_operations counter_fops = {
    .owner = THIS_MODULE,
    .open = counter_open,
    .release = counter_release,
    .read = counter_read,
    .write = counter_write,
    .llseek = no_llseek,
};

static struct miscdevice counter_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "counter",
    .fops = &counter_fops,
    .mode = 0600,
};

/* Doc procfs chi xem thong ke, khong lam tang counter. */
/*
 * counter_info_show: Xuat snapshot counter va tong doc/ghi ma khong tang counter.
 * Parameters: m: seq_file dau ra; v: iterator khong su dung.
 * Return: 0 khi hoan tat.
 */
static int counter_info_show(struct seq_file *m, void *v)
{
    mutex_lock(&counter_lock);

    seq_printf(m, "counter=%d\n", atomic_read(&counter));
    seq_printf(m, "total_reads=%d\n", atomic_read(&total_reads));
    seq_printf(m, "total_writes=%d\n", atomic_read(&total_writes));

    mutex_unlock(&counter_lock);
    return 0;
}

/*
 * counter_info_open: Khoi tao seq_file cho /proc/counter_info.
 * Parameters: inode: inode procfs, khong su dung; file: file dang mo.
 * Return: 0 khi thanh cong, ma loi am neu single_open that bai.
 */
static int counter_info_open(struct inode *inode, struct file *file)
{
    return single_open(file, counter_info_show, NULL);
}

static const struct proc_ops counter_proc_ops = {
    .proc_open = counter_info_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/*
 * counter_init: Dang ky miscdevice va procfs; rollback miscdevice neu tao procfs loi.
 * Parameters: Khong co tham so.
 * Return: 0 khi thanh cong; ma loi am cua misc_register hoac -ENOMEM.
 */
static int __init counter_init(void)
{
    int ret;

    ret = misc_register(&counter_device);
    if (ret)
        return ret;

    if (!proc_create("counter_info", 0444, NULL, &counter_proc_ops)) {
        misc_deregister(&counter_device);
        return -ENOMEM;
    }

    pr_info("counter_driver: loaded, major=10, minor=%d\n",
            counter_device.minor);
    return 0;
}

/*
 * counter_exit: Xoa procfs va huy dang ky miscdevice.
 * Parameters: Khong co tham so.
 * Return: void, khong tra ve gia tri.
 */
static void __exit counter_exit(void)
{
    remove_proc_entry("counter_info", NULL);
    misc_deregister(&counter_device);
    pr_info("counter_driver: unloaded\n");
}

module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pham Tran Gia Long - SE201095");
MODULE_DESCRIPTION("Assignment 01 - Atomic counter miscdevice");
MODULE_VERSION("1.0");
