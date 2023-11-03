#include "modbus/modbus.h"

void *modbus_arch_malloc(int size) { return malloc(size); }
void modbus_arch_free(void *ptr) { free(ptr); }
void modbus_arch_memset(void *s, int c, int l) { memset(s, c, l); }
void modbus_arch_memcpy(void *d, void *s, int l) { memcpy(d, s, l); }
uint16_t modbus_arch_htons(uint16_t v) { return htons(v); }

static void modbus_driver_slave_init() {}
static void modbus_driver_slave_kill() {}
static int modbus_driver_slave_recv(uint8_t *buf, int max) { return 0; }
static int modbus_driver_slave_send(uint8_t *buf, int max) { return 0; }

static modbus_t m_slave;
static modbus_driver_t m_slave_driver = {
    .init = modbus_driver_slave_init,
    .kill = modbus_driver_slave_kill,
    .recv = modbus_driver_slave_recv,
    .send = modbus_driver_slave_send,
};

void modbus_slave_init() {
  memset(&m_slave, 0, sizeof(modbus_t));

  m_slave.type = MODBUS_TYPE_SLAVE;
  m_slave.addr = 1;
  m_slave.driver = &m_slave_driver;
  m_slave.parser = &modbus_parser_rtu;

  // m_slave.hooks.slave.read_coils = wsfs_modbus_coils_read;
  // m_slave.hooks.slave.read_discrete_inputs = wsfs_modbus_coils_read;
  // m_slave.hooks.slave.write_coil = wsfs_modbus_coils_write;
  // m_slave.hooks.slave.write_coils = wsfs_modbus_coils_write;

  // m_slave.hooks.slave.read_input_registers = wsfs_modbus_register_read;
  // m_slave.hooks.slave.read_holding_registers = wsfs_modbus_register_read;
  // m_slave.hooks.slave.write_register = wsfs_modbus_register_write;

  modbus_init(&m_slave);
}

void modbus_slave_idle() { modbus_idle(&m_slave); }

void main() {
  modbus_slave_init();
  while (1) {
    modbus_slave_idle();
  }
}