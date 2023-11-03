#ifndef __MODBUS_CONFIG_H__
#define __MODBUS_CONFIG_H__
#include "inttypes.h"

#define MODBUS_ADDR_BROADCAST (0)
#define MODBUS_ADDR_SLAVE (1)
#define MODBUS_MALLOC_MAX (256 * 2)

void* modbus_arch_malloc(int size);
void modbus_arch_free(void* ptr);
void modbus_arch_memset(void* s, int c, int l);
void modbus_arch_memcpy(void* d, void* s, int l);
uint16_t modbus_arch_htons(uint16_t v);

#endif