
// This file is a part of MRNIU/SimpleKernel (https://github.com/MRNIU/SimpleKernel).
//
// first_fit.c for MRNIU/SimpleKernel.

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "assert.h"
#include "mem/firstfit.h"

#define FF_USED         (0x00)
#define FF_UNUSED       (0x01)

static void init(pmm_page_t * page_start, size_t page_count);
static ptr_t alloc(size_t bytes);
static void free(ptr_t addr_start, size_t bytes);
static size_t free_pages_count(void);

pmm_manage_t firstfit_manage = {
    "Fitst Fit",
    &init,
    &alloc,
    &free,
    &free_pages_count
};

// typedef pmm_page_linkedlist_t list_entry_t;
typedef pmm_page_t list_entry_t;

static inline void list_init_head(list_entry_t * list);

// 在中间添加元素
static inline void list_add_middle(list_entry_t * prev,  list_entry_t * next, list_entry_t * new);

// 在 prev 后添加项
static inline void list_add_after(list_entry_t * prev, list_entry_t * new);

// 在 next 前添加项
static inline void list_add_before(list_entry_t * next, list_entry_t * new);

// 删除元素
static inline void list_del(list_entry_t * list);

// 返回前面的元素
static inline list_entry_t * list_prev(list_entry_t * list);

// 返回后面的的元素
static inline list_entry_t * list_next(list_entry_t * list);

// 初始化
void list_init_head(list_entry_t * list) {
    list->next = list;
    list->prev = list;
    return;
}

// 在中间添加元素
void list_add_middle(list_entry_t * prev,  list_entry_t * next, list_entry_t * new) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

// 在 prev 后添加项
void list_add_after(list_entry_t * prev, list_entry_t * new) {
    list_add_middle(prev, prev->next, new);
    return;
}

// 在 next 前添加项
void list_add_before(list_entry_t * next, list_entry_t * new) {
    list_add_middle(next->prev, next, new);
    return;
}

// 删除元素
void list_del(list_entry_t * list) {
    list->next->prev = list->prev;
    list->prev->next = list->next;
    return;
}

// 返回前面的元素
list_entry_t * list_prev(list_entry_t * list) {
    return list->prev;
}

// 返回后面的的元素
list_entry_t * list_next(list_entry_t * list) {
    return list->next;
}

typedef
    struct firstfit_manage {
    // 物理内存起始地址
    ptr_t		pmm_addr_start;
    // 物理内存结束地址
    ptr_t		pmm_addr_end;
    // 物理内存页的总数量
    uint32_t		phy_page_count;
    // 物理内存页的当前数量
    uint32_t		phy_page_now_count;
    // 空闲链表
    list_entry_t *	free_list;
} firstfit_manage_t;

// First Fit 算法需要的信息
static firstfit_manage_t ff_manage;

// 物理页信息保存地址
static list_entry_t * pmm_info = NULL;

void init(pmm_page_t * page_start, size_t page_count) {
    pmm_info = (list_entry_t *)page_start;
    // 设置可用页信息
    ff_manage.phy_page_count = page_count;
    ff_manage.phy_page_now_count = page_count;
    // 设置管理内存起止
    ff_manage.pmm_addr_start = page_start[0].phy_addr;
    ff_manage.pmm_addr_end = page_start[page_count - 1].phy_addr;
    // 设置链表信息
    ff_manage.free_list = pmm_info;
    list_init_head(ff_manage.free_list);
    return;
}

ptr_t alloc(size_t bytes) {
    // 计算需要的页数
    size_t pages = bytes / PMM_PAGE_SIZE;
    // 不足一页的+1
    if(bytes % PMM_PAGE_SIZE != 0) {
        pages++;
    }
    list_entry_t * entry = ff_manage.free_list;
    while(entry != NULL) {
        // 查找符合长度且未使用的内存
        if( (entry->npages >= pages) && (entry->flag == FF_UNUSED) ) {
            // 如果剩余大小足够
            if(entry->npages - pages > 1) {
                // 添加新的链表项
                list_entry_t * tmp = (list_entry_t *)(entry + sizeof(list_entry_t) );
                tmp->phy_addr = entry->phy_addr + pages * PMM_PAGE_SIZE;
                tmp->npages =  entry->npages - pages;
                tmp->ref = 0;
                tmp->flag = FF_UNUSED;
                list_add_after(entry, tmp);
            }
            // 不够的话直接分配
            entry->npages = pages;
            entry->ref = 1;
            entry->flag = FF_USED;
            ff_manage.phy_page_now_count -= pages;
            return entry->phy_addr;
        }
        // 没找到的话就查找下一个
        else if(list_next(entry) != ff_manage.free_list) {
            entry = list_next(entry);
        }
        else {
            printk_err("Error at firstfit.c: ptr_t alloc(uint32_t)\n");
            break;
        }
    }
    printk_err("Error at firstfit.c: ptr_t alloc(uint32_t)\n");
    return (ptr_t)NULL;
}

void free(ptr_t addr_start, size_t bytes) {
    // 计算需要的页数
    size_t pages = bytes / PMM_PAGE_SIZE;
    // 不足一页的+1
    if(bytes % PMM_PAGE_SIZE != 0) {
        pages++;
    }
    // 首先找到地址对应的管理节点
    list_entry_t * entry = ff_manage.free_list;
    while( ( (entry = list_next(entry) ) != ff_manage.free_list) && (entry->phy_addr != addr_start) );

    // 释放所有页
    entry->ref = 0;
    entry->flag = FF_UNUSED;

    // 如果于相邻链表有空闲的则合并
    // 后面
    if(entry->next != entry && (entry->next)->flag == FF_UNUSED) {
        list_entry_t * next = entry->next;
        entry->npages += next->npages;
        next->npages = 0;
        list_del(next);
    }
    // 前面
    if(entry->prev != entry && (entry->prev)->flag == FF_UNUSED) {
        list_entry_t * prev = entry->prev;
        prev->npages += entry->npages;
        entry->npages = 0;
        list_del(entry);
    }
    ff_manage.phy_page_now_count += pages;
    return;
}

size_t free_pages_count() {
    return ff_manage.phy_page_now_count;
}

#ifdef __cplusplus
}
#endif
