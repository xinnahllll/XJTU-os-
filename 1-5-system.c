#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    pid_t pid;
    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork Failed");
        return 1;
    } else if (pid == 0) {  // 子进程调用system函数
        printf("child process1 PID: %d\n", getpid());
        system("./system_call");  // 调用辅助程序
        printf("child process PID: %d\n", getpid());  // system后仍能执行
    } else {  // 父进程等待
        printf("parent process PID: %d\n", getpid());
        wait(NULL);
    }
    return 0;
}
