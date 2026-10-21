#ifndef ASGN1_IOCTL_H
#define ASGN1_IOCTL_H

#include <linux/ioctl.h>

#define ASGN1_MAGIC 'G'
#define ASGN1_VERSION_SIZE 64

struct asgn1_stats {
    int open_count;
    int write_count;
    int read_count;
    int buffer_len;
    int last_write_size;
};

/* Xoa buffer va reset bo dem doc/ghi; khong truyen du lieu. */
#define ASGN1_RESET_BUFFER \
    _IO(ASGN1_MAGIC, 0)

/* Kernel tra struct thong ke ve userspace. */
#define ASGN1_GET_STATS \
    _IOR(ASGN1_MAGIC, 1, struct asgn1_stats)

/* Userspace gui mode: 0 = kernel, 1 = echo. */
#define ASGN1_SET_MODE \
    _IOW(ASGN1_MAGIC, 2, int)

/* Kernel tra chuoi version trong buffer 64 byte. */
#define ASGN1_GET_VERSION \
    _IOR(ASGN1_MAGIC, 3, char[ASGN1_VERSION_SIZE])

#endif
