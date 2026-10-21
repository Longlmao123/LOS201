#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "asgn1_ioctl.h"

/*
 * Goi ioctl theo lenh reset, stats, mode hoac version.
 * argc, argv: tham so dong lenh.
 * Tra ve 0 khi thanh cong; 1 khi loi; 2 khi sai cu phap.
 */
int main(int argc, char *argv[])
{
    int fd;
    int result;
    int mode;
    struct asgn1_stats stats = {0};
    char version[ASGN1_VERSION_SIZE] = {0};

    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s reset|stats|version|mode 0|mode 1\n",
                argv[0]);
        return 2;
    }

    fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) {
        perror("open /dev/asgn1");
        return 1;
    }

    if (strcmp(argv[1], "reset") == 0 && argc == 2) {
        result = ioctl(fd, ASGN1_RESET_BUFFER);
        if (result == 0)
            puts("RESET_BUFFER OK");

    } else if (strcmp(argv[1], "stats") == 0 && argc == 2) {
        result = ioctl(fd, ASGN1_GET_STATS, &stats);
        if (result == 0) {
            printf("open_count=%d\n", stats.open_count);
            printf("write_count=%d\n", stats.write_count);
            printf("read_count=%d\n", stats.read_count);
            printf("buffer_len=%d\n", stats.buffer_len);
            printf("last_write_size=%d\n", stats.last_write_size);
        }

    } else if (strcmp(argv[1], "version") == 0 && argc == 2) {
        result = ioctl(fd, ASGN1_GET_VERSION, version);
        if (result == 0) {
            version[ASGN1_VERSION_SIZE - 1] = '\0';
            printf("version=%s\n", version);
        }

    } else if (strcmp(argv[1], "mode") == 0 && argc == 3 &&
               (strcmp(argv[2], "0") == 0 ||
                strcmp(argv[2], "1") == 0)) {
        mode = argv[2][0] - '0';
        result = ioctl(fd, ASGN1_SET_MODE, &mode);
        if (result == 0)
            printf("SET_MODE OK: %d (%s)\n",
                   mode, mode == 0 ? "kernel" : "echo");

    } else {
        fprintf(stderr, "Invalid command or arguments\n");
        close(fd);
        return 2;
    }

    if (result < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
