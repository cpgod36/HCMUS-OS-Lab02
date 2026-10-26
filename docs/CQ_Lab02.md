**HỆ ĐIỀU HÀNH**

**PROJECT**  **2** **–** **System call**

# Quy tắc chung:

- Project được thực hiện theo nhóm: **mỗi nhóm 3**  **sinh viên**

- **Tất cả các bài tập giống nhau sẽ được chấm 0 điểm trong toàn bộ quá trình thực hành (mặc dù các bài tập và dự án thực hành khác cũng được chấm điểm).**

- Môi trường cài đặt: **Linux**

# Nộp bài:

**Nộp bài tập trực tiếp trên trang web của khóa học (MOODLE), không chấp nhận bài nộp qua email hoặc các hình thức khác.**

Tên tệp: **StudentID1_StudentID2_StudentID3.zip** (với StudentID1 < StudentID2< StudentID3)

VD: Nhóm của bạn có 2 sinh viên: 2312001 và 2312002, tên tệp là: **2312001_2312002_2312003.zip**

**Bao gồm:**

- **StudentID1_StudentID2_StudentID3_Report.pdf:** Báo cáo nên ngắn gọn và súc tích. Không cố gắng đưa mã nguồn vào báo cáo. Mục đích của báo cáo là để bạn có cơ hội làm rõ giải pháp của mình, nêu ra những khó khăn gặp phải, và bổ sung thông tin hữu ích cho việc chấm điểm. Nếu bạn gặp vấn đề cụ thể, thử các phương pháp không thành công, hoặc có yêu cầu chưa được thực hiện đầy đủ, thì việc giải thích trong báo cáo có thể giúp chúng tôi đánh giá và chấm điểm một cách công bằng hơn.

- **Realease**  : Tệp diff (bản vá diff, Ví dụ: $ git diff >

< **StudentID1_StudentID2_StudentID3** .patch)

- **Source**  : Tệp Zip của xv6 (phiên bản được make clean)

**Lưu ý: Cần thực hiện đúng các yêu cầu trên, nếu không, bài làm sẽ không được chấm.**

# Đánh giá

Bài làm của nhóm sẽ được chấm điểm dựa trên tính hoàn thiện, tính chính xác, phong cách lập trình, mức độ kiểm tra kỹ lưỡng, giải pháp và sự hiểu biết về mã nguồn.

Khi quản lý khóa học này, chúng tôi cố gắng hết sức để đánh giá công bằng từng cá nhân dựa trên sự đóng góp của mỗi người cho project.

# Yêu cầu

Bài thực hành này sẽ giúp bạn tìm hiểu cách thêm mới một system call vào xv6

## T race

Trong bài tập này, bạn sẽ thêm một tính năng theo dõi lệnh gọi hệ thống có thể giúp bạn khi debug các bài lab sau. Bạn sẽ tạo một system call mới là trace  để điều khiển theo dõi. Lệnh này sẽ lấy một đối số, một số nguyên "mask", có các bit chỉ định các system call nào cần theo dõi. Ví dụ, để theo dõi lệnh gọi hệ thống fork, một chương trình sẽ gọi trace(1 << SYS_fork) , trong đó SYS_fork là mã số syscall được định nghĩa từ kernel/syscall.h . Bạn phải sửa đổi kernel xv6 để in ra mỗi dòng thông tin khi có system call bị theo dõi được gọi, (nghĩa là mã số system call được đặt trong mask) lúc này sẽ in ra thông tin gồm: tên system call được gọi và id của tiến trình gọi system call này, bạn không cần phải in các đối số của system call. System call  trace sẽ bật theo dõi cho tiến trình gọi nó và bất kỳ tiến trình con nào mà nó phân nhánh sau đó, nhưng không được ảnh hưởng đến các quy trình khác.

Viết một chương trình trace cấp độ người dùng chạy một chương trình khác với chức năng theo dõi được bật (xem user/trace.c ). Khi hoàn tất, bạn sẽ thấy đầu ra như thế này:

$ trace 32 grep hello README

3: syscall read -> 1023

3: syscall read -> 966

3: syscall read -> 70

3: syscall read -> 0

$

$ trace 2147483647 grep hello README

4: syscall trace -> 0

4: syscall exec -> 3

4: syscall open -> 3

4: syscall read -> 1023

4: syscall read -> 966

4: syscall read -> 70

4: syscall read -> 0

4: syscall close -> 0

$

$ grep hello README

$

$ trace 2 usertests forkforkfork

usertests starting

test forkforkfork: 407: syscall fork -> 408

408: syscall fork -> 409

409: syscall fork -> 410

410: syscall fork -> 411

409: syscall fork -> 412

410: syscall fork -> 413

409: syscall fork -> 414

411: syscall fork -> 415

...

$

Trong ví dụ đầu tiên ở trên, trace gọi lệnh grep theo dõi chỉ lệnh gọi hệ thống read. 32 là 1<<SYS_read . Trong ví dụ thứ hai, trace chạy lệnh grep trong khi theo dõi tất cả các lệnh gọi hệ thống; 2147483647 có tất cả 31 bit thấp được đặt. Trong ví dụ thứ ba, chương trình không được theo dõi, do đó không có đầu ra trace nào được in ra. Trong ví dụ thứ tư, các lệnh gọi hệ thống fork của tất cả các con cháu của bài kiểm tra forkforkfork trong usertests đang được theo dõi. Giải pháp của bạn là đúng nếu chương trình của bạn hoạt động như được hiển thị ở trên (mặc dù ID quy trình có thể khác).

Một số gợi ý:

- Thêm $U/_trace vào UPROGS trong Makefile

- Chạy **make qemu** và bạn sẽ thấy trình biên dịch không thể biên dịch user/trace.c , vì các stub không gian người dùng cho lệnh gọi hệ thống theo dõi vẫn chưa tồn tại: thêm một nguyên mẫu cho trace vào user/user.h , một stub vào user/usys.pl và một số syscall vào kernel/syscall.h . Makefile gọi tập lệnh perl user/usys.pl , tạo ra user/usys.S , các stub lệnh gọi hệ thống thực tế, sử dụng lệnh RISC-V ecall để chuyển sang kernel. Sau khi bạn sửa các sự cố biên dịch, hãy chạy **trace 32 grep hello README** ; nó sẽ không thành công vì bạn vẫn chưa triển khai lệnh gọi hệ thống trong kernel.

- Thêm hàm sys_trace() trong kernel/sysproc.c để triển khai lệnh gọi hệ thống mới bằng cách ghi nhớ đối số của lệnh trong một biến mới trong cấu trúc proc (xem kernel/proc.h ). Các hàm để truy xuất đối số lệnh gọi hệ thống từ không gian người dùng nằm trong kernel/syscall.c và bạn có thể xem ví dụ về cách sử dụng chúng trong kernel/sysproc.c . Thêm sys_trace mới của bạn vào mảng syscalls trong kernel/syscall.c .

- Sửa đổi fork() (xem kernel/proc.c ) để sao chép mặt nạ theo dõi từ tiến trình cha sang tiến trình con.

- Sửa đổi hàm syscall() trong kernel/syscall.c để in ra kết quả theo dõi. Bạn sẽ cần thêm một mảng tên syscall để lập chỉ mục vào.

## Sysinfo

Trong bài tập này, bạn sẽ thêm một system call mới là sysinfo , để thu thập thông tin về thông tin system đang chạy. Lệnh gọi hệ thống này lấy một đối số: là con trỏ struct sysinfo (xem kernel/sysinfo.h ). Kernel sẽ điền vào các trường của struct này: trường freemem sẽ được đặt thành số byte bộ nhớ trống và trường nproc sẽ được đặt thành số tiến trình có trạng thái không phải là UNUSED . Viết một chương trình thử nghiệm sysinfotest ; bạn sẽ vượt qua bài tập này nếu nó in ra "sysinfotest: OK".

Một số gợi ý:

- Thêm $U/_sysinfotest vào UPROGS trong Makefile

- Chạy **make qemu** ; user/sysinfotest.c sẽ không biên dịch được. Thêm system call sysinfo, thực hiện theo các bước tương tự như trong bài tập trước. Để khai báo nguyên mẫu cho sysinfo() trong user/user.h, bạn cần khai báo trước sự tồn tại của struct sysinfo :

	struct sysinfo;

    	int sysinfo(struct sysinfo *);

Sau khi khắc phục được sự cố biên dịch, hãy chạy **sysinfotest** ; lệnh này sẽ không thành công vì bạn chưa triển khai system call trong kernel.

- sysinfo cần sao chép struct sysinfo ra user space;  xem sys_fstat() ( kernel/sysfile.c ) và filestat() ( kernel/file.c ) để biết ví dụ về cách thực hiện việc đó bằng cách sử dụng copyout() .

- Để thu hồi bộ nhớ trống, hãy thêm một hàm vào kernel/kalloc.c

- Để thu hồi tiến trình, hãy thêm một hàm vào kernel/proc.c

### 3.  Process Tree

Trong bài tập này, bạn sẽ thêm một system call mới là ptree để thu thập thông tin các tiến trình đang tồn tại trong hệ thống, từ đó xây dựng một chương trình người dùng có tên pstree để hiển thị cây tiến trình theo quan hệ cha – con.

System call này nhận hai đối số: một con trỏ tới mảng cấu trúc ptreeinfo và một số nguyên max. Kernel sẽ duyệt bảng tiến trình và điền thông tin của tối đa max tiến trình vào mảng này. Mỗi phần tử trong mảng sẽ chứa các thông tin cơ bản của một tiến trình, bao gồm: pid, ppid, state, memsize và name.

Ví dụ, bạn có thể định nghĩa cấu trúc như sau (xem thêm trong kernel/ptree.h):

struct ptreeinfo {

    int pid;

    int ppid;

    int state;

    uint64 memsize;   // số byte bộ nhớ user của tiến trình

    char name[16];

};

System call có nguyên mẫu:

int ptree(struct ptreeinfo *buf, int max);

Giá trị trả về:

- Trả về **số phần tử thực tế**  đã ghi vào buf nếu thành công.

- Trả về -1 nếu đối số không hợp lệ hoặc có lỗi xảy ra.

Dựa trên dữ liệu do ptree trả về, hãy viết một chương trình người dùng tên là pstree để in cây tiến trình ra màn hình theo dạng thụt lề. Ví dụ, đầu ra có thể giống như sau:

$ pstree

1 init state=3 mem=4096

  2 sh state=3 mem=8192

    3 pstree state=4 mem=8192

    4 trace state=3 mem=4096

      5 grep state=3 mem=4096

Quan hệ cha – con phải được thể hiện đúng theo ppid. Chương trình của bạn được xem là đúng nếu có thể hiển thị hợp lý cấu trúc cây tiến trình hiện tại của hệ thống và in ra đúng các thông tin cơ bản của từng tiến trình, mặc dù giá trị pid, state và memsize thực tế có thể khác.

Một số gợi ý:

- Thêm $U/_pstree vào UPROGS trong Makefile.

- Thêm khai báo nguyên mẫu của ptree() vào user/user.h.

- Thêm stub cho ptree vào user/usys.pl và thêm số hiệu syscall vào kernel/syscall.h.

- Cài đặt hàm sys_ptree() trong kernel để sao chép thông tin tiến trình từ kernel sang user space.

- Bạn có thể tạo thêm tệp tiêu đề, ví dụ kernel/ptree.h, để định nghĩa struct ptreeinfo, sau đó include ở cả kernel và user space.

- Để lấy thông tin tiến trình, hãy duyệt bảng tiến trình trong kernel/proc.c.

- Chỉ cần lấy các tiến trình có trạng thái khác UNUSED.

- Trong chương trình pstree, sau khi nhận danh sách tiến trình từ ptree, hãy tự xây dựng quan hệ cha – con trong user space và in ra theo dạng cây.

- Không nên in trực tiếp cây tiến trình trong kernel; kernel chỉ nên trả dữ liệu, còn việc hiển thị nên thực hiện ở chương trình người dùng.

- Cần xử lý đúng các trường hợp lỗi như: buf không hợp lệ, max <= 0, hoặc số lượng tiến trình thực tế lớn hơn max.

# Tài liệu tham khảo

	https://pdos.csail.mit.edu/6.828/2020/labs/syscall.html
