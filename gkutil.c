
#define GKUTIL_GLOBAL
#include "gkutil.h"
#undef GKUTIL_GLOBAL

void gk_setup(void) {
    for (gkPin pin=0; pin < GK_NUM_PINS; ++pin) {
        gk_pin_mode_setters[pin] = &gk_pin_set_mode_simple;
        gk_pin_writers[pin] = &gk_pin_write_simple;
        gk_pin_readers[pin] = &gk_pin_read_simple;
    }
}

void gk_protect_serial_pins(void) {
    for (gkPin pin = 0; pin < 2; ++pin) {
        gk_pin_set_mode_setter(pin, (void*)0 );
        gk_pin_set_writer(pin, (void*)0 );
        gk_pin_set_reader(pin, (void*)0 );
  }
}

void gk_pin_set_mode(gkPin pin, gkPinMode mode, gkPinAction level) {
    if (pin < GK_NUM_PINS && gk_pin_mode_setters[pin])
        gk_pin_mode_setters[pin](pin, mode, level);
}

void gk_pin_write(gkPin pin, gkPinAction action) {
    if (pin < GK_NUM_PINS && gk_pin_writers[pin])
        gk_pin_writers[pin](pin, action);
}

uint8_t gk_pin_read(gkPin pin) {
    if (pin < GK_NUM_PINS && gk_pin_readers[pin])
        return gk_pin_readers[pin](pin);
    else
        return false;
}

void gk_pin_set_mode_simple(gkPin pin, gkPinMode mode, gkPinAction level) {
    uint8_t bit = digitalPinToBitMask(pin);
    uint8_t port = digitalPinToPort(pin);
    if (!port)
        return;
    volatile uint8_t* out_reg = portOutputRegister(port);
    volatile uint8_t* mode_reg = portModeRegister(port);

    uint8_t SREG_orig = SREG;
    cli();
    gk_reg_setters[mode](mode_reg, bit);
    gk_reg_setters[level](out_reg, bit);
    SREG = SREG_orig;
}

void gk_pin_write_simple(gkPin pin, gkPinAction action) {
    uint8_t bit = digitalPinToBitMask(pin);
    volatile uint8_t* out = portOutputRegister(digitalPinToPort(pin));
    uint8_t SREG_orig = SREG;
    cli();
    gk_reg_setters[action](out, bit);
    SREG = SREG_orig;
}

uint8_t gk_pin_read_simple(gkPin pin) {
    uint8_t port = digitalPinToPort(pin);
    uint8_t bit = digitalPinToBitMask(pin);
    return !!(*portInputRegister(port) & bit);
}

void gk_reg_on(volatile uint8_t *reg, uint8_t bits) {
    *reg |= bits;
}

void gk_reg_off(volatile uint8_t *reg, uint8_t bits) {
    *reg &= ~bits;
}

void gk_reg_toggle(volatile uint8_t *reg, uint8_t bits) {
    *reg ^= bits;
}

void gk_reg_pass(volatile uint8_t *reg, uint8_t bits) {
    return;
}
