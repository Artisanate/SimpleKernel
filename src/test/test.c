
// This file is a part of MRNIU/SimpleKernel (https://github.com/MRNIU/SimpleKernel).
//
// test.c for MRNIU/SimpleKernel.

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "stdint.h"
#include "debug.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "../drv/clock/include/clock.h"
#include "../drv/keyboard/include/keyboard.h"
#include "heap/heap.h"
#include "task/task.h"
#include "sched/sched.h"
#include "include/test.h"

bool test_pmm(void) {
    printk_test("Test pmm_alloc :\n");
    ptr_t allc_addr1 = pmm_alloc(1);
    printk_test("pmm_alloc addr: 0x%08X\n", allc_addr1);
    ptr_t allc_addr2 = pmm_alloc(9000);
    printk_test("pmm_alloc addr: 0x%08X\n", allc_addr2);
    printk_test("Free pages count: %d\n", pmm_free_pages_count() );
    pmm_free(allc_addr2, 9000);
    printk_test("Free pages count: %d\n", pmm_free_pages_count() );
    ptr_t allc_addr3 = pmm_alloc(1);
    printk_test("pmm_alloc addr: 0x%08X\n", allc_addr3);
    ptr_t allc_addr4 = pmm_alloc(1);
    printk_test("pmm_alloc addr: 0x%08X\n", allc_addr4);
    pmm_free(allc_addr1, 1);
    pmm_free(allc_addr3, 1);
    pmm_free(allc_addr4, 1);
    printk_test("Free pages count: %d\n", pmm_free_pages_count() );
    printk_test("---------Pmm test end.---------\n");
    return true;
}

bool test_vmm(void) {
    printk_test("Test vmm:\n");
    printk_test("map1:\n");
    ptr_t pa1 = pmm_alloc(VMM_PAGE_SIZE);
    ptr_t va1 = 0x80000;
    map(pgd_kernel, va1, pa1, VMM_PAGE_PRESENT | VMM_PAGE_RW);
    bzero( (void *)va1, VMM_PAGE_SIZE);

    printk_test("map2:\n");
    ptr_t pa2 = pmm_alloc(VMM_PAGE_SIZE);
    ptr_t va2 = 0x81000;
    map(pgd_kernel, va2, pa2, VMM_PAGE_PRESENT | VMM_PAGE_RW);
    bzero( (void *)va2, VMM_PAGE_SIZE);


    printk_test("---------Vmm test end.---------\n");
    return true;
}

bool test_libc(void) {
    return true;
}

int32_t test_thread1(void * arg __UNUSED__) {
    for(int i = 0 ; i < 79 ; i++) {
        printk("1");
    }
    printk("\n");
    return 1;
}

int32_t test_thread2(void * arg __UNUSED__) {
    for(int i = 0 ; i < 79 ; i++) {
        printk("2");
    }
    printk("\n");
    return 2;
}

int32_t test_thread3(void * arg __UNUSED__) {
    for(int i = 0 ; i < 79 ; i++) {
        printk("3");
    }
    printk("\n");
    return 3;
}

bool test_task(void) {
    pid_t pid1 = kernel_thread(test_thread1, NULL, 0);
    printk_debug("kernel_thread(test_thread1, NULL, 0), pid: 0x%08X\n", pid1);
    pid_t pid2 = kernel_thread(test_thread2, NULL, 0);
    printk_debug("kernel_thread(test_thread2, NULL, 0), pid: 0x%08X\n", pid2);
    // pid_t pid3 = kernel_thread(test_thread3, NULL, 0);
    // printk_debug("kernel_thread(test_thread3, NULL, 0), pid: 0x%08X\n", pid3);

    // printk_debug("set_task_name\n");
    // set_task_name(pid1, "test1");
    // set_task_name(pid2, "test2");
    // set_task_name(pid3, "test3");
    // show_task(TASK_MAX + 1);

    // printk_debug("Show init task info:\n");
    // printk_debug("----------------------------\t\n");
    // show_task(0);
    printk_debug("----------------------------\t\n");
    // while(1);
    printk_debug("test task switch\n");
    task_pcb_t * task0 = get_task(0);
    task_pcb_t * task1 = get_task(1);
    task_pcb_t * task2 = get_task(2);
    printk_debug("switch from 0 to 1\n");

    switch_to(task0, task1, task0);

    // printk_debug("switch from 0 to 2\n");
    // switch_to(task0, task2, task0);
    // printk_debug("switch from 2 to 1\n");
    // printk_debug("switch from 1 to 0\n");

    printk_test("---------Task test end.---------\n");
    return true;
}

bool test_sched(void) {
    return true;
}

bool test_heap(void) {
    printk_test("Test Heap kmalloc :\n");
    ptr_t allc_addr1 = (ptr_t)kmalloc(1);
    printk_test("kmalloc heap addr: 0x%08X\n", allc_addr1);
    ptr_t allc_addr2 = (ptr_t)kmalloc(9000);
    printk_test("kmalloc heap addr: 0x%08X\n", allc_addr2);
    ptr_t allc_addr3 = (ptr_t)kmalloc(4095);
    printk_test("kmalloc heap addr: 0x%08X\n", allc_addr3);
    ptr_t allc_addr4 = (ptr_t)kmalloc(12);
    printk_test("kmalloc heap addr: 0x%08X\n", allc_addr4);
    printk_test("Test Heap kfree: 0x%08X\n", allc_addr1);
    kfree( (ptr_t)allc_addr1);
    printk_test("Test Heap kfree: 0x%08X\n", allc_addr2);
    kfree( (ptr_t)allc_addr2);
    printk_test("Test Heap kfree: 0x%08X\n", allc_addr3);
    kfree( (ptr_t)allc_addr3);
    printk_test("Test Heap kfree: 0x%08X\n", allc_addr4);
    kfree( (ptr_t)allc_addr4);
    ptr_t new_addr = (ptr_t)kmalloc(9000);
    printk_test("New kmalloc heap addr: 0x%08X\n", new_addr);
    printk_test("---------Heap test end.---------\n");
    return true;
}

bool test(void) {
    test_pmm();
    // test_vmm();
    // test_libc();
    // test_heap();
    // test_task();
    // test_sched();
    return true;
}

#ifdef __cplusplus
}
#endif
