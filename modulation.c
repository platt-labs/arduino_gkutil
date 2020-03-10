
#include <Arduino.h>

#define MODULATION_GLOBAL
#include "modulation.h"
#undef MODULATION_GLOBAL

int modulator_threshold_a = 0;
int modulator_threshold_b = 0;

#if MODULATION_TIMER == TIMER2B
#define CONFIGURE_TIMER_REGISTERS {             \
    TCCR2A = _BV(WGM20);                        \
    TCCR2B = _BV(WGM22) | _BV(CS20);            \
    OCR2A = (uint8_t) modulator_threshold_a;    \
    OCR2B = (uint8_t) modulator_threshold_b;    \
}
#define SET_TIMER_OUTPUT(level) {                   \
    gk_reg_setters[level](&TCCR2A, _BV(COM2B1));    \
}
#else
#error Timer not supported.
#endif

void gk_modulation_setup(void) {
    gkPin pin = 0;
    for (pin; pin < NUM_DIGITAL_PINS; ++pin) {
        if (digitalPinToTimer(pin) == MODULATION_TIMER) {
            modulated_pin = pin;
            break;
        }
    }
    if (pin==NUM_DIGITAL_PINS)
        return; // No modulated pin found!

    uint8_t pin_port = digitalPinToPort(pin);
    uint8_t pin_bit = digitalPinToBitMask(pin);
    volatile uint8_t* mode_reg = portModeRegister(pin_port);
	volatile uint8_t* out_reg = portOutputRegister(pin_port);

    gkPinMode orig_mode = (*mode_reg & pin_bit) ?
            GK_PIN_MODE_OUTPUT : GK_PIN_MODE_INPUT;
    gkPinAction orig_level = (*out_reg & pin_bit) ?
            GK_PIN_SET_ON : GK_PIN_SET_OFF;

    // Threshold values could probably be calculated a little more flexibly
    // but this will work okay for now.
    modulator_threshold_a = F_CPU / 2000 / CARRIER_FREQUENCY_KHZ;
    modulator_threshold_b = modulator_threshold_a / CARRIER_DUTY_DIVISOR;

    uint8_t SREG_orig = SREG;
    cli();
    *mode_reg |= pin_bit;
    *out_reg &= ~pin_bit;
    CONFIGURE_TIMER_REGISTERS;
    SREG = SREG_orig;

    gk_pin_set_mode_setter(pin, &gk_pin_set_mode_modulator);
    gk_pin_set_writer(pin, &gk_pin_write_modulator);
    gk_pin_set_reader(pin, &gk_pin_read_simple);

    gk_pin_set_mode_modulator(pin, orig_mode, orig_level);
}

// On mode set, the data direction register (DDR, "mode") must be set
// appropriately, as normal. Additionally, the timer output register
// bit must be set off for input, or equal to "level" for output. Finally, the
// conventional output register must be set equal to "level" for input, or
// ignored (or off?) for output.
void gk_pin_set_mode_modulator(gkPin pin, gkPinMode mode, gkPinAction level) {
    uint8_t bit = digitalPinToBitMask(pin);
    uint8_t port = digitalPinToPort(pin);
    volatile uint8_t* out_reg = portOutputRegister(port);
    volatile uint8_t* mode_reg = portModeRegister(port);

    gkPinAction timer_action = (mode==GK_PIN_MODE_OUTPUT) ?
            GK_PIN_SET_OFF : level;
    gkPinAction output_action = (mode==GK_PIN_MODE_INPUT) ?
            level : GK_PIN_SET_OFF;

    uint8_t SREG_orig = SREG;
    cli();
    gk_reg_setters[mode](mode_reg, bit);
    gk_reg_setters[output_action](out_reg, bit);
    SET_TIMER_OUTPUT(timer_action);
    SREG = SREG_orig;
}

void gk_pin_write_modulator(gkPin pin, gkPinAction level) {
    uint8_t SREG_orig = SREG;
    cli();
    SET_TIMER_OUTPUT(level);
    SREG=SREG_orig;
}
