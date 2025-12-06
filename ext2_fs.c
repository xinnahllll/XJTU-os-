#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>

// 宏定义：文件系统核心参数
#define VOLUME_NAME "EXT2FS"         // 卷名
#define EXT2_NAME_LEN 255            // 文件名最大长度
#define FOPEN_TABLE_MAX 16           // 文件打开表最大大小
#define BLOCK_SIZE 512               // 数据块大小（512字节）
#define DATA_BLOCK_COUNT 4096        // 数据块总数
#define INODE_COUNT 4096             // inode总数
#define DISK_SIZE 4611               // 磁盘总块数（2.25MB = 2360832字节）
#define READ_DISK 1                  // 读磁盘标识
#define WRITE_DISK 0                 // 写磁盘标识
#define GDT_START 0                  // 组描述符起始偏移（第0块）
#define BLOCK_BITMAP_START 512       // 数据块位图起始偏移（第1块）
#define INODE_BITMAP_START 1024      // inode位图起始偏移（第2块）
#define INODE_TABLE_START 1536       // inode表起始偏移（第3块）
#define DATA_BLOCK_START (1536 + 512 * 512)  // 数据块起始偏移（第3+512=515块）

// 文件类型枚举
enum {
    FT_UNKNOWN,  // 未知类型
    FT_REG_FILE, // 普通文件
    FT_DIR,      // 目录
    FT_CHRDEV,   // 字符设备
    FT_BLKDEV,   // 块设备
    FT_FIFO,     // 管道
    FT_SOCK,     // 套接字
    FT_SYMLINK   // 符号链接
};

// 组描述符结构体（32字节，补充登录密码关联）
typedef struct group_desc {
    char bg_volume_name[10];         // 卷名
    unsigned short bg_block_bitmap;  // 数据块位图块号
    unsigned short bg_inode_bitmap;  // inode位图块号
    unsigned short bg_inode_table;   // inode表起始块号
    unsigned short bg_free_blocks_count; // 空闲块数
    unsigned short bg_free_inodes_count; // 空闲inode数
    unsigned short bg_used_dirs_count;   // 目录总数
    char bg_password[10];            // 密码存储（之前是全局变量，现在存到结构体）
} group_desc;

// 索引结点结构体（64字节，时间字段改为time_t匹配time(NULL)）
typedef struct inode {
    unsigned short i_mode;           // 高8位：文件类型；低8位：访问权限
    unsigned short i_blocks;         // 文件占用的数据块数
    unsigned int i_size;             // 文件/目录大小（字节）
    time_t i_atime;                  // 最后访问时间（修正为time_t类型）
    time_t i_ctime;                  // 创建时间（修正为time_t类型）
    time_t i_mtime;                  // 最后修改时间（修正为time_t类型）
    time_t i_dtime;                  // 删除时间（修正为time_t类型）
    unsigned short i_block[8];       // 数据块指针（6直接+1一级间接+1二级间接）
    char i_pad[24];                  // 填充位（确保64字节对齐，0xff填充）
} inode;

// 目录项结构体（变长7-261字节）
typedef struct dir_entry {
    unsigned short inode;            // 关联的inode号
    unsigned short rec_len;          // 目录项总长度
    unsigned char name_len;          // 文件名长度
    unsigned char file_type;         // 文件类型（对应FT_*枚举）
    char name[EXT2_NAME_LEN];        // 文件名（最大255字符）
} dir_entry;

// 全局缓冲区与状态变量（内存临时存储）
group_desc gdt;                      // 组描述符缓冲区
inode inode_buf;                     // inode操作缓冲区
dir_entry dir_buf;                   // 目录项操作缓冲区
unsigned char block_bitmap[BLOCK_SIZE];  // 数据块位图缓冲区（512字节）
unsigned char inode_bitmap[BLOCK_SIZE];  // inode位图缓冲区（512字节）
unsigned char gdt_Buffer[BLOCK_SIZE];    // 组描述符读写缓冲区（512字节）
unsigned char inode_Buffer[BLOCK_SIZE];  // inode表读写缓冲区（512字节）
unsigned char Buffer[BLOCK_SIZE * 2];    // 数据块读写缓冲区（1024字节，避免溢出）
FILE *fp = NULL;                     // 模拟磁盘文件指针（对应Ext2文件）
char current_path[256] = "root";     // 当前路径（默认根目录）
unsigned short current_dir = 1;      // 当前目录的inode号（默认根目录inode=1）
unsigned short fopen_table[FOPEN_TABLE_MAX] = {0};  // 文件打开表（最多16个）
char search_file_name[EXT2_NAME_LEN];    // 待查找文件名（全局临时存储）
int flag = 1;                        // 写文件循环控制标志（文档3原始设计）

// 函数原型声明（避免隐式声明报错）
void disk_IO(long offset, void *buf, int rw_flag);
void load_group_desc(void);
void update_group_desc(void);
void load_inode_entry(unsigned short k);
void update_inode_entry(unsigned short k);
void load_block_entry(unsigned short i);
void update_block_entry(unsigned short i);
void load_block_bitmap(void);
void update_block_bitmap(void);
void load_inode_bitmap(void);
void update_inode_bitmap(void);
int bitmap_find_0(unsigned char bitmap[]);
void bitmap_neg_k(unsigned char bitmap[], int k);
void initialize_inode(void);
unsigned short new_inode(void);
void free_inode(unsigned short i);
unsigned short new_block(void);
void free_block(unsigned short i);
void new_dir_entry(char file_name[], unsigned char file_type);
unsigned short is_open(unsigned short inode_num);
unsigned short access_file(unsigned short inode_num, unsigned short (*func)(unsigned short));
unsigned short search_free_dir_in_block(unsigned short block);
unsigned short search_in_block(unsigned short block);
unsigned short search_file(char name[]);
unsigned short delete_in_block(unsigned short block);
unsigned short free_file_block(unsigned short block);
unsigned short print_file(unsigned short block);
unsigned short print_dir(unsigned short block);
unsigned short add_file_block(unsigned short inode_num);
char *gets_s(char *buffer, int num);
void initialize_memory(void);
void initialize_disk(void);
unsigned short login(char password[]);
void change_password(void);
void dir(void);
void mkdir(char name[]);
void rmdir(char name[]);
void create(char name[]);
void delete(char name[]);
void cd(char path[]);
void attrib(char name[], unsigned char change);
void open(char name[]);
void close(char name[]);
void read(char name[]);
void stopWrite();
void write(char name[]);
void check_disk(void);
void format(void);
void shell(void);

/************************ I/O操作层：模拟磁盘块读写 ************************/
// 功能：按块读写磁盘（offset：块起始偏移；buf：数据缓冲区；rw_flag：读写标识）
void disk_IO(long offset, void *buf, int rw_flag) {
    fseek(fp, offset, SEEK_SET);  // 定位到目标块
    if (rw_flag == READ_DISK) {
        fread(buf, BLOCK_SIZE, 1, fp);  // 读1个块到缓冲区
    } else {
        fwrite(buf, BLOCK_SIZE, 1, fp); // 从缓冲区写1个块到磁盘
        fflush(fp);                     // 强制刷新，避免数据缓存
    }
}

// 功能：从磁盘加载组描述符到内存
void load_group_desc(void) {
    disk_IO(GDT_START, gdt_Buffer, READ_DISK);
    gdt = ((group_desc *)gdt_Buffer)[0];  // 缓冲区强制转换为组描述符
}

// 功能：将内存中的组描述符写回磁盘
void update_group_desc(void) {
    memset(gdt_Buffer, 0xff, sizeof(gdt_Buffer));  // 清空缓冲区（填充0xff）
    ((group_desc *)gdt_Buffer)[0] = gdt;            // 组描述符写入缓冲区
    disk_IO(GDT_START, gdt_Buffer, WRITE_DISK);     // 写回磁盘第0块
}

// 功能：加载第k个inode到内存（inode号从1开始，对应数组索引k-1）
void load_inode_entry(unsigned short k) {
    k--;  // 转换为0-based索引
    unsigned short i = k / 8;  // 所在块号（每个块存8个inode：512/64=8）
    unsigned short j = k % 8;  // 块内偏移（0-7）
    disk_IO(INODE_TABLE_START + i * BLOCK_SIZE, inode_Buffer, READ_DISK);
    inode_buf = ((inode *)inode_Buffer)[j];  // 缓冲区转换为inode结构
}

// 功能：将内存中的inode写回磁盘（第k个inode）
void update_inode_entry(unsigned short k) {
    k--;  // 转换为0-based索引
    unsigned short i = k / 8;
    unsigned short j = k % 8;
    disk_IO(INODE_TABLE_START + i * BLOCK_SIZE, inode_Buffer, READ_DISK);
    ((inode *)inode_Buffer)[j] = inode_buf;  // 覆盖缓冲区中的目标inode
    disk_IO(INODE_TABLE_START + i * BLOCK_SIZE, inode_Buffer, WRITE_DISK);
}

// 功能：加载第i个数据块到缓冲区
void load_block_entry(unsigned short i) {
    disk_IO(DATA_BLOCK_START + i * BLOCK_SIZE, Buffer, READ_DISK);
}

// 功能：将缓冲区中的数据写回第i个数据块（补全文档3缺失的函数）
void update_block_entry(unsigned short i) {
    disk_IO(DATA_BLOCK_START + i * BLOCK_SIZE, Buffer, WRITE_DISK);
}

// 功能：加载数据块位图到内存
void load_block_bitmap(void) {
    disk_IO(BLOCK_BITMAP_START, block_bitmap, READ_DISK);
}

// 功能：将数据块位图写回磁盘
void update_block_bitmap(void) {
    disk_IO(BLOCK_BITMAP_START, block_bitmap, WRITE_DISK);
}

// 功能：加载inode位图到内存
void load_inode_bitmap(void) {
    disk_IO(INODE_BITMAP_START, inode_bitmap, READ_DISK);
}

// 功能：将inode位图写回磁盘
void update_inode_bitmap(void) {
    disk_IO(INODE_BITMAP_START, inode_bitmap, WRITE_DISK);
}

/********************* 文件系统底层：inode/块/目录项管理***********************/
// 功能：查找位图中第一个空闲位（返回位索引，-1表示无空闲）
int bitmap_find_0(unsigned char bitmap[]) {
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        if (bitmap[i] == 0b11111111) continue;  // 该字节无空闲位，跳过
        unsigned char mask = 0b10000000;        // 从最高位（bit7）开始检查
        for (int j = 0; j < 8; ++j) {
            if (!(bitmap[i] & mask)) {          // 找到空闲位（0）
                return i * 8 + j;
            }
            mask >>= 1;  // 掩码右移，检查下一位
        }
    }
    return -1;  // 无空闲位
}

// 功能：将位图中第k位取反（0→1：占用；1→0：释放）
void bitmap_neg_k(unsigned char bitmap[], int k) {
    int i = k / 8;          // 目标字节索引
    int j = k % 8;          // 目标位索引（0-7）
    unsigned char mask = 0b10000000;
    for (int t = 0; t < j; ++t) mask >>= 1;  // 定位到第k位
    bitmap[i] ^= mask;     // 异或取反（1变0，0变1）
}

// 功能：初始化inode默认属性（默认权限：rw-，类型：未知）
void initialize_inode(void) {
    inode_buf.i_mode = FT_UNKNOWN << 8;  // 高8位：文件类型（默认未知）
    ((unsigned char *)(&inode_buf.i_mode))[1] = 0b00000110;  // 低8位：权限（rw-）
    inode_buf.i_blocks = 0;              // 初始无数据块
    inode_buf.i_size = 0;                // 初始大小0字节
    inode_buf.i_atime = time(NULL);      // 访问时间：当前时间
    inode_buf.i_ctime = time(NULL);      // 创建时间：当前时间
    inode_buf.i_mtime = time(NULL);      // 修改时间：当前时间
    inode_buf.i_dtime = 0;               // 未删除（0表示有效）
    memset(inode_buf.i_block, 0, sizeof(inode_buf.i_block));  // 数据块指针清空
    memset(inode_buf.i_pad, 0xff, sizeof(inode_buf.i_pad));    // 填充位设为0xff
}

// 功能：分配一个空闲inode（返回inode号，0表示分配失败）
unsigned short new_inode(void) {
    load_group_desc();
    if (gdt.bg_free_inodes_count == 0) {  // 无空闲inode
        printf("There is no inode to be allocated!\n");
        return 0;
    }
    // 查找inode位图空闲位并标记为占用
    load_inode_bitmap();
    unsigned short i = bitmap_find_0(inode_bitmap);
    bitmap_neg_k(inode_bitmap, i);
    update_inode_bitmap();
    // 初始化inode并写入inode表
    initialize_inode();
    update_inode_entry(i + 1);  // inode号=索引+1（1-based）
    // 更新组描述符（空闲inode数-1）
    gdt.bg_free_inodes_count--;
    update_group_desc();
    return i + 1;  // 返回分配的inode号
}

// 功能：释放一个inode（inode号从1开始）
void free_inode(unsigned short i) {
    // 标记inode位图对应位为空闲
    load_inode_bitmap();
    bitmap_neg_k(inode_bitmap, i - 1);  // 转换为0-based索引
    update_inode_bitmap();
    // 更新组描述符（空闲inode数+1）
    load_group_desc();
    gdt.bg_free_inodes_count++;
    update_group_desc();
}

// 功能：分配一个空闲数据块（返回块号，0表示分配失败）
unsigned short new_block(void) {
    load_group_desc();
    if (gdt.bg_free_blocks_count == 0) {  // 无空闲数据块
        printf("There is no block to be allocated!\n");
        return 0;
    }
    // 查找数据块位图空闲位并标记为占用
    load_block_bitmap();
    unsigned short i = bitmap_find_0(block_bitmap);
    bitmap_neg_k(block_bitmap, i);
    update_block_bitmap();
    // 更新组描述符（空闲数据块数-1）
    gdt.bg_free_blocks_count--;
    update_group_desc();
    return i;  // 返回分配的块号（0-based）
}

// 功能：释放一个数据块（块号从0开始）
void free_block(unsigned short i) {
    // 标记数据块位图对应位为空闲
    load_block_bitmap();
    bitmap_neg_k(block_bitmap, i);
    update_block_bitmap();
    // 更新组描述符（空闲数据块数+1）
    load_group_desc();
    gdt.bg_free_blocks_count++;
    update_group_desc();
}

// 功能：创建目录项（关联inode，分配资源）
void new_dir_entry(char file_name[], unsigned char file_type) {
    memset(&dir_buf, 0, sizeof(dir_entry));
    dir_buf.inode = new_inode();          // 为目录项分配inode
    dir_buf.name_len = strlen(file_name); // 文件名长度
    dir_buf.rec_len = 7 + dir_buf.name_len; // 目录项总长度（7字节头+文件名）
    dir_buf.file_type = file_type;        // 文件类型
    strcpy(dir_buf.name, file_name);      // 文件名赋值

    // 更新inode的文件类型和权限
    load_inode_entry(dir_buf.inode);
    ((unsigned char *)(&inode_buf.i_mode))[0] = file_type;  // 高8位：文件类型
    // 可执行文件判断（.exe/.bin/.com或无扩展名）
    char *extension = strchr(file_name, '.');
    if (!extension || !strcmp(extension, ".exe") || !strcmp(extension, ".bin") || !strcmp(extension, ".com")) {
        ((unsigned char *)(&inode_buf.i_mode))[1] = 0b00000111;  // 权限：rwx
    }
    update_inode_entry(dir_buf.inode);

    // 若为目录：分配数据块，创建"."和".."目录项
    if (file_type == FT_DIR) {
        load_group_desc();
        gdt.bg_used_dirs_count++;  // 目录总数+1
        update_group_desc();

        unsigned short i = new_block();  // 为目录分配数据块
        memset(Buffer, 0, sizeof(Buffer));
        dir_entry temp;

        // 创建当前目录项"."（指向自身inode）
        memset(&temp, 0, sizeof(dir_entry));
        temp.inode = dir_buf.inode;
        temp.rec_len = 8;
        temp.name_len = 1;
        temp.file_type = FT_DIR;
        strcpy(temp.name, ".");
        ((dir_entry *)Buffer)[0] = temp;

        // 创建上级目录项".."（指向当前目录inode）
        memset(&temp, 0, sizeof(dir_entry));
        temp.inode = current_dir;
        temp.rec_len = 9;
        temp.name_len = 2;
        temp.file_type = FT_DIR;
        strcpy(temp.name, "..");
        ((dir_entry *)(Buffer + 8))[0] = temp;

        // 写回数据块
        update_block_entry(i);

        // 更新目录inode属性
        load_inode_entry(dir_buf.inode);
        inode_buf.i_blocks = 1;  // 占用1个数据块
        inode_buf.i_size = 17;   // 大小17字节（"."8字节+".."9字节）
        inode_buf.i_block[0] = i; // 数据块指针指向分配的块
        ((unsigned char *)(&inode_buf.i_mode))[1] = 0b00000110;  // 目录权限：rw-
        update_inode_entry(dir_buf.inode);
    }
}

// 功能：检查文件是否已打开（返回1=已打开，0=未打开）
unsigned short is_open(unsigned short inode_num) {
    for (int i = 0; i < FOPEN_TABLE_MAX; ++i) {
        if (fopen_table[i] == inode_num) {
            return 1;
        }
    }
    return 0;
}

// 功能：遍历文件的所有数据块并执行指定操作（函数指针回调）
unsigned short access_file(unsigned short inode_num, unsigned short (*func)(unsigned short)) {
    unsigned short indirect_1 = 0, indirect_2 = 0;  // 间接索引计数器
    load_inode_entry(inode_num);

    for (unsigned short i = 0; i < inode_buf.i_blocks; /**/) {
        if (i < 6) {  // 直接索引（前6个数据块）
            load_block_entry(inode_buf.i_block[i]);
            unsigned short ret = func(inode_buf.i_block[i]);
            update_block_entry(inode_buf.i_block[i]);
            if (ret != 0) return ret;
            ++i;
        } else if (i == 6) {  // 一级间接索引（第7个数据块存储块号列表）
            load_block_entry(inode_buf.i_block[i]);
            unsigned short j = ((unsigned short *)Buffer)[indirect_1];
            if (j == 0 || indirect_1 == BLOCK_SIZE / sizeof(unsigned short)) {
                ++i;
                indirect_1 = 0;
                continue;
            }
            ++indirect_1;
            load_block_entry(j);
            unsigned short ret = func(j);
            update_block_entry(j);
            if (ret != 0) return ret;
        } else {  // 二级间接索引（第8个数据块存储一级索引块号）
            load_block_entry(inode_buf.i_block[i]);
            unsigned short j = ((unsigned short *)Buffer)[indirect_1];
            if (j == 0 || indirect_1 == BLOCK_SIZE / sizeof(unsigned short)) {
                break;
            }
            load_block_entry(j);
            unsigned short k = ((unsigned short *)Buffer)[indirect_2];
            if (k == 0) {
                ++indirect_1;
                indirect_2 = 0;
                continue;
            }
            if (indirect_2 == BLOCK_SIZE / sizeof(unsigned short)) {
                ++indirect_1;
                indirect_2 = 0;
                continue;
            }
            ++indirect_2;
            load_block_entry(k);
            unsigned short ret = func(k);
            update_block_entry(k);
            if (ret != 0) return ret;
        }
    }
    return 0;
}

// 功能：在单个数据块中查找空闲位置并写入目录项
unsigned short search_free_dir_in_block(unsigned short block) {
    unsigned short current_pos = 0;
    memset(Buffer + BLOCK_SIZE, 0, BLOCK_SIZE);  // 清空缓冲区高位
    load_block_entry(block);

    dir_entry temp = ((dir_entry *)Buffer)[0];
    do {
        // 计算当前目录项的空闲空间（rec_len - 实际长度）
        unsigned short free_space = temp.rec_len - (temp.name_len + 7);
        if (free_space >= dir_buf.rec_len) {  // 空闲空间足够容纳新目录项
            // 缩短当前目录项长度，腾出空间
            temp.rec_len = temp.name_len + 7;
            ((dir_entry *)(Buffer + current_pos))[0].rec_len = temp.rec_len;
            // 写入新目录项
            dir_buf.rec_len = free_space;
            ((dir_entry *)(Buffer + current_pos + temp.rec_len))[0] = dir_buf;
            update_block_entry(block);
            return 1;
        }
        current_pos += temp.rec_len;
        temp = ((dir_entry *)(Buffer + current_pos))[0];
    } while (temp.inode != 0);

    // 数据块末尾有空闲空间
    if (BLOCK_SIZE - current_pos > dir_buf.rec_len) {
        ((dir_entry *)(Buffer + current_pos))[0] = dir_buf;
        update_block_entry(block);
        return 1;
    }
    return 0;  // 无空闲空间
}

// 功能：在单个数据块中查找指定文件（返回inode号，0=未找到）
unsigned short search_in_block(unsigned short block) {
    unsigned short current_pos = 0;
    memset(Buffer + BLOCK_SIZE, 0, BLOCK_SIZE);  // 清空缓冲区高位
    load_block_entry(block);

    dir_buf = ((dir_entry *)Buffer)[0];
    do {
        if (!strcmp(dir_buf.name, search_file_name)) {  // 文件名匹配
            return dir_buf.inode;
        }
        current_pos += dir_buf.rec_len;
        dir_buf = ((dir_entry *)(Buffer + current_pos))[0];
    } while (dir_buf.inode != 0);
    return 0;  // 未找到
}

// 功能：在当前目录下查找指定文件（返回inode号，0=未找到）
unsigned short search_file(char name[]) {
    strcpy(search_file_name, name);  // 保存待查找文件名
    return access_file(current_dir, search_in_block);  // 遍历当前目录数据块
}

// 功能：在单个数据块中删除指定目录项（返回inode号，0=未找到）
unsigned short delete_in_block(unsigned short block) {
    unsigned short current_pos = 0, pre_pos = 0;
    memset(Buffer + BLOCK_SIZE, 0, BLOCK_SIZE);  // 清空缓冲区高位
    load_block_entry(block);

    dir_buf = ((dir_entry *)Buffer)[0];
    do {
        if (!strcmp(dir_buf.name, search_file_name)) {  // 找到目标目录项
            // 扩展前一个目录项长度，跳过当前目录项（逻辑删除）
            ((dir_entry *)(Buffer + pre_pos))[0].rec_len += dir_buf.rec_len;
            update_block_entry(block);
            return dir_buf.inode;
        }
        pre_pos = current_pos;
        current_pos += dir_buf.rec_len;
        dir_buf = ((dir_entry *)(Buffer + current_pos))[0];
    } while (dir_buf.inode != 0);
    return 0;  // 未找到
}

// 功能：释放文件的单个数据块（回调函数，供access_file使用）
unsigned short free_file_block(unsigned short block) {
    free_block(block);
    return 0;
}

// 功能：输出文件单个数据块的内容（回调函数，供access_file使用）
unsigned short print_file(unsigned short block) {
    load_block_entry(block);
    for (unsigned short i = 0; i < BLOCK_SIZE; ++i) {
        if (Buffer[i] == 0) return 1;  // 遇到0表示文件结束
        putchar(Buffer[i]);
    }
    return 0;
}

// 功能：输出目录单个数据块的内容（回调函数，供access_file使用）
unsigned short print_dir(unsigned short block) {
    inode inode_temp = inode_buf;  // 保存当前inode_buf状态
    unsigned short current_pos = 0;
    memset(Buffer + BLOCK_SIZE, 0, BLOCK_SIZE);  // 清空缓冲区高位
    load_block_entry(block);

    dir_buf = ((dir_entry *)Buffer)[0];
    do {
        load_inode_entry(dir_buf.inode);
        // 输出文件名
        printf("%-10s ", dir_buf.name);
        // 输出文件类型
        switch (dir_buf.file_type) {
            case FT_DIR: printf("<DIR>   "); break;
            case FT_REG_FILE: printf("<FILE>  "); break;
            default: printf("<UNKNOWN>"); break;
        }
        // 输出访问权限（r=4, w=2, x=1）
        unsigned char perm = ((unsigned char *)(&inode_buf.i_mode))[1];
        switch (perm) {
            case 2: printf("__w__  "); break;
            case 4: printf("r____  "); break;
            case 6: printf("r_w__  "); break;
            case 7: printf("r_w_x  "); break;
            default: printf("ERROR  "); break;
        }
        // 输出文件大小
        printf("%-12hu ", inode_buf.i_size);
        // 输出时间戳（转换为字符串）
        char time_str[26];
        strcpy(time_str, ctime(&inode_buf.i_ctime));
        time_str[24] = '\0';  // 去除换行符
        printf("%-25s ", time_str);
        strcpy(time_str, ctime(&inode_buf.i_atime));
        time_str[24] = '\0';
        printf("%-25s ", time_str);
        strcpy(time_str, ctime(&inode_buf.i_mtime));
        time_str[24] = '\0';
        printf("%-25s\n", time_str);

        current_pos += dir_buf.rec_len;
        dir_buf = ((dir_entry *)(Buffer + current_pos))[0];
    } while (dir_buf.inode != 0);

    inode_buf = inode_temp;  // 恢复inode_buf状态
    return 0;
}

// 功能：为文件新增一个数据块（返回新块号）
unsigned short add_file_block(unsigned short inode_num) {
    unsigned short new_block_num = 0;
    load_inode_entry(inode_num);

    if (inode_buf.i_blocks < 6) {  // 直接索引未满（前6块）
        new_block_num = new_block();
        inode_buf.i_block[inode_buf.i_blocks] = new_block_num;
        inode_buf.i_blocks++;
    } else if (inode_buf.i_blocks == 6) {  // 一级间接索引（第7块存储块号）
        new_block_num = new_block();
        // 初始化一级间接索引块（存储新块号）
        memset(Buffer, 0, sizeof(Buffer));
        ((unsigned short *)Buffer)[0] = new_block_num;
        update_block_entry(inode_buf.i_block[6]);
        inode_buf.i_blocks++;
    } else if (inode_buf.i_blocks == 7) {  // 一级间接索引块未满
        load_block_entry(inode_buf.i_block[6]);
        // 查找一级间接索引块中的空闲位置
        unsigned short j;
        for (j = 0; j < BLOCK_SIZE / sizeof(unsigned short) && ((unsigned short *)Buffer)[j] != 0; ++j);
        if (j == BLOCK_SIZE / sizeof(unsigned short)) {  // 一级间接索引块已满
            new_block_num = new_block();
            inode_buf.i_block[7] = new_block_num;
            // 初始化二级间接索引块
            memset(Buffer, 0, sizeof(Buffer));
            unsigned short temp_block = new_block();
            ((unsigned short *)Buffer)[0] = temp_block;
            update_block_entry(new_block_num);
            // 初始化二级间接索引指向的块
            memset(Buffer, 0, sizeof(Buffer));
            new_block_num = new_block();
            ((unsigned short *)Buffer)[0] = new_block_num;
            update_block_entry(temp_block);
            inode_buf.i_blocks++;
        } else {  // 一级间接索引块有空闲位置
            new_block_num = new_block();
            ((unsigned short *)Buffer)[j] = new_block_num;
            update_block_entry(inode_buf.i_block[6]);
        }
    } else {  // 二级间接索引
        load_block_entry(inode_buf.i_block[7]);
        // 查找二级间接索引块中的空闲一级索引块
        unsigned short j;
        for (j = 0; j < BLOCK_SIZE / sizeof(unsigned short) && ((unsigned short *)Buffer)[j] != 0; ++j);
        if (j == BLOCK_SIZE / sizeof(unsigned short)) {  // 二级间接索引块已满
            printf("The file has reached the maximum capacity!\n");
            return 0;
        }
        // 加载一级索引块
        unsigned short indirect_block = ((unsigned short *)Buffer)[j];
        load_block_entry(indirect_block);
        // 查找一级索引块中的空闲位置
        unsigned short k;
        for (k = 0; k < BLOCK_SIZE / sizeof(unsigned short) && ((unsigned short *)Buffer)[k] != 0; ++k);
        if (k == BLOCK_SIZE / sizeof(unsigned short)) {  // 一级索引块已满
            // 分配新的一级索引块
            unsigned short new_indirect_block = new_block();
            ((unsigned short *)Buffer)[j] = new_indirect_block;
            update_block_entry(inode_buf.i_block[7]);
            // 初始化新的一级索引块
            memset(Buffer, 0, sizeof(Buffer));
            new_block_num = new_block();
            ((unsigned short *)Buffer)[0] = new_block_num;
            update_block_entry(new_indirect_block);
        } else {  // 一级索引块有空闲位置
            new_block_num = new_block();
            ((unsigned short *)Buffer)[k] = new_block_num;
            update_block_entry(indirect_block);
        }
    }

    update_inode_entry(inode_num);  // 写回inode更新
    return new_block_num;
}

// 功能：安全读取字符串（处理换行符，避免缓冲区溢出）
char *gets_s(char *buffer, int num) {
    if (fgets(buffer, num, stdin) == NULL) {
        return NULL;
    }
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';  // 去除换行符
    }
    return buffer;
}

/************************ 初始化模块：内存与磁盘初始化 ************************/
// 功能：初始化内存变量（文件打开表、组描述符、当前路径等）
void initialize_memory(void) {
    memset(fopen_table, 0, sizeof(fopen_table));
    strcpy(gdt.bg_volume_name, VOLUME_NAME);
    strcpy(gdt.bg_password, "123456"); // 初始密码存到组描述符
    gdt.bg_block_bitmap = 1;
    gdt.bg_inode_bitmap = 2;
    gdt.bg_inode_table = 3;
    gdt.bg_free_blocks_count = DATA_BLOCK_COUNT;
    gdt.bg_free_inodes_count = INODE_COUNT;
    gdt.bg_used_dirs_count = 0; // 初始目录数设为0
    current_dir = 1;
    strcpy(current_path, "root");
}

// 功能：初始化磁盘（清空磁盘、写入组描述符、创建根目录）
void initialize_disk(void) {
    // 清空磁盘所有块（填充0）
    memset(Buffer, 0, sizeof(Buffer));
    for (int i = 0; i < DISK_SIZE; ++i) {
        disk_IO(i * BLOCK_SIZE, Buffer, WRITE_DISK);
    }
    // 写入组描述符到磁盘第0块
    update_group_desc();
    // 创建根目录（inode号=1，类型=目录）
    new_dir_entry("root", FT_DIR);
}

/******************************* 命令层：实现用户指令**************************/
// 功能：登录验证（返回1=成功，0=失败）
unsigned short login(char password[]) {
    load_group_desc();
    return !strcmp(gdt.bg_password, password);
}

// 功能：修改密码
void change_password(void) {
    char old_pwd[10], new_pwd[10], confirm_pwd[10];
    printf("Old password: ");
    gets_s(old_pwd, 9);

    load_group_desc(); // 加载组描述符（包含密码）
    if (strcmp(gdt.bg_password, old_pwd)) {  // 验证旧密码
        printf("Password error!\n");
        return;
    }

    printf("New password(no more than 9): ");
    gets_s(new_pwd, 9);
    printf("Confirm password: ");
    gets_s(confirm_pwd, 9);

    if (strcmp(new_pwd, confirm_pwd)) {
        printf("Please try again.\n");
        return;
    }

    strcpy(gdt.bg_password, new_pwd); // 密码存到组描述符
    update_group_desc(); // 写回磁盘
    printf("The password is changed.\n");
}

// 功能：列出当前目录内容（ls命令）
void dir(void) {
    // 输出表头
    printf("%-10s %-8s %-6s %-12s %-25s %-25s %-25s\n",
           "name", "type", "mode", "size(Byte)", "creat time", "access time", "modify time");
    // 遍历当前目录数据块并输出
    access_file(current_dir, print_dir);
    // 更新当前目录的访问时间
    load_inode_entry(current_dir);
    inode_buf.i_atime = time(NULL);
    update_inode_entry(current_dir);
}

// 功能：创建目录（mkdir命令）
void mkdir(char name[]) {
    // 检查目录是否已存在
    unsigned short exist_inode = search_file(name);
    if (exist_inode != 0) {
        printf("A directory with the same name exists.\n");
        return;
    }

    // 创建目录项（类型=目录）
    new_dir_entry(name, FT_DIR);
    // 将目录项添加到当前目录
    if (!access_file(current_dir, search_free_dir_in_block)) {
        // 当前目录数据块已满，新增数据块
        unsigned short new_block_num = add_file_block(current_dir);
        memset(Buffer, 0, sizeof(Buffer));
        ((dir_entry *)Buffer)[0] = dir_buf;
        update_block_entry(new_block_num);
    }

    // 更新当前目录的大小
    load_inode_entry(current_dir);
    inode_buf.i_size += (7 + strlen(name));  // 目录项长度=7+文件名长度
    update_inode_entry(current_dir);
}

// 功能：删除空目录（rmdir命令）
void rmdir(char name[]) {
    // 禁止删除特殊目录（.和..）
    if (!strcmp(name, ".") || !strcmp(name, "..")) {
        printf("Wrong command!\n");
        return;
    }

    // 检查目录是否存在
    unsigned short dir_inode = search_file(name);
    if (dir_inode == 0) {
        printf("The directory does not exist.\n");
        return;
    }

    // 检查是否为目录类型
    load_inode_entry(dir_inode);
    if (((unsigned char *)(&inode_buf.i_mode))[0] != FT_DIR) {
        printf("Wrong command!\n");
        return;
    }

    // 检查目录是否为空（空目录大小=17字节：.和..）
    if (inode_buf.i_size != 17) {
        printf("Cannot delete non empty directory!\n");
        return;
    }

    // 从当前目录中删除目录项
    access_file(current_dir, delete_in_block);
    // 释放目录的inode和数据块
    access_file(dir_inode, free_file_block);
    load_inode_entry(dir_inode);
    inode_buf.i_dtime = time(NULL);  // 标记删除时间
    update_inode_entry(dir_inode);
    free_inode(dir_inode);

    // 更新当前目录的大小和组描述符
    load_inode_entry(current_dir);
    inode_buf.i_size -= (7 + strlen(name));
    update_inode_entry(current_dir);
    load_group_desc();
    gdt.bg_used_dirs_count--;  // 目录总数-1
    update_group_desc();
}

// 功能：创建文件（create命令）
void create(char name[]) {
    // 检查文件是否已存在
    unsigned short exist_inode = search_file(name);
    if (exist_inode != 0) {
        printf("A file with the same name exists.\n");
        return;
    }

    // 创建文件目录项（类型=普通文件）
    new_dir_entry(name, FT_REG_FILE);
    // 将目录项添加到当前目录
    if (!access_file(current_dir, search_free_dir_in_block)) {
        // 当前目录数据块已满，新增数据块
        unsigned short new_block_num = add_file_block(current_dir);
        memset(Buffer, 0, sizeof(Buffer));
        ((dir_entry *)Buffer)[0] = dir_buf;
        update_block_entry(new_block_num);
    }

    // 更新当前目录的大小
    load_inode_entry(current_dir);
    inode_buf.i_size += (7 + strlen(name));  // 目录项长度=7+文件名长度
    update_inode_entry(current_dir);
}

// 功能：删除文件（delete命令）
void delete(char name[]) {
    // 检查文件是否存在
    unsigned short file_inode = search_file(name);
    if (file_inode == 0) {
        printf("The file does not exist.\n");
        return;
    }

    // 检查是否为普通文件
    load_inode_entry(file_inode);
    if (((unsigned char *)(&inode_buf.i_mode))[0] != FT_REG_FILE) {
        printf("Wrong command!\n");
        return;
    }

    // 检查文件是否已打开
    if (is_open(file_inode)) {
        printf("The file is in use! Please close it first.\n");
        return;
    }

    // 从当前目录中删除文件目录项
    access_file(current_dir, delete_in_block);
    // 释放文件的inode和数据块
    access_file(file_inode, free_file_block);
    load_inode_entry(file_inode);
    inode_buf.i_dtime = time(NULL);  // 标记删除时间
    update_inode_entry(file_inode);
    free_inode(file_inode);

    // 更新当前目录的大小
    load_inode_entry(current_dir);
    inode_buf.i_size -= (7 + strlen(name));
    update_inode_entry(current_dir);
}

// 功能：切换目录（cd命令）
void cd(char path[]) {
    if (!strcmp(path, "") || !strcmp(path, "~")) {  // 返回根目录
        current_dir = 1;
        strcpy(current_path, "root");
    } else if (!strcmp(path, ".")) {  // 留在当前目录
        return;
    } else if (!strcmp(path, "..")) {  // 返回上级目录
        if (current_dir == 1) {  // 已在根目录，无上级
            printf("Already in root directory!\n");
            return;
        }
        // 读取当前目录的".."目录项（指向上级inode）
        load_inode_entry(current_dir);
        load_block_entry(inode_buf.i_block[0]);
        current_dir = ((dir_entry *)(Buffer + 8))[0].inode;
        // 更新当前路径（去除最后一级目录）
        for (int k = strlen(current_path) - 1; k >= 0; --k) {
            if (current_path[k] == '/') {
                current_path[k] = '\0';
                break;
            }
            if (k == 0) {  // 上级目录为根目录
                strcpy(current_path, "root");
                break;
            }
        }
    } else {  // 切换到指定目录
        unsigned short dir_inode = search_file(path);
        if (dir_inode == 0) {  // 目录不存在
            printf("No such directory exits!\n");
            return;
        }
        // 检查是否为目录类型
        load_inode_entry(dir_inode);
        if (((unsigned char *)(&inode_buf.i_mode))[0] != FT_DIR) {
            printf("No such directory exits!\n");
            return;
        }
        // 更新当前目录和路径
        current_dir = dir_inode;
        if (!strcmp(current_path, "root")) {
            sprintf(current_path, "root/%s", path);
        } else {
            sprintf(current_path, "%s/%s", current_path, path);
        }
    }
}

// 功能：修改文件权限（chmod命令）
void attrib(char name[], unsigned char change) {
    // 检查文件是否存在
    unsigned short file_inode = search_file(name);
    if (file_inode == 0) {
        printf("The file does not exist.\n");
        return;
    }

    // 检查权限码是否合法（仅支持2=写、4=读、6=读写、7=读写执行）
    if (change != 2 && change != 4 && change != 6 && change != 7) {
        printf("Wrong modification!\n");
        return;
    }

    // 更新文件权限
    load_inode_entry(file_inode);
    ((unsigned char *)(&inode_buf.i_mode))[1] = change;
    inode_buf.i_atime = time(NULL);  // 更新访问时间
    inode_buf.i_mtime = time(NULL);  // 更新修改时间
    update_inode_entry(file_inode);
}

// 功能：打开文件（open命令）
void open(char name[]) {
    // 检查文件是否存在
    unsigned short file_inode = search_file(name);
    if (file_inode == 0) {
        printf("The file does not exist.\n");
        return;
    }

    // 检查文件是否已打开
    if (is_open(file_inode)) {
        printf("The file has opened.\n");
        return;
    }

    // 检查访问权限（需读/写/执行权限）
    load_inode_entry(file_inode);
    unsigned char perm = ((unsigned char *)(&inode_buf.i_mode))[1];
    if (!(perm & 4) && !(perm & 2) && !(perm & 1)) {
        printf("You do not have permission to open this file.\n");
        return;
    }

    // 将文件添加到打开文件表
    for (unsigned short i = 0; i < FOPEN_TABLE_MAX; ++i) {
        if (fopen_table[i] == 0) {
            fopen_table[i] = file_inode;
            // 更新文件访问时间
            inode_buf.i_atime = time(NULL);
            update_inode_entry(file_inode);
            printf("File opened successfully!\n");
            return;
        }
    }

    // 打开文件数达上限
    printf("The number of files opened has reached the maximum.\n");
}

// 功能：关闭文件（close命令）
void close(char name[]) {
    // 检查文件是否存在
    unsigned short file_inode = search_file(name);
    if (file_inode == 0) {
        printf("The file does not exist.\n");
        return;
    }

    // 检查文件是否已打开
    if (!is_open(file_inode)) {
        printf("The file does not open.\n");
        return;
    }

    // 从打开文件表中移除文件
    for (unsigned short i = 0; i < FOPEN_TABLE_MAX; ++i) {
        if (fopen_table[i] == file_inode) {
            fopen_table[i] = 0;
            // 更新文件访问时间
            load_inode_entry(file_inode);
            inode_buf.i_atime = time(NULL);
            update_inode_entry(file_inode);
            printf("File closed successfully!\n");
            return;
        }
    }
}

// 功能：读文件（read命令）
void read(char name[]) {
    // 检查文件是否存在
    unsigned short file_inode = search_file(name);
    if (file_inode == 0) {
        printf("The file does not exist.\n");
        return;
    }

    // 检查访问权限（需读权限）
    load_inode_entry(file_inode);
    unsigned char perm = ((unsigned char *)(&inode_buf.i_mode))[1];
    if (!(perm & 4)) {
        printf("You do not have permission to read this file.\n");
        return;
    }

    // 检查文件是否已打开
    if (!is_open(file_inode)) {
        printf("The file does not open.\n");
        return;
    }

    // 读取文件内容
    printf("File content:\n");
    access_file(file_inode, print_file);
    printf("\n");

    // 更新文件访问时间
    inode_buf.i_atime = time(NULL);
    update_inode_entry(file_inode);
}

// 功能：写文件中断处理（接收SIGQUIT信号，ctrl+\触发）
void stopWrite() {
    flag = 0;
}

// 功能：写文件（write命令，追加模式，ctrl+\结束）
void write(char name[]) {
    // 检查文件是否存在
    unsigned short file_inode = search_file(name);
    if (file_inode == 0) {
        printf("The file does not exist.\n");
        return;
    }

    // 检查访问权限（需写权限）
    load_inode_entry(file_inode);
    unsigned char perm = ((unsigned char *)(&inode_buf.i_mode))[1];
    if (!(perm & 2)) {
        printf("You do not have permission to write this file.\n");
        return;
    }

    // 检查文件是否已打开
    if (!is_open(file_inode)) {
        printf("The file does not open.\n");
        return;
    }

    // 定位到文件末尾
    unsigned short block_num = 0, pos = 0;
    if (inode_buf.i_blocks == 0) {  // 无数据块，分配新块
        block_num = new_block();
        inode_buf.i_block[0] = block_num;
        inode_buf.i_blocks = 1;
        update_inode_entry(file_inode);
        load_block_entry(block_num);
        pos = 0;
    } else if (inode_buf.i_blocks <= 6) {  // 直接索引，定位到最后一块
        block_num = inode_buf.i_block[inode_buf.i_blocks - 1];
        load_block_entry(block_num);
        pos = inode_buf.i_size % BLOCK_SIZE;
        if (pos == 0) {  // 最后一块已满，新增数据块
            block_num = add_file_block(file_inode);
            load_block_entry(block_num);
            pos = 0;
        }
    } else if (inode_buf.i_blocks == 7) {  // 一级间接索引
        load_block_entry(inode_buf.i_block[6]);
        unsigned short indirect_idx = (inode_buf.i_size / BLOCK_SIZE) - 6;
        block_num = ((unsigned short *)Buffer)[indirect_idx];
        load_block_entry(block_num);
        pos = inode_buf.i_size % BLOCK_SIZE;
        if (pos == 0) {  // 最后一块已满，新增数据块
            block_num = add_file_block(file_inode);
            load_block_entry(block_num);
            pos = 0;
        }
    } else {  // 二级间接索引
        load_block_entry(inode_buf.i_block[7]);
        unsigned short indirect1_idx = (inode_buf.i_size / BLOCK_SIZE - 6 - 256) / 256;
        unsigned short indirect1_block = ((unsigned short *)Buffer)[indirect1_idx];
        load_block_entry(indirect1_block);
        unsigned short indirect2_idx = (inode_buf.i_size / BLOCK_SIZE - 6 - 256) % 256;
        block_num = ((unsigned short *)Buffer)[indirect2_idx];
        load_block_entry(block_num);
        pos = inode_buf.i_size % BLOCK_SIZE;
        if (pos == 0) {  // 最后一块已满，新增数据块
            block_num = add_file_block(file_inode);
            load_block_entry(block_num);
            pos = 0;
        }
    }

    // 注册SIGQUIT信号处理函数（ctrl+\结束写入）
    // 注册SIGQUIT信号处理函数（ctrl+\结束写入）
    signal(SIGQUIT, stopWrite);
    printf("Enter content (press Ctrl+\\ to end):\n");
    flag = 1;
    unsigned short new_size = 0;
    // 在这里添加ch的声明！
    char ch; 

    // 写入数据（删除之前多余的getchar()）
    while (flag) {
        ch = getchar();
        if (!flag) break;  // 收到中断信号，退出循环
        Buffer[pos] = ch;
        pos++;
        new_size++;
        // 数据块已满，写回并分配新块
        if (pos == BLOCK_SIZE) {
            update_block_entry(block_num);
            block_num = add_file_block(file_inode);
            load_block_entry(block_num);
            pos = 0;
        }
    }

    // 写回最后一块数据
    Buffer[pos] = '\0';
    update_block_entry(block_num);

    // 更新文件大小和时间戳
    inode_buf.i_size += new_size - 1;  // 减去中断信号占用的1字节
    inode_buf.i_atime = time(NULL);    // 更新访问时间
    inode_buf.i_mtime = time(NULL);    // 更新修改时间
    update_inode_entry(file_inode);
    printf("\nWrite completed!\n");
}

// 功能：查看磁盘信息（check命令）
void check_disk(void) {
    load_group_desc();
    printf("Volume Name: %s\n", gdt.bg_volume_name);
    printf("Block Size: %dBytes\n", BLOCK_SIZE);
    printf("Free Block: %u\n", gdt.bg_free_blocks_count);
    printf("Free Inode: %u\n", gdt.bg_free_inodes_count);
    printf("Directories: %u\n", gdt.bg_used_dirs_count);
}

// 功能：格式化磁盘（format命令）
void format(void) {
    // 初始化内存变量和磁盘
    initialize_memory();
    initialize_disk();
    printf("Format succeeded!\n");
    // 输出格式化后的磁盘信息
    check_disk();
}

/************************ 用户接口层：命令行交互************************/
// 功能：用户交互shell（解析命令并执行）
void shell(void) {
    char cmd[256] = "";
    while (1) {
        // 输出命令提示符（当前路径）
        printf("[%s]# ", current_path);
        gets_s(cmd, 256);  // 读取用户命令

        // 解析命令
        if (!strcmp(cmd, "quit")) {  // 退出
            printf("Exit EXT2 file system!\n");
            break;
        } else if (!strcmp(cmd, "format")) {  // 格式化
            format();
        } else if (!strcmp(cmd, "check")) {  // 查看磁盘信息
            check_disk();
        } else if (!strcmp(cmd, "password")) {  // 修改密码
            change_password();
        } else if (!strcmp(cmd, "ls")) {  // 列出目录
            dir();
        } else if (!strncmp(cmd, "mkdir ", 6)) {  // 创建目录
            mkdir(cmd + 6);
        } else if (!strncmp(cmd, "rmdir ", 6)) {  // 删除目录
            rmdir(cmd + 6);
        } else if (!strncmp(cmd, "create ", 7)) {  // 创建文件
            create(cmd + 7);
        } else if (!strncmp(cmd, "delete ", 7)) {  // 删除文件
            delete(cmd + 7);
        } else if (!strncmp(cmd, "cd ", 3)) {  // 切换目录
            cd(cmd + 3);
        } else if (!strncmp(cmd, "chmod ", 6)) {  // 修改权限
            char *file_name = cmd + 6;
            printf("modification: ");
            unsigned char perm;
            scanf("%hhu", &perm);
            getchar();  // 吸收换行符
            attrib(file_name, perm);
        } else if (!strncmp(cmd, "open ", 5)) {  // 打开文件
            open(cmd + 5);
        } else if (!strncmp(cmd, "close ", 6)) {  // 关闭文件
            close(cmd + 6);
        } else if (!strncmp(cmd, "read ", 5)) {  // 读文件
            read(cmd + 5);
        } else if (!strncmp(cmd, "write ", 6)) {  // 写文件
            write(cmd + 6);
        } else {  // 未知命令
      printf("Wrong command! Supported commands: quit/format/check/password/ls/mkdir/rmdir/create/delete/cd/chmod/open/close/read/write\n");
        }
    }
}

/**************************** 主函数：程序入口 *******************************/
int main(void) {
    // 打开模拟磁盘文件（./Ext2），不存在则创建
    fp = fopen("./Ext2", "rb+");
    if (fp == NULL) {  // 文件不存在，创建并初始化
        fp = fopen("./Ext2", "wb+");
        initialize_memory();  // 初始化内存
        initialize_disk();     // 初始化磁盘
    } else {  // 文件已存在，加载组描述符和密码
        load_group_desc();
    }

    // 登录验证（循环直到密码正确）
    char password[10];
    printf("Password: ");
    gets_s(password, 9);
    while (!login(password)) {  // 密码错误，重新输入
        printf("Error!Please re-enter!\n");
        printf("Password: ");
        gets_s(password, 9);
    }

    // 登录成功，进入shell交互
    printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
    printf("          Welcome to EXT2 file system!\n");
    printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
    shell();

    // 关闭模拟磁盘
    fclose(fp);
    return 0;
}
