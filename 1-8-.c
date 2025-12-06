#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>  // 用于获取TID

// 线程1：调用system函数，输出PID和TID
void* thread1_func(void* arg) {
    printf("thread1 create success!\n");
    // 获取线程TID和进程PID
    printf("thread1 tid = %ld ,pid = %d\n", syscall(SYS_gettid), getpid());
    system("./system_call");  // 调用辅助程序
    printf("thread1 systemcall return\n");
    return NULL;
}

// 线程2：同上
void* thread2_func(void* arg) {
    printf("thread2 create success!\n");
    printf("thread2 tid = %ld ,pid = %d\n", syscall(SYS_gettid), getpid());
    system("./system_call");
    printf("thread2 systemcall return\n");
    return NULL;
}

int main() {
    pthread_t t1, t2;
    // 创建线程
    pthread_create(&t1, NULL, thread1_func, NULL);
    pthread_create(&t2, NULL, thread2_func, NULL);
    // 等待线程结束
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0;
}
