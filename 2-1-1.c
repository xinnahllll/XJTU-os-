#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
int flag = 0;
// 信号处理函数
void inter_handler(int sig) {
// 设置标志位，表示接收到了信号
flag = 1;
}
// 等待信号的函数
void waiting() {
// 挂起进程，直到接收到信号
pause();
}
void alarm_handler(int sig) {  
// 接收到闹钟中断时执行的操作
flag = 1;
}
int main() {
// 设置信号处理函数
signal(SIGINT, inter_handler); // Ctrl+C 产生的信号
signal(SIGQUIT, inter_handler); // Ctrl+\ 产生的信号
pid_t pid1 = -1, pid2 = -1;
while (pid1 == -1) pid1 = fork(); // 创建第一个子进程
if (pid1 > 0) {
while (pid2 == -1) pid2 = fork(); // 创建第二个子进程
if (pid2 > 0) {
// 父进程
sleep(5); // 等待 5 秒
if (!flag) {
// 如果 5 秒内没有接收到信号，手动发送信号
kill(pid1, SIGUSR1); // 向子进程 1 发送信号 16
kill(pid2, SIGUSR2); // 向子进程 2 发送信号 17
}
wait(NULL); // 等待子进程 1 终止
wait(NULL); // 等待子进程 2 终止
printf("\nParent process is killed!!\n");
} else {
// 子进程 2
waiting(); // 等待信号
printf("\nChild process 2 is killed by parent !!\n");
return 0;
}
} else {
// 子进程 1
waiting(); // 等待信号
printf("\nChild process 1 is killed by parent !!\n");
return 0;
}
return 0;
}
