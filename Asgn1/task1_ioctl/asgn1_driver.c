/*
 * asgn1_driver.c - ASSIGNMENT-01: Character device driver don gian cho Linux 5.15 (ARM)
 *
 * Driver nay tao ra thiet bi ky tu /dev/asgn1 va cung cap 3 giao dien:
 *
 *   1. /dev/asgn1                      : ghi du lieu vao / doc du lieu ra (cdev)
 *   2. /proc/asgn1_info                : thong ke driver, dung seq_file (procfs)
 *   3. /sys/class/asgn1_class/asgn1/<attr>   : cac thuoc tinh chi doc (sysfs)
 *
 * Luong du lieu khi ghi:
 *   user buffer --copy_from_user--> device_buffer --> /proc va /sys doc lai
 *
 * Kien truc goi ham (userspace -> kernel):
 *   write()/read() -> system call -> VFS -> file_operations (asgn1_fops)
 *                  -> asgn1_write()/asgn1_read() -> device_buffer
 */
#include <linux/module.h>     /* module_init/module_exit, MODULE_* */
#include <linux/kernel.h>     /* pr_info, pr_err, pr_warn */
#include <linux/fs.h>         /* file_operations, register_chrdev */
#include <linux/uaccess.h>    /* copy_to_user, copy_from_user */
#include <linux/cdev.h>       /* character device */
#include <linux/device.h>     /* class_create, device_create, DEVICE_ATTR_RO */
#include <linux/proc_fs.h>    /* proc_create, proc_ops */
#include <linux/seq_file.h>   /* seq_file API, single_open */
#include <linux/slab.h>
#include <linux/mutex.h>      /* mutex_lock / mutex_unlock */
#include <linux/string.h>
#include "asgn1_ioctl.h"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pham Tran Gia Long - SE201095");
MODULE_DESCRIPTION("ASSIGNMENT-01: Simple Character Device Driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME "asgn1"          /* ten node trong /dev */
#define CLASS_NAME "asgn1_class"     /* ten class trong /sys/class */
#define BUFFER_SIZE 1024            /* kich thuoc bo dem trong kernel */
#define DRIVER_MAJOR 241            /* major number co dinh (thuoc dai dung cho thu nghiem) */

/* ===== Trang thai toan cuc cua driver ===== */
static int major_number = DRIVER_MAJOR;
static char device_buffer[BUFFER_SIZE];  /* bo dem luu du lieu user ghi vao */
static int buffer_len = 0;               /* so byte hop le dang co trong bo dem */
static int open_count = 0;               /* so lan /dev/asgn1 duoc mo */
static struct class *asgn1_class = NULL;
static struct device *asgn1_device = NULL;

/*
 * Co che mutex:
 * Nhieu tien trinh co the cung mo/ghi /dev/asgn1 tai cung mot thoi diem
 * (may co nhieu CPU, hoac mot tien trinh bi chuyen CPU giua chung). Cac thao
 * tac "open_count++" va "ghi device_buffer + cap nhat buffer_len" khong phai
 * thao tac nguyen tu, nen neu hai luong chay xen ke se sinh ra race condition
 * (dem sai, buffer_len khong khop voi noi dung buffer).
 * Mutex dam bao tai mot thoi diem chi mot luong duoc vao vung "critical
 * section"; cac luong khac se ngu cho den khi mutex duoc nha.
 * Dung mutex thay vi spinlock vi copy_from_user() co the bi ngu (page fault
 * khi trang bo nho cua user chua nap), ma spinlock khong cho phep ngu.
 * DEFINE_MUTEX khoi tao mutex ngay luc bien dich, khong can goi mutex_init().
 */
static DEFINE_MUTEX(asgn1_mutex);
/* Bo dem cac lan doc/ghi thanh cong. */
static int write_count = 0;
static int read_count = 0;
static int last_write_size = 0;

/* Mode 1: echo; Mode 0: them tien to [KERNEL]. */
static int driver_mode = 1;
/* ===================================================================== */
/*                        PROCFS: /proc/asgn1_info                        */
/* ===================================================================== */

static struct proc_dir_entry *proc_entry;

/*
 * seq_file API:
 * Neu tu viet read() cho file trong /proc, ta phai tu xu ly offset, kich
 * thuoc bo dem va truong hop noi dung dai hon mot trang. seq_file lo giup
 * phan do: ta chi can in noi dung bang seq_printf() vao doi tuong seq_file
 * (m), phan con lai (cap phat bo dem, cat theo offset, lseek) do seq_read()
 * va seq_lseek() xu ly.
 *
 * asgn1_proc_show() duoc goi moi lan nguoi dung doc /proc/asgn1_info; no in ra
 * cac so lieu thong ke hien tai cua driver.
 */
/*
 * Xuat thong ke qua /proc/asgn1_info.
 * m: doi tuong seq_file nhan noi dung.
 * v: tham so iterator, khong su dung.
 * Tra ve 0 khi thanh cong.
 * Mutex bao ve trang thai trong luc doc thong ke.
 */
/*
 * asgn1_proc_show: Xuat snapshot thong ke va noi dung buffer qua procfs.
 * Parameters: m: seq_file dau ra; v: iterator khong su dung.
 * Return: 0 khi hoan tat.
 */
static int asgn1_proc_show(struct seq_file *m, void *v)
{
    mutex_lock(&asgn1_mutex);

    seq_printf(m, "ASSIGNMENT-01 Driver Statistics\n");
    seq_printf(m, "===============================\n");
    seq_printf(m, "Driver name     : %s\n", DEVICE_NAME);
    seq_printf(m, "Major number    : %d\n", major_number);
    seq_printf(m, "Buffer size     : %d bytes\n", BUFFER_SIZE);
    seq_printf(m, "Data length     : %d bytes\n", buffer_len);
    seq_printf(m, "Open count      : %d\n", open_count);
    seq_printf(m, "Write count     : %d\n", write_count);
    seq_printf(m, "Read count      : %d\n", read_count);
    seq_printf(m, "Last write size : %d bytes\n", last_write_size);
    seq_printf(m, "Mode            : %d (%s)\n",
               driver_mode,
               driver_mode == 0 ? "kernel" : "echo");

    if (buffer_len > 0)
        seq_printf(m, "Last data       : [%s]\n", device_buffer);
    else
        seq_printf(m, "Last data       : (empty)\n");

    mutex_unlock(&asgn1_mutex);
    return 0;
}

/*
 * single_open(): bien the don gian cua seq_file, dung khi toan bo noi dung
 * file co the in ra trong mot lan goi ham show (khong can duyet tung phan
 * tu nhu mot danh sach dai). No tao san ra cac ham iterator (start/next/
 * stop) va gan asgn1_proc_show lam ham show. Tham so thu ba (NULL) la du lieu
 * rieng, se duoc truyen vao tham so "v"/"private" cua ham show.
 * Phai di cung single_release() o ham release de giai phong bo nho.
 */
/*
 * asgn1_proc_open: Khoi tao seq_file cho /proc/asgn1_info.
 * Parameters: inode: inode procfs, khong su dung; file: file dang mo.
 * Return: 0 khi thanh cong, ma loi am neu single_open that bai.
 */
static int asgn1_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, asgn1_proc_show, NULL);
}

/*
 * Tu kernel 5.6, file trong /proc dung struct proc_ops (khong dung
 * file_operations nua), cac truong co tien to "proc_".
 */
static const struct proc_ops asgn1_proc_fops = {
    .proc_open = asgn1_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/* ===================================================================== */
/*                  SYSFS: /sys/class/asgn1_class/asgn1/<attr>                  */
/* ===================================================================== */

/*
 * Ham "show": duoc goi khi nguoi dung doc mot file thuoc tinh sysfs.
 * Ham ghi gia tri dang chuoi vao "buf" (bo dem co kich thuoc toi da la
 * mot trang, PAGE_SIZE) va tra ve so byte da ghi.
 * Ten ham phai theo quy uoc <ten_thuoc_tinh>_show de macro DEVICE_ATTR_RO
 * tim thay.
 */
/*
 * Doc do dai buffer duoi mutex.
 * dev, attr: thong tin thuoc tinh; buf: buffer dau ra sysfs.
 * Tra ve so byte da ghi vao buf.
 */
/*
 * buffer_len_show: Xuat do dai buffer, doc trang thai duoi mutex.
 * Parameters: dev, attr: metadata sysfs khong su dung; buf: buffer kernel dau ra.
 * Return: So byte da dinh dang trong buf.
 */
static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    ssize_t ret;

    mutex_lock(&asgn1_mutex);
    ret = sprintf(buf, "%d\n", buffer_len);
    mutex_unlock(&asgn1_mutex);

    return ret;
}

/*
 * Doc tong so lan mo thiet bi duoi mutex.
 * dev, attr: thong tin thuoc tinh; buf: buffer dau ra sysfs.
 * Tra ve so byte da ghi vao buf.
 */
/*
 * open_count_show: Xuat tong so lan mo thiet bi, doc duoi mutex.
 * Parameters: dev, attr: metadata sysfs khong su dung; buf: buffer kernel dau ra.
 * Return: So byte da dinh dang trong buf.
 */
static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    ssize_t ret;

    mutex_lock(&asgn1_mutex);
    ret = sprintf(buf, "%d\n", open_count);
    mutex_unlock(&asgn1_mutex);

    return ret;
}

/*
 * Doc noi dung buffer duoi mutex.
 * dev, attr: thong tin thuoc tinh; buf: buffer dau ra sysfs.
 * Tra ve so byte da ghi vao buf.
 */
/*
 * last_data_show: Xuat du lieu cuoi hoac thong bao buffer rong duoi mutex.
 * Parameters: dev, attr: metadata sysfs khong su dung; buf: buffer kernel dau ra.
 * Return: So byte da dinh dang trong buf.
 */
static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
    ssize_t ret;

    mutex_lock(&asgn1_mutex);

    if (buffer_len > 0)
        ret = sprintf(buf, "%s\n", device_buffer);
    else
        ret = sprintf(buf, "(empty)\n");

    mutex_unlock(&asgn1_mutex);
    return ret;
}

/*
 * Macro DEVICE_ATTR_RO(name):
 * Tu dong sinh ra bien "struct device_attribute dev_attr_name" voi ten file
 * la "name", quyen truy cap 0444 (chi doc) va con tro .show tro den ham
 * name_show(). Nho do khong phai khai bao tay tung truong cua struct.
 */
static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);

/*
 * Gom cac thuoc tinh thanh mot mang ket thuc bang NULL, roi gom vao mot
 * attribute_group de dang ky/huy dang ky tat ca cung mot luc.
 *
 * Co che kobject: moi struct device nhung ben trong no mot kobject (doi
 * tuong co so cua mo hinh thiet bi Linux). Moi kobject duoc bieu dien thanh
 * mot THU MUC trong sysfs, con moi attribute la mot FILE nam trong thu muc
 * do. Vi vay dang ky group vao asgn1_device->kobj se sinh ra cac file
 * buffer_len, open_count, last_data ben trong
 * /sys/class/asgn1_class/asgn1/.
 */
static struct attribute *asgn1_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    NULL,
};

static struct attribute_group asgn1_attr_group = {
    .attrs = asgn1_attrs,
};

/* ===================================================================== */
/*                        FILE OPERATIONS (/dev/asgn1)                    */
/* ===================================================================== */

/*
 * asgn1_open(): duoc goi khi mot tien trinh mo /dev/asgn1 (open()).
 * Tang bo dem so lan mo. Thao tac nay phai nam trong mutex vi "open_count++"
 * la doc-sua-ghi, neu hai tien trinh mo dong thoi co the mat mot lan dem.
 */
/*
 * asgn1_open: Tang tong so lan mo va ghi log trong cung vung mutex.
 * Parameters: inode: inode thiet bi; file: file dang mo; ca hai khong su dung.
 * Return: 0 khi thanh cong.
 */
static int asgn1_open(struct inode *inode, struct file *file) {
    mutex_lock(&asgn1_mutex);
    open_count++;
    pr_info("asgn1_driver: device opened (count=%d)\n", open_count);
    mutex_unlock(&asgn1_mutex);
    return 0;
}

/*
 * asgn1_release(): duoc goi khi tien trinh dong file (close()) va khong con
 * tham chieu nao toi file do. Driver nay khong cap phat tai nguyen rieng cho
 * tung lan mo nen chi can ghi log.
 */
/*
 * asgn1_release: Ghi log khi lan tham chieu cuoi toi file duoc dong.
 * Parameters: inode: inode thiet bi; file: file dang dong; ca hai khong su dung.
 * Return: 0 khi hoan tat.
 */
static int asgn1_release(struct inode *inode, struct file *file) {
    pr_info("asgn1_driver: device closed\n");
    return 0;
}

/*
 * asgn1_read(): duoc goi khi tien trinh doc /dev/asgn1 (read()).
 *
 * Tai sao dung copy_to_user() ma khong dung memcpy():
 *  - "buf" la con tro thuoc khong gian dia chi cua USER. Con tro nay co the
 *    sai, tro toi vung chua duoc anh xa, hoac nam tren trang bi swap ra dia.
 *    memcpy() khong kiem tra gi ca: neu dia chi hong, kernel se bi oops/
 *    panic, hoac (tren he thong co bao ve truy cap) bi chan hoan toan.
 *  - copy_to_user() kiem tra dia chi, xu ly duoc loi trang (page fault) mot
 *    cach an toan va tra ve so byte KHONG sao chep duoc. Neu khac 0, ta tra
 *    ve -EFAULT de bao loi "dia chi khong hop le" cho chuong trinh nguoi
 *    dung thay vi lam sap he thong.
 *  - Ngoai ra no ngan user lua kernel doc/ghi vao vung nho kernel (van de
 *    bao mat).
 *
 * Co che offset: *offset la vi tri doc hien tai cua file. Khi offset da
 * toi het du lieu (>= buffer_len) ta tra ve 0, tuc la EOF; lenh "cat" se goi
 * read() lien tuc cho den khi nhan 0 thi moi dung.
 *
 * Ham doc giu asgn1_mutex khi truy cap buffer va cap nhat counter.
 * Nhap nhanh out de nha mutex trong ca truong hop thanh cong va bao loi.
 */
/*
 * Doc du lieu tu buffer sang userspace.
 * file: file dang mo; buf: buffer userspace.
 * len: so byte yeu cau; offset: vi tri doc hien tai.
 * Tra ve so byte da doc, 0 tai EOF, hoac ma loi am.
 * read_count chi tang khi doc thanh cong it nhat 1 byte.
 */
/*
 * asgn1_read: Doc buffer sang userspace va cap nhat offset, read_count.
 * Parameters: file: file dang mo, khong su dung; buf: dich userspace; len: so byte yeu cau; offset: vi tri doc.
 * Return: So byte da doc; 0 tai EOF/len bang 0; -EINVAL hoac -EFAULT khi loi.
 */
static ssize_t asgn1_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    ssize_t ret;

    if (*offset < 0)
        return -EINVAL;

    if (len == 0)
        return 0;

    mutex_lock(&asgn1_mutex);

    if (*offset >= buffer_len) {
        ret = 0;
        goto out;
    }

    if (len > buffer_len - *offset)
        len = buffer_len - *offset;

    /* Dung uaccess de sao chep an toan sang user memory. */
    if (copy_to_user(buf, device_buffer + *offset, len)) {
        ret = -EFAULT;
        goto out;
    }

    *offset += len;
    read_count++;
    ret = len;

    pr_info("asgn1_driver: read %zu bytes, read_count=%d\n",
            len, read_count);
out:
    mutex_unlock(&asgn1_mutex);
    return ret;
}


/*
 * asgn1_write(): duoc goi khi tien trinh ghi vao /dev/asgn1 (write()).
 *
 * Tai sao dung copy_from_user() ma khong dung memcpy(): ly do tuong tu
 * copy_to_user() nhung theo chieu nguoc lai. "buf" nam o khong gian user, co
 * the khong hop le; copy_from_user() kiem tra va bao loi -EFAULT thay vi lam
 * kernel bi sap. Khong bao gio tin tuong con tro tu user.
 *
 * Giua chung cua ham: kiem tra do dai de tranh tran bo dem (buffer
 * overflow): toi da chi nhan BUFFER_SIZE - 1 byte, chua 1 byte cho ky tu
 * ket thuc '\0'.
 *
 * Mutex bao ve doan "chep du lieu + cap nhat buffer_len + them '\0'" de hai
 * luong ghi dong thoi khong tron lan noi dung buffer. Luu y phai nha mutex
 * ca trong nhanh bao loi (-EFAULT), neu khong mutex bi khoa vinh vien
 * (deadlock).
 *
 * Moi lan ghi se GHI DE toan bo noi dung cu (khong noi tiep), nen *offset
 * khong duoc cap nhat.
 */
/*
 * Ghi du lieu vao buffer, thay the noi dung cu.
 * file: file dang mo; buf: du lieu trong userspace.
 * len: so byte user gui; offset: khong dung de noi du lieu.
 * Tra ve so byte user da ghi, hoac ma loi am.
 * Mode 0 them [KERNEL]; mode 1 giu nguyen du lieu.
 */
/*
 * asgn1_write: Thay the buffer bang du lieu user, them tien to theo mode.
 * Parameters: file: file dang mo, khong su dung; buf: nguon userspace; len: so byte; offset: khong su dung.
 * Return: So byte user da ghi, 0 neu len bang 0, hoac ma loi am.
 */
static ssize_t asgn1_write(struct file *file,
                          const char __user *buf,
                          size_t len, loff_t *offset)
{
    char *temp;
    const char prefix[] = "[KERNEL]";
    size_t prefix_len;
    ssize_t ret;

    if (len == 0)
        return 0;
    temp = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!temp)
        return -ENOMEM;


    mutex_lock(&asgn1_mutex);

    prefix_len = (driver_mode == 0) ? sizeof(prefix) - 1 : 0;

    /* Chua cho tien to va ky tu ket thuc chuoi. */
    if (len > BUFFER_SIZE - 1 - prefix_len) {
        ret = -EMSGSIZE;
        goto out;
    }

    /* Hai buffer nay deu nam trong kernel nen dung memcpy. */
    if (prefix_len > 0)
        memcpy(temp, prefix, prefix_len);

    /* Du lieu user phai duoc chep bang copy_from_user. */
    if (copy_from_user(temp + prefix_len, buf, len)) {
        ret = -EFAULT;
        goto out;
    }

    temp[prefix_len + len] = '\0';

    /* Chi thay noi dung cu khi da nhan du du lieu. */
    memcpy(device_buffer, temp, prefix_len + len + 1);
    buffer_len = prefix_len + len;
    last_write_size = len;
    write_count++;
    ret = len;

    pr_info("asgn1_driver: write %zu bytes, mode=%d, "
            "buffer_len=%d, write_count=%d\n",
            len, driver_mode, buffer_len, write_count);

out:
    mutex_unlock(&asgn1_mutex);
    kfree(temp);
    return ret;
}

/*
 * Bang file_operations: "bang gan" giua cac system call cua userspace voi
 * cac ham cua driver. VFS dua vao bang nay de goi dung ham khi co open(),
 * read(), write(), close() tren /dev/asgn1.
 * .owner = THIS_MODULE giup kernel dem tham chieu module, khong cho phep gui
 * lenh rmmod khi thiet bi dang duoc mo.
 */
/*
 * Xu ly cac lenh ioctl.
 * file: file thiet bi dang mo.
 * cmd: ma lenh trong asgn1_ioctl.h.
 * arg: dia chi du lieu trong userspace.
 * Tra ve 0 khi thanh cong, ma loi am khi that bai.
 */
/*
 * asgn1_ioctl: Xu ly reset, lay thong ke, doi mode va lay version.
 * Parameters: file: file dang mo, khong su dung; cmd: ma ioctl; arg: dia chi userspace neu lenh can.
 * Return: 0 khi thanh cong; -EFAULT, -EINVAL hoac -ENOTTY khi loi.
 */
static long asgn1_ioctl(struct file *file,
                       unsigned int cmd, unsigned long arg)
{
    struct asgn1_stats stats = {0};
    char version[ASGN1_VERSION_SIZE] =
        "asgn1_driver v1.0 - SE201095";
    void __user *user_arg = (void __user *)arg;
    int new_mode;
    long ret = 0;

    pr_info("asgn1_driver: ioctl cmd=0x%x\n", cmd);

    mutex_lock(&asgn1_mutex);

    switch (cmd) {
    case ASGN1_RESET_BUFFER:
        memset(device_buffer, 0, sizeof(device_buffer));
        buffer_len = 0;
        write_count = 0;
        read_count = 0;
        last_write_size = 0;
        pr_info("asgn1_driver: RESET_BUFFER completed\n");
        break;

    case ASGN1_GET_STATS:
        stats.open_count = open_count;
        stats.write_count = write_count;
        stats.read_count = read_count;
        stats.buffer_len = buffer_len;
        stats.last_write_size = last_write_size;

        if (copy_to_user(user_arg, &stats, sizeof(stats)))
            ret = -EFAULT;

        pr_info("asgn1_driver: GET_STATS result=%ld\n", ret);
        break;

    case ASGN1_SET_MODE:
        if (copy_from_user(&new_mode, user_arg,
                           sizeof(new_mode))) {
            ret = -EFAULT;
            break;
        }

        if (new_mode != 0 && new_mode != 1) {
            ret = -EINVAL;
            break;
        }

        driver_mode = new_mode;
        pr_info("asgn1_driver: SET_MODE mode=%d\n",
                driver_mode);
        break;

    case ASGN1_GET_VERSION:
        if (copy_to_user(user_arg, version, sizeof(version)))
            ret = -EFAULT;

        pr_info("asgn1_driver: GET_VERSION result=%ld\n", ret);
        break;

    default:
        ret = -ENOTTY;
        pr_warn("asgn1_driver: unknown ioctl command\n");
        break;
    }

    mutex_unlock(&asgn1_mutex);
    return ret;
}

static struct file_operations asgn1_fops = {
    .owner = THIS_MODULE,
    .open = asgn1_open,
    .release = asgn1_release,
    .read = asgn1_read,
    .write = asgn1_write,
    .unlocked_ioctl = asgn1_ioctl,

};

/* ===================================================================== */
/*                        INIT / EXIT cua module                         */
/* ===================================================================== */

/*
 * asgn1_init(): chay mot lan khi nap module (insmod). Cac buoc theo thu tu:
 *   1. register_chrdev(): dang ky major number va bang file_operations.
 *   2. class_create(): tao class "asgn1_class" trong /sys/class.
 *   3. device_create(): tao device "asgn1" thuoc class do. Neu he thong co
 *      devtmpfs, node /dev/asgn1 tu dong xuat hien; neu khong thi phai tao
 *      bang mknod.
 *   4. sysfs_create_group(): them cac file thuoc tinh vao thu muc sysfs.
 *   5. proc_create(): tao /proc/asgn1_info.
 * Neu mot buoc that bai, phai huy nhung buoc da lam truoc do (theo thu tu
 * nguoc lai) roi moi tra ve loi, de khong ro ri tai nguyen. Loi o buoc 4
 * va 5 chi canh bao vi chung khong anh huong chuc nang chinh cua thiet bi.
 * Macro __init danh dau ham chi dung luc nap, kernel se giai phong bo nho
 * cua ham nay sau khi khoi tao xong.
 */
/*
 * asgn1_init: Dang ky major, class, device, sysfs va procfs cua driver.
 * Parameters: Khong co tham so.
 * Return: 0 khi nap thanh cong; ma loi am neu dang ky major/class/device that bai. Loi sysfs/procfs hien chi duoc canh bao.
 */
static int __init asgn1_init(void) {
    int ret;
    pr_info("asgn1_driver: initializing module\n");
    ret = register_chrdev(major_number, DEVICE_NAME, &asgn1_fops);
    if (ret < 0) {
        pr_err("asgn1_driver: register_chrdev failed: %d\n", ret);
        return ret;
    }
    asgn1_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(asgn1_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(asgn1_class);
    }
    asgn1_device = device_create(asgn1_class, NULL,
                                MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(asgn1_device)) {
        class_destroy(asgn1_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(asgn1_device);
    }

    /* Dang ky nhom thuoc tinh sysfs vao kobject cua thiet bi */
    ret = sysfs_create_group(&asgn1_device->kobj, &asgn1_attr_group);
    if (ret)
        pr_warn("asgn1_driver: sysfs_create_group failed\n");

    /* Tao /proc/asgn1_info (quyen 0444: moi nguoi chi duoc doc) */
    proc_entry = proc_create("asgn1_info", 0444, NULL, &asgn1_proc_fops);
    if (!proc_entry)
        pr_warn("asgn1_driver: failed to create /proc/asgn1_info\n");
    else
        pr_info("asgn1_driver: /proc/asgn1_info created\n");

    pr_info("asgn1_driver: loaded, major=%d\n", major_number);
    return 0;
}

/*
 * asgn1_exit(): chay khi go module (rmmod). Phai go bo moi thu da tao trong
 * asgn1_init() theo THU TU NGUOC LAI (cai tao sau thi huy truoc), de khong
 * de lai file /proc, thuoc tinh sysfs hay node thiet bi "mo coi". Macro
 * __exit danh dau ham chi dung luc go module.
 */
/*
 * asgn1_exit: Thu hoi procfs, sysfs, device, class va major da dang ky.
 * Parameters: Khong co tham so.
 * Return: void, khong tra ve gia tri.
 */
static void __exit asgn1_exit(void) {
    if (proc_entry)
        proc_remove(proc_entry);
    sysfs_remove_group(&asgn1_device->kobj, &asgn1_attr_group);
    device_destroy(asgn1_class, MKDEV(major_number, 0));
    class_destroy(asgn1_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("asgn1_driver: module unloaded\n");
}

module_init(asgn1_init);   /* khai bao ham chay khi insmod */
module_exit(asgn1_exit);   /* khai bao ham chay khi rmmod */
