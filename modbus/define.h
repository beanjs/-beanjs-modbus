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

typedef enum {
  MODBUS_TYPE_SLAVE = 0,
  MODBUS_TYPE_MASTER = 1,
} modbus_type_t;

typedef struct {
  void (*init)();
  void (*kill)();

  int (*recv)(uint8_t* buf, int max);
  int (*send)(uint8_t* buf, int len);
} modbus_driver_t;

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
      uint8_t* u8;
      uint16_t* u16;
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
      uint8_t* u8;
      uint16_t* u16;
    };
  } payload;
} modbus_reply_t;

typedef void (*modbus_hook_t)(uint8_t, void*);
typedef void (*modbus_free_t)(void*);

typedef struct {
  uint8_t addr;
  union {
    modbus_request_t req;
    modbus_reply_t rep;
  };
  uint16_t crc16;
} modbus_package_t;

typedef struct {
  bool (*decode)(modbus_type_t type, modbus_package_t* p, modbus_buffer_t* b);
  bool (*encode)(modbus_type_t type, modbus_package_t* p, modbus_buffer_t* b);
} modbus_parser_t;

typedef struct {
  void (*reserve0)(uint8_t, void*);
  void (*read_coils)(uint8_t addr, modbus_request_t* req);
  void (*read_discrete_inputs)(uint8_t addr, modbus_request_t* req);
  void (*read_holding_registers)(uint8_t addr, modbus_request_t* req);
  void (*read_input_registers)(uint8_t addr, modbus_request_t* req);
  void (*write_coil)(uint8_t addr, modbus_request_t* req);
  void (*write_register)(uint8_t addr, modbus_request_t* req);
  void (*reserve7)(uint8_t, void*);
  void (*reserve8)(uint8_t, void*);
  void (*reserve9)(uint8_t, void*);
  void (*reserve10)(uint8_t, void*);
  void (*reserve11)(uint8_t, void*);
  void (*reserve12)(uint8_t, void*);
  void (*reserve13)(uint8_t, void*);
  void (*reserve15)(uint8_t, void*);
  void (*write_coils)(uint8_t addr, modbus_request_t* req);
  void (*write_registers)(uint8_t addr, modbus_request_t* req);
} modbus_hooks_slave_t;

typedef struct {
  void (*reserve0)(uint8_t, void*);
  void (*read_coils)(uint8_t addr, modbus_reply_t* rep);
  void (*read_discrete_inputs)(uint8_t addr, modbus_reply_t* rep);
  void (*read_holding_registers)(uint8_t addr, modbus_reply_t* rep);
  void (*read_input_registers)(uint8_t addr, modbus_reply_t* rep);
  void (*write_coil)(uint8_t addr, modbus_reply_t* rep);
  void (*write_register)(uint8_t addr, modbus_reply_t* rep);
  void (*reserve7)(uint8_t, void*);
  void (*reserve8)(uint8_t, void*);
  void (*reserve9)(uint8_t, void*);
  void (*reserve10)(uint8_t, void*);
  void (*reserve11)(uint8_t, void*);
  void (*reserve12)(uint8_t, void*);
  void (*reserve13)(uint8_t, void*);
  void (*reserve15)(uint8_t, void*);
  void (*write_coils)(uint8_t addr, modbus_reply_t* rep);
  void (*write_registers)(uint8_t addr, modbus_reply_t* rep);
} modbus_hooks_master_t;

typedef struct {
  uint8_t* temp;
  uint8_t addr;  // slave only
  modbus_type_t type;
  modbus_driver_t* driver;
  modbus_parser_t* parser;
  modbus_buffer_t inbuf;
  modbus_buffer_t oubuf;

  union {
    modbus_hooks_slave_t slave;
    modbus_hooks_master_t master;
  } hooks;

} modbus_t;

#endif