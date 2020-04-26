
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

typedef pmm_page_linkedlist_t list_entry_t;

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

// 返回 chunk_info
static inline pmm_page_t * list_pmm_page(list_entry_t * list);

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

// 返回 chunk_info
pmm_page_t * list_pmm_page(list_entry_t * list) {
    return &(list->pmm_page);
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
    // 位于内核结束后，大小为 (PMM_MAX_SIZE / PMM_PAGE_SIZE)*sizeof(list_entry_t)
    // 后面的操作是进行页对齐
    pmm_info = (list_entry_t *)( ( (ptr_t)(&kernel_end + PMM_PAGE_SIZE) & PMM_PAGE_MASK) );
    uint32_t pmm_info_size = (PMM_MAX_SIZE / PMM_PAGE_SIZE) * sizeof(list_entry_t);
    bzero(pmm_info, pmm_info_size);

    // 越过内核使用的内存
    pmm_info->pmm_page.phy_addr = page_start[0].phy_addr;
    pmm_info->pmm_page.npages = page_count;
    pmm_info->pmm_page.ref = 0;
    pmm_info->pmm_page.flag = FF_UNUSED;

    ff_manage.phy_page_count = page_count;
    ff_manage.phy_page_now_count = page_count;

    ff_manage.pmm_addr_start = page_start[0].phy_addr;
    ff_manage.pmm_addr_end = page_start[page_count - 1].phy_addr;

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
        if( (list_pmm_page(entry)->npages >= pages) && (list_pmm_page(entry)->flag == FF_UNUSED) ) {
            // 如果剩余大小足够
            if(list_pmm_page(entry)->npages - pages > 1) {
                // 添加新的链表项
                list_entry_t * tmp = (list_entry_t *)(entry + sizeof(list_entry_t) );
                list_pmm_page(tmp)->phy_addr = entry->pmm_page.phy_addr + pages * PMM_PAGE_SIZE;
                list_pmm_page(tmp)->npages =  entry->pmm_page.npages - pages;
                list_pmm_page(tmp)->ref = 0;
                list_pmm_page(tmp)->flag = FF_UNUSED;
                list_add_after(entry, tmp);
            }
            // 不够的话直接分配
            list_pmm_page(entry)->npages = pages;
            list_pmm_page(entry)->ref = 1;
            list_pmm_page(entry)->flag = FF_USED;
            ff_manage.phy_page_now_count -= pages;
            return list_pmm_page(entry)->phy_addr;
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
    while( ( (entry = list_next(entry) ) != ff_manage.free_list) && (list_pmm_page(entry)->phy_addr != addr_start) );

    // 释放所有页
    list_pmm_page(entry)->ref = 0;
    list_pmm_page(entry)->flag = FF_UNUSED;

    // 如果于相邻链表有空闲的则合并
    // 后面
    if(entry->next != entry && list_pmm_page(entry->next)->flag == FF_UNUSED) {
        list_entry_t * next = entry->next;
        list_pmm_page(entry)->npages += list_pmm_page(next)->npages;
        list_pmm_page(next)->npages = 0;
        list_del(next);
    }
    // 前面
    if(entry->prev != entry && list_pmm_page(entry->prev)->flag == FF_UNUSED) {
        list_entry_t * prev = entry->prev;
        list_pmm_page(prev)->npages += list_pmm_page(entry)->npages;
        list_pmm_page(entry)->npages = 0;
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
