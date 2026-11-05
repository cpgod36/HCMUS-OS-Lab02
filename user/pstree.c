#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/ptree.h"
#include "user/user.h"

struct stack_item {
  int index;
  int depth;
};

// Static arrays to avoid overflowing the 1-page user stack.
// Every process has one parent, so it is pushed at most once: NPROC is enough.
// With LAB_SYSCALL exec does not zero the BSS, so statics must be set explicitly.
static struct ptreeinfo buf[NPROC];
static struct stack_item stack[NPROC];
static int top;

static void
push(int index, int depth)
{
  stack[top].index = index;
  stack[top].depth = depth;
  top++;
}

// A process is a root if its ppid is not present in the process list.
static int
is_root(int i, int n)
{
  for (int j = 0; j < n; j++) {
    if (buf[j].pid == buf[i].ppid)
      return 0;
  }
  return 1;
}

int
main(int argc, char *argv[])
{
  if (argc > 1) {
    fprintf(2, "Usage: pstree\n");
    exit(1);
  }

  int n = ptree(buf, NPROC);
  if (n < 0) {
    fprintf(2, "pstree: ptree failed\n");
    exit(1);
  }

  // Non-recursive DFS. Push in reverse order so earlier entries are popped first.
  top = 0;
  for (int i = n - 1; i >= 0; i--) {
    if (is_root(i, n))
      push(i, 0);
  }

  while (top > 0) {
    struct stack_item item = stack[--top];
    struct ptreeinfo *p = &buf[item.index];

    for (int s = 0; s < item.depth * 2; s++)
      printf(" ");
    printf("%d %s state=%d mem=%ld\n", p->pid, p->name, p->state, p->memsize);

    for (int j = n - 1; j >= 0; j--) {
      if (buf[j].ppid == p->pid)
        push(j, item.depth + 1);
    }
  }

  exit(0);
}
