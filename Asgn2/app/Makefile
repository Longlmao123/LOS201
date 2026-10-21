# Pham Tran Gia Long - SE201095: HOST and ARMv7 application builds.
CC_HOST ?= gcc
CROSS_COMPILE ?= arm-none-linux-gnueabihf-
CC_ARM ?= $(CROSS_COMPILE)gcc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -std=c11 -O2 -g
WARNINGS := -Wall -Wextra
THREAD_FLAGS := -pthread
LDLIBS += -lrt -lm
SOURCES := sms_app.c sms_config.c sms_ipc.c sms_jitter.c circular_buffer.c \
           sensor_thread.c processor_thread.c alert_thread.c logger_thread.c
HEADERS := $(wildcard *.h) ../driver/sms_sensor.h

.PHONY: all arm host debug test clean
all: arm
arm: sms_app
host: sms_app_host sms_app_x86
debug: sms_app_debug

sms_app: $(SOURCES) $(HEADERS)
	$(CC_ARM) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(THREAD_FLAGS) $(SOURCES) $(LDFLAGS) -o $@ $(LDLIBS)

sms_app_host: $(SOURCES) $(HEADERS)
	$(CC_HOST) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(THREAD_FLAGS) $(SOURCES) $(LDFLAGS) -o $@ $(LDLIBS)

sms_app_x86: sms_app_host
	cp sms_app_host sms_app_x86

sms_app_debug: $(SOURCES) $(HEADERS)
	$(CC_HOST) $(CPPFLAGS) -std=c11 -O0 -g3 $(WARNINGS) $(THREAD_FLAGS) $(SOURCES) $(LDFLAGS) -o $@ $(LDLIBS)

test_circular_buffer: test_circular_buffer.c circular_buffer.c circular_buffer.h sms_types.h
	$(CC_HOST) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(THREAD_FLAGS) test_circular_buffer.c circular_buffer.c $(LDFLAGS) -o $@

test_processor: test_processor.c $(filter-out sms_app.c,$(SOURCES)) $(HEADERS)
	$(CC_HOST) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(THREAD_FLAGS) test_processor.c $(filter-out sms_app.c,$(SOURCES)) $(LDFLAGS) -o $@ $(LDLIBS)

test: test_circular_buffer test_processor
	./test_circular_buffer
	./test_processor

clean:
	rm -f sms_app sms_app_host sms_app_x86 sms_app_debug test_circular_buffer test_processor
