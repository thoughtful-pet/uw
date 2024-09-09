/*
 * AMW hash based on https://github.com/Nicoshev/rapidhash
 *
 * Rapidhash has the following advantages to be adopted:
 *   - Clean maintainable code.
 *   - Simple algorithm.
 *
 * Implementation differences:
 *   - Tail is padded with zeroes and the code is simplified.
 *   - Although data length is known for all AMW values, it is excluded
 *     from hash calculations to make AMW code simpler and less error prone.
 *     But it can be uncommented for testing against the original implementation.
 */

#include "uw_value.h"

#include "rapidhash.h"

//#define TRACE(...)  printf(__VA_ARGS__)
#define TRACE(...)

void _uw_hash_init(UwHashContext* ctx /*, size_t len*/)
{
    ctx->seed = RAPID_SEED ^ rapid_mix(RAPID_SEED ^ RAPID_SECRET_0, RAPID_SECRET_1) /* ^ len */;
    ctx->see1 = ctx->seed;
    ctx->see2 = ctx->seed;
    //ctx->len = len;
    ctx->buf_size = 0;
}

void _uw_hash_uint64(UwHashContext* ctx, uint64_t data)
{
    if (ctx->buf_size == 6) {
        ctx->buf_size = 0;

        ctx->seed = rapid_mix(ctx->buffer[0] ^ RAPID_SECRET_0, ctx->buffer[1] ^ ctx->seed);
        ctx->see1 = rapid_mix(ctx->buffer[2] ^ RAPID_SECRET_1, ctx->buffer[3] ^ ctx->see1);
        ctx->see2 = rapid_mix(ctx->buffer[4] ^ RAPID_SECRET_2, ctx->buffer[5] ^ ctx->see2);
        TRACE("AMW round seed=%llx\n", (unsigned long long) ctx->seed);
    }
    ctx->buffer[ctx->buf_size++] = data;
}

UwType_Hash _uw_hash_finish(UwHashContext* ctx)
{
    ctx->seed ^= ctx->see1 ^ ctx->see2;

    while (ctx->buf_size < 2) {
        TRACE("PAD %d\n", ctx->buf_size);
        ctx->buffer[ctx->buf_size++] = 0;
    }

    if (ctx->buf_size > 2) {
        ctx->seed = rapid_mix(ctx->buffer[0] ^ RAPID_SECRET_2, ctx->buffer[1] ^ ctx->seed ^ RAPID_SECRET_1);
        TRACE("AMW %d seed=%llx\n", ctx->buf_size, (unsigned long long) ctx->seed);
        if (ctx->buf_size > 4) {
            ctx->seed = rapid_mix(ctx->buffer[2] ^ RAPID_SECRET_2, ctx->buffer[3] ^ ctx->seed);
            TRACE("AMW %d seed=%llx\n", ctx->buf_size, (unsigned long long) ctx->seed);
        }
    }

    uint64_t a = ctx->buffer[ctx->buf_size - 2] ^ RAPID_SECRET_1;
    uint64_t b = ctx->buffer[ctx->buf_size - 1] ^ ctx->seed;
    TRACE("AMW a=%llx b=%llx\n", (unsigned long long) a, (unsigned long long) b);
    rapid_mum(&a, &b);

    return rapid_mix(a ^ RAPID_SECRET_0 /* ^ ctx->len */, b ^ RAPID_SECRET_1);
}
