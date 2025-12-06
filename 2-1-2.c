//设置闹钟中断
#include <stdio.h> 
#include <unistd.h> 
#include <sys/wait.h> 
#include <stdlib.h> 
#include <signal.h> 

int flag = 0; 

void inter_handler(int sig) { 
    flag = 1; 
} 

void waiting() { 
    pause(); 
} 

void alarm_handler(int sig) { 
    flag = 1; 
} 

// 子进程的SIGUSR1/SIGUSR2处理函数
void child_signal_handler(int sig) {
    if (sig == SIGUSR1) {
        printf("\nChild process 1 is killed by parent!!\n");
    } else if (sig == SIGUSR2) {
        printf("\nChild process 2 is killed by parent!!\n");
    }
    exit(0); // 子进程打印后主动退出
}

int main() { 
    signal(SIGINT, inter_handler); 
    signal(SIGQUIT, inter_handler); 
    signal(SIGALRM, alarm_handler); 

    pid_t pid1 = -1, pid2 = -1; 
    while (pid1 == -1) pid1 = fork(); 

    if (pid1 > 0) { 
        while (pid2 == -1) pid2 = fork(); 
        if (pid2 > 0) { 
            alarm(5); 
            waiting(); 
            if (flag) { 
                kill(pid1, SIGUSR1); 
                kill(pid2, SIGUSR2); 
            } 
            wait(NULL); 
            wait(NULL); 
            printf("\nParent process is killed!!\n"); 
        } else { 
            // 子进程2：注册SIGUSR2处理函数
            signal(SIGUSR2, child_signal_handler);
            waiting(); 
            return 0; 
        } 
    } else { 
        // 子进程1：注册SIGUSR1处理函数
        signal(SIGUSR1, child_signal_handler);
        waiting(); 
        return 0; 
    } 
    return 0; 
}
