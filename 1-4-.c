#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int global = 100;

int main() {
    pid_t pid;
    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork Failed");
        return 1;
    } else if (pid == 0) {  // 子进程：先+10，return前再+10
        global += 10;
        printf("Child before return: global = %d\n", global);
        global += 10;  // return前新增操作
    } else {  // 父进程：先-10，wait后再+20
        global -= 10;
        printf("Parent before wait: global = %d\n", global);
        wait(NULL);
        global += 20;
        printf("Parent after wait: global = %d\n", global);
    }
    return 0;
}
