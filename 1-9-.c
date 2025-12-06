#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>

void* thread1_func(void* arg) {
    printf("thread1 create success!\n");
    printf("thread1 tid = %ld ,pid = %d\n", syscall(SYS_gettid), getpid());
    return NULL;
}

void* thread2_func(void* arg) {
    printf("thread2 create success!\n");
    printf("thread2 tid = %ld ,pid = %d\n", syscall(SYS_gettid), getpid());
    
    // 在线程2中调用exec
    execl("./system_call", "system_call", NULL);
    return NULL;
}

int main() {
    pthread_t t1, t2;
    
    // 先创建线程1
    pthread_create(&t1, NULL, thread1_func, NULL);
    // 稍微延迟，确保线程1先输出
    usleep(1000);
    // 再创建线程2
    pthread_create(&t2, NULL, thread2_func, NULL);
    
    // 等待线程结束
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    
    return 0;
}
}
