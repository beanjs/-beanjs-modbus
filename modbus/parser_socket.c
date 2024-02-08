#include "parser.h"

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

static bool parser_decode(modbus_role_t role, modbus_package_t *p,
                          modbus_buffer_t *b) {
  modbus_mbap_t *mbap = p->extra;

  if (!modbus_buffer_read_u16(b, &mbap->transaction, true)) {
    return false;
  }

  if (!modbus_buffer_read_u16(b, &mbap->protocol, true)) {
    return false;
  }

  uint16_t length;
  if (!modbus_buffer_read_u16(b, &length, true)) {
    return false;
  }

  if (length != modbus_buffer_length(b)) {
    return false;
  }

  if (!modbus_buffer_read_u8(b, &p->addr)) {
    return false;
  }

  if (!modbus_buffer_read_u8(b, &p->req.opcode)) {
    return false;
  }

  if (role == MODBUS_ROLE_SLAVE) {
    return parser_decode_request(&p->req, b);
  }

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

static bool parser_encode(modbus_role_t role, modbus_package_t *p,
                          modbus_buffer_t *b) {
  modbus_mbap_t *mbap = p->extra;
  modbus_buffer_t mbap_writer;
  modbus_buffer_copy(&mbap_writer, b);

  uint16_t vskip = 0;
  modbus_buffer_write_u16(b, &vskip, true);
  modbus_buffer_write_u16(b, &vskip, true);
  modbus_buffer_write_u16(b, &vskip, true);

  if (!modbus_buffer_write_u8(b, &p->addr)) {
    return false;
  }

  if (!modbus_buffer_write_u8(b, &p->req.opcode)) {
    return false;
  }

  if (role == MODBUS_ROLE_SLAVE) {
    if (!parser_encode_reply(&p->rep, b)) {
      return false;
    }
  }

  vskip = modbus_buffer_length(b) - 6;
  modbus_buffer_write_u16(&mbap_writer, &mbap->transaction, true);
  modbus_buffer_write_u16(&mbap_writer, &mbap->protocol, true);
  modbus_buffer_write_u16(&mbap_writer, &vskip, true);
  return true;
}

bool modbus_parser_socket_decode(modbus_role_t role, modbus_package_t *p,
                                 void *driver) {
  modbus_buffer_t stream;
  modbus_driver_socket_t *drv = driver;

  int recv_len = drv->recv(driver, drv->cache, drv->cache_len);
  if (recv_len == 0) return false;

  modbus_buffer_init_reader(&stream, drv->cache, recv_len);
  return parser_decode(role, p, &stream);
}

bool modbus_parser_socket_encode(modbus_role_t role, modbus_package_t *p,
                                 void *driver) {
  modbus_buffer_t stream;
  modbus_driver_socket_t *drv = driver;

  modbus_buffer_init_writer(&stream, drv->cache, drv->cache_len);
  if (!parser_encode(role, p, &stream)) {
    return false;
  }

  int retry_max = 20;
  int retry_cnt = 0;
  int send_len = modbus_buffer_length(&stream);
  while (send_len) {
    if (retry_cnt == retry_max) {
      return false;
    }

    int sent_len = drv->send(driver, drv->cache, send_len);
    if (sent_len < 0) return false;
    if (sent_len == send_len) break;

    modbus_buffer_skip(&stream, sent_len);
    send_len = modbus_buffer_length(&stream);
    retry_cnt++;
  }

  return true;
}

modbus_parser_t modbus_parser_socket = {
    .decode = modbus_parser_socket_decode,
    .encode = modbus_parser_socket_encode,
};