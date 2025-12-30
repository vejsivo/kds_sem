#ifndef CRC_H
#define CRC_H

#include <stdint.h>
#include <stddef.h>


void crc32_init(void);

uint32_t crc32(const uint8_t *buf, size_t len);

#endif
