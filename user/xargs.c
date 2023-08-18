#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  char *argv1[MAXARG];
  char buf[512], c, *p = buf;
  int i, m = 0;
  for (i = 0; i < argc - 1; i++) {
    argv1[i] = argv[i + 1];
    // printf("%s\n", argv1[i]);
  }
  while (read(0, &c, sizeof(c)) == sizeof(c)) {
    if (c == '\n' || c == ' ') {
      buf[m++] = 0;
      argv1[i++] = p;
      p = buf + m;
    } else {
      buf[m++] = c;
    }
  }
  // for (int j = 0; j < i; j++) {
  // printf("%s\n", argv1[j]);
  // }

  if (fork() == 0) {
    exec(argv1[0], argv1);
  } else {
    wait(0);
  }
  exit(0);
}
