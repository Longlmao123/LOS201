# LOS201 — Embedded Linux

## Thành viên nhóm thực hiện

1. Pham Tran Gia Long
2. Vo Hoang Minh Loc

Repository tổng hợp các bài lab và assignment về Linux ARM trong QEMU:
khởi động hệ thống, kernel driver, ứng dụng đa luồng, debugging và đo lường hiệu năng.

## Nội dung

| Bài | Chủ đề |
|---|---|
| [Lab 01](embedded_lab1/) | Môi trường Linux ARM, kernel và boot QEMU |
| [Lab 02](embedded_lab2/) | Linux character device driver |
| [Lab 03](embedded_lab3/) | Debugging, kiểm tra bộ nhớ và phân tích hiệu năng |
| [Assignment 01](Asgn1/) | ioctl và counter driver |
| [Assignment 02](Asgn2/) | Sensor Monitoring System với bốn worker thread |

## Mạch kiến thức

Lab1 tạo môi trường để boot Linux ARM. Lab2 thực hành giao tiếp với kernel
driver. Lab3 bổ sung kỹ năng tìm lỗi và đo hiệu năng. Hai assignment kết hợp
các phần này thành driver mở rộng và một hệ thống giám sát cảm biến mô phỏng.

README từng bài giải thích nội dung, cách sử dụng và những điểm rút ra.
Các báo cáo và log thực nghiệm nằm trong thư mục tương ứng.

## Môi trường assignment

- Ubuntu HOST; ARM GNU Toolchain 13.2.1.
- Linux target 5.15.0; QEMU vexpress-a9, Cortex-A9.
- C, Makefile, shell; pthread và POSIX message queues.
- GDB, Valgrind, strace và perf.

Các số đo được ghi cùng điều kiện chạy. Đặc biệt, phân biệt HOST/MOCK với
QEMU/DRIVER khi đọc kết quả ASM2. Bản ASM2 hiện tại chưa kèm video demo.
