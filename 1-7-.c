#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

int shared = 0;
sem_t sem;  // 定义信号量

// 线程1：PV操作保护加100
void* thread1_func(void* arg) {
    for (int i = 0; i < 100000; i++) {
        sem_wait(&sem);  // P操作：获取信号量（加锁）
        shared += 100;
        sem_post(&sem);  // V操作：释放信号量（解锁）
    }
    return NULL;
}

// 线程2：PV操作保护减100
void* thread2_func(void* arg) {
    for (int i = 0; i < 100000; i++) {
        sem_wait(&sem);  // P操作
        shared -= 100;
        sem_post(&sem);  // V操作
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;
    sem_init(&sem, 0, 1);  // 初始化信号量：初始值1（互斥访问）

    // 创建线程
    if (pthread_create(&t1, NULL, thread1_func, NULL) == 0) {
        printf("thread1 create success!\n");
    }
    if (pthread_create(&t2, NULL, thread2_func, NULL) == 0) {
        printf("thread2 create success!\n");
    }

    // 等待线程结束
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    sem_destroy(&sem);  // 销毁信号量
    printf("variable result: %d\n", shared);
    return 0;
}
