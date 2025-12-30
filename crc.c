#include "crc.h"

static uint32_t crc32_table[256];
static int crc32_initialized = 0;

void crc32_init(void)
{
    if (crc32_initialized)
        return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++){
            c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }

    crc32_initialized = 1;
}

uint32_t crc32(const uint8_t *buf, size_t len)
{
    uint32_t c = 0xFFFFFFFFU;

    for (size_t i = 0; i < len; i++)
        c = crc32_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);

    return c ^ 0xFFFFFFFFU;
}
