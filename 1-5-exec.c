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
    } else if (pid == 0) {  // 子进程调用exec函数
        printf("child process1 PID: %d\n", getpid());
        execl("./system_call", "system_call", NULL);  // 替换当前进程
        // exec后的代码不会执行（进程映像已替换）
    } else {  // 父进程等待
        printf("parent process PID: %d\n", getpid());
        wait(NULL);
    }
    return 0;
}
