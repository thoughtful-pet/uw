#include "string.h"

#include "uw.h"

static void* std_alloc(unsigned nbytes)
{
    return calloc(1, nbytes);
}

static void* std_realloc(void* block, unsigned old_nbytes, unsigned new_nbytes)
{
    void* new_block = realloc(block, new_nbytes);
    if (new_block && new_nbytes > old_nbytes) {
        memset(((uint8_t*) new_block) + old_nbytes, 0, new_nbytes - old_nbytes);
    }
    return new_block;
}

static void std_free(void* block, unsigned nbytes)
{
    free(block);
}

UwAllocator _uw_std_allocator = {
    .alloc   = std_alloc,
    .realloc = std_realloc,
    .free    = std_free
};


static void* nofail_alloc(unsigned nbytes)
{
    void* block = std_alloc(nbytes);
    if (!block) {
        uw_panic("std_alloc(%u)\n", nbytes);
    }
    return block;
}

static void* nofail_realloc(void* block, unsigned old_nbytes, unsigned new_nbytes)
{
    void* new_block = std_realloc(block, old_nbytes, new_nbytes);
    if (!new_block) {
        uw_panic("std_realloc(%p, %u, %u)\n", block, old_nbytes, new_nbytes);
    }
    return new_block;
}

static void nofail_free(void* block, unsigned nbytes)
{
    free(block);
}

UwAllocator _uw_nofail_allocator = {
    .alloc   = nofail_alloc,
    .realloc = nofail_realloc,
    .free    = nofail_free
};


UwAllocator _uw_default_allocator = {
    .alloc   = std_alloc,
    .realloc = std_realloc,
    .free    = std_free
};

UwAllocator _uw_pchunks_allocator = {
    .alloc   = std_alloc,
    .realloc = std_realloc,
    .free    = std_free
};
