# SETUP — Lab02 xv6 System Call (Hệ điều hành 24_22, HCMUS)

> Tài liệu cho thành viên nhóm **và AI agent của họ**. Đọc hết trước khi làm.
> Đề bài đầy đủ: [docs/CQ_Lab02.md](docs/CQ_Lab02.md). Hướng dẫn xv6: [docs/HuongDan_XV6.md](docs/HuongDan_XV6.md) (mục 4: thêm syscall, mục 5: `struct proc`). Sách xv6: [docs/xv6-book/](docs/xv6-book/) (chương cần đọc: xem [docs/README.md](docs/README.md)).

## 0. Nhóm và phân công

| Thành viên | MSSV | GitHub | Nhánh git | Bài phụ trách | Số hiệu syscall |
|---|---|---|---|---|---|
| Phan Chí Cao (**nhóm trưởng**) | 24120026 | `cpgod36` | `cao` | **trace** + gộp code, nộp bài, report | `SYS_trace 22` |
| Võ Phùng Nhật Long | 24120088 | `nhlcngdz` | `long` | **sysinfo** | `SYS_sysinfo 23` |
| Nguyễn Hoàng Nhật | 24120110 | `nh-996` | `nhat` | **ptree + pstree** | `SYS_ptree 24` |

- Repo: https://github.com/cpgod36/HCMUS-OS-Lab02 (**private**, không được public: đề phạt 0 điểm nếu bài giống nhóm khác).
- Nhánh chính: `syscall` (nhánh lab syscall của MIT). Tag `base` = commit gốc của MIT (dùng để tạo patch nộp bài).
- Issue: #1 trace (Cao), #2 sysinfo (Long), #3 ptree/pstree (Nhật).

**Khác LAB1:** lab này phải sửa `kernel/` và cả 3 bài đều đụng **cùng một nhóm file** (`syscall.h`, `syscall.c`, `usys.pl`, `user.h`, `defs.h`, `sysproc.c`, `proc.c`, `Makefile`). Mục 4.1 quy định chỗ thêm code để hạn chế conflict.

---

## 1. Cài môi trường (làm 1 lần)

**Đã làm ở LAB1 thì bỏ qua mục 1.1–1.4**, chỉ làm 1.5 và 1.6.

Mục tiêu: Ubuntu 22.04 trên WSL2, toolchain RISC-V, **QEMU ≥ 7** (QEMU 6.2 mặc định của Ubuntu 22.04 bị treo khi boot xv6-labs-2024).

### 1.1. Cài Ubuntu 22.04 (PowerShell trên Windows)

```powershell
wsl -l -v                         # xem đã có Ubuntu-22.04 chưa
wsl --install -d Ubuntu-22.04     # nếu chưa có; sau đó tạo username/password khi được hỏi
```

Mọi lệnh bên dưới chạy **trong Ubuntu** (`wsl -d Ubuntu-22.04`).

### 1.2. Toolchain

```bash
sudo apt-get update
sudo apt-get install -y git build-essential gdb-multiarch qemu-system-misc \
    gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu software-properties-common
```

### 1.3. Nâng cấp QEMU (bắt buộc trên Ubuntu 22.04)

```bash
qemu-system-riscv64 --version          # nếu là 6.x thì làm tiếp
sudo apt remove -y qemu-system-misc qemu-system-common
sudo add-apt-repository -y ppa:canonical-server/server-backports
sudo apt update
sudo apt install -y qemu-system-misc
qemu-system-riscv64 --version          # kỳ vọng: 9.x (máy nhóm trưởng: 9.0.2)
```

### 1.4. GitHub CLI + đăng nhập

Xem mục 1.4 trong `SETUP.md` của LAB1 (cài `gh`, `gh auth login` do người dùng tự làm, agent không làm thay).

### 1.5. Nhận lời mời và clone

1. Mở email hoặc https://github.com/cpgod36/HCMUS-OS-Lab02/invitations → **Accept**.
2. Clone **trong WSL, vào thư mục home** (build nhanh hơn `/mnt/c`, `/mnt/d` và tránh lỗi CRLF; **không** clone bằng Git for Windows):

```bash
cd ~
git clone https://github.com/cpgod36/HCMUS-OS-Lab02.git
cd HCMUS-OS-Lab02
git checkout -b long        # Nhật: nhat
```

### 1.6. Kiểm tra môi trường

```bash
make qemu
```

Kỳ vọng thấy `init: starting sh` và dấu nhắc `$`. **Thoát QEMU: `Ctrl+A` rồi bấm `x`.**

---

## 2. Quy trình làm việc với git

```bash
git checkout long                         # luôn làm trên nhánh của mình
git pull origin syscall                   # cập nhật thay đổi mới từ syscall (nên làm trước khi bắt đầu)
# ... code + test ...
git add kernel/kalloc.c kernel/proc.c ...
git commit -m "sysinfo: count free memory in kalloc.c"
git push -u origin long
gh pr create --base syscall --title "sysinfo" --body "..."   # khi xong bài
```

Quy tắc:
- **Không** push thẳng lên `syscall`, **không** `git push --force`, **không** merge PR (nhóm trưởng review và merge).
- Commit nhỏ, message rõ ràng. Lịch sử commit là bằng chứng đóng góp cá nhân (đề chấm theo đóng góp từng người).
- Không commit file build (`*.o`, `_*`, `fs.img`, `user/usys.S`... đã có trong `.gitignore`). Kiểm tra `git status` trước khi commit.
- **Thứ tự merge:** trace (#1) merge trước vì nó tạo mảng tên syscall trong `syscall.c` mà hai bài kia phải bổ sung. Long và Nhật cứ làm song song; khi trace đã vào `syscall`, chạy `git pull origin syscall`, giải quyết conflict (mục 4.1), rồi thêm tên syscall của mình vào mảng tên.

---

## 3. Build, chạy, test

| Lệnh | Tác dụng |
|---|---|
| `make qemu` | Build và boot xv6 (thoát: `Ctrl+A`, `x`) |
| `make clean` | Xoá file build. Chạy khi thấy code mới không có hiệu lực hoặc build báo lỗi lạ sau khi pull |
| `make grade` | Chấm tự động lab syscall của MIT |
| `make GRADEFLAGS=trace grade` | Chỉ chạy các test có tên chứa `trace` |

`make grade` **chỉ** có test cho **trace** (6 test). Các test `answers-syscall.txt`, `attack`, `time` là của MIT, **không thuộc đề của lớp → fail là bình thường**.
- **sysinfo**: grader của bản 2024 không có test, phải tự chạy `sysinfotest` trong QEMU, kỳ vọng `sysinfotest: OK`.
- **ptree/pstree**: bài của giảng viên, không có test có sẵn. Tự test thủ công (mục 5) và chụp màn hình cho report.

---

## 4. Ghi chú kỹ thuật xv6 (đọc kỹ, đặc biệt là agent)

### 4.1. Các bước thêm một syscall và chỗ đặt code

Mỗi syscall đi qua 5 chỗ (chi tiết: [HuongDan_XV6.md mục 4](docs/HuongDan_XV6.md)). Để 3 nhánh merge dễ, **luôn thêm vào cuối danh sách, theo đúng thứ tự trace → sysinfo → ptree**:

| File | Thêm gì | Vị trí |
|---|---|---|
| `kernel/syscall.h` | `#define SYS_trace 22` / `SYS_sysinfo 23` / `SYS_ptree 24` | sau `SYS_close 21` |
| `kernel/syscall.c` | `extern uint64 sys_xxx(void);` và `[SYS_xxx] sys_xxx,` | sau dòng của `sys_close` |
| `kernel/syscall.c` | tên trong mảng tên syscall (do trace tạo) | sau `"close"` |
| `user/usys.pl` | `entry("xxx");` | cuối file |
| `user/user.h` | nguyên mẫu hàm | sau `int uptime(void);` |
| `Makefile` | `$U/_trace\` / `$U/_sysinfotest\` / `$U/_pstree\` (+ chương trình test nếu có) | sau `$U/_zombie\` trong khối `UPROGS=\` đầu tiên (dòng bắt đầu bằng **TAB**) |
| `kernel/defs.h` | nguyên mẫu hàm kernel mới | dưới comment `// kalloc.c` hoặc `// proc.c` tương ứng |

Conflict khi pull: **giữ tất cả các dòng của cả hai bên**, sắp lại đúng thứ tự trace → sysinfo → ptree.

`user/usys.pl` sinh ra `user/usys.S` (stub dùng lệnh `ecall`), `syscall()` trong `kernel/syscall.c` đọc số hiệu từ `a7` rồi gọi hàm trong mảng `syscalls[]`, giá trị trả về ghi vào `a0`.

### 4.2. Lấy đối số và chép dữ liệu ra user space

- Bản 2024: `argint(int n, int *ip)` và `argaddr(int n, uint64 *ip)` trả về **`void`** (không kiểm tra `< 0` được như sách cũ). Phải tự kiểm tra giá trị đối số.
- Không được dereference con trỏ user trong kernel. Chép ra bằng `copyout(p->pagetable, dstva, (char *)&src, len)`; trả về `-1` nếu địa chỉ user không hợp lệ → syscall trả `-1`. Mẫu: `sys_fstat()` (`kernel/sysfile.c`) và `filestat()` (`kernel/file.c`).
- **Kernel stack chỉ có 1 trang (4096 byte)**: không khai báo mảng lớn trên stack trong kernel (ví dụ mảng `NPROC` phần tử `struct ptreeinfo` ≈ 2.5 KB là quá nguy hiểm).

### 4.3. Tiến trình (`kernel/proc.h`, `kernel/proc.c`)

- `proc[NPROC]` (`NPROC = 64`) là bảng tiến trình toàn cục trong `proc.c`; duyệt: `for(p = proc; p < &proc[NPROC]; p++)`.
- `enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE }` → giá trị số 0..5.
- **Khoá:** `state`, `pid`, `killed`... đọc khi giữ `p->lock`; `p->parent` đọc khi giữ `wait_lock` (biến `static` trong `proc.c`, nên hàm cần nó phải viết trong `proc.c`). Thứ tự khoá của xv6: `wait_lock` trước, `p->lock` sau. `p->sz`, `p->name` là dữ liệu riêng của tiến trình.
- Field mới thêm vào `struct proc` phải được khởi tạo trong `allocproc()`/`freeproc()` và (nếu cần kế thừa) chép trong `fork()`.
- `printf` trong kernel hỗ trợ `%d %ld %x %p %s`, không có width/padding.

### 4.4. Bộ nhớ vật lý (`kernel/kalloc.c`)

- Trang trống nằm trong danh sách liên kết `kmem.freelist` (mỗi nút là `struct run`), được bảo vệ bởi `kmem.lock`. Mỗi trang `PGSIZE` = 4096 byte.

### 4.5. Thư viện user — KHÔNG có libc

Như LAB1: chỉ có hàm trong `user/user.h`. `printf` chỉ hỗ trợ `%d %ld %lld %u %lu %x %lx %p %s %%`, **không có `%c`, không có width/padding** (thụt lề phải tự in khoảng trắng). In lỗi ra stderr bằng `fprintf(2, ...)`. Chương trình kết thúc bằng `exit(0)`, không `return` từ `main`.

---

## 5. Quy ước chung của nhóm

> ⚠️ **ĐỀ XUẤT, CHƯA CHỐT.** Cứ làm theo đề xuất; nếu muốn khác thì báo nhóm trưởng. Mọi quy ước phải ghi lại trong report.

| Bài | Vấn đề đề bài để mở | Đề xuất |
|---|---|---|
| trace | Format | Đúng output mẫu: `<pid>: syscall <tên> -> <giá trị trả về>` (in **sau** khi syscall chạy xong để có giá trị trả về). Grader so khớp format này |
| trace | Kế thừa mask | Chép trong `fork()`; `exec()` giữ nguyên mask; reset về 0 khi cấp phát/giải phóng proc |
| sysinfo | Đếm | `freemem` = số trang trong freelist × `PGSIZE` (giữ `kmem.lock`). `nproc` = số proc có `state != UNUSED` (giữ `p->lock` từng proc) |
| sysinfo | Lỗi | `copyout` thất bại → trả `-1` (`sysinfotest` có test con trỏ không hợp lệ) |
| ptree | `struct ptreeinfo` | Đúng như đề, đặt trong `kernel/ptree.h`, include ở cả kernel và user (`#include "kernel/ptree.h"`) |
| ptree | `ppid` của init | `0` (init không có cha) |
| ptree | `state`, `memsize` | `state` = giá trị số của `enum procstate`; `memsize` = `p->sz` |
| ptree | Đối số lỗi | `max <= 0` → `-1`; `copyout` thất bại (buf không hợp lệ) → `-1` |
| ptree | Số tiến trình > `max` | Ghi `max` tiến trình đầu tiên (theo thứ tự trong `proc[]`), trả về `max`, không coi là lỗi |
| ptree | Cách ghi | Đọc từng proc vào **một** biến `struct ptreeinfo` cục bộ (giữ khoá), nhả `p->lock` rồi `copyout` vào `buf + n*sizeof(struct ptreeinfo)` |
| pstree | Format | `<2 khoảng trắng × độ sâu><pid> <name> state=<n> mem=<bytes>`, đúng output mẫu |
| pstree | Gốc cây, thứ tự con | Gốc = tiến trình có `ppid` không xuất hiện trong danh sách (thường chỉ có init). Con in theo thứ tự xuất hiện trong mảng |
| pstree | Buffer | Gọi `ptree(buf, NPROC)` với mảng `NPROC` phần tử (`#include "kernel/param.h"`) |
| chung | Lỗi tham số chương trình user | In usage ra stderr (`fprintf(2, ...)`) và `exit(1)` |

**Test cho ptree (Nhật):** đề yêu cầu xử lý lỗi và chấm "mức độ kiểm tra kỹ lưỡng" → viết thêm `user/ptreetest.c` kiểm tra: `max <= 0`, buf không hợp lệ (ví dụ `(struct ptreeinfo *)0xeaeb0b5b00002f5e` như `sysinfotest`), `max` nhỏ hơn số tiến trình, và cây nhiều tầng (fork vài cấp rồi gọi `pstree`/`ptree`).

---

## 6. Khi hoàn thành bài — gửi cho nhóm trưởng

1. PR vào `syscall` từ nhánh của mình. Code build không warning, chạy đúng các output mẫu trong đề (và `make GRADEFLAGS=trace grade` vẫn pass sau khi merge).
2. Một đoạn ngắn (markdown, trong mô tả PR) cho report:
   - Ý tưởng giải pháp, đường đi từ user → kernel của syscall (không dán code, đề cấm đưa code vào report).
   - Các case đã test và kết quả, kèm **ảnh chụp màn hình** QEMU.
   - Khó khăn gặp phải, quy ước đã chọn.
3. **Hiểu code của mình.** Đề chấm cả "sự hiểu biết về mã nguồn", có thể bị hỏi trực tiếp.

---

## 7. Dành riêng cho AI agent

- Phạm vi: chỉ làm bài của người đang dùng bạn (mục 0). Được sửa các file chung ở mục 4.1 **chỉ để thêm dòng của syscall mình**, đúng vị trí quy định; không sửa/xoá dòng của syscall khác. Không sửa `SETUP.md`/`docs/` (báo nhóm trưởng nếu thấy sai).
- Đọc đúng phần đề của bài mình trong [docs/CQ_Lab02.md](docs/CQ_Lab02.md) và tuân theo mục 4 và 5 của file này.
- Sau mỗi thay đổi: `make qemu` phải build sạch (xv6 build với `-Werror`). Chạy thử output mẫu của đề trong QEMU.
- Git: chỉ commit/push lên nhánh của người dùng (`long` / `nhat`). Không push `syscall`, không force push, không merge, không đổi cài đặt repo.
- Không chép lời giải có sẵn trên mạng (đề phạt 0 điểm nếu bài giống nhau). Viết code dễ đọc, có comment ngắn ở chỗ khó, để thành viên giải thích được khi bị hỏi.
- Không cần làm `answers-syscall.txt`, `attack`/`secret` (phần "Attack xv6") của MIT.

## 8. Nộp bài (nhóm trưởng làm)

```bash
git checkout syscall && git pull
git diff base -- . ':(exclude)docs' ':(exclude)SETUP.md' > 24120026_24120088_24120110.patch
make clean
# zip: 24120026_24120088_24120110.zip gồm: 24120026_24120088_24120110_Report.pdf + .patch + source zip
```
