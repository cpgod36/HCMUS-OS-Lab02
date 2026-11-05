#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/riscv.h"
#include "kernel/ptree.h"
#include "user/user.h"

// Keep all test buffers static to preserve the 4 KB user stack
static struct ptreeinfo full_buf[NPROC];
static struct ptreeinfo small_buf[4];

// Index of pid in the first n entries of full_buf, or -1.
static int
find_pid(int pid, int n)
{
  for (int i = 0; i < n; i++) {
    if (full_buf[i].pid == pid)
      return i;
  }
  return -1;
}

// With LAB_SYSCALL neither kalloc nor exec clears memory, so a page handed out next
// keeps its old content. Leave non-zero pages on the free list: the next kalloc page
// and the next exec'd BSS then expose anything that assumes zeroed memory.
static void
dirty_free_pages(void)
{
  int size = 64 * PGSIZE;
  char *p = sbrk(size);
  if (p == (char *)-1) {
    printf("FAIL: sbrk failed\n");
    exit(1);
  }
  memset(p, 0x5a, size);
  sbrk(-size);
}

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

  // Invalid unmapped user pointer
  if (ptree((struct ptreeinfo *)0xeaeb0b5b00002f5e, 10) != -1) {
    printf("FAIL: ptree with invalid bad pointer should return -1\n");
    exit(1);
  }

  // Null pointer
  if (ptree((struct ptreeinfo *)0, 10) != -1) {
    printf("FAIL: ptree((struct ptreeinfo *)0, 10) should return -1\n");
    exit(1);
  }

  printf("test invalid args: OK\n");
}

// A buffer that starts valid but runs into unmapped memory must fail.
// copyout may already have written the mapped part, so only the return value is checked.
void
test_cross_page(void)
{
  // A fresh page: the top of the heap would otherwise be this program's stack page.
  char *page = sbrk(PGSIZE);
  if (page == (char *)-1) {
    printf("FAIL: sbrk failed\n");
    exit(1);
  }
  struct ptreeinfo *last = (struct ptreeinfo *)(page + PGSIZE - sizeof(struct ptreeinfo));

  // Exactly one entry fits before the end of the heap.
  if (ptree(last, 1) != 1) {
    printf("FAIL: ptree with last slot and max=1 should return 1\n");
    exit(1);
  }

  // init, sh and this program are alive, so more than one entry is written.
  if (ptree(last, NPROC) != -1) {
    printf("FAIL: ptree crossing into an unmapped page should return -1\n");
    exit(1);
  }

  sbrk(-PGSIZE);
  printf("test cross page: OK\n");
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
  dirty_free_pages();

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

// ptree(small_buf, max) must write exactly max entries and leave the next one untouched.
static void
check_limit(int max)
{
  memset(small_buf, 0x5a, sizeof(small_buf));

  int n = ptree(small_buf, max);
  if (n != max) {
    printf("FAIL: ptree(small_buf, %d) returned %d\n", max, n);
    exit(1);
  }

  for (int i = 0; i < max; i++) {
    if (small_buf[i].pid != full_buf[i].pid) {
      printf("FAIL: element %d mismatch with max=%d\n", i, max);
      exit(1);
    }
  }

  unsigned char *sentinel = (unsigned char *)&small_buf[max];
  for (int i = 0; i < sizeof(struct ptreeinfo); i++) {
    if (sentinel[i] != 0x5a) {
      printf("FAIL: kernel overwrote past max=%d (byte %d = 0x%x)\n", max, i, sentinel[i]);
      exit(1);
    }
  }
}

void
test_max_limit(void)
{
  int total = ptree(full_buf, NPROC);
  if (total <= 0) {
    printf("FAIL: ptree failed\n");
    exit(1);
  }

  // small_buf has room for a sentinel entry after each of these.
  for (int max = 1; max <= 3 && max <= total; max++)
    check_limit(max);

  printf("test max limit & sentinel: OK\n");
}

void
test_multilevel_tree(void)
{
  int sync_pipe[2];
  int info_pipe[2];
  int st;

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
    close(info_pipe[1]);
    read(sync_pipe[0], &ch, 1);
    close(sync_pipe[0]);
    wait(&st);
    exit(st);
  }

  // Parent process
  close(sync_pipe[0]);
  close(info_pipe[1]);

  int recv_child_pid = 0, recv_gc_pid = 0;
  if (read(info_pipe[0], &recv_child_pid, sizeof(recv_child_pid)) != sizeof(recv_child_pid) ||
      read(info_pipe[0], &recv_gc_pid, sizeof(recv_gc_pid)) != sizeof(recv_gc_pid)) {
    printf("FAIL: did not receive child and grandchild pids\n");
    exit(1);
  }
  close(info_pipe[0]);

  int n = ptree(full_buf, NPROC);
  if (n <= 0) {
    printf("FAIL: ptree failed in multilevel test\n");
    exit(1);
  }

  int child = find_pid(recv_child_pid, n);
  int gc = find_pid(recv_gc_pid, n);
  if (child < 0) {
    printf("FAIL: child process not found in ptree\n");
    exit(1);
  }
  if (gc < 0) {
    printf("FAIL: grandchild process not found in ptree\n");
    exit(1);
  }
  if (full_buf[child].ppid != parent_pid) {
    printf("FAIL: child ppid is %d, expected %d\n", full_buf[child].ppid, parent_pid);
    exit(1);
  }
  if (full_buf[gc].ppid != recv_child_pid) {
    printf("FAIL: grandchild ppid is %d, expected %d\n", full_buf[gc].ppid, recv_child_pid);
    exit(1);
  }

  // Release child and grandchild
  write(sync_pipe[1], "xx", 2);
  close(sync_pipe[1]);
  wait(&st);
  if (st != 0) {
    printf("FAIL: multilevel child exited with status %d\n", st);
    exit(1);
  }

  printf("test multilevel tree: OK\n");
}

// Test a deep chain of processes (overflows the 1-page user stack if pstree recurses).
// NPROC is 64: this leaves room for init, sh, this program and pstree.
#define DEEP_LEVELS 58

// Run pstree with its stdout captured; returns its exit status, or -1 if it could not start.
// Counts the lines it printed and the deepest indentation among them.
static int
run_pstree(int *lines, int *maxindent)
{
  char *argv[] = {"pstree", 0};
  int out[2];
  int st;

  if (pipe(out) < 0)
    return -1;

  int pid = fork();
  if (pid < 0)
    return -1;
  if (pid == 0) {
    close(1);
    dup(out[1]);
    close(out[0]);
    close(out[1]);
    exec("pstree", argv);
    exit(1);
  }
  close(out[1]);

  char c;
  int indent = 0, in_indent = 1;
  *lines = 0;
  *maxindent = 0;
  while (read(out[0], &c, 1) == 1) {
    if (c == '\n') {
      (*lines)++;
      indent = 0;
      in_indent = 1;
    } else if (in_indent && c == ' ') {
      indent++;
    } else {
      in_indent = 0;
      if (indent > *maxindent)
        *maxindent = indent;
    }
  }
  close(out[0]);

  if (wait(&st) != pid)
    return -1;
  return st;
}

void
test_deep_chain(void)
{
  int sync_pipe[2];
  int ready_pipe[2];
  int st;

  if (pipe(sync_pipe) < 0 || pipe(ready_pipe) < 0) {
    printf("FAIL: deep chain pipe creation failed\n");
    exit(1);
  }

  int top_pid = fork();
  if (top_pid < 0) {
    printf("FAIL: deep chain initial fork failed\n");
    exit(1);
  }

  if (top_pid == 0) {
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
        // Close ready_pipe first: if a descendant fails, the top parent must see EOF
        char ch;
        close(ready_pipe[1]);
        read(sync_pipe[0], &ch, 1);
        close(sync_pipe[0]);
        wait(&st);
        exit(st);
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
  close(sync_pipe[0]);

  char ok;
  int ready = read(ready_pipe[0], &ok, 1);
  close(ready_pipe[0]);
  if (ready != 1) {
    printf("FAIL: deep chain was not built\n");
    close(sync_pipe[1]);
    wait(0);
    exit(1);
  }

  // Verify the ppid links: exactly one child at each level, DEEP_LEVELS in total.
  int n = ptree(full_buf, NPROC);
  if (n < DEEP_LEVELS) {
    printf("FAIL: ptree returned %d, expected >= %d\n", n, DEEP_LEVELS);
    exit(1);
  }
  int cur = top_pid;
  for (int lvl = 1; lvl < DEEP_LEVELS; lvl++) {
    int next = -1;
    for (int i = 0; i < n; i++) {
      if (full_buf[i].ppid == cur) {
        if (next != -1) {
          printf("FAIL: pid %d has more than one child in the chain\n", cur);
          exit(1);
        }
        next = full_buf[i].pid;
      }
    }
    if (next == -1) {
      printf("FAIL: chain ends at level %d, expected %d\n", lvl, DEEP_LEVELS);
      exit(1);
    }
    cur = next;
  }

  // How deep this program itself sits in the tree (init is 0).
  int mydepth = 0;
  for (int i = find_pid(getpid(), n); i >= 0 && full_buf[i].ppid != 0; i = find_pid(full_buf[i].ppid, n))
    mydepth++;

  // pstree must survive the deep tree and print all of it: the n processes above plus itself.
  int lines, maxindent;
  dirty_free_pages();
  st = run_pstree(&lines, &maxindent);
  if (st != 0) {
    printf("FAIL: pstree exited with status %d on a deep tree\n", st);
    exit(1);
  }
  if (lines != n + 1) {
    printf("FAIL: pstree printed %d lines, expected %d\n", lines, n + 1);
    exit(1);
  }
  if (maxindent != 2 * (mydepth + DEEP_LEVELS)) {
    printf("FAIL: pstree deepest indent is %d, expected %d\n", maxindent, 2 * (mydepth + DEEP_LEVELS));
    exit(1);
  }

  // Release and wait for deep chain
  close(sync_pipe[1]);
  wait(&st);
  if (st != 0) {
    printf("FAIL: deep chain exited with status %d\n", st);
    exit(1);
  }

  printf("test deep chain (%d levels): OK\n", DEEP_LEVELS);
}

int
main(int argc, char *argv[])
{
  printf("ptreetest: start\n");
  test_invalid_args();
  test_cross_page();
  test_basic();
  test_zeroed_memory();
  test_max_limit();
  test_multilevel_tree();
  test_deep_chain();
  printf("ptreetest: OK\n");
  exit(0);
}
