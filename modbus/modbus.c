#include "modbus.h"

static void hook_noop(uint8_t addr, void *) {}

static void hook_run(modbus_t *m, modbus_package_t *p) {
  modbus_hook_t *hooks = (modbus_hook_t *)&m->hooks;
  modbus_hook_t hook_func = 0;
  modbus_free_t free_func = 0;
  void *hook_arg;

  if (m->role == MODBUS_ROLE_SLAVE) {
    modbus_hook_t forward_func = hook_noop;
    if (m->hooks.forward) {
      forward_func = (modbus_hook_t)m->hooks.forward;
    }

    if (m->slave.addr != p->addr) {
      forward_func(p->addr, &p->req);
      return modbus_request_free(&p->req);
    }

    if (MODBUS_BROADCAST_ADDRESS == p->addr) {
      forward_func(p->addr, &p->req);
      return modbus_request_free(&p->req);
    }

    hook_func = hooks[p->req.opcode & MODBUS_OPCODE_FUNC_MASK];
    hook_arg = &p->req;
    free_func = (modbus_free_t)modbus_request_free;
  }

  if (m->role == MODBUS_ROLE_MASTER) {
    hook_func = hooks[p->rep.opcode & MODBUS_OPCODE_FUNC_MASK];
    hook_arg = &p->rep;
    free_func = (modbus_free_t)modbus_reply_free;
  }

  if (hook_func) {
    hook_func(p->addr, hook_arg);
  }

  if (!hook_func && m->role == MODBUS_ROLE_SLAVE) {
    modbus_reply_t rep;
    modbus_error_init(&rep, &p->req, 0x01);
    modbus_reply_send(&rep, p->addr, m);
    modbus_reply_free(&rep);
  }

  if (free_func) free_func(hook_arg);
}

void modbus_init(modbus_t *m) {
  modbus_driver_t *driver = m->driver;

  driver->init(driver);
}

void modbus_kill(modbus_t *m) {
  modbus_driver_t *driver = m->driver;
  driver->kill(driver);
}

void modbus_idle(modbus_t *m) {
  modbus_package_t package;
  modbus_parser_t *parser = m->parser;
  void *driver = m->driver;

  modbus_arch_memset(&package, 0, sizeof(package));

  package.extra = m->extra;
  if (!parser->decode(m->role, &package, driver)) return;
  hook_run(m, &package);
}

void modbus_request_init(modbus_request_t *req, uint8_t opcode) {
  modbus_arch_memset(req, 0, sizeof(modbus_request_t));
  req->opcode = opcode;

  if (MODBUS_REQUEST_HAS_PAYLOAD(req->opcode)) {
    req->payload.u8 = modbus_arch_malloc(MODBUS_PAYLOAD_BUFFER_SIZE);
    modbus_arch_memset(req->payload.u8, 0, MODBUS_PAYLOAD_BUFFER_SIZE);
  }
}

void modbus_request_send(modbus_request_t *req, uint8_t addr, modbus_t *m) {
  modbus_package_t package;
  modbus_parser_t *parser = m->parser;
  void *driver = m->driver;

  modbus_arch_memset(&package, 0, sizeof(modbus_package_t));
  modbus_arch_memcpy(&package.req, req, sizeof(modbus_request_t));

  package.addr = addr;
  package.extra = m->extra;

  parser->encode(m->role, &package, driver);
}

void modbus_request_free(modbus_request_t *req) {
  if (req->payload.u8) {
    modbus_arch_free(req->payload.u8);
    req->payload.u8 = 0;
    req->payload.length = 0;
  }
}

void modbus_reply_init(modbus_reply_t *rep, modbus_request_t *req) {
  modbus_arch_memset(rep, 0, sizeof(modbus_reply_t));
  rep->opcode = req->opcode;
  rep->address = req->address;
  rep->length = req->length;

  if (MODBUS_REPLY_HAS_PAYLOAD(rep->opcode)) {
    rep->payload.u8 = modbus_arch_malloc(MODBUS_PAYLOAD_BUFFER_SIZE);
    modbus_arch_memset(rep->payload.u8, 0, MODBUS_PAYLOAD_BUFFER_SIZE);

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

void modbus_reply_send(modbus_reply_t *rep, uint8_t addr, modbus_t *m) {
  modbus_parser_t *parser = m->parser;
  void *driver = m->driver;

  modbus_package_t package;
  modbus_arch_memset(&package, 0, sizeof(modbus_package_t));
  modbus_arch_memcpy(&package.rep, rep, sizeof(modbus_reply_t));

  package.addr = addr;
  package.extra = m->extra;

  parser->encode(m->role, &package, driver);
}

void modbus_reply_free(modbus_reply_t *rep) {
  if (rep->payload.u8) {
    modbus_arch_free(rep->payload.u8);
    rep->payload.u8 = 0;
    rep->payload.length = 0;
  }
}

void modbus_error_init(modbus_reply_t *rep, modbus_request_t *req,
                       uint8_t code) {
  modbus_reply_init(rep, req);
  if (rep->payload.u8 == 0) {
    rep->payload.u8 = modbus_arch_malloc(MODBUS_PAYLOAD_BUFFER_SIZE);
    modbus_arch_memset(rep->payload.u8, 0, MODBUS_PAYLOAD_BUFFER_SIZE);
  }

  rep->opcode |= MODBUS_OPCODE_ERROR_MASK;
  rep->payload.u8[0] = code;
  rep->payload.length = 1;
}