#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/ptree.h"
#include "user/user.h"

struct stack_item {
  int index;
  int depth;
};

// Static arrays to avoid overflowing the 1-page user stack
static struct ptreeinfo buf[NPROC];
static struct stack_item stack[NPROC];
static int visited[NPROC];
static int is_root[NPROC];

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

  // Determine root processes.
  // A process is a root if its ppid is not present in the process list.
  for (int i = 0; i < n; i++) {
    is_root[i] = 1;
    for (int j = 0; j < n; j++) {
      if (buf[j].pid == buf[i].ppid) {
        is_root[i] = 0;
        break;
      }
    }
  }

  // Non-recursive DFS using static stack to avoid user stack overflow
  int top = 0;

  // Push roots in reverse order so earlier roots are popped first
  for (int i = n - 1; i >= 0; i--) {
    if (is_root[i]) {
      visited[i] = 1;
      stack[top].index = i;
      stack[top].depth = 0;
      top++;
    }
  }

  while (top > 0) {
    top--;
    int idx = stack[top].index;
    int depth = stack[top].depth;

    for (int s = 0; s < depth * 2; s++)
      printf(" ");
    printf("%d %s state=%d mem=%ld\n",
           buf[idx].pid, buf[idx].name, buf[idx].state, buf[idx].memsize);

    // Push children in reverse order so earlier children are popped first
    for (int j = n - 1; j >= 0; j--) {
      if (buf[j].ppid == buf[idx].pid && !visited[j]) {
        visited[j] = 1;
        stack[top].index = j;
        stack[top].depth = depth + 1;
        top++;
      }
    }
  }

  exit(0);
}
