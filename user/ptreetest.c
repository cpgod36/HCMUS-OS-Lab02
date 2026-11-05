#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/ptree.h"
#include "user/user.h"

// Toàn bộ buffer test dùng static để bảo toàn user stack (4 KB)
static struct ptreeinfo full_buf[NPROC];
static struct ptreeinfo small_buf[4];
static struct ptreeinfo test_buf[NPROC];
static struct ptreeinfo deep_buf[NPROC];

void
test_invalid_args(void)
{
  if (ptree(full_buf, 0) != -1) {
    printf("FAIL: ptree(buf, 0) should return -1\n");
    exit(1);
  }

  if (ptree(full_buf, -1) != -1) {
    printf("FAIL: ptree(buf, -1) should return -1\n");
    exit(1);
  }

  if (ptree(full_buf, -100) != -1) {
    printf("FAIL: ptree(buf, -100) should return -1\n");
    exit(1);
  }

  // Địa chỉ user unmapped không hợp lệ
  if (ptree((struct ptreeinfo *)0xeaeb0b5b00002f5e, 10) != -1) {
    printf("FAIL: ptree with invalid bad pointer should return -1\n");
    exit(1);
  }

  // Con trỏ 0 (vùng nhớ text chỉ đọc trong xv6)
  if (ptree((struct ptreeinfo *)0, 10) != -1) {
    printf("FAIL: ptree((struct ptreeinfo *)0, 10) should return -1\n");
    exit(1);
  }

  printf("test invalid args: OK\n");
}

void
test_basic(void)
{
  int n = ptree(full_buf, NPROC);
  if (n <= 0 || n > NPROC) {
    printf("FAIL: ptree returned %d (expected 1..%d)\n", n, NPROC);
    exit(1);
  }

  int mypid = getpid();
  int found_self = 0;
  int found_init = 0;

  for (int i = 0; i < n; i++) {
    if (full_buf[i].pid <= 0) {
      printf("FAIL: invalid pid %d at index %d\n", full_buf[i].pid, i);
      exit(1);
    }
    if (full_buf[i].state <= 0 || full_buf[i].state > 5) {
      printf("FAIL: invalid state %d for pid %d\n", full_buf[i].state, full_buf[i].pid);
      exit(1);
    }
    if (full_buf[i].memsize == 0) {
      printf("FAIL: memsize is 0 for pid %d\n", full_buf[i].pid);
      exit(1);
    }
    if (full_buf[i].pid == mypid) {
      found_self = 1;
      if (strcmp(full_buf[i].name, "ptreetest") != 0) {
        printf("FAIL: current process name is %s, expected ptreetest\n", full_buf[i].name);
        exit(1);
      }
    }
    if (full_buf[i].pid == 1) {
      found_init = 1;
      if (full_buf[i].ppid != 0) {
        printf("FAIL: init ppid is %d, expected 0\n", full_buf[i].ppid);
        exit(1);
      }
    }
  }

  if (!found_self) {
    printf("FAIL: self process (pid %d) not found in ptree\n", mypid);
    exit(1);
  }
  if (!found_init) {
    printf("FAIL: init process (pid 1) not found in ptree\n");
    exit(1);
  }

  printf("test basic: OK\n");
}

void
test_zeroed_memory(void)
{
  int n = ptree(full_buf, NPROC);
  if (n <= 0) {
    printf("FAIL: ptree failed\n");
    exit(1);
  }

  for (int i = 0; i < n; i++) {
    unsigned char *raw = (unsigned char *)&full_buf[i];

    // Check 4-byte padding between state (offset 8..11) and memsize (offset 16..23)
    // Offsets 12..15 must be 0 to avoid kernel info leak
    for (int b = 12; b < 16; b++) {
      if (raw[b] != 0) {
        printf("FAIL: padding byte %d of proc %d is not 0 (0x%x)\n",
               b, full_buf[i].pid, raw[b]);
        exit(1);
      }
    }

    // Check trailing bytes of name[16] after '\0' are zeroed
    int namelen = strlen(full_buf[i].name);
    for (int b = namelen; b < 16; b++) {
      if (full_buf[i].name[b] != 0) {
        printf("FAIL: trailing byte %d of name '%s' is not 0 (0x%x)\n",
               b, full_buf[i].name, (unsigned char)full_buf[i].name[b]);
        exit(1);
      }
    }
  }

  printf("test zeroed padding & name: OK\n");
}

void
test_max_limit(void)
{
  int total = ptree(full_buf, NPROC);
  if (total <= 0) {
    printf("FAIL: ptree failed\n");
    exit(1);
  }

  // Fill small_buf with sentinel byte 0x5a
  memset(small_buf, 0x5a, sizeof(small_buf));

  // Test max = 1: kernel should only write small_buf[0]; small_buf[1] must retain sentinel
  int n1 = ptree(small_buf, 1);
  if (n1 != 1) {
    printf("FAIL: ptree(small_buf, 1) returned %d, expected 1\n", n1);
    exit(1);
  }
  if (small_buf[0].pid != full_buf[0].pid) {
    printf("FAIL: first element mismatch with max=1\n");
    exit(1);
  }

  unsigned char *sentinel1 = (unsigned char *)&small_buf[1];
  for (int i = 0; i < sizeof(struct ptreeinfo); i++) {
    if (sentinel1[i] != 0x5a) {
      printf("FAIL: kernel overwrote past max=1 (byte %d = 0x%x)\n", i, sentinel1[i]);
      exit(1);
    }
  }

  // Test max = 2 if total >= 2
  if (total >= 2) {
    memset(small_buf, 0x5a, sizeof(small_buf));
    int n2 = ptree(small_buf, 2);
    if (n2 != 2) {
      printf("FAIL: ptree(small_buf, 2) returned %d, expected 2\n", n2);
      exit(1);
    }
    if (small_buf[0].pid != full_buf[0].pid || small_buf[1].pid != full_buf[1].pid) {
      printf("FAIL: elements mismatch with max=2\n");
      exit(1);
    }

    // small_buf[2] must retain sentinel value 0x5a
    unsigned char *sentinel2 = (unsigned char *)&small_buf[2];
    for (int i = 0; i < sizeof(struct ptreeinfo); i++) {
      if (sentinel2[i] != 0x5a) {
        printf("FAIL: kernel overwrote past max=2 (byte %d = 0x%x)\n", i, sentinel2[i]);
        exit(1);
      }
    }
  }

  printf("test max limit & sentinel: OK\n");
}

void
test_multilevel_tree(void)
{
  int sync_pipe[2];
  int info_pipe[2];

  if (pipe(sync_pipe) < 0 || pipe(info_pipe) < 0) {
    printf("FAIL: pipe creation failed\n");
    exit(1);
  }

  int parent_pid = getpid();
  int child_pid = fork();
  if (child_pid < 0) {
    printf("FAIL: fork child failed\n");
    exit(1);
  }

  if (child_pid == 0) {
    // Child process
    close(sync_pipe[1]);
    close(info_pipe[0]);

    int my_child_pid = getpid();
    write(info_pipe[1], &my_child_pid, sizeof(my_child_pid));

    int gc_pid = fork();
    if (gc_pid < 0) {
      printf("FAIL: fork grandchild failed\n");
      exit(1);
    }

    if (gc_pid == 0) {
      // Grandchild process
      int my_gc_pid = getpid();
      write(info_pipe[1], &my_gc_pid, sizeof(my_gc_pid));
      close(info_pipe[1]);

      char ch;
      read(sync_pipe[0], &ch, 1);
      close(sync_pipe[0]);
      exit(0);
    }

    // Child waits on sync_pipe then waits for grandchild
    char ch;
    read(sync_pipe[0], &ch, 1);
    close(sync_pipe[0]);
    close(info_pipe[1]);
    wait(0);
    exit(0);
  }

  // Parent process
  close(sync_pipe[0]);
  close(info_pipe[1]);

  int recv_child_pid = 0, recv_gc_pid = 0;
  read(info_pipe[0], &recv_child_pid, sizeof(recv_child_pid));
  read(info_pipe[0], &recv_gc_pid, sizeof(recv_gc_pid));
  close(info_pipe[0]);

  int n = ptree(test_buf, NPROC);
  if (n <= 0) {
    printf("FAIL: ptree failed in multilevel test\n");
    exit(1);
  }

  int found_child = 0, found_gc = 0;
  for (int i = 0; i < n; i++) {
    if (test_buf[i].pid == recv_child_pid) {
      found_child = 1;
      if (test_buf[i].ppid != parent_pid) {
        printf("FAIL: child ppid is %d, expected %d\n", test_buf[i].ppid, parent_pid);
        exit(1);
      }
    }
    if (test_buf[i].pid == recv_gc_pid) {
      found_gc = 1;
      if (test_buf[i].ppid != recv_child_pid) {
        printf("FAIL: grandchild ppid is %d, expected %d\n", test_buf[i].ppid, recv_child_pid);
        exit(1);
      }
    }
  }

  if (!found_child) {
    printf("FAIL: child process not found in ptree\n");
    exit(1);
  }
  if (!found_gc) {
    printf("FAIL: grandchild process not found in ptree\n");
    exit(1);
  }

  // Release child and grandchild
  write(sync_pipe[1], "xx", 2);
  close(sync_pipe[1]);
  wait(0);

  printf("test multilevel tree: OK\n");
}

// Test deep chain of 24 processes (previously overflowed 1-page user stack in recursive pstree)
#define DEEP_LEVELS 24

void
test_deep_chain(void)
{
  int sync_pipe[2];
  int ready_pipe[2];

  if (pipe(sync_pipe) < 0 || pipe(ready_pipe) < 0) {
    printf("FAIL: deep chain pipe creation failed\n");
    exit(1);
  }

  int fork_pid = fork();
  if (fork_pid < 0) {
    printf("FAIL: deep chain initial fork failed\n");
    exit(1);
  }

  if (fork_pid == 0) {
    // In descendant branch
    close(ready_pipe[0]);
    close(sync_pipe[1]);

    for (int lvl = 1; lvl < DEEP_LEVELS; lvl++) {
      int next = fork();
      if (next < 0) {
        printf("FAIL: fork failed at depth %d\n", lvl);
        exit(1);
      }
      if (next > 0) {
        // Wait for release signal from parent
        char ch;
        read(sync_pipe[0], &ch, 1);
        close(sync_pipe[0]);
        close(ready_pipe[1]);
        wait(0);
        exit(0);
      }
    }

    // Deepest child process (lvl = DEEP_LEVELS - 1)
    write(ready_pipe[1], "K", 1);
    close(ready_pipe[1]);

    char ch;
    read(sync_pipe[0], &ch, 1);
    close(sync_pipe[0]);
    exit(0);
  }

  // Parent process
  close(ready_pipe[1]);

  char ok;
  read(ready_pipe[0], &ok, 1);
  close(ready_pipe[0]);

  // Verify ptree with 24-level chain active
  int n = ptree(deep_buf, NPROC);
  if (n < DEEP_LEVELS) {
    printf("FAIL: ptree returned %d, expected >= %d\n", n, DEEP_LEVELS);
    exit(1);
  }

  // Release and wait for deep chain
  close(sync_pipe[1]);
  close(sync_pipe[0]);
  wait(0);

  printf("test deep chain (24 levels): OK\n");
}

int
main(int argc, char *argv[])
{
  printf("ptreetest: start\n");
  test_invalid_args();
  test_basic();
  test_zeroed_memory();
  test_max_limit();
  test_multilevel_tree();
  test_deep_chain();
  printf("ptreetest: OK\n");
  exit(0);
}
