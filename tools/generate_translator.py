#!/usr/bin/env python3


# This script is set to test translation bins and whatnot

handle_bits = 64 # total number of bits in the address
max_levels = 3
bits_per_level = 9
size_bits = 5
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
print(f'#include <stdio.h>')
print(f'#include <alaska/translation_types.h>')
print(f'#define unlikely(x) __builtin_expect((x), 0)')


size_drillers = {} # S -> F

for n in range(0, size_base ** size_bits):
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
    if remaining < 0 or indirection_levels <= 1:
        # print(f'cannot do higher than n={n}')
        break

    size_drillers[n] = f'alaska_map_drill_{size}'

    print(f'')
    print(f'// ', end='')
    bits(1, 'F')
    bits(arena_bits, 'A')
    bits(size_bits, 'S')
    bits(wasted_bits, '_')
    for i in range(indirection_levels):
        bits(bits_per_level, str(i))
    bits(offset_bits, '.')

    print()

    level_types = ['uint64_t'] * (indirection_levels - 1)
    level_types.append('alaska_map_entry_t')

    print(f'alaska_map_entry_t *alaska_map_drill_{size}(uint64_t address, uint64_t *lvl0, int allocate) {{')

    for i in range(indirection_levels):
        # if i == 0:
        print(f'  uint64_t ind{i} = (address >> {offset_bits + bits_per_level * (indirection_levels - i - 1)}) & 0b{(2 << (bits_per_level - 1)) - 1:b}; // index into {level_types[i]}')
    # we have the table for level 0 (it was passed in as an argument). This loop computes the *next*
    # table based on the previous one. Iteration 0 (i=0) computes and outputs a variable lvl1
    for i in range(indirection_levels - 1):
        level = i + 1 # indirection_levels - i - 1
        lname = f'lvl{level}'
        prevname = f'lvl{i}'
        print(f'  {level_types[level]} *{lname} = ({level_types[level]} *){prevname}[ind{i}];')
        print(f'  if (unlikely(lvl{level} == NULL)) {{ // slow: allocation')
        print(f'    if (allocate == 0) return NULL;')
        print(f'    {lname} = ({level_types[level]} *)alaska_alloc_map_frame(ind{i}, sizeof({level_types[level]}), {2 << bits_per_level - 1});')
        print(f'    {prevname}[ind{i}] = (uint64_t){lname};')
        print(f'  }}')
        # print(f'  current = next;')


    last = indirection_levels - 1
    print(f'  return &lvl{last}[ind{last}];')

    print(f'}}')
    # break


print(f'// A table to map the S bits in a handle address to the drill size')
print(f'alaska_map_driller_t alaska_drillers[{size_base ** size_bits}] = {{')
for n in range(0, size_base ** size_bits):
    if n in size_drillers:
        print(f'  {size_drillers[n]},')
    else:
        print('  NULL,')
    pass

print(f'}};')

# for i in size_drillers:
#     print(i)