# Assignment 01 — Linux Device Drivers

## Thành viên nhóm thực hiện

1. Pham Tran Gia Long
2. Vo Hoang Minh Loc

Assignment mở rộng character device driver và xây dựng counter driver
trên Linux ARM trong QEMU.

## Task 1 — ioctl

Driver `/dev/asgn1` hỗ trợ xóa buffer/reset bộ đếm đọc ghi, trả thống kê,
chuyển chế độ kernel/echo và trả chuỗi phiên bản. Ứng dụng `asgn1_ctl`
gửi các lệnh `reset`, `stats`, `mode` và `version` từ userspace.

## Task 2 — Counter

Driver `/dev/counter` hỗ trợ đọc và cập nhật bộ đếm.
Thông tin hoạt động được kiểm tra qua `/proc/counter_info`.
Các kiểm tra gồm trạng thái ban đầu, đọc/ghi, từ chối đầu vào sai
và dọn device/procfs/sysfs sau khi unload.

## Source và bằng chứng

Source, header, Makefile, binary/module và logs được lấy từ gói nộp
`SE201095_PhamTranGiaLong_Asgn01.tar.gz` trên máy thực hiện.
Báo cáo PDF được giữ trong thư mục bài theo cấu trúc gói nộp.

Các phiên kiểm tra đã ghi lại kết quả Task1 và Task2. Khi đánh giá lại,
đối chiếu đầu ra test với log gốc thay vì chỉ dựa vào việc build thành công.

## Những điều rút ra

- Phân biệt biên dịch thành công với chạy đúng trên target ARM.
- Kiểm tra mã trả về khi trao đổi dữ liệu userspace/kernel.
- Đối chiếu bộ đếm với các thao tác đọc/ghi thực tế.
- Kiểm tra cleanup sau unload và giữ log để truy vết phiên thử.
