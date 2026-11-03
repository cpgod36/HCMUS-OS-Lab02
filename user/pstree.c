#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/ptree.h"
#include "user/user.h"

static struct ptreeinfo buf[NPROC];
static int visited[NPROC];

static void
print_indent(int depth)
{
  for (int i = 0; i < depth * 2; i++) {
    printf(" ");
  }
}

static void
print_subtree(int n, int parent_pid, int depth)
{
  for (int i = 0; i < n; i++) {
    if (buf[i].ppid == parent_pid && !visited[i]) {
      visited[i] = 1;
      print_indent(depth);
      printf("%d %s state=%d mem=%ld\n",
             buf[i].pid, buf[i].name, buf[i].state, buf[i].memsize);
      print_subtree(n, buf[i].pid, depth + 1);
    }
  }
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

  if (n == 0) {
    exit(0);
  }

  // Determine root processes.
  // A process is a root if its ppid is not present in the process list.
  int is_root[NPROC];
  for (int i = 0; i < n; i++) {
    is_root[i] = 1;
    for (int j = 0; j < n; j++) {
      if (buf[j].pid == buf[i].ppid) {
        is_root[i] = 0;
        break;
      }
    }
  }

  memset(visited, 0, sizeof(visited));

  // Print roots first, then their descendant trees
  for (int i = 0; i < n; i++) {
    if (is_root[i] && !visited[i]) {
      visited[i] = 1;
      printf("%d %s state=%d mem=%ld\n",
             buf[i].pid, buf[i].name, buf[i].state, buf[i].memsize);
      print_subtree(n, buf[i].pid, 1);
    }
  }

  // In case any circular or unreachable nodes remain
  for (int i = 0; i < n; i++) {
    if (!visited[i]) {
      visited[i] = 1;
      printf("%d %s state=%d mem=%ld\n",
             buf[i].pid, buf[i].name, buf[i].state, buf[i].memsize);
      print_subtree(n, buf[i].pid, 1);
    }
  }

  exit(0);
}
