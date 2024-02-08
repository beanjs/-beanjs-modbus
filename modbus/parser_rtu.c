#include "parser.h"

static int driver_reader(void *arg, uint8_t *buf, int max) {
  modbus_driver_rtu_t *drv = arg;
  return drv->recv(arg, buf, max);
}

static int driver_writer(void *arg, uint8_t *buf, int len) {
  if (len == 0) return len;
  modbus_driver_rtu_t *drv = arg;
  return drv->send(arg, buf, len);
}

static uint16_t parser_crc16(modbus_buffer_t *b, int len) {
  static const uint16_t table[2] = {0x0000, 0xA001};
  uint16_t crc = 0xFFFF;
  uint8_t val;
  char bit = 0;
  unsigned int xor = 0;

  while (len--) {
    modbus_buffer_read_u8(b, &val);
    crc ^= val;

    for (bit = 0; bit < 8; bit++) {
      xor = crc & 1;
      crc >>= 1;
      crc ^= table[xor];
    }
  }

  return crc;
}

static bool parser_decode_request(modbus_request_t *req, modbus_buffer_t *b) {
  if (!modbus_buffer_read_u16(b, &req->address, true)) {
    return false;
  }

  if (!modbus_buffer_read_u16(b, &req->length, true)) {
    return false;
  }

  if (MODBUS_REQUEST_HAS_PAYLOAD(req->opcode)) {
    if (!modbus_buffer_read_u8(b, &req->payload.length)) {
      return false;
    }

    req->payload.u8 = modbus_arch_malloc(MODBUS_PAYLOAD_BUFFER_SIZE);
    if (MODBUS_REQUEST_PAYLOAD_BIT(req->opcode)) {
      int readed = modbus_buffer_read(b, req->payload.u8, req->payload.length);
      if (readed != req->payload.length) {
        return false;
      }
    }

    if (MODBUS_REQUEST_PAYLOAD_U16(req->opcode)) {
      uint8_t length = req->payload.length / 2;
      uint16_t *ptr = req->payload.u16;
      for (uint8_t i = 0; i < length; i++) {
        if (!modbus_buffer_read_u16(b, ptr + i, true)) {
          return false;
        }
      }
    }
  }

  return true;
}

static bool parser_decode_reply(modbus_reply_t *rep, modbus_buffer_t *b) {
  if (MODBUS_REPLY_HAS_ATTR(rep->opcode)) {
    if (!modbus_buffer_read_u16(b, &rep->address, true)) {
      return false;
    }

    if (!modbus_buffer_read_u16(b, &rep->length, true)) {
      return false;
    }
  }

  if (MODBUS_REPLY_HAS_PAYLOAD(rep->opcode)) {
    rep->payload.u8 = modbus_arch_malloc(MODBUS_PAYLOAD_BUFFER_SIZE);

    if (!MODBUS_OPCODE_IS_ERROR(rep->opcode)) {
      if (!modbus_buffer_read_u8(b, &rep->payload.length)) {
        return false;
      }
    } else {
      rep->payload.length = 1;
    }

    if (MODBUS_REPLY_PAYLOAD_IS_BIT(rep->opcode)) {
      int readed = modbus_buffer_read(b, rep->payload.u8, rep->payload.length);
      if (readed != rep->payload.length) {
        return false;
      }
    }

    if (MODBUS_REPLY_PAYLOAD_IS_U16(rep->opcode)) {
      uint8_t length = rep->payload.length / 2;
      uint16_t *ptr = rep->payload.u16;

      for (uint8_t i = 0; i < length; i++) {
        if (!modbus_buffer_read_u16(b, ptr + i, true)) {
          return false;
        }
      }
    }
  }

  return true;
}

static bool parser_decode(modbus_role_t role, modbus_package_t *p,
                          modbus_buffer_t *b) {
  uint16_t crc16;
  modbus_buffer_t reader, crc_reader;
  modbus_buffer_copy(&reader, b);
  modbus_buffer_copy(&crc_reader, b);

  if (!modbus_buffer_read_u8(&reader, &p->addr)) {
    return false;
  }

  if (!modbus_buffer_read_u8(&reader, &p->req.opcode)) {
    return false;
  }

  if (!MODBUS_OPCODE_ALLOWED(p->req.opcode)) {
    goto on_error;
  }

  if (role == MODBUS_ROLE_SLAVE) {
    if (!parser_decode_request(&p->req, &reader)) {
      return false;
    }
  }

  if (role == MODBUS_ROLE_MASTER) {
    if (!parser_decode_reply(&p->rep, &reader)) {
      return false;
    }
  }

  if (!modbus_buffer_read_u16(&reader, &crc16, false)) {
    return false;
  }

  int len = 0;
  len += modbus_buffer_length(&crc_reader);
  len -= modbus_buffer_length(&reader);

  if (crc16 != parser_crc16(&crc_reader, len - 2)) {
    goto on_error;
  }

  modbus_buffer_copy(b, &reader);
  return true;
on_error:
  modbus_buffer_read(b, (uint8_t *)&crc16, 1);
  return false;
}

static bool parser_encode_reply(modbus_reply_t *rep, modbus_buffer_t *b) {
  if (MODBUS_REPLY_HAS_ATTR(rep->opcode)) {
    if (!modbus_buffer_write_u16(b, &rep->address, true)) {
      return false;
    }

    if (!modbus_buffer_write_u16(b, &rep->length, true)) {
      return false;
    }
  }

  if (MODBUS_REPLY_HAS_PAYLOAD(rep->opcode)) {
    if (!MODBUS_OPCODE_IS_ERROR(rep->opcode)) {
      if (!modbus_buffer_write_u8(b, &rep->payload.length)) {
        return false;
      }
    }

    if (MODBUS_REPLY_PAYLOAD_IS_BIT(rep->opcode)) {
      int writed = modbus_buffer_write(b, rep->payload.u8, rep->payload.length);
      if (writed != rep->payload.length) {
        return false;
      }
    }

    if (MODBUS_REPLY_PAYLOAD_IS_U16(rep->opcode)) {
      uint8_t length = rep->payload.length / 2;
      uint16_t *ptr = rep->payload.u16;
      for (uint8_t i = 0; i < length; i++) {
        if (!modbus_buffer_write_u16(b, ptr + i, true)) {
          return false;
        }
      }
    }
  }

  return true;
}

static bool parser_encode_request(modbus_request_t *req, modbus_buffer_t *b) {
  if (!modbus_buffer_write_u16(b, &req->address, true)) {
    return false;
  }

  if (!modbus_buffer_write_u16(b, &req->length, true)) {
    return false;
  }

  if (MODBUS_REQUEST_HAS_PAYLOAD(req->opcode)) {
    if (!modbus_buffer_write_u8(b, &req->payload.length)) {
      return false;
    }

    if (MODBUS_REQUEST_PAYLOAD_BIT(req->opcode)) {
      int writed = modbus_buffer_write(b, req->payload.u8, req->payload.length);
      if (writed != req->payload.length) {
        return false;
      }
    }

    if (MODBUS_REQUEST_PAYLOAD_U16(req->opcode)) {
      uint8_t length = req->payload.length / 2;
      uint16_t *ptr = req->payload.u16;

      for (uint8_t i = 0; i < length; i++) {
        if (!modbus_buffer_write_u16(b, ptr + i, true)) {
          return false;
        }
      }
    }
  }

  return true;
}

static bool parser_encode(modbus_role_t role, modbus_package_t *p,
                          modbus_buffer_t *b) {
  modbus_buffer_t writer;
  modbus_buffer_t crc_reader;
  modbus_buffer_copy(&writer, b);
  modbus_buffer_copy(&crc_reader, b);

  if (!modbus_buffer_write_u8(&writer, &p->addr)) {
    return false;
  }

  if (!modbus_buffer_write_u8(&writer, &p->rep.opcode)) {
    return false;
  }

  if (role == MODBUS_ROLE_SLAVE) {
    if (!parser_encode_reply(&p->rep, &writer)) {
      return false;
    }
  }

  if (role == MODBUS_ROLE_MASTER) {
    if (!parser_encode_request(&p->req, &writer)) {
      return false;
    }
  }

  crc_reader.readpos = b->writpos;
  crc_reader.writpos = writer.writpos;
  crc_reader.flag = 0;
  if (crc_reader.readpos == crc_reader.writpos) {
    crc_reader.flag = MODBUS_BUFFER_FULL;
  }

  int len = 0;
  uint16_t crc16;
  len = modbus_buffer_length(&crc_reader);
  crc16 = parser_crc16(&crc_reader, len);
  modbus_buffer_write_u16(&writer, &crc16, false);

  modbus_buffer_copy(b, &writer);
  return true;
}

bool modbus_parser_rtu_decode(modbus_role_t role, modbus_package_t *p,
                              void *driver) {
  modbus_driver_rtu_t *drv = driver;
  modbus_buffer_t *inbuf = &drv->inbuf;
  modbus_buffer_t *oubuf = &drv->oubuf;

  modbus_buffer_writer(inbuf, driver_reader, driver);
  modbus_buffer_reader(oubuf, driver_writer, driver);

  if (modbus_buffer_is_empty(inbuf)) return false;

  return parser_decode(role, p, inbuf);
}

bool modbus_parser_rtu_encode(modbus_role_t role, modbus_package_t *p,
                              void *driver) {
  modbus_driver_rtu_t *drv = driver;
  modbus_buffer_t *oubuf = &drv->oubuf;

  return parser_encode(role, p, oubuf);
}

modbus_parser_t modbus_parser_rtu = {
    .decode = modbus_parser_rtu_decode,
    .encode = modbus_parser_rtu_encode,
};