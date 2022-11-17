#!/usr/bin/env python3


# This script is set to test translation bins and whatnot

handle_bits = 64 # total number of bits in the address
max_levels = 3
bits_per_level = 9
size_bits = 0
arena_bits = 0
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

print(f'// ', end='')
bits(64, '*')
print('|')

for n in range(0, 64):
    # the offset into the allocation
    offset_bits = n + 6
    # the size of the bin in the handle address space
    size = size_base ** offset_bits


    # How many bits are available for indirection?
    info_bits = 1 + size_bits + arena_bits
    mapping_bits = 31
    wasted = (64 - n) - (mapping_bits + info_bits)
    if wasted < 0:
        break

    print(f'// ', end='')
    bits(info_bits, 'F')
    bits(mapping_bits, '*')
    bits(wasted, '_')
    bits(n, '.')

    print(f'| {size}')
