#ifndef __MODBUS_MODBUS_H__
#define __MODBUS_MODBUS_H__

#include "arch.h"
#include "define.h"
#include "parser.h"

void modbus_init(modbus_t* m);
void modbus_idle(modbus_t* m);
void modbus_kill(modbus_t* m);

void modbus_request_init(modbus_request_t* req, uint8_t opcode);
void modbus_request_send(modbus_request_t* req, uint8_t addr, modbus_t* m);
void modbus_request_free(modbus_request_t* req);

void modbus_reply_init(modbus_reply_t* rep, modbus_request_t* req);
void modbus_reply_send(modbus_reply_t* rep, uint8_t addr, modbus_t* m);
void modbus_reply_free(modbus_reply_t* rep);

void modbus_error_init(modbus_reply_t* rep, modbus_request_t* req,
                       uint8_t code);

#endif