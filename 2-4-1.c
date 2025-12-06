#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 1024    // 页面大小（假设4KB，这里用1024简化）
#define MAX_FRAMES 4      // 物理块最大数量（可调整，比如2、3、4）
#define MAX_PAGES 10      // 进程最大页面数（假设进程有10个页面）
#define MAX_ACCESS 20     // 最大页面访问次数（模拟访问序列长度）

// 页表项结构
typedef struct {
    int page_num;         // 页面号
    int frame_num;        // 对应的物理块号（-1表示未分配）
    int valid;            // 有效位（1：在物理内存；0：不在）
    int access_time;      // 最近访问时间（LRU用）
    int use_bit;          // 访问位（Clock算法用，1：已访问；0：未访问）
    int dirty;            // 修改位（可选，本次实验暂不使用）
} PageTableEntry;

// 物理块结构
typedef struct {
    int frame_num;        // 物理块号
    int page_num;         // 当前存放的页面号（-1表示空闲）
    int next_frame;       // Clock算法用，指向下一个物理块（环形链表）
} Frame;

// 全局变量
PageTableEntry page_table[MAX_PAGES];  // 页表（10个页面）
Frame frames[MAX_FRAMES];              // 物理块（4个）
int current_time = 0;                  // 全局时间戳（LRU记录访问时间）
int frame_count = 0;                   // 当前已使用的物理块数
int page_fault = 0;                    // 缺页次数

// 初始化函数：重置页表、物理块、计数器
void init(int frame_size) {
    // 初始化页表
    for (int i = 0; i < MAX_PAGES; i++) {
        page_table[i].page_num = i;
        page_table[i].frame_num = -1;
        page_table[i].valid = 0;
        page_table[i].access_time = 0;
        page_table[i].use_bit = 0;
        page_table[i].dirty = 0;
    }

    // 初始化物理块（Clock算法构建环形链表）
    for (int i = 0; i < frame_size; i++) {
        frames[i].frame_num = i;
        frames[i].page_num = -1;
        frames[i].next_frame = (i + 1) % frame_size;  // 环形链表
    }

    current_time = 0;
    frame_count = 0;
    page_fault = 0;
}
//定义核心数据结构



//实现FIFO算法
// FIFO算法：页面置换
void fifo_replace(int new_page, int frame_size) {
    // 步骤1：查找是否有空闲物理块
    int free_frame = -1;
    for (int i = 0; i < frame_size; i++) {
        if (frames[i].page_num == -1) {  // 找到空闲块
            free_frame = i;
            break;
        }
    }

    if (free_frame != -1) {  // 有空闲块：直接分配
        frames[free_frame].page_num = new_page;
        page_table[new_page].frame_num = free_frame;
        page_table[new_page].valid = 1;
        frame_count++;
        printf("FIFO：分配空闲块%d给页面%d\n", free_frame, new_page);
        return;
    }

    // 步骤2：无空闲块：置换队首页面（FIFO核心）
    // 找到最早进入的页面（队列第一个）
    int replace_frame = 0;  // FIFO队列：按物理块分配顺序，队首是最早进入的
    int old_page = frames[replace_frame].page_num;

    // 更新物理块和页表
    frames[replace_frame].page_num = new_page;
    page_table[old_page].valid = 0;  // 旧页面置为无效
    page_table[old_page].frame_num = -1;
    page_table[new_page].frame_num = replace_frame;
    page_table[new_page].valid = 1;

    printf("FIFO：置换块%d（旧页面%d）→ 新页面%d\n", replace_frame, old_page, new_page);
}

// FIFO算法主函数：处理页面访问
void fifo_algorithm(int access_sequence[], int access_len, int frame_size) {
    init(frame_size);  // 初始化
    printf("\n===== FIFO页面置换算法（物理块数：%d）=====\n", frame_size);

    for (int i = 0; i < access_len; i++) {
        int page = access_sequence[i];
        current_time++;
        printf("\n第%d次访问页面：%d\n", i + 1, page);

        if (page_table[page].valid == 1) {  // 页面已在内存：命中
            printf("页面%d命中，物理块号：%d\n", page, page_table[page].frame_num);
            continue;
        }

        // 页面不在内存：缺页中断
        page_fault++;
        printf("缺页中断！页面%d不在内存\n", page);
        fifo_replace(page, frame_size);  // 调用FIFO置换
    }

    // 输出统计结果
    float fault_rate = (float)page_fault / access_len * 100;
    printf("\n===== FIFO统计结果 =====\n");
    printf("总访问次数：%d\n", access_len);
    printf("缺页次数：%d\n", page_fault);
    printf("缺页率：%.2f%%\n", fault_rate);
}

//实现LRU算法
// LRU算法：页面置换（找到访问时间最早的页面）
void lru_replace(int new_page, int frame_size) {
    // 步骤1：查找空闲块
    int free_frame = -1;
    for (int i = 0; i < frame_size; i++) {
        if (frames[i].page_num == -1) {
            free_frame = i;
            break;
        }
    }

    if (free_frame != -1) {  // 有空闲块：分配并记录访问时间
        frames[free_frame].page_num = new_page;
        page_table[new_page].frame_num = free_frame;
        page_table[new_page].valid = 1;
        page_table[new_page].access_time = current_time;
        frame_count++;
        printf("LRU：分配空闲块%d给页面%d（访问时间：%d）\n", free_frame, new_page, current_time);
        return;
    }

    // 步骤2：无空闲块：找到访问时间最早的页面（LRU核心）
    int lru_frame = 0;
    int min_time = page_table[frames[0].page_num].access_time;
    for (int i = 1; i < frame_size; i++) {
        int page = frames[i].page_num;
        if (page_table[page].access_time < min_time) {
            min_time = page_table[page].access_time;
            lru_frame = i;
        }
    }

    int old_page = frames[lru_frame].page_num;
    // 更新物理块和页表
    frames[lru_frame].page_num = new_page;
    page_table[old_page].valid = 0;
    page_table[old_page].frame_num = -1;
    page_table[new_page].frame_num = lru_frame;
    page_table[new_page].valid = 1;
    page_table[new_page].access_time = current_time;

    printf("LRU：置换块%d（旧页面%d，访问时间：%d）→ 新页面%d（访问时间：%d）\n", 
           lru_frame, old_page, min_time, new_page, current_time);
}

// LRU算法主函数
void lru_algorithm(int access_sequence[], int access_len, int frame_size) {
    init(frame_size);
    printf("\n===== LRU页面置换算法（物理块数：%d）=====\n", frame_size);

    for (int i = 0; i < access_len; i++) {
        int page = access_sequence[i];
        current_time++;
        printf("\n第%d次访问页面：%d\n", i + 1, page);

        if (page_table[page].valid == 1) {  // 页面命中：更新访问时间
            page_table[page].access_time = current_time;
            printf("页面%d命中，物理块号：%d（更新访问时间：%d）\n", 
                   page, page_table[page].frame_num, current_time);
            continue;
        }

        // 缺页中断
        page_fault++;
        printf("缺页中断！页面%d不在内存\n", page);
        lru_replace(page, frame_size);
    }

    // 统计结果
    float fault_rate = (float)page_fault / access_len * 100;
    printf("\n===== LRU统计结果 =====\n");
    printf("总访问次数：%d\n", access_len);
    printf("缺页次数：%d\n", page_fault);
    printf("缺页率：%.2f%%\n", fault_rate);
}


//实现clock算法
// Clock算法：页面置换
void clock_replace(int new_page, int frame_size, int *clock_ptr) {
    // 步骤1：查找空闲块
    int free_frame = -1;
    for (int i = 0; i < frame_size; i++) {
        if (frames[i].page_num == -1) {
            free_frame = i;
            break;
        }
    }

    if (free_frame != -1) {  // 有空闲块：分配并置访问位为1
        frames[free_frame].page_num = new_page;
        page_table[new_page].frame_num = free_frame;
        page_table[new_page].valid = 1;
        page_table[new_page].use_bit = 1;
        frame_count++;
        printf("Clock：分配空闲块%d给页面%d（访问位：1）\n", free_frame, new_page);
        *clock_ptr = frames[free_frame].next_frame;  // 指针移动到下一个块
        return;
    }

    // 步骤2：无空闲块：遍历环形链表，找访问位为0的页面（Clock核心）
    while (1) {
        int current_frame = *clock_ptr;
        int page = frames[current_frame].page_num;

        if (page_table[page].use_bit == 0) {  // 找到可置换的页面
            // 置换
            int old_page = page;
            frames[current_frame].page_num = new_page;
            page_table[old_page].valid = 0;
            page_table[old_page].frame_num = -1;
            page_table[new_page].frame_num = current_frame;
            page_table[new_page].valid = 1;
            page_table[new_page].use_bit = 1;

            printf("Clock：置换块%d（旧页面%d，访问位：0）→ 新页面%d（访问位：1）\n", 
                   current_frame, old_page, new_page);
            *clock_ptr = frames[current_frame].next_frame;  // 指针移动
            break;
        } else {  // 访问位为1：置为0，继续遍历
            page_table[page].use_bit = 0;
            printf("Clock：块%d（页面%d）访问位置0，指针移动到下一块\n", 
                   current_frame, page);
            *clock_ptr = frames[current_frame].next_frame;
        }
    }
}

// Clock算法主函数
void clock_algorithm(int access_sequence[], int access_len, int frame_size) {
    init(frame_size);
    int clock_ptr = 0;  // Clock指针，初始指向第一个物理块
    printf("\n===== Clock页面置换算法（物理块数：%d）=====\n", frame_size);

    for (int i = 0; i < access_len; i++) {
        int page = access_sequence[i];
        current_time++;
        printf("\n第%d次访问页面：%d\n", i + 1, page);

        if (page_table[page].valid == 1) {  // 页面命中：置访问位为1
            page_table[page].use_bit = 1;
            printf("页面%d命中，物理块号：%d（访问位置1）\n", 
                   page, page_table[page].frame_num);
            continue;
        }

        // 缺页中断
        page_fault++;
        printf("缺页中断！页面%d不在内存\n", page);
        clock_replace(page, frame_size, &clock_ptr);
    }

    // 统计结果
    float fault_rate = (float)page_fault / access_len * 100;
    printf("\n===== Clock统计结果 =====\n");
    printf("总访问次数：%d\n", access_len);
    printf("缺页次数：%d\n", page_fault);
    printf("缺页率：%.2f%%\n", fault_rate);
}

//主函数
int main() {
    // 模拟页面访问序列（可修改，比如教材中的经典序列：2,3,2,1,5,2,4,5,3,2,5,2）
    int access_sequence[MAX_ACCESS] = {2, 3, 2, 1, 5, 2, 4, 5, 3, 2, 5, 2};
    int access_len = 12;  // 访问序列长度
    int frame_size;       // 物理块数（用户输入）

    // 选择物理块数
    printf("请输入物理块数量（1-%d）：", MAX_FRAMES);
    scanf("%d", &frame_size);
    if (frame_size < 1 || frame_size > MAX_FRAMES) {
        printf("输入无效，默认使用3个物理块\n");
        frame_size = 3;
    }

    // 调用三种算法
    fifo_algorithm(access_sequence, access_len, frame_size);
    lru_algorithm(access_sequence, access_len, frame_size);
    clock_algorithm(access_sequence, access_len, frame_size);

    return 0;
}
