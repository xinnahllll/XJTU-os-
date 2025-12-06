#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int global = 100;  // 全局变量，初始值100

int main() {
    pid_t pid;
    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork Failed");
        return 1;
    } else if (pid == 0) {  // 子进程：全局变量+10
        global += 10;
        printf("Child: global = %d\n", global);
    } else {  // 父进程：全局变量-10，等待子进程
        global -= 10;
        printf("Parent: global = %d\n", global);
        wait(NULL);
    }
    return 0;
}
