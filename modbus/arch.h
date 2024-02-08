#ifndef __MODBUS_ARCH_H__
#define __MODBUS_ARCH_H__

#include <stdio.h>

#include "inttypes.h"

void* modbus_arch_malloc(int size);
void modbus_arch_free(void* ptr);
void modbus_arch_memset(void* s, int c, int l);
void modbus_arch_memcpy(void* d, void* s, int l);
uint16_t modbus_arch_htons(uint16_t v);

#endif