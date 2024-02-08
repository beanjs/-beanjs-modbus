#ifndef __MODBUS_DEFINE_H__
#define __MODBUS_DEFINE_H__

#include "buffer.h"
#include "inttypes.h"
#include "stdbool.h"

#define MODBUS_OPCODE_READ_COILS (0x01)
#define MODBUS_OPCODE_DISCRETE_INPUTS (0x02)
#define MODBUS_OPCODE_READ_HOLDING_REGISTERS (0x03)
#define MODBUS_OPCODE_READ_INPUT_REGISTERS (0x04)
#define MODBUS_OPCODE_WRITE_COIL (0x05)
#define MODBUS_OPCODE_WRITE_REGISTER (0x06)
#define MODBUS_OPCODE_WRITE_COILS (0x0F)
#define MODBUS_OPCODE_WRITE_REGISTERS (0x10)

#define MODBUS_OPCODE_ERROR_MASK (0x80)
#define MODBUS_OPCODE_FUNC_MASK (0x7F)

#define MODBUS_WRITE_COIL_TRUE (0xFF00)
#define MODBUS_WRITE_COIL_FALSE (0x0000)

#define MODBUS_OPCODE_IS_ERROR(op) \
  ((op & MODBUS_OPCODE_ERROR_MASK) == MODBUS_OPCODE_ERROR_MASK)

#define MODBUS_OPCODE_FUNC(op) ((op) & (MODBUS_OPCODE_FUNC_MASK))

#define MODBUS_OPCODE_ALLOWED(op)                                      \
  ((MODBUS_OPCODE_FUNC(op) == MODBUS_OPCODE_READ_COILS) ||             \
   (MODBUS_OPCODE_FUNC(op) == MODBUS_OPCODE_DISCRETE_INPUTS) ||        \
   (MODBUS_OPCODE_FUNC(op) == MODBUS_OPCODE_READ_HOLDING_REGISTERS) || \
   (MODBUS_OPCODE_FUNC(op) == MODBUS_OPCODE_READ_INPUT_REGISTERS) ||   \
   (MODBUS_OPCODE_FUNC(op) == MODBUS_OPCODE_WRITE_COIL) ||             \
   (MODBUS_OPCODE_FUNC(op) == MODBUS_OPCODE_WRITE_REGISTER) ||         \
   (MODBUS_OPCODE_FUNC(op) == MODBUS_OPCODE_WRITE_COILS) ||            \
   (MODBUS_OPCODE_FUNC(op) == MODBUS_OPCODE_WRITE_REGISTERS) ||        \
   (MODBUS_OPCODE_IS_ERROR(op)))

#define MODBUS_REQUEST_PAYLOAD_BIT(op) ((op == MODBUS_OPCODE_WRITE_COILS))
#define MODBUS_REQUEST_PAYLOAD_U16(op) ((op == MODBUS_OPCODE_WRITE_REGISTERS))

#define MODBUS_REQUEST_HAS_PAYLOAD(op) \
  (MODBUS_REQUEST_PAYLOAD_BIT(op) || (MODBUS_REQUEST_PAYLOAD_U16(op)))

#define MODBUS_REPLY_PAYLOAD_IS_BIT(op) \
  ((op == MODBUS_OPCODE_READ_COILS) ||  \
   (op == MODBUS_OPCODE_DISCRETE_INPUTS) || MODBUS_OPCODE_IS_ERROR(op))

#define MODBUS_REPLY_PAYLOAD_IS_U16(op)            \
  ((op == MODBUS_OPCODE_READ_HOLDING_REGISTERS) || \
   (op == MODBUS_OPCODE_READ_INPUT_REGISTERS))

#define MODBUS_REPLY_HAS_ATTR(op)                                              \
  ((op == MODBUS_OPCODE_WRITE_COIL) || (op == MODBUS_OPCODE_WRITE_REGISTER) || \
   (op == MODBUS_OPCODE_WRITE_COILS) || (op == MODBUS_OPCODE_WRITE_REGISTERS))

#define MODBUS_REPLY_HAS_PAYLOAD(op) \
  (MODBUS_REPLY_PAYLOAD_IS_BIT(op) || (MODBUS_REPLY_PAYLOAD_IS_U16(op)))

#define MODBUS_BROADCAST_ADDRESS (0)
#define MODBUS_PAYLOAD_BUFFER_SIZE (256)

typedef enum {
  MODBUS_ROLE_SLAVE = 0,
  MODBUS_ROLE_MASTER = 1,
} modbus_role_t;

typedef void (*modbus_hook_t)(uint8_t addr, void *reqOrRep);
typedef void (*modbus_free_t)(void *);

typedef struct {
  uint8_t opcode;
  uint16_t address;
  union {
    uint16_t length;
    uint16_t value;
  };
  struct {
    uint8_t length;
    union {
      uint8_t *u8;
      uint16_t *u16;
    };
  } payload;
} modbus_request_t;

typedef struct {
  uint8_t opcode;
  uint16_t address;
  union {
    uint16_t length;
    uint16_t value;
  };
  struct {
    uint8_t length;
    union {
      uint8_t *u8;
      uint16_t *u16;
    };
  } payload;
} modbus_reply_t;

typedef struct {
  uint16_t transaction;
  uint16_t protocol;
} modbus_mbap_t;

typedef struct {
  uint8_t addr;
  union {
    modbus_request_t req;
    modbus_reply_t rep;
  };
  void *extra;
} modbus_package_t;

typedef struct {
  modbus_hook_t reserve0;
  modbus_hook_t read_coils;
  modbus_hook_t read_discrete_inputs;
  modbus_hook_t read_holding_registers;
  modbus_hook_t read_input_registers;
  modbus_hook_t write_coil;
  modbus_hook_t write_register;
  modbus_hook_t reserve7;
  modbus_hook_t reserve8;
  modbus_hook_t reserve9;
  modbus_hook_t reserve10;
  modbus_hook_t reserve11;
  modbus_hook_t reserve12;
  modbus_hook_t reserve13;
  modbus_hook_t reserve14;
  modbus_hook_t write_coils;
  modbus_hook_t write_registers;
  modbus_hook_t forward;
} modbus_hooks_t;

typedef struct {
  void (*init)(void *this);
  void (*kill)(void *this);
} modbus_driver_t;

typedef struct {
  void (*init)(void *this);
  void (*kill)(void *this);

  int (*recv)(void *this, uint8_t *buf, int max);
  int (*send)(void *this, uint8_t *buf, int len);

  modbus_buffer_t inbuf;
  modbus_buffer_t oubuf;
} modbus_driver_rtu_t;

typedef struct {
  void (*init)(void *this);
  void (*kill)(void *this);

  int (*recv)(void *this, uint8_t *buf, int max);
  int (*send)(void *this, uint8_t *buf, int len);

  uint8_t *cache;
  uint16_t cache_len;
  uint8_t *extra;
  uint16_t extra_len;

  union {
    struct {
      int sock;
    } slave;
  };
} modbus_driver_socket_t;

typedef struct {
  bool (*decode)(modbus_role_t role, modbus_package_t *p, void *driver);
  bool (*encode)(modbus_role_t role, modbus_package_t *p, void *driver);
} modbus_parser_t;

typedef struct {
  modbus_role_t role;
  modbus_parser_t *parser;
  modbus_hooks_t hooks;

  void *driver;
  void *extra;

  union {
    struct {
      uint8_t addr;
    } slave;
  };

} modbus_t;

#endif