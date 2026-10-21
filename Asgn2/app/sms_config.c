/*
 * Xu ly CLI cho ung dung sensor monitoring.
 * Author: Pham Tran Gia Long - SE201095
 * Date: 2026-10-21
 */
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "sms_config.h"

static void print_usage(const char *program)
{
    printf(
        "Usage: %s [options]\n"
        "  -i MS          Sampling interval, default 500\n"
        "  -t VALUE       Temperature threshold, default 40\n"
        "  -h VALUE       Humidity threshold, default 80\n"
        "  -d SEC         Duration, default 60; 0 = unlimited\n"
        "  -v             Verbose output\n"
        "  --fifo         Sensor SCHED_FIFO priority 50 (default)\n"
        "  --other        Sensor SCHED_OTHER priority 0\n"
        "  --mock         Simulated input for host diagnostics\n"
        "  --device PATH  Device path, default /dev/sms_sensor\n"
        "  --log PATH     Log path, default /tmp/sms.log\n"
        "  --help         Show this help\n",
        program);
}

/* Chi chap nhan so nguyen khong am, khong ky tu thua. */
static int parse_unsigned(const char *text,
                           unsigned int maximum,
                           unsigned int *output)
{
    char *end;
    unsigned long value;

    if (text == NULL || text[0] < '0' || text[0] > '9')
        return -1;

    errno = 0;
    value = strtoul(text, &end, 10);

    if (errno != 0 || *end != '\0' || value > maximum)
        return -1;

    *output = (unsigned int)value;
    return 0;
}

static int parse_threshold(const char *text, float *output)
{
    char *end;
    float value;

    if (text == NULL || text[0] == '\0')
        return -1;

    errno = 0;
    value = strtof(text, &end);

    if (errno != 0 || end == text ||
        *end != '\0' || !isfinite(value))
        return -1;

    *output = value;
    return 0;
}

int sms_parse_config(int argc, char **argv, AppConfig *config)
{
    static const struct option options[] = {
        {"fifo",   no_argument,       NULL, 1000},
        {"other",  no_argument,       NULL, 1001},
        {"mock",   no_argument,       NULL, 1002},
        {"device", required_argument, NULL, 1003},
        {"log",    required_argument, NULL, 1004},
        {"help",   no_argument,       NULL, 1005},
        {NULL,     0,                 NULL, 0}
    };
    int option;

    *config = (AppConfig) {
        .interval_ms = 500U,
        .duration_sec = 60U,
        .temperature_threshold = 40.0f,
        .humidity_threshold = 80.0f,
        .verbose = false,
        .use_fifo = true,
        .mock_sensor = false,
        .device_path = SMS_DEFAULT_DEVICE,
        .log_path = SMS_DEFAULT_LOG
    };

    optind = 1;
    opterr = 0;

    while ((option = getopt_long(argc, argv, "i:t:h:d:v",
                                  options, NULL)) != -1) {
        switch (option) {
        case 'i':
            if (parse_unsigned(optarg, 60000U,
                               &config->interval_ms) < 0 ||
                config->interval_ms == 0)
                goto invalid;
            break;

        case 't':
            if (parse_threshold(optarg,
                    &config->temperature_threshold) < 0)
                goto invalid;
            break;

        case 'h':
            if (parse_threshold(optarg,
                    &config->humidity_threshold) < 0)
                goto invalid;
            break;

        case 'd':
            if (parse_unsigned(optarg, UINT_MAX,
                               &config->duration_sec) < 0)
                goto invalid;
            break;

        case 'v':
            config->verbose = true;
            break;

        case 1000:
            config->use_fifo = true;
            break;

        case 1001:
            config->use_fifo = false;
            break;

        case 1002:
            config->mock_sensor = true;
            break;

        case 1003:
            if (optarg[0] == '\0')
                goto invalid;
            config->device_path = optarg;
            break;

        case 1004:
            if (optarg[0] == '\0')
                goto invalid;
            config->log_path = optarg;
            break;

        case 1005:
            print_usage(argv[0]);
            return 1;

        default:
            goto invalid;
        }
    }

    if (optind != argc)
        goto invalid;

    return 0;

invalid:
    fprintf(stderr,
            "Invalid arguments. Use %s --help\n", argv[0]);
    return -1;
}
