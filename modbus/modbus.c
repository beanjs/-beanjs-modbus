#include "modbus.h"

#include <stdio.h>

#include "config.h"

static void modbus_hook_noop(uint8_t addr, void*) {}

static void modbus_run_hook(modbus_t* m, modbus_package_t* p) {
  modbus_hook_t* hooks = (modbus_hook_t*)&m->hooks;
  modbus_hook_t hook_func = 0;
  modbus_free_t free_func = 0;
  void* hook_arg;

  if (m->type == MODBUS_TYPE_SLAVE) {
    modbus_hook_t forward_func = modbus_hook_noop;
    if (m->hooks.slave.forward) {
      forward_func = (modbus_hook_t)m->hooks.slave.forward;
    }

    if (m->addr != p->addr) {
      forward_func(p->addr, &p->req);
      // 这里需要手动释放请求并返回，不触发回调函数
      return modbus_request_free(&p->req);
    }

    if (MODBUS_ADDR_BROADCAST == p->addr) {
      // 往下层广播数据
      forward_func(p->addr, &p->req);
    }

    hook_func = hooks[p->req.opcode & MODBUS_OPCODE_FUNC_MASK];
    hook_arg = &p->req;
    free_func = (modbus_free_t)modbus_request_free;
  }

  if (m->type == MODBUS_TYPE_MASTER) {
    hook_func = hooks[p->rep.opcode & MODBUS_OPCODE_FUNC_MASK];
    hook_arg = &p->rep;
    free_func = (modbus_free_t)modbus_reply_free;
  }

  if (hook_func) {
    hook_func(p->addr, hook_arg);
  }

  if (!hook_func && m->type == MODBUS_TYPE_SLAVE) {
    modbus_reply_t rep;
    modbus_error_init(&rep, &p->req, 0x01);
    modbus_reply_send(&rep, p->addr, m);
    modbus_reply_free(&rep);
  }

  if (free_func) free_func(hook_arg);
}

static void modbus_run_recv(modbus_t* m, uint8_t* buf, int len) {
  modbus_driver_t* driver = m->driver;
  modbus_buffer_t* inbuf = &m->inbuf;

  len = driver->recv(buf, len);
  if (len < 0) {
    driver->kill();
    driver->init();
  }

  if (len > 0) {
    modbus_buffer_write(inbuf, buf, len);
  }
}

static void modbus_run_send(modbus_t* m, uint8_t* buf, int len) {
  modbus_driver_t* driver = m->driver;
  modbus_buffer_t* oubuf = &m->oubuf;

  if (modbus_buffer_is_empty(oubuf)) return;

  modbus_buffer_t writer;
  modbus_buffer_copy(&writer, oubuf);
  len = modbus_buffer_read(&writer, buf, len);
  len = driver->send(buf, len);
  modbus_buffer_skip(oubuf, len);
}

static void modbus_run_decode(modbus_t* m, uint8_t* buf, int len) {
  modbus_parser_t* parser = m->parser;
  modbus_buffer_t* inbuf = &m->inbuf;
  modbus_package_t clpkg;
  modbus_arch_memset(&clpkg, 0, sizeof(modbus_package_t));

  if (!parser->decode(m->type, &clpkg, inbuf)) return;

  modbus_run_hook(m, &clpkg);
}

void modbus_init(modbus_t* m) {
  modbus_buffer_init(&m->inbuf, MODBUS_MALLOC_MAX);
  modbus_buffer_init(&m->oubuf, MODBUS_MALLOC_MAX);
  m->cache = modbus_arch_malloc(MODBUS_MALLOC_MAX);
  m->driver->init();
}

void modbus_idle(modbus_t* m) {
  modbus_run_recv(m, m->cache, MODBUS_MALLOC_MAX);
  modbus_run_decode(m, m->cache, MODBUS_MALLOC_MAX);
  modbus_run_send(m, m->cache, MODBUS_MALLOC_MAX);
}

void modbus_kill(modbus_t* m) {
  m->driver->kill();
  modbus_arch_free(m->cache);
  modbus_buffer_kill(&m->inbuf);
  modbus_buffer_kill(&m->oubuf);
}

void modbus_request_init(modbus_request_t* req, uint8_t opcode) {
  modbus_arch_memset(req, 0, sizeof(modbus_request_t));
  req->opcode = opcode;

  if (MODBUS_REQUEST_HAS_PAYLOAD(req->opcode)) {
    req->payload.u8 = modbus_arch_malloc(MODBUS_MALLOC_MAX);
    modbus_arch_memset(req->payload.u8, 0, MODBUS_MALLOC_MAX);
  }
}

void modbus_request_send(modbus_request_t* req, uint8_t addr, modbus_t* m) {
  modbus_buffer_t* oubuf = &m->oubuf;
  modbus_parser_t* parser = m->parser;

  modbus_package_t clpkg;
  modbus_arch_memset(&clpkg, 0, sizeof(modbus_package_t));
  modbus_arch_memcpy(&clpkg.req, req, sizeof(modbus_request_t));
  clpkg.addr = addr;

  parser->encode(m->type, &clpkg, oubuf);
}
void modbus_request_free(modbus_request_t* req) {
  if (req->payload.u8) {
    modbus_arch_free(req->payload.u8);
    req->payload.u8 = 0;
    req->payload.length = 0;
  }
}

void modbus_reply_init(modbus_reply_t* rep, modbus_request_t* req) {
  modbus_arch_memset(rep, 0, sizeof(modbus_reply_t));
  rep->opcode = req->opcode;
  rep->address = req->address;
  rep->length = req->length;

  if (MODBUS_REPLY_HAS_PAYLOAD(rep->opcode)) {
    rep->payload.u8 = modbus_arch_malloc(MODBUS_MALLOC_MAX);
    modbus_arch_memset(rep->payload.u8, 0, MODBUS_MALLOC_MAX);

    if (MODBUS_REPLY_PAYLOAD_IS_BIT(rep->opcode)) {
      rep->payload.length = req->length / 8;
      if (req->length % 8 != 0) {
        rep->payload.length++;
      }
    }

    if (MODBUS_REPLY_PAYLOAD_IS_U16(rep->opcode)) {
      rep->payload.length = req->length * 2;
    }
  }
}

void modbus_error_init(modbus_reply_t* rep, modbus_request_t* req,
                       uint8_t code) {
  modbus_reply_init(rep, req);
  if (rep->payload.length == 0) {
    rep->payload.u8 = modbus_arch_malloc(MODBUS_MALLOC_MAX);
    modbus_arch_memset(rep->payload.u8, 0, MODBUS_MALLOC_MAX);
  }

  rep->opcode |= MODBUS_OPCODE_ERROR_MASK;
  rep->payload.u8[0] = code;
  rep->payload.length = 1;
}

void modbus_reply_send(modbus_reply_t* rep, uint8_t addr, modbus_t* m) {
  if (addr == MODBUS_ADDR_BROADCAST) return;

  modbus_buffer_t* oubuf = &m->oubuf;
  modbus_parser_t* parser = m->parser;

  modbus_package_t clpkg;
  modbus_arch_memset(&clpkg, 0, sizeof(modbus_package_t));
  modbus_arch_memcpy(&clpkg.rep, rep, sizeof(modbus_reply_t));
  clpkg.addr = addr;

  parser->encode(m->type, &clpkg, oubuf);
}

void modbus_reply_free(modbus_reply_t* rep) {
  if (rep->payload.u8) {
    modbus_arch_free(rep->payload.u8);
    rep->payload.u8 = 0;
    rep->payload.length = 0;
  }
}