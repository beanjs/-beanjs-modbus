#include "buffer.h"

#include "config.h"

void modbus_buffer_init(modbus_buffer_t* b, int capacity) {
  modbus_arch_memset(b, 0, sizeof(modbus_buffer_t));

  b->capacity = capacity;
  b->raws = modbus_arch_malloc(capacity);
  b->flag |= MODBUS_BUFFER_EMPTY;
}

void modbus_buffer_kill(modbus_buffer_t* b) {
  modbus_arch_free(b->raws);
  modbus_arch_memset(b, 0, sizeof(modbus_buffer_t));
}

void modbus_buffer_skip(modbus_buffer_t* b, int len) {
  int length = modbus_buffer_length(b);
  if (len > length) {
    len = length;
  }

  b->readpos += len;
  b->readpos %= b->capacity;
  if (b->readpos == b->writpos) {
    b->flag |= MODBUS_BUFFER_EMPTY;
  }
  b->flag &= ~MODBUS_BUFFER_FULL;
}

void modbus_buffer_copy(modbus_buffer_t* d, modbus_buffer_t* s) {
  modbus_arch_memcpy(d, s, sizeof(modbus_buffer_t));
}

bool modbus_buffer_is_full(modbus_buffer_t* b) {
  return (b->flag & MODBUS_BUFFER_FULL) == MODBUS_BUFFER_FULL;
}

bool modbus_buffer_is_empty(modbus_buffer_t* b) {
  return (b->flag & MODBUS_BUFFER_EMPTY) == MODBUS_BUFFER_EMPTY;
}

int modbus_buffer_length(modbus_buffer_t* b) {
  if (modbus_buffer_is_empty(b)) return 0;
  if (modbus_buffer_is_full(b)) return b->capacity;

  int len = 0;
  len += b->writpos - b->readpos;
  len += b->capacity;
  len %= b->capacity;
  return len;
}

int modbus_buffer_free(modbus_buffer_t* b) {
  return b->capacity - modbus_buffer_length(b);
}

int modbus_buffer_write(modbus_buffer_t* b, uint8_t* raw, int len) {
  int writed = 0;
  if (modbus_buffer_free(b) < len) {
    return writed;
  }

  uint8_t* write_pos;
  int write_len;

  write_pos = &b->raws[b->writpos];
  if (b->readpos > b->writpos) {
    write_len = (b->readpos - b->writpos);
  } else {
    write_len = (b->capacity - b->writpos);
  }

  if (write_len > len) {
    write_len = len;
  }

  modbus_arch_memcpy(write_pos, raw, write_len);
  b->writpos += write_len;
  b->writpos %= b->capacity;
  if (b->writpos == b->readpos) {
    b->flag |= MODBUS_BUFFER_FULL;
  }
  b->flag &= ~MODBUS_BUFFER_EMPTY;

  len -= write_len;
  raw += write_len;
  writed += write_len;

  if (len > 0) {
    writed += modbus_buffer_write(b, raw, len);
  }

  return writed;
}

int modbus_buffer_read(modbus_buffer_t* b, uint8_t* raw, int len) {
  int readed = 0;

  if (modbus_buffer_is_empty(b)) {
    return readed;
  }

  uint8_t* read_pos;
  int read_len;

  read_pos = &b->raws[b->readpos];
  if (b->writpos > b->readpos) {
    read_len = b->writpos - b->readpos;
  } else {
    read_len = b->capacity - b->readpos;
  }

  if (read_len > len) {
    read_len = len;
  }

  modbus_arch_memcpy(raw, read_pos, read_len);
  b->readpos += read_len;
  b->readpos %= b->capacity;
  if (b->readpos == b->writpos) {
    b->flag |= MODBUS_BUFFER_EMPTY;
  }
  b->flag &= ~MODBUS_BUFFER_FULL;

  len -= read_len;
  raw += read_len;
  readed += read_len;

  if (len > 0) {
    readed += modbus_buffer_read(b, raw, len);
  }

  return readed;
}

bool modbus_buffer_read_u8(modbus_buffer_t* b, uint8_t* v) {
  int readed = modbus_buffer_read(b, v, 1);

  return readed == 1;
}

bool modbus_buffer_write_u8(modbus_buffer_t* b, uint8_t* v) {
  int writed = modbus_buffer_write(b, v, 1);

  return writed == 1;
}

bool modbus_buffer_read_u16(modbus_buffer_t* b, uint16_t* v, bool msb) {
  uint16_t val;
  int readed = modbus_buffer_read(b, (uint8_t*)&val, 2);
  if (msb) {
    val = modbus_arch_htons(val);
  }

  *v = val;
  return readed == 2;
}

bool modbus_buffer_write_u16(modbus_buffer_t* b, uint16_t* v, bool msb) {
  uint16_t val = *v;
  if (msb) {
    val = modbus_arch_htons(val);
  }

  int writed = modbus_buffer_write(b, (uint8_t*)&val, 2);
  return writed == 2;
}