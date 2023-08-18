#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  int p1[2], p2[2];
  pipe(p1);
  pipe(p2);
  // printf("%d %d %d %d\n", p1[0], p1[1], p2[0], p2[1]);
  if (fork() == 0) {
    close(p1[1]);
    close(p2[0]);
    char b;
    read(p1[0], &b, 1);
    // printf("child %d %d\n", p1[0], p2[1]);
    printf("%d received ping\n", getpid());
    write(p2[1], &b, 1);
    exit(0);
  } else {
    close(p1[0]);
    close(p2[1]);
    char b;
    write(p1[1], &b, 1);
    read(p2[0], &b, 1);
    // printf("parent %d %d\n", p1[1], p2[0]);
    printf("%d received pong\n", getpid());
    exit(0);
  }
}
