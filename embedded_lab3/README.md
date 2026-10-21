# Lab 03 — Debugging và phân tích ứng dụng Linux

## Thành viên nhóm thực hiện

1. Pham Tran Gia Long
2. Vo Hoang Minh Loc

Lab sử dụng các chương trình C để thực hành tìm lỗi, kiểm tra bộ nhớ và
phân tích hiệu năng. Các thao tác chẩn đoán là nền tảng cho phần kiểm thử ASM2.

## Chương trình thực hành

| Source trong `apps/` | Vai trò |
|---|---|
| `buggy_app.c` | Ví dụ phục vụ tìm lỗi |
| `leaky_app.c` | Ví dụ phục vụ kiểm tra rò rỉ bộ nhớ |
| `leaky_app_fixed.c` | Phiên bản sửa của ví dụ |
| `leaky_app_measure.c` | Phiên bản phục vụ quan sát bộ nhớ |
| `perf_target.c` | Chương trình phục vụ phân tích hiệu năng |

Các binary đã build được giữ cùng source. Trước khi chạy, dùng `file apps/*`
để phân biệt chương trình HOST với ARM; không chạy trực tiếp binary ARM trên HOST x86_64.

## File thực nghiệm

- `logs/`: log và dữ liệu đọc được từ quá trình thực hành.
- `output/initramfs_lab3.cpio.gz`: initramfs đã tạo cho Lab3.
- `rootfs/`: các file cấu hình boot được tìm thấy, nếu có.
- `kernel_configs/`: cấu hình kernel được tìm thấy, nếu có.

Repo giữ source và kết quả cần thiết; toolchain và toàn bộ cây source kernel
được quản lý ở môi trường build riêng.

## Bài học rút ra

Phân biệt môi trường HOST và ARM; build có thông tin debug để dùng GDB;
đối chiếu lỗi với source; giữ log trước và sau sửa để giải thích kết quả.
Kết quả đo cần được đọc cùng điều kiện chạy, không suy từ tên file.
