
// This file is a part of MRNIU/SimpleKernel (https://github.com/MRNIU/SimpleKernel).
//
// pmm.c for MRNIU/SimpleKernel.

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"

#include "string.h"
#include "assert.h"
#include "debug.h"
#include "cpu.hpp"
#include "sync.hpp"
#include "mem/pmm.h"
#include "mem/firstfit.h"

// 物理页帧数组长度
static size_t phy_pages_count = 0;

static const pmm_manage_t * pmm_manager  = &firstfit_manage;

// 从 GRUB 读取物理内存信息
static void pmm_get_ram_info(e820map_t * e820map);
void pmm_get_ram_info(e820map_t * e820map) {
    ptr_t entry = (ptr_t)mmap_entries;
    ptr_t tag = (ptr_t)mmap_tag;
    for(e820map->nr_map = 0 ; entry < tag + mmap_tag->size ;
        entry += (ptr_t)( (multiboot_tag_mmap_t *)mmap_tag)->entry_size, e820map->nr_map++) {
        e820map->map[e820map->nr_map].addr = (e820_addr_t)( (multiboot_memory_map_entry_t *)entry)->addr;
        e820map->map[e820map->nr_map].length = (e820_len_t)( (multiboot_memory_map_entry_t *)entry)->len;
        e820map->map[e820map->nr_map].type = (e820_type_t)( (multiboot_memory_map_entry_t *)entry)->type;
        assert(e820map->nr_map < E820_MAX, "pmm.c: e820map->nr_map bigger than E820_MAX.");
    }
    return;
}

// 物理页信息保存地址
static pmm_page_t * pmm_pages = NULL;
// 物理页信息大小
static size_t pmm_pages_size = 0;

static mem_zone_t mem_zone_dma = {
    .zone_start_address  = ZONE_DMA_ADDR,
    .zone_end_address    = 0,
    .zone_length = 0,
    .attribute = ZONE_DMA,
    .page_using_count = 0,
    .page_free_count = 0,
};

static mem_zone_t mem_zone_normal = {
    .zone_start_address  = ZONE_NORMAL_ADDR,
    .zone_end_address    =    0,
    .zone_length =    0,
    .attribute = ZONE_NORMAL,
    .page_using_count =    0,
    .page_free_count =    0,
};

static mem_zone_t mem_zone_high = {
    .zone_start_address  = ZONE_HIGHMEM_ADDR,
    .zone_end_address    = 0,
    .zone_length = 0,
    .attribute = ZONE_HIGHMEM,
    .page_using_count = 0,
    .page_free_count = 0,
};

void pmm_phy_init(e820map_t * e820map) {
    // 这里假设最小物理地址为 512MB，没有考虑更小的情况
    // 位于内核结束后，大小为 sizeof(pmm_page_t) * phy_pages_count
    // 后面的操作是进行页对齐
    pmm_pages = (pmm_page_t *)( ( (ptr_t)(&kernel_end + PMM_PAGE_SIZE) & PMM_PAGE_MASK) );
    ptr_t start_addr = (ptr_t)NULL;
    ptr_t end_addr = (ptr_t)NULL;
    // 首先计算可用物理页总数
    for(size_t i = 0 ; i < e820map->nr_map ; i++) {
        if(e820map->map[i].type == MULTIBOOT_MEMORY_AVAILABLE
            && (e820map->map[i].addr & 0xFFFFFFFF) == 0x100000) {
            // printk_debug("1\n");
            start_addr = (ptr_t)e820map->map[i].addr;
            for(e820_addr_t addr = e820map->map[i].addr ;
                addr < e820map->map[i].addr + e820map->map[i].length ;
                addr += PMM_PAGE_SIZE) {
                phy_pages_count++;
            }
        }
    }
    pmm_pages_size = sizeof(pmm_page_t) * phy_pages_count;
    bzero(pmm_pages, pmm_pages_size);
    end_addr = start_addr + (ptr_t)(PMM_PAGE_SIZE * phy_pages_count);
    assert(start_addr == 0x100000, "pmm.c: start_addr != 0x100000.");
    // 按照地址设置信息
    size_t i = 0;
    for(ptr_t addr = start_addr ; addr < end_addr ; addr += PMM_PAGE_SIZE) {
        if(addr < ZONE_HIGHMEM_ADDR) {
            if(addr < ZONE_NORMAL_ADDR) {
                pmm_pages[i].zone_info = &mem_zone_dma;
                pmm_pages[i].phy_addr = addr;
                pmm_pages[i].attribute = 0;
                pmm_pages[i].ref = 0;
                pmm_pages[i].age = 0;
            }
            else {
                pmm_pages[i].zone_info = &mem_zone_normal;
                pmm_pages[i].phy_addr = addr;
                pmm_pages[i].attribute = 0;
                pmm_pages[i].ref = 0;
                pmm_pages[i].age = 0;
            }
        }
        else if(addr > ZONE_HIGHMEM_ADDR) {
            pmm_pages[i].zone_info = &mem_zone_high;
            pmm_pages[i].phy_addr = addr;
            pmm_pages[i].attribute = 0;
            pmm_pages[i].ref = 0;
            pmm_pages[i].age = 0;
        }
        i++;
        assert(i <= phy_pages_count, "pmm.c: i bigger than phy_page_count.");
    }

    // 设置 zone 信息
    for(i = 0 ; i < phy_pages_count ; i++) {
        // dma 区域
        if(pmm_pages[i].phy_addr < ZONE_NORMAL_ADDR) {
            mem_zone_dma.zone_end_address = pmm_pages[i].phy_addr + PMM_PAGE_SIZE;
            mem_zone_dma.zone_length += PMM_PAGE_SIZE;
            // 补齐未统计的低端 1MB
            if(mem_zone_dma.zone_length == 0xF00000) {
                mem_zone_dma.zone_length += 0x100000;
            }
            mem_zone_dma.page_free_count++;
        }
        // normal 区域
        else if(pmm_pages[i].phy_addr < ZONE_HIGHMEM_ADDR) {
            mem_zone_normal.zone_end_address = pmm_pages[i].phy_addr + PMM_PAGE_SIZE;
            mem_zone_normal.zone_length += PMM_PAGE_SIZE;
            mem_zone_normal.page_free_count++;
        }
        // highmem 区域
        else if(pmm_pages[i].phy_addr > ZONE_HIGHMEM_ADDR) {
            mem_zone_high.zone_end_address = pmm_pages[i].phy_addr + PMM_PAGE_SIZE;
            mem_zone_high.zone_length += PMM_PAGE_SIZE;
            mem_zone_high.page_free_count++;
        }
    }

    // 长度为 16MB
    assert(mem_zone_dma.zone_length == ZONE_NORMAL_ADDR - ZONE_DMA_ADDR, "pmm.c: mem_zone_dma.zone_length != ZONE_NORMAL_ADDR - ZONE_DMA_ADDR.");
    // 结束地址为 16MB
    assert(mem_zone_dma.zone_end_address == ZONE_NORMAL_ADDR, "pmm.c: mem_zone_dma.zone_end_address != ZONE_NORMAL_ADDR.");

    // 长度为可用物理内存 - 分给 DMA 的 15MB(因为前 1MB 已经分出去了)
    assert(mem_zone_normal.zone_length == phy_pages_count * PMM_PAGE_SIZE - 0xF00000, "pmm.c: mem_zone_normal.zone_length != (phy_pages_count * PMM_PAGE_SIZE) - 0x100000")
    // 结束地址为 开始地址 + 长度
    assert(mem_zone_normal.zone_end_address == ZONE_NORMAL_ADDR + mem_zone_normal.zone_length, "pmm.c: mem_zone_dma.zone_end_address != ZONE_NORMAL_ADDR + mem_zone_normal.zone_length.");

    // printk_debug("mem_zone_high.zone_start_address = 0x%X, mem_zone_high.zone_end_address = 0x%X ", (uint32_t)mem_zone_high.zone_start_address, (uint32_t)mem_zone_high.zone_end_address);
    // printk_debug("mem_zone_high.zone_length = 0x%X\n", (uint32_t)mem_zone_high.zone_length);

    return;
}

void pmm_mamage_init(pmm_page_t * page_start, size_t page_count) {
    // 传递页指针与页数量
    // 要注意在低 1MB 可能有已经被使用的内存
    pmm_manager->pmm_manage_init(page_start, page_count);
    return;
}

void pmm_init() {
    bool intr_flag = false;
    local_intr_store(intr_flag);
    {
        e820map_t e820map;
        bzero(&e820map, sizeof(e820map_t) );
        pmm_get_ram_info(&e820map);
        pmm_phy_init(&e820map);
        pmm_mamage_init(pmm_pages, phy_pages_count);

        printk_info("pmm_init\n");
        printk_info("phy_pages_count: %d\n", phy_pages_count);
        printk_info("phy_pages_allow_count: %d\n", pmm_free_pages_count() );
    }
    local_intr_restore(intr_flag);
    return;
}

ptr_t pmm_alloc(size_t byte) {
    ptr_t page;
    page = pmm_manager->pmm_manage_alloc(byte);
    return page;
}

void pmm_free(ptr_t addr, size_t byte) {
    pmm_manager->pmm_manage_free(addr, byte);
    return;
}

size_t pmm_free_pages_count(void) {
    return pmm_manager->pmm_manage_free_pages_count();
}

#ifdef __cplusplus
}
#endif
