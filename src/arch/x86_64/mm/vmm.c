
// This file is a part of MRNIU/SimpleKernel (https://github.com/MRNIU/SimpleKernel).
//
// vmm.c for MRNIU/SimpleKernel.

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "string.h"
#include "cpu.hpp"
#include "debug.h"
#include "sync.hpp"
#include "intr/include/intr.h"
#include "mem/vmm.h"

// 内核页目录区域
pgd_t pgd_kernel[VMM_PAGE_TABLES_PRE_PAGE_DIRECTORY] __attribute__( (aligned(VMM_PAGE_SIZE) ) );
// 内核页表区域
pte_t pte_kernel[VMM_PAGE_TABLES_KERNEL][VMM_PAGES_PRE_PAGE_TABLE] __attribute__( (aligned(VMM_PAGE_SIZE) ) );
// 内核栈区域
pte_t pte_kernel_stack[VMM_PAGES_PRE_PAGE_TABLE] __attribute__( (aligned(VMM_PAGE_SIZE) ) );

void vmm_init(void) {
    bool intr_flag = false;
    local_intr_store(intr_flag);
    {
        register_interrupt_handler(INT_PAGE_FAULT, &page_fault);

        // 映射全部内核
        uint32_t pgd_idx = VMM_PGD_INDEX(KERNEL_BASE);
        for(uint32_t i = pgd_idx, j = 0 ; i < VMM_PAGE_DIRECTORIES_KERNEL + pgd_idx ; i++, j++) {
            pgd_kernel[i] = ( (ptr_t)VMM_LA_PA( (ptr_t)pte_kernel[j]) | VMM_PAGE_PRESENT | VMM_PAGE_RW | VMM_PAGE_KERNEL);
        }
        ptr_t * pte = (ptr_t *)pte_kernel;
        for(uint32_t i = 0 ; i < VMM_PAGES_PRE_PAGE_TABLE * VMM_PAGE_TABLES_KERNEL ; i++) {
            pte[i] = (i << 12) | VMM_PAGE_PRESENT | VMM_PAGE_RW | VMM_PAGE_KERNEL;
        }
        // 映射内核栈
        // 0x2FF
        pgd_idx = VMM_PGD_INDEX(KERNEL_STACK_TOP);
        pgd_kernel[pgd_idx] = ( (ptr_t)VMM_LA_PA( (ptr_t)pte_kernel_stack) | VMM_PAGE_PRESENT | VMM_PAGE_RW | VMM_PAGE_KERNEL);
        // i: 0x3F8~0x400
        for(uint32_t i = VMM_PAGES_PRE_PAGE_TABLE - KERNEL_STACK_PAGES, j = VMM_PAGES_PRE_PAGE_TABLE * 2 ; i < VMM_PAGES_PRE_PAGE_TABLE ; i++, j++) {
            pte_kernel_stack[i] = (j << 12) | VMM_PAGE_PRESENT | VMM_PAGE_RW | VMM_PAGE_KERNEL;
        }
        switch_pgd(VMM_LA_PA( (ptr_t)pgd_kernel) );
        printk_info("vmm_init\n");
    }
    local_intr_restore(intr_flag);
    return;
}

// 以页为单位
void map(pgd_t * pgd_now, ptr_t va, ptr_t pa, uint32_t flags) {
    uint32_t pgd_idx = VMM_PGD_INDEX(va);
    uint32_t pte_idx = VMM_PTE_INDEX(va);
    // 从页目录中查找页表
    pte_t * pte = (pte_t *)(pgd_now[pgd_idx] & VMM_PAGE_MASK);
    printk_debug("pte1:0x%08X pte_idx:0x%08X", pte, pte_idx);
    printk_debug("pgd_now[pgd_idx]:0x%08X &pgd_now[pgd_idx]:0x%08X", pgd_now[pgd_idx], &pgd_now[pgd_idx]);
    // 如果页表不存在
    if(pte == NULL) {
        // 创建页表，分配空间
        pte = (pte_t *)pmm_alloc(VMM_PAGE_SIZE);
        printk_debug("pte1.5:0x%08X VMM_PA_LA( (ptr_t)pte):0x%08X", pte, VMM_PA_LA( (ptr_t)pte) );
        // 添加到页目录中
        pgd_now[pgd_idx] = (ptr_t)( (ptr_t)pte | VMM_PAGE_PRESENT | VMM_PAGE_RW | VMM_PAGE_KERNEL);
        // 这一步需要获取 pte 的线性地址，直接转换显然是错误的
        pte = (pte_t *)VMM_PA_LA( (ptr_t)pte);

        // printk_debug("---2:0x%08X ", pgd_now[pgd_idx]);
    } else {
        // 这一步需要获取 pte 的线性地址，直接转换显然是错误的
        // pte = (pte_t *)VMM_PA_LA( (ptr_t)pte);
    }
    pte[pte_idx] = (pa & VMM_PAGE_MASK) | flags;
    printk_debug("pte2:0x%08X &pte[pte_idx]:0x%08X pte[233]:0x%08X", pte, &pte[pte_idx], pte[233]);

    // 通知 CPU 更新页表缓存
    CPU_INVLPG(va);
    return;
}

void unmap(pgd_t * pgd_now, ptr_t va) {
    uint32_t pgd_idx = VMM_PGD_INDEX(va);
    uint32_t pte_idx = VMM_PTE_INDEX(va);
    pte_t * pte = (pte_t *)(pgd_now[pgd_idx] & VMM_PAGE_MASK);
    // 转换到内核线性地址
    pte = (pte_t *)VMM_PA_LA( (ptr_t)pte);
    pte[pte_idx] = 0;
    // 通知 CPU 更新页表缓存
    CPU_INVLPG(va);
    return;
}

// 如果虚拟地址 va 映射到物理地址则返回 1
// 同时如果 pa 不是空指针则把物理地址写入 pa 参数
uint32_t get_mapping(pgd_t * pgd_now, ptr_t va, ptr_t * pa) {
    // printk_debug("pgd_now: 0x%08X, va: 0x%08X\n", pgd_now, va);
    uint32_t pgd_idx = VMM_PGD_INDEX(va);
    uint32_t pte_idx = VMM_PTE_INDEX(va);
    // printk_debug("pgd_idx: 0x%08X, pte_idx: 0x%08X\t", pgd_idx, pte_idx);
    pte_t * pte = (pte_t *)(pgd_now[pgd_idx] & VMM_PAGE_MASK);
    // printk_debug("pte: 0x%08X ", pte);
    if(pte == NULL) {
        return 0;
    }
    // 转换到内核线性地址
    pte = (pte_t *)VMM_PA_LA( (ptr_t)pte);
    // printk_debug("pte: 0x%08X, pte[pte_idx]: 0x%08X\n", pte, pte[pte_idx]);
    // 如果地址有效而且指针不为 NULL
    if( (void *)pte[pte_idx] != NULL) {

        if( (void *)pa != NULL) {
            *pa = pte[pte_idx] & VMM_PAGE_MASK;
        }
        // printk_debug("-+++++++++\n");
        return 1;
    }
    return 0;
}

void switch_pgd(ptr_t pd) {
    bool intr_flag = false;
    local_intr_store(intr_flag);
    {
        __asm__ volatile ("mov %0, %%cr3" : : "r" (pd) );
    }
    local_intr_restore(intr_flag);
}

void page_fault(pt_regs_t * pt_regs) {
#ifdef __x86_64__
    uint64_t cr2;
    __asm__ volatile ("movq %%cr2,%0" : "=r" (cr2) );
#else
    uint32_t cr2;
    __asm__ volatile ("mov %%cr2,%0" : "=r" (cr2) );
#endif
    printk("Page fault at 0x%08X, virtual faulting address 0x%08X\n", pt_regs->eip, cr2);
    printk_err("Error code: 0x%08X\n", pt_regs->err_code);

    // bit 0 为 0 指页面不存在内存里
    if(!(pt_regs->err_code & 0x1) )
        printk_color(red, "Because the page wasn't present.\n");
    // bit 1 为 0 表示读错误，为 1 为写错误
    if(pt_regs->err_code & 0x2)
        printk_err("Write error.\n");
    else
        printk_err("Read error.\n");
    // bit 2 为 1 表示在用户模式打断的，为 0 是在内核模式打断的
    if(pt_regs->err_code & 0x4)
        printk_err("In user mode.\n");
    else
        printk_err("In kernel mode.\n");
    // bit 3 为 1 表示错误是由保留位覆盖造成的
    if(pt_regs->err_code & 0x8)
        printk_err("Reserved bits being overwritten.\n");
    // bit 4 为 1 表示错误发生在取指令的时候
    if(pt_regs->err_code & 0x10)
        printk_err("The fault occurred during an instruction fetch.\n");
    while(1);
}

#ifdef __cplusplus
}
#endif
