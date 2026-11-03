#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/ptree.h"
#include "user/user.h"

static struct ptreeinfo buf[NPROC];

void
test_invalid_args(void)
{
  if (ptree(buf, 0) != -1) {
    printf("FAIL: ptree(buf, 0) should return -1\n");
    exit(1);
  }

  if (ptree(buf, -1) != -1) {
    printf("FAIL: ptree(buf, -1) should return -1\n");
    exit(1);
  }

  if (ptree(buf, -100) != -1) {
    printf("FAIL: ptree(buf, -100) should return -1\n");
    exit(1);
  }

  // Invalid user pointer (high unmapped address)
  if (ptree((struct ptreeinfo *)0xeaeb0b5b00002f5e, 10) != -1) {
    printf("FAIL: ptree with invalid bad pointer should return -1\n");
    exit(1);
  }

  // NULL or 0 pointer (read-only / unmapped text page in xv6)
  if (ptree((struct ptreeinfo *)0, 10) != -1) {
    printf("FAIL: ptree((struct ptreeinfo *)0, 10) should return -1\n");
    exit(1);
  }

  printf("test invalid args: OK\n");
}

void
test_basic(void)
{
  int n = ptree(buf, NPROC);
  if (n <= 0 || n > NPROC) {
    printf("FAIL: ptree returned %d (expected 1..%d)\n", n, NPROC);
    exit(1);
  }

  int mypid = getpid();
  int found_self = 0;
  int found_init = 0;

  for (int i = 0; i < n; i++) {
    if (buf[i].pid <= 0) {
      printf("FAIL: invalid pid %d at index %d\n", buf[i].pid, i);
      exit(1);
    }
    if (buf[i].state <= 0 || buf[i].state > 5) {
      printf("FAIL: invalid state %d for pid %d\n", buf[i].state, buf[i].pid);
      exit(1);
    }
    if (buf[i].memsize == 0) {
      printf("FAIL: memsize is 0 for pid %d\n", buf[i].pid);
      exit(1);
    }
    if (buf[i].pid == mypid) {
      found_self = 1;
      if (strcmp(buf[i].name, "ptreetest") != 0) {
        printf("FAIL: current process name is %s, expected ptreetest\n", buf[i].name);
        exit(1);
      }
    }
    if (buf[i].pid == 1) {
      found_init = 1;
      if (buf[i].ppid != 0) {
        printf("FAIL: init ppid is %d, expected 0\n", buf[i].ppid);
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
test_max_limit(void)
{
  struct ptreeinfo full_buf[NPROC];
  struct ptreeinfo small_buf[2];

  int total = ptree(full_buf, NPROC);
  if (total <= 0) {
    printf("FAIL: ptree failed\n");
    exit(1);
  }

  // Test max = 1
  int n1 = ptree(small_buf, 1);
  if (n1 != 1) {
    printf("FAIL: ptree(small_buf, 1) returned %d, expected 1\n", n1);
    exit(1);
  }
  if (small_buf[0].pid != full_buf[0].pid) {
    printf("FAIL: first element mismatch with max=1\n");
    exit(1);
  }

  // Test max = 2 if total >= 2
  if (total >= 2) {
    int n2 = ptree(small_buf, 2);
    if (n2 != 2) {
      printf("FAIL: ptree(small_buf, 2) returned %d, expected 2\n", n2);
      exit(1);
    }
    if (small_buf[0].pid != full_buf[0].pid || small_buf[1].pid != full_buf[1].pid) {
      printf("FAIL: elements mismatch with max=2\n");
      exit(1);
    }
  }

  printf("test max limit: OK\n");
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

  struct ptreeinfo test_buf[NPROC];
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

int
main(int argc, char *argv[])
{
  printf("ptreetest: start\n");
  test_invalid_args();
  test_basic();
  test_max_limit();
  test_multilevel_tree();
  printf("ptreetest: OK\n");
  exit(0);
}
