# Hướng dẫn thực hành XV6

> Chuyển từ `HƯỚNG DẪN THỰC HÀNH XV6.pdf` (ThS. Lê Viết Long), giữ nguyên nội dung.

## 1. Cài đặt WSL trên Windows

**Bước 1: Kiểm tra phiên bản Windows hỗ trợ.** WSL yêu cầu:
- Windows 10 Version 2004 trở lên (Build 19041+)
- Windows 11 (tất cả phiên bản đều hỗ trợ)

Để kiểm tra: `Win + R` → gõ `winver` → Enter.

**Bước 2:** Mở PowerShell với quyền Administrator (chuột phải → Run as administrator).

**Bước 3: Cài đặt WSL bằng lệnh**

```powershell
wsl --install -d Ubuntu-22.04
```

Lệnh này tự động cài WSL, cài Ubuntu và kích hoạt các tính năng cần thiết. Sau khi cài xong, cần khởi động lại máy.

**Bước 4:** Mở Start Menu → gõ Ubuntu → chọn. Lần đầu sẽ yêu cầu tạo user và password.

**Bước 5:** Mở Terminal WSL (Ubuntu), chạy thử:

```bash
sudo apt update
sudo apt install git
```

## 2. Cài đặt XV6 trên WSL

**Bước 1:** Di chuyển đến ổ D và tạo thư mục chứa mã nguồn (WSL truy cập ổ Windows qua `/mnt`):

```bash
cd /mnt/d
mkdir HDH
cd HDH
```

Lúc này bạn đang ở `D:\HDH` của Windows (đường dẫn `/mnt/d/HDH`).

**Bước 2: Cài các công cụ cần thiết**

```bash
sudo apt-get update && sudo apt-get upgrade
sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

Tham khảo thêm: https://pdos.csail.mit.edu/6.1810/2024/tools.html

**Bước 3: Tải mã nguồn xv6**

```bash
git clone git://g.csail.mit.edu/xv6-labs-2024
cd xv6-labs-2024
```

**Bước 4:** Biên dịch: `make`
**Bước 5:** Chạy xv6 bằng QEMU: `make qemu`
**Bước 6:** Dọn dẹp (nếu cần): `make clean`

### Fix lỗi treo QEMU (từ `fix.md`)

```bash
qemu-system-riscv64 --version

sudo apt remove -y qemu-system-misc qemu-system-common qemu
sudo add-apt-repository ppa:canonical-server/server-backports
sudo apt update
sudo apt install -y qemu-system-misc
```

## 3. Viết chương trình trên XV6

### Một số syscall có sẵn

| Syscall | Mục đích |
|---|---|
| `fork()` | Tạo tiến trình con |
| `wait()` | Chờ tiến trình con kết thúc |
| `getpid()` | Lấy PID hiện tại |
| `sleep(t)` | Ngủ trong t clock ticks (~1 tick = 10ms) |
| `sbrk(n)` | Cấp phát thêm n bytes bộ nhớ |
| `exec(name, args)` | Chạy chương trình khác |
| `kill(pid)` | Kết thúc tiến trình theo PID |
| `read(fd, buf, n)` | Đọc từ file descriptor |
| `write(fd, buf, n)` | Ghi ra file descriptor |
| `open/close()` | Mở/đóng file |

*(Bảng trong PDF bị lệch 1 dòng giữa 2 cột; đây là bản đã căn lại.)*

### Ví dụ: chương trình `sleeptest`

**Bước 1:** Tạo `user/sleeptest.c`:

```c
#include "user/user.h"

int main() {
   printf("Sleeping for 100 ticks...\n");
   sleep(100); // ngủ khoảng 1 giây
   printf("Woke up!\n");
   exit(0);
}
```

**Bước 2:** Thêm vào Makefile, tại dòng `UPROGS=\`:

```make
UPROGS=\
	...
	$U/_wc\
	$U/_zombie\
	$U/_sleeptest\
```

**Bước 3:** `make clean` rồi `make qemu`.

**Bước 4:** Tại dấu nhắc `$` gõ `sleeptest`. Kết quả mong đợi:

```
Sleeping for 100 ticks...
Woke up!
```

### Quá trình biên dịch

1. **Biên dịch kernel:** `kernel/*.c`, `*.S` → `kernel/*.o`; dùng `riscv64-unknown-elf-ld` liên kết thành `kernel/kernel`.
2. **Biên dịch user programs:** mỗi `user/*.c` → `.o`; dùng `user/user.ld` link với thư viện user (`ulib.o`, `usys.o`, `umalloc.o`, `printf.o`...) → ELF, ví dụ `user/sleeptest.c` → `user/_sleeptest`.
3. **Tạo fs.img:** `mkfs/mkfs` tạo image chứa toàn bộ chương trình user (`_init`, `_sh`, `_sleeptest`...). fs.img được mount làm file system khi QEMU chạy.
4. **Khởi động QEMU:** giả lập máy RISC-V, nạp `kernel/kernel`, mount fs.img làm ổ chính. Kernel gọi `init`, `init` gọi `sh`.
5. **Thực thi:** gõ `sleeptest` → shell gọi `exec("sleeptest")` → kernel đọc file từ fs.img, nạp vào bộ nhớ và chạy.

```
user/sleeptest.c --(riscv64 gcc)--> .o --(user.ld + ulib.o, usys.o, printf.o...)--> user/_sleeptest
    --(mkfs)--> fs.img --> QEMU (kernel + fs)
```

## 4. Các bước thêm một system call mới vào XV6

Ví dụ syscall `hello(name)`:

1. Khai báo trong `user/user.h`:
   ```c
   int hello(char *name); // In ra "Hello, <name>"
   ```
2. Viết hàm xử lý trong `kernel/sysproc.c`:
   ```c
   uint64
   sys_hello(void) {
      char *name;
      if (argstr(0, &name) < 0)
          return -1;
      printf("Hello, %s\n", name);
      return 0;
   }
   ```
   *(Lưu ý: trong xv6-labs-2024 `argstr` nhận buffer: `char name[64]; if (argstr(0, name, sizeof(name)) < 0)` — đoạn mẫu trong PDF dùng chữ ký cũ.)*
3. Liên kết trong `kernel/syscall.c`:
   ```c
   extern uint64 sys_hello(void);
   static uint64 (*syscalls[])(void) = {
      ...
      [SYS_hello] = sys_hello,
   };
   ```
4. Số hiệu syscall trong `kernel/syscall.h`:
   ```c
   #define SYS_hello 22
   ```
5. Entry point trong `user/usys.pl`:
   ```perl
   entry("hello");
   ```
6. Chương trình test `user/testhello.c`:
   ```c
   #include "kernel/types.h"
   #include "user/user.h"

   int main(int argc, char *argv[]) {
      if (argc < 2) {
          printf("Usage: hello <name>\n");
          exit(1);
      }
      hello(argv[1]);
      exit(0);
   }
   ```
7. Thêm vào Makefile: `$U/_testhello\`

## 5. Khối quản lý tiến trình (`struct proc`)

`struct proc` là cấu trúc dữ liệu trung tâm đại diện cho một tiến trình; mỗi tiến trình là một phần tử trong mảng `proc[NPROC]`.

| Thành phần | Ý nghĩa |
|---|---|
| `int pid` | Mã định danh tiến trình |
| `enum procstate state` | Trạng thái (RUNNING, SLEEPING, ...) |
| `char name[16]` | Tên tiến trình (debug) |
| `void *chan` | Khác NULL → tiến trình đang `sleep()` chờ trên chan |
| `int killed` | Đã bị yêu cầu hủy |
| `int xstate` | Mã trạng thái trả về khi `exit()` |
| `struct proc *parent` | Tiến trình cha |
| `uint64 sz` | Kích thước bộ nhớ tiến trình (bytes) |
| `pagetable_t pagetable` | Bảng trang không gian user |
| `uint64 kstack` | Địa chỉ ảo kernel stack |
| `struct trapframe *trapframe` | Trạng thái thanh ghi khi trap |
| `struct context context` | Trạng thái để chuyển ngữ cảnh |
| `struct file *ofile[NOFILE]` | Các file đang mở |
| `struct inode *cwd` | Thư mục làm việc hiện tại |

### Trạng thái (`enum procstate`)

| Trạng thái | Mô tả |
|---|---|
| UNUSED | Slot chưa được sử dụng |
| USED | Slot đã cấp phát nhưng chưa chạy |
| SLEEPING | Đang chờ sự kiện |
| RUNNABLE | Sẵn sàng để scheduler chạy |
| RUNNING | Đang chạy trên CPU |
| ZOMBIE | Đã kết thúc, chờ cha `wait()` |

### Vòng đời

```
fork() → USED → RUNNABLE → RUNNING
                    ↖ SLEEPING ↙
                             RUNNING → ZOMBIE → UNUSED
```

Tiến trình bắt đầu từ USED, được scheduler chọn chạy (RUNNING), có thể sleep (SLEEPING) rồi quay lại RUNNABLE, cuối cùng thành ZOMBIE khi `exit()`.

### Đồng bộ hoá

- `p->lock` dùng khi truy cập hầu hết các trường (state, chan, killed).
- `wait_lock` dùng riêng để bảo vệ `parent`, đảm bảo an toàn khi cha–con tương tác (`exit()`/`wait()`).

### Sleep–wakeup và `chan`

- `sleep(chan, lock)` → đưa tiến trình vào SLEEPING, gán `p->chan = chan`.
- `wakeup(chan)` → đánh thức (RUNNABLE) mọi tiến trình đang SLEEPING trên đúng `chan`.
- `chan` là con trỏ tùy ý, chỉ để phân biệt lý do chờ.

### trapframe vs context

- `trapframe`: toàn bộ thanh ghi user khi trap vào kernel (syscall, ngắt...).
- `context`: các thanh ghi callee-saved, dùng khi `swtch()` chuyển tiến trình.

### Liên hệ với CPU

Mỗi CPU có `struct cpu`: `cpu->proc` là tiến trình đang chạy; `cpu->context` là trạng thái trước khi nhảy vào scheduler.

## 6. Fork trong XV6

`fork()` tạo tiến trình con là bản sao của cha (bao gồm cả bảng mô tả tệp); con bắt đầu chạy ngay sau lệnh `fork()`. Cha và con có các file descriptor giống nhau nhưng thao tác độc lập. Thứ tự chạy cha/con phụ thuộc scheduler.

Giá trị trả về:
- Trong **con**: `fork()` trả về `0`.
- Trong **cha**: trả về **PID của con** (số dương).

## 7. Pipe trong XV6

`pipe(fd)` tạo kênh giao tiếp gồm 2 file descriptor:
- `fd[0]`: đầu **đọc**
- `fd[1]`: đầu **ghi**

Đặc điểm:
- Dữ liệu ghi vào `fd[1]` được lưu trong buffer kernel, đọc ra từ `fd[0]` thì bị xoá khỏi pipe.
- Pipe đơn hướng, không tên, tồn tại tạm thời; không còn ai dùng thì kernel giải phóng.

Các bước sử dụng:
1. `int fd[2]; pipe(fd);`
2. `fork()` — cả cha và con kế thừa fd của pipe.
3. **Đóng đầu không dùng:** tiến trình chỉ ghi đóng `fd[0]`; chỉ đọc đóng `fd[1]`.
4. Ghi bằng `write(fd[1], ...)`, đọc bằng `read(fd[0], ...)`.

Ví dụ:

```c
#include "kernel/types.h"
#include "user/user.h"

int main() {
    int fd[2];          // Mảng lưu 2 file descriptors
    char buffer[20];

    pipe(fd);           // Tạo pipe

    if (fork() == 0) {
        // Tiến trình con
        close(fd[1]);   // Đóng đầu ghi vì con chỉ đọc
        read(fd[0], buffer, sizeof(buffer));
        printf("Child received: %s\n", buffer);
        close(fd[0]);
    } else {
        // Tiến trình cha
        close(fd[0]);   // Đóng đầu đọc vì cha chỉ ghi
        write(fd[1], "Hello from parent", 18);
        close(fd[1]);
    }
    exit(0);
}
```

Lưu ý:
- **Đồng bộ:** không có dữ liệu thì `read` bị block; buffer đầy thì `write` bị block.
- **Đóng pipe:** khi một đầu bị đóng hết, `read` trả về 0 (EOF) / `write` trả lỗi.

## 8. Bảng trang trong XV6 (Sv39)

Mỗi tiến trình (kể cả kernel) có bảng trang riêng ánh xạ địa chỉ ảo → vật lý, dựa trên Sv39 của RISC-V.

| Bit | Vai trò |
|---|---|
| 63–39 | Bỏ qua (phải là 0) |
| 38–30 | Chỉ số cấp 2 |
| 29–21 | Chỉ số cấp 1 |
| 20–12 | Chỉ số cấp 0 |
| 11–0 | Offset trong trang 4KB |

- Mỗi bảng trang có 512 mục (512 × 8 byte = 4KB); tra 1 địa chỉ cần tối đa 3 lần truy cập bảng trang.
- Không có ánh xạ → page fault. OS quản lý bằng cách tạo/sửa PTE. Bảng trang của tiến trình lưu trong `proc->pagetable`.

| File | Vai trò |
|---|---|
| `vm.c` | Quản lý bảng trang (tạo, ánh xạ, hủy, cấp phát, copy) |
| `defs.h` | Khai báo hàm quản lý bộ nhớ |
| `riscv.h` | Bit trong PTE, macro Sv39 |
| `memlayout.h` | Sơ đồ vùng nhớ ảo kernel |
| `proc.h` | `pagetable_t pagetable` của mỗi tiến trình |
| `trampoline.S` | Mã chuyển user ↔ kernel |
| `kernel.ld` | Vị trí trampoline, etext... |

| Hàm (vm.c) | Chức năng |
|---|---|
| `kvmmake()` | Tạo bảng trang kernel |
| `uvmcreate()` | Tạo bảng trang trống cho tiến trình user |
| `walk()` | Duyệt 3 cấp để tìm (hoặc tạo) PTE |
| `mappages()` | Ánh xạ va → pa với quyền truy cập |
| `walkaddr()` | Trả địa chỉ vật lý nếu va được ánh xạ |
| `uvmalloc()` / `uvmdealloc()` | Cấp phát / thu hồi bộ nhớ tiến trình |
| `uvmcopy()` | Sao chép bộ nhớ cha → con |
| `uvmfree()` | Giải phóng toàn bộ bộ nhớ tiến trình |
| `copyin()` / `copyout()` / `copyinstr()` | Chuyển dữ liệu user ↔ kernel |

Vai trò: cách ly bộ nhớ giữa các tiến trình, bảo vệ kernel, hỗ trợ cấp phát động, chuyển không gian địa chỉ khi context switch.
