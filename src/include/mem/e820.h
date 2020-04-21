
// This file is a part of MRNIU/SimpleKernel (https://github.com/MRNIU/SimpleKernel).
//
// e820.h for MRNIU/SimpleKernel.


#ifndef _E820_H_
#define _E820_H_

#define E820_MAX            8
#define E820_RAM            1
#define E820_RESERVED       2
#define E820_ACPI           3
#define E820_NVS            4
#define E820_UNUSABLE       5

typedef uint64_t e820_addr_t;
typedef uint64_t e820_len_t;
typedef uint32_t e820_type_t;

typedef
    struct e820entry {
    e820_addr_t		addr;
    e820_len_t		length;
    e820_type_t		type;
} __attribute__( (packed) ) e820entry_t;

typedef
    struct e820map {
    size_t		nr_map;
    e820entry_t		map[E820_MAX];
} e820map_t;

#endif /* _E820_H_ */
