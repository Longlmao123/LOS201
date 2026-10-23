# Lab 03 — Debugging and Performance Analysis

[Back to Main Page](../README.md) · [Applications](apps/) · [Experiment Logs](logs/) · [Boot Image](output/)

Lab 03 focuses on debugging C programs, detecting memory leaks, tracing system calls, profiling CPU activity, and measuring scheduling latency.

The main tools used in this lab are **GDB, Valgrind, strace, perf, and cyclictest**.

## Table of Contents

- [Objectives and Environment](#objectives-and-environment)
- [Key Files](#key-files)
- [Debugging with GDB](#debugging-with-gdb)
- [Memory Analysis with Valgrind](#memory-analysis-with-valgrind)
- [System Calls and CPU Profiling](#system-calls-and-cpu-profiling)
- [Scheduling Latency](#scheduling-latency)
- [Lessons Learned](#lessons-learned)

---

## Objectives and Environment

The primary objective of this laboratory is to develop practical skills in debugging, memory analysis, system-call tracing, and performance evaluation in an Embedded Linux environment.

| Area | Tool | Objective |
|---|---|---|
| Debugging | GDB / gdb-multiarch | Identify program failures and inspect runtime data |
| Memory Analysis | Valgrind Memcheck | Detect memory leaks and invalid memory operations |
| System-Call Tracing | strace | Analyze interactions between applications and the Linux kernel |
| CPU Profiling | perf | Identify performance bottlenecks and CPU-intensive operations |
| Scheduling Analysis | cyclictest | Measure thread wake-up latency and scheduling jitter |

### Execution Environment

The laboratory uses two execution environments:

- **HOST:** Ubuntu Linux running on an x86_64 system.
- **TARGET:** ARM Linux running inside QEMU.

The HOST environment is used for compiling native applications and running diagnostic tools.

The ARM environment is used for testing embedded applications under QEMU.

> **Note:** ARM executables cannot run directly on an x86_64 HOST without an appropriate emulator or compatibility environment.

---

## Key Files

The repository contains source code, experimental logs, and supporting files.

| File / Directory | Description |
|---|---|
| [apps/buggy_app.c](apps/buggy_app.c) | Application containing intentional programming errors |
| [apps/leaky_app.c](apps/leaky_app.c) | Original application containing memory-management defects |
| [apps/leaky_app_fixed.c](apps/leaky_app_fixed.c) | Corrected application with improved memory management |
| [apps/perf_target.c](apps/perf_target.c) | Application used for CPU profiling and system-call analysis |
| [logs/](logs/) | Experimental results and diagnostic logs |
| [output/initramfs_lab3.cpio.gz](output/initramfs_lab3.cpio.gz) | Initial RAM filesystem image for Lab 03 |

---

## Debugging with GDB

### 1. Objective

GNU Debugger (GDB) is used to identify programming errors by inspecting the execution state of an application.

The debugging process includes:

1. Compiling an application with debugging symbols.
2. Starting a debugging session.
3. Setting breakpoints.
4. Inspecting variables and memory.
5. Examining the call stack.
6. Identifying the source of program failures.

### 2. Compile the Application

From the `embedded_lab3` directory:

```bash
gcc -g -O0 apps/buggy_app.c -o /tmp/lab3_buggy
```

The compilation options are:

| Option | Description |
|---|---|
| `-g` | Include debugging information |
| `-O0` | Disable compiler optimizations |
| `-o` | Specify the output executable |

### 3. Start GDB

```bash
gdb /tmp/lab3_buggy
```

### 4. Debugging Commands

```gdb
break process_data
run
print len
print *data@5
next
backtrace
```

| Command | Function |
|---|---|
| `break` | Set a breakpoint |
| `run` | Start program execution |
| `print` | Display a variable or expression |
| `next` | Execute the next source line |
| `step` | Step into a function |
| `backtrace` | Display the function call stack |
| `continue` | Resume execution |
| `quit` | Exit GDB |

### 5. Identified Programming Errors

The application contains intentional programming errors.

| Function | Error | Explanation |
|---|---|---|
| `process_data` | Array out-of-bounds | The loop condition allows access beyond the array boundary |
| `create_message` | Use-after-free | A pointer is returned after its allocated memory has been released |
| `calculate` | Integer overflow | Multiplication may exceed the representable range of a signed integer |

For example:

```c
for (int i = 0; i <= len; i++)
```

The condition `i <= len` is incorrect when `len` represents the number of array elements.

The corrected condition is:

```c
for (int i = 0; i < len; i++)
```

### 6. Results and Observations

GDB allows developers to inspect variables, identify invalid memory access, and trace the execution path leading to a failure.

However, undefined behavior does not always cause an immediate crash. Therefore, debugging conclusions should be based on source-code analysis and runtime evidence.

**Evidence:**

[Saved GDB Session](logs/gdb_session.txt)

---

## Memory Analysis with Valgrind

### 1. Objective

Valgrind Memcheck is used to detect memory-management problems in C applications.

Common problems include:

- Memory leaks.
- Invalid memory reads.
- Invalid memory writes.
- Use-after-free errors.
- Incorrect memory deallocation.
- Access to uninitialized memory.

### 2. Compile the Applications

Compile the original application:

```bash
gcc -g -O0 apps/leaky_app.c -o /tmp/lab3_leaky
```

Compile the corrected application:

```bash
gcc -g -O0 apps/leaky_app_fixed.c -o /tmp/lab3_fixed
```

### 3. Run Valgrind

Analyze the original application:

```bash
valgrind --leak-check=full \
    --show-leak-kinds=all \
    /tmp/lab3_leaky
```

Analyze the corrected application:

```bash
valgrind --leak-check=full \
    --show-leak-kinds=all \
    /tmp/lab3_fixed
```

### 4. Experimental Results

The recorded Valgrind results show the following differences.

| Metric | Original Application | Corrected Application |
|---|---:|---:|
| Total Allocations | 352 | 352 |
| Total Frees | 102 | 352 |
| Memory at Exit | 16,900 bytes | 0 bytes |
| Remaining Blocks | 250 | 0 |
| Error Summary | 3 errors | 0 errors |

### 5. Memory Leak Analysis

The original application contains three groups of definitely lost memory:

| Memory Group | Lost Memory |
|---|---:|
| Group 1 | 2,000 bytes |
| Group 2 | 2,100 bytes |
| Group 3 | 12,800 bytes |
| **Total** | **16,900 bytes** |

The corrected version properly releases dynamically allocated memory associated with:

- Record structures.
- Name buffers.
- Score arrays.
- Temporary message buffers.

### 6. Conclusion

The recorded Valgrind results demonstrate that the corrected application successfully releases its allocated memory during the tested execution.

The corrected application reports:

```text
in use at exit: 0 bytes in 0 blocks
ERROR SUMMARY: 0 errors from 0 contexts
```

This result confirms that Memcheck did not detect memory leaks or other reported errors during the recorded execution.

It does not guarantee that every possible execution path is free from memory-management defects.

### Evidence

- [Original Valgrind Report](logs/valgrind_report.txt)
- [Corrected Valgrind Report](logs/valgrind_fixed.txt)

---

## System Calls and CPU Profiling

### 1. Objective

System-call tracing and CPU profiling are used to understand application behavior and identify performance bottlenecks.

Two tools are used:

- **strace:** Monitors system calls made by an application.
- **perf:** Collects performance statistics and profiles CPU execution.

### 2. Compile the Test Application

```bash
gcc -g -O2 apps/perf_target.c -o /tmp/lab3_perf
```

### 3. System-Call Analysis with strace

Execute:

```bash
strace -c /tmp/lab3_perf
```

The `-c` option generates a summary of system-call statistics.

Typical information includes:

| Metric | Description |
|---|---|
| System Call | Name of the Linux system call |
| Calls | Number of invocations |
| Errors | Number of failed calls |
| Time | Time spent executing system calls |
| Percentage | Relative time spent in each system call |

To capture a detailed system-call trace:

```bash
strace -o /tmp/lab3_strace.log /tmp/lab3_perf
```

The output can be used to investigate file operations, memory allocation, process execution, and other kernel interactions.

### 4. CPU Profiling with perf

Collect performance statistics:

```bash
perf stat /tmp/lab3_perf
```

Record profiling samples:

```bash
perf record -g -o /tmp/lab3_perf.data /tmp/lab3_perf
```

Display the profiling report:

```bash
perf report -i /tmp/lab3_perf.data
```

### 5. Recorded Performance Results

The recorded experiment produced the following statistics.

| Metric | Recorded Value |
|---|---:|
| Execution Time | Approximately 0.02356 seconds |
| CPU Cycles | 104,712,852 |
| Instructions | 375,723,714 |

These results represent a specific execution under the recorded HOST environment.

Performance measurements may vary depending on:

- CPU frequency.
- Background processes.
- System load.
- Kernel configuration.
- Compiler optimization.
- Performance-monitoring permissions.

### 6. Analysis

`strace` provides information about how frequently an application interacts with the kernel.

`perf` provides information about CPU utilization, instruction execution, and potential performance bottlenecks.

Combining both tools helps distinguish system-call overhead from CPU-intensive computation.

### Evidence

- [strace Summary](logs/strace_summary.txt)
- [Full strace Log](logs/strace_full.txt)
- [perf Statistics](logs/perf_stat.txt)
- [perf Report](logs/perf_report.txt)

---

## Scheduling Latency

### 1. Objective

Scheduling latency represents the delay between the expected wake-up time of a thread and the time at which it actually resumes execution.

This metric is important for evaluating the responsiveness of real-time and embedded Linux systems.

### 2. Measurement Tool

The experiment uses `cyclictest`, a utility included in the Linux `rt-tests` package.

An example command is:

```bash
sudo cyclictest \
    --mlockall \
    --threads=4 \
    --priority=80 \
    --interval=1000 \
    --loops=10000
```

### 3. Command Options

| Option | Description |
|---|---|
| `--mlockall` | Lock memory to reduce paging-related delays |
| `--threads=4` | Create four measurement threads |
| `--priority=80` | Assign real-time scheduling priority |
| `--interval=1000` | Set the measurement interval to 1,000 microseconds |
| `--loops=10000` | Request 10,000 measurement cycles |

> The command above is an example for reproducing the experiment. The original configuration should be verified against the saved experimental logs.

### 4. Experimental Results

The final recorded latency values are:

| Thread | Minimum Latency (µs) | Average Latency (µs) | Maximum Latency (µs) |
|---|---:|---:|---:|
| 0 | 4 | 10 | 1187 |
| 1 | 4 | 11 | 1556 |
| 2 | 4 | 10 | 962 |
| 3 | 4 | 11 | 1045 |

The final recorded cycle counts differ slightly between threads.

The results therefore represent the actual values recorded in the experiment rather than assuming that all threads completed an identical number of iterations.

### 5. Scheduling Latency Histogram

![Scheduling Latency Histogram](logs/latency_histogram.png)

The histogram illustrates the distribution of measured scheduling latency.

### 6. Results Analysis

The measurements show relatively low average scheduling latency, with occasional larger delays.

The largest observed latency was:

**1,556 microseconds**

However, this value represents the maximum observed during the experiment, not a guaranteed worst-case scheduling bound.

Potential causes of scheduling delays include:

- Interrupt handling.
- Kernel scheduling activity.
- CPU contention.
- Background system processes.
- Hardware and virtualization overhead.

### 7. Real-Time Considerations

Real-time Linux performance depends on both scheduling policies and kernel configuration.

Assigning a high thread priority does not automatically guarantee deterministic execution.

Similarly, using a real-time scheduling priority does not prove that the kernel is configured with PREEMPT_RT.

Additional experiments under controlled workloads are required to evaluate real-time performance.

### Evidence

- [cyclictest Results](logs/cyclictest_standard.txt)
- [Histogram Data](logs/hist_standard.txt)

---

## Lessons Learned

This laboratory provided practical experience in debugging and performance analysis for Linux applications.

### 1. Debugging

GDB is useful for locating programming errors through breakpoints, variable inspection, and call-stack analysis.

Understanding memory access and program control flow is essential for identifying the causes of application failures.

### 2. Memory Management

Valgrind demonstrates the importance of correctly managing dynamically allocated memory.

Memory ownership must be considered for both dynamically allocated structures and their internal members.

### 3. System-Call Analysis

strace provides visibility into the interactions between user-space applications and the Linux kernel.

It is particularly useful for investigating system-call failures and understanding application behavior.

### 4. Performance Profiling

perf helps identify CPU-intensive operations and analyze execution efficiency.

Profiling results should always be interpreted in the context of the execution environment.

### 5. Scheduling Performance

cyclictest provides experimental measurements of thread wake-up latency.

Average latency, maximum observed latency, and scheduling jitter are important metrics when evaluating time-sensitive applications.

### 6. Practical Applications

The debugging and profiling techniques introduced in this laboratory are useful for developing and evaluating more complex embedded applications.

These techniques are also applied in [Assignment 2](../Asgn2/README.md), which involves multithreaded processing, synchronization, debugging, and performance analysis.

---

## Conclusion

Lab 03 demonstrates the importance of systematic debugging and performance evaluation in Embedded Linux development.

By combining **GDB, Valgrind, strace, perf, and cyclictest**, developers can investigate software defects, improve memory management, identify performance bottlenecks, and evaluate scheduling behavior.

The laboratory establishes a practical foundation for analyzing the reliability, efficiency, and responsiveness of Linux-based embedded applications.
