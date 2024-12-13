#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "uw.h"


#define BUBBLEWRAP  32

typedef struct {
    void* addr;
    union {
        unsigned nbytes;
        void* _padding;
    };
} MemBlockInfo;

bool _uw_allocator_verbose = false;
unsigned _uw_blocks_allocated = 0;

static inline size_t align(size_t n, size_t alignment)
/*
 * Align `n` to `alignment` boundary which must be a power of two or zero.
 */
{
    if (alignment > 1) {
        register size_t a = alignment - 1;
        return (n + a) & ~a;
    } else {
        return n;
    }
}

static inline size_t align_to_page(size_t n)
/*
 * Align `n` to page boundary.
 */
{
    size_t page_size = sysconf(_SC_PAGE_SIZE);
    return align(n, page_size);
}

static unsigned calc_memsize(unsigned nbytes)
{
    return align_to_page(sizeof(MemBlockInfo) + nbytes + BUBBLEWRAP * 2);
}

static uint8_t* block_to_region(void* block)
{
    size_t page_size = sysconf(_SC_PAGE_SIZE);
    return (uint8_t*) (((ptrdiff_t) block) & ~(page_size - 1));
}

static uint8_t* block_from_region(uint8_t* region_start) {
    return region_start + sizeof(MemBlockInfo) + BUBBLEWRAP;
}

static void* do_debug_alloc(const char* caller_name, unsigned nbytes)
{
    unsigned memsize = calc_memsize(nbytes);

    uint8_t* region_start = mmap(NULL, memsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region_start == MAP_FAILED) {
        fprintf(stderr, "%s: cannot mmap %u bytes\n", caller_name, memsize);
        exit(1);
    }
    uint8_t* region_end  = region_start + memsize;
    uint8_t* block_start = region_start + sizeof(MemBlockInfo) + BUBBLEWRAP;
    uint8_t* block_end   = block_start + nbytes;

    if (region_end <= block_end) {
        fprintf(stderr, "%s: region_end %p must be greater than block_end %p\n",
                caller_name, region_end, block_end);
        exit(1);
    }

    memset(region_start, 0xFF, block_start - region_start);
    memset(block_end, 0xFF, region_end - block_end);

    MemBlockInfo* info = (MemBlockInfo*) region_start;
    info->addr = block_start;
    info->nbytes = nbytes;

    _uw_blocks_allocated++;
    return (void*) block_start;
}

static void* debug_alloc(unsigned nbytes)
{
    void* block = do_debug_alloc(__func__, nbytes);
    if (_uw_allocator_verbose) {
        printf("%s: %u bytes -> %p\n", __func__, nbytes, block);
    }
    return block;
}

static bool same_16_chars(uint8_t* block, uint8_t chr)
/*
 * Check if 16 bytes of the block are equal to chr.
 */
{
    for (unsigned i = 0; i < 16; i++ ) {
        if (block[i] != chr) {
            return false;
        }
    }
    return true;
}

static void check_region(const char* caller_name, void* block, unsigned nbytes)
{
    unsigned memsize = calc_memsize(nbytes);
    uint8_t* region_start = block_to_region(block);
    uint8_t* region_end  = region_start + memsize;
    uint8_t* block_start = block_from_region(region_start);
    uint8_t* block_end   = block_start + nbytes;

    unsigned num_damaged_lower = 0;
    for (unsigned i = sizeof(MemBlockInfo) + BUBBLEWRAP - 1; i >= sizeof(MemBlockInfo); i--) {
        if (region_start[i] != 0xFF) {
            num_damaged_lower++;
        }
    }

    unsigned num_damaged_upper = 0;
    for (uint8_t* p = block_end; p < region_end; p++) {
        if (*p != 0xFF) {
            num_damaged_upper++;
        }
    }

    if (num_damaged_upper || num_damaged_lower) {
        fprintf(stderr, "%s: damaged %u bytes below %p and %u bytes above %u\n",
                caller_name, num_damaged_lower, block, num_damaged_upper, nbytes);
        // dump block
        bool prev_row_same_char = false;
        uint8_t prev_row_char = 0;
        bool skipping = false;
        unsigned column = 0;
        for (unsigned i = 0; i < memsize;) {
            if (column == 0) {
                if (prev_row_same_char && memsize - i > 16
                    && same_16_chars(region_start + i, prev_row_char)) {
                    i += 16;
                    if (!skipping) {
                        skipping = true;
                        fputs("...\n",  stderr);
                    }
                    continue;
                }
                prev_row_same_char = true;
                prev_row_char = region_start[i];
                skipping = false;
                fprintf(stderr, "%p: ", (void*) (((ptrdiff_t) region_start) + i));
            }
            if (prev_row_char != region_start[i]) {
                prev_row_same_char = false;
            }
            fprintf(stderr, "%02x", region_start[i++]);
            column++;
            if (column == 8) {
                fprintf(stderr, " - ");
            } else if (column == 16) {
                fputc('\n', stderr);
                column = 0;
            } else {
                fputc(' ', stderr);
            }
        }
        if (column < 16) {
            fputc('\n', stderr);
        }
        exit(1);
    }
}

static void* debug_realloc(void* block, unsigned old_nbytes, unsigned new_nbytes)
{
    if (!block) {
        void* new_block = do_debug_alloc(__func__, new_nbytes);
        if (_uw_allocator_verbose) {
            fprintf(stderr, "%s: %p %u bytes --> %p %u bytes\n",
                    __func__, block, old_nbytes, new_block, new_nbytes);
        }
        return new_block;
    }

    check_region(__func__, block, old_nbytes);

    unsigned old_memsize = calc_memsize(old_nbytes);
    unsigned new_memsize = calc_memsize(new_nbytes);

    uint8_t* region_start = block_to_region(block);
    uint8_t* block_start = block_from_region(region_start);

    uint8_t* new_block_start;
    uint8_t* new_block_end;

    if (new_memsize > old_memsize) {
        uint8_t* new_region_start = mmap(NULL, new_memsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (new_region_start == MAP_FAILED) {
            fprintf(stderr, "%s: cannot mmap %u bytes\n", __func__, new_memsize);
            exit(1);
        }
        uint8_t* new_region_end  = new_region_start + new_memsize;

        new_block_start = new_region_start + sizeof(MemBlockInfo) + BUBBLEWRAP;
        new_block_end   = new_block_start + new_nbytes;

        memcpy(new_region_start, region_start, (block_start - region_start) + old_nbytes);
        memset(new_block_end, 0xFF, new_region_end - new_block_end);

        MemBlockInfo* info = (MemBlockInfo*) new_region_start;
        info->addr = new_block_start;
        info->nbytes = new_nbytes;

        munmap(region_start, old_memsize);

    } else {
        new_block_start = block_start;
        new_block_end   = block_start + new_nbytes;

        MemBlockInfo* info = (MemBlockInfo*) region_start;
        info->nbytes = new_nbytes;
    }

    if (new_nbytes > old_nbytes) {
        memset(new_block_start + old_nbytes, 0, new_nbytes - old_nbytes);
    } else if (new_nbytes < old_nbytes) {
        memset(new_block_start + new_nbytes, 0xFF, old_nbytes - new_nbytes);
    }

    if (_uw_allocator_verbose) {
        fprintf(stderr, "%s: %p %u bytes --> %p %u bytes\n",
                __func__, block, old_nbytes, new_block_start, new_nbytes);
    }
    return (void*) new_block_start;
}

static void debug_free(void* block, unsigned nbytes)
{
    check_region(__func__, block, nbytes);

    uint8_t* region_start = block_to_region(block);
    unsigned memsize = calc_memsize(nbytes);

    munmap(region_start, memsize);

    if (_uw_allocator_verbose) {
        fprintf(stderr, "%s: %p %u bytes\n", __func__, block, nbytes);
    }
    _uw_blocks_allocated--;
}

UwAllocator _uw_debug_allocator = {
    .alloc   = debug_alloc,
    .realloc = debug_realloc,
    .free    = debug_free
};
