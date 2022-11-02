#!/usr/bin/env python3


# This script is set to test translation bins and whatnot

handle_bits = 64 # total number of bits in the address
max_levels = 6
bits_per_level = 9
size_bits = 4
arena_bits = 3
size_base = 2


# how big is each intermediate table size
intermediate_table_size = 8 * (2 ** bits_per_level)

suffixes = ['B', 'KB', 'MB', 'GB', 'TB', 'PB']
def humansize(nbytes):
    i = 0
    while nbytes >= 1024 and i < len(suffixes)-1:
        nbytes /= 1024.
        i += 1
    f = ('%.2f' % nbytes).rstrip('0').rstrip('.')
    return '%s %s' % (f, suffixes[i])



def bits(count, letter):
    for i in range(count):
        print(letter, end='')



print(f'// Automatically generated. Do not edit!')
print(f'#include <stdlib.h>')
print(f'#include <stdint.h>')
print(f'#include <alaska/translation_types.h>')
print(f'#define unlikely(x) __builtin_expect((x),0)')

for n in range(0, size_bits ** size_bits):
    # the offset into the allocation
    offset_bits = n + 6
    # the size of the bin in the handle address space
    size = size_base ** offset_bits


    # How many bits are available for indirection?
    info_bits = 1 + size_bits + arena_bits
    overhead_without_indirection = info_bits + offset_bits
    remaining_without_indirection = handle_bits - overhead_without_indirection
    indirection_levels = min(remaining_without_indirection // bits_per_level, max_levels)
    indirection_bits = indirection_levels * bits_per_level

    wasted_bits = remaining_without_indirection - indirection_bits

    overhead = info_bits + offset_bits + indirection_bits
    remaining = handle_bits - overhead
    if remaining < 0 or indirection_levels <= 0:
        # print(f'cannot do higher than n={n}')
        break

    print(f'')
    print(f'// n={n}')
    print(f'// size={size}')
    print(f'// ', end='')
    bits(1, 'F')
    bits(arena_bits, 'A')
    bits(size_bits, 'S')
    bits(wasted_bits, '_')
    for i in range(indirection_levels):
        bits(bits_per_level, str(indirection_levels - i - 1))
    bits(offset_bits, '.')
    print()
    print(f'uint64_t *__alaska_drill_size_{size}(uint64_t address, uint64_t *top_level, int allocate) {{')
    print(f'  uint64_t *current = top_level;')
    if indirection_levels > 1:
        print(f'  uint64_t *next;')
    for i in range(indirection_levels):
        print(f'  uint64_t ind{i} = (address >> {offset_bits + bits_per_level * i}) & 0b{(2 << bits_per_level) - 1:b};')
    for i in range(indirection_levels):
        level = indirection_levels - i - 1
        print(f'  // level {level}')
        if i == indirection_levels - 1:
            print(f'  return &current[ind{level}];')
        else:
            print(f'  next = (uint64_t*)current[ind{level}];')
            print(f'  if (unlikely(next == NULL)) {{;')
            print(f'    if (allocate == 0) return NULL;')
            print(f'    next = calloc(sizeof(uint64_t), {2 << bits_per_level - 1});')
            print(f'    current[ind{level}] = (uint64_t)next;')
            print(f'  }}')
            print(f'  current = next;')
    print(f'}}')

    # allocation_count = (2 ** bits_per_level) ** indirection_levels
    # address_space_size = allocation_count * size
    # # print out the handle format
    # bits(1, 'F')
    # bits(arena_bits, 'A')
    # bits(size_bits, 'S')
    # bits(wasted_bits, '_')
    # for i in range(indirection_levels):
    #     bits(bits_per_level, str(indirection_levels - i - 1))
    # bits(offset_bits, '.')

    # table_count = 0 + ((2 ** bits_per_level) ** (indirection_levels - 1))
    # table_cost = intermediate_table_size * table_count
    # table_fraction = table_cost / address_space_size
    # print(f' size: {humansize(size):7s} | {humansize(address_space_size):6s}/{humansize(table_cost):6s} ({int(table_fraction * 100):3}% overhead)')
