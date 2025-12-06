#include <stdio.h>
#include <pthread.h>

int shared = 0;  // 共享变量，初始值0

// 线程1：对shared加100，循环10万次
void* thread1_func(void* arg) {
    for (int i = 0; i < 100000; i++) {
        shared += 100;
    }
    return NULL;
}

// 线程2：对shared减100，循环10万次
void* thread2_func(void* arg) {
    for (int i = 0; i < 100000; i++) {
        shared -= 100;
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;

    // 创建线程1
    if (pthread_create(&t1, NULL, thread1_func, NULL) == 0) {
        printf("thread1 create success!\n");
    }
    // 创建线程2
    if (pthread_create(&t2, NULL, thread2_func, NULL) == 0) {
        printf("thread2 create success!\n");
    }

    // 等待两个线程结束
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("variable result: %d\n", shared);  // 输出最终结果
    return 0;
}
