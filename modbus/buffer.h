#ifndef __MODBUS_BUFFER_H__
#define __MODBUS_BUFFER_H__

#include "inttypes.h"
#include "stdbool.h"

#define MODBUS_BUFFER_EMPTY 0x01
#define MODBUS_BUFFER_FULL 0x02
#define MODBUS_BUFFER_ALLOC 0x04

typedef struct {
  int capacity;
  int readpos;
  int writpos;
  uint8_t flag;
  uint8_t* raws;
} modbus_buffer_t;

void modbus_buffer_init(modbus_buffer_t* b, int capacity);
void modbus_buffer_kill(modbus_buffer_t* b);

void modbus_buffer_init_reader(modbus_buffer_t* b, uint8_t* src, int len);
void modbus_buffer_init_writer(modbus_buffer_t* b, uint8_t* src, int capacity);

void modbus_buffer_skip(modbus_buffer_t* b, int len);
void modbus_buffer_copy(modbus_buffer_t* d, modbus_buffer_t* s);

int modbus_buffer_length(modbus_buffer_t* b);
int modbus_buffer_free(modbus_buffer_t* b);
bool modbus_buffer_is_empty(modbus_buffer_t* b);
bool modbus_buffer_is_full(modbus_buffer_t* b);

int modbus_buffer_write(modbus_buffer_t* b, uint8_t* raw, int len);
int modbus_buffer_read(modbus_buffer_t* b, uint8_t* raw, int len);

bool modbus_buffer_read_u8(modbus_buffer_t* b, uint8_t* v);
bool modbus_buffer_write_u8(modbus_buffer_t* b, uint8_t* v);

bool modbus_buffer_read_u16(modbus_buffer_t* b, uint16_t* v, bool msb);
bool modbus_buffer_write_u16(modbus_buffer_t* b, uint16_t* v, bool msb);

typedef int (*buffer_stream_t)(void* arg, uint8_t* buf, int max);
int modbus_buffer_writer(modbus_buffer_t* b, buffer_stream_t reader, void* arg);
int modbus_buffer_reader(modbus_buffer_t* b, buffer_stream_t writer, void* arg);
#endif