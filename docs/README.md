# Tài liệu Lab02 (bản markdown)

| File | Nguồn | Nội dung |
|---|---|---|
| [CQ_Lab02.md](CQ_Lab02.md) | [Lab2_OS_System_Call.docx](Lab2_OS_System_Call.docx) (bản gốc) | **Đề bài**: quy định nộp, 3 bài trace, sysinfo, ptree/pstree |
| [HuongDan_XV6.md](HuongDan_XV6.md) | `HƯỚNG DẪN THỰC HÀNH XV6.pdf` + `fix.md` | Cài WSL/xv6, fix QEMU, **mục 4: các bước thêm system call**, **mục 5: `struct proc` và trạng thái tiến trình**, fork |
| [xv6-book/](xv6-book/) | `book-xv6-riscv-rev4.pdf` | Sách xv6 tách theo chương (text thô, mất hình) |

## Chương sách liên quan tới Lab02

- [ch02-operating-system-organization.md](xv6-book/ch02-operating-system-organization.md): 2.2 user mode/supervisor mode và system call, 2.5 process overview (`struct proc`).
- [ch04-traps-and-system-calls.md](xv6-book/ch04-traps-and-system-calls.md): 4.3 đường đi của một system call (`ecall` → `syscall()`), **4.4 lấy đối số và chép dữ liệu user/kernel (`argint`, `argaddr`, `copyout`)**. Chương quan trọng nhất của lab này.
- [ch03-page-tables.md](xv6-book/ch03-page-tables.md): 3.5 physical memory allocator (`kalloc.c`, freelist) cho sysinfo; 3.7 `sbrk` (`p->sz`) cho ptree.
- [ch07-scheduling.md](xv6-book/ch07-scheduling.md): 7.8 wait/exit (`p->parent`, `wait_lock`) cho ptree.
- [ch06-locking.md](xv6-book/ch06-locking.md): đọc nếu chưa rõ khi nào cần `acquire`/`release`.
