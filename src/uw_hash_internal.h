#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct _UwHashContext {
    uint64_t seed;
    uint64_t see1;
    uint64_t see2;
    uint64_t buffer[6];
    //size_t len;
    int buf_size;
};

void _uw_hash_init(UwHashContext* ctx /*, size_t len */);
void _uw_hash_uint64(UwHashContext* ctx, uint64_t data);  // this prototype is duplicated in uw_value_base.h
UwType_Hash _uw_hash_finish(UwHashContext* ctx);

#ifdef __cplusplus
}
#endif
