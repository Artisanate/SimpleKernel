
// This file is a part of MRNIU/SimpleKernel (https://github.com/MRNIU/SimpleKernel).
//
// heap.h for MRNIU/SimpleKernel.

#ifndef _HEAP_H_
#define _HEAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stddef.h"

// 堆起始地址 2GB 处，向上增长
#define HEAP_START      (0x80000000UL)
// 堆最大容量 4MB
#define HEAP_MAX_SIZE   (0x400000UL)

// 堆管理结构体
typedef
    struct heap_manage {
    // 管理算法的名称
    const char *      name;
    // 初始化
    void (* heap_manage_init)(ptr_t addr_start);
    // 内存申请，单位为 Byte
    ptr_t (* heap_manage_malloc)(size_t byte);
    // 进程栈申请
    ptr_t (* heap_manage_malloc_stack)(void);
    // 释放内存
    void (* heap_manage_free)(ptr_t addr);
    // 释放栈内存
    void (* heap_manage_free_stack)(ptr_t addr);
} heap_manage_t;

// 初始化堆
void heap_init(void);

// 内存申请，单位为 Byte
ptr_t kmalloc(size_t byte);

// 栈申请
ptr_t kmalloc_stack(void);

// 内存释放
void kfree(ptr_t p);

// 栈释放
void kfree_stack(ptr_t p);

#ifdef __cplusplus
}
#endif

#endif /* _HEAP_H_ */
