#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid, pid1;
    pid = fork();  // 创建子进程

    if (pid < 0) {
        fprintf(stderr, "Fork Failed");  // 创建失败提示
        return 1;
    } else if (pid == 0) {  // 子进程执行逻辑
        pid1 = getpid();  // 获取子进程自身PID
        printf("child: pid = %d\n", pid);   // fork在子进程返回0
        printf("child: pid1 = %d\n", pid1); // 子进程真实PID
    } else {  // 父进程执行逻辑
        pid1 = getpid();  // 获取父进程自身PID
        printf("parent: pid = %d\n", pid);  // fork在父进程返回子进程PID
        printf("parent: pid1 = %d\n", pid1); // 父进程真实PID
        wait(NULL);  // 父进程等待子进程结束
    }
    return 0;
}
