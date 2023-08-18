#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  int pi[2], p = 2;
  int forked = 0;
  printf("prime %d\n", p);
  pipe(pi);
  int listen_fd = pi[0];
  if (fork() == 0) {
    // 不需要pi[1]，因为写是从下面fork后创建的pipe写。每个子进程保留listen_fd和新建后的pi[1]
    close(pi[1]);
    read(listen_fd, &p, 4);
    printf("prime %d\n", p);
    // printf("%d\n", getpid());
    while (1) {
      int n;
      int ret = read(listen_fd, &n, 4);
      // printf("%d %d\n", ret, pi[1]);
      // printf("%d\n", p);
      // 流水线最后一个子进程退出
      if (ret == 0 && forked == 0) {
        printf("%d\n", listen_fd);
        close(listen_fd); // 最后一个子进程的pi[1]在创建它时已经被关闭了
        exit(0);
        // 流水线中的进程依次退出
      } else if (ret == 0 && forked == 1) {
        close(pi[1]);
        close(listen_fd);
        // printf("%d\n", close(listen_fd));
        // printf("%d %d\n", listen_fd, pi[1]);
        // 实际打印时因为写得快，所以出现质数打印一行，就有一个进程进入此分支
        while (wait(0) != -1)
          ;
        printf("end\n");
        exit(0);
      } // 当数字符合条件且当前进程为流水线上最后一个子进程时，创建新的子进程
      if (n % p && forked == 0) {
        // printf("%d\n", p);
        pipe(pi); // 创建新pipe，用于父进程写和子进程读
        if (fork() == 0) {
          close(pi[1]); // 最后一个子进程暂时不需要pi[1]
          listen_fd = pi[0];
          read(listen_fd, &p, 4);
          printf("prime %d\n", p);
        } else {
          // printf("%d\n", pi[1]);
          forked = 1;
          // 父进程不需要pi[0]，因为读是从listen_fd读。也就是它的父进程创建的pipe
          close(pi[0]);
          write(pi[1], &n, 4);
          // 写完后不能close(pi[1])，否则下一次pipe的时候新pi[0]就会占用这个fd，要保存下来和子进程通信用
          // 流水线中每个进程占用一个连续递增的fd来写
        }
        // 数字符合条件且当前进程非最后一个子进程，则传递数字
      } else if (n % p && forked == 1) {
        write(pi[1], &n, 4); // 此时pi[0]已经被close了
      }
    }
  } else {
    close(pi[0]);
    for (int i = 3; i <= 35; i++) {
      if (i % p) {
        write(pi[1], &i, 4);
      }
      if (i == 35) {
        // int flag = -1;
        // write(pi[1], &flag, 4);
        close(pi[1]); // 通知流水线进程可以退出
        while (wait(0) != -1)
          ;
      }
    }
    exit(0);
  }
}
