#ifndef GKUTIL_H
#define GKUTIL_H

#include <Arduino.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifdef GKUTIL_GLOBAL
#define EXTERN
#else
#define EXTERN extern
#endif

typedef uint8_t gkPin;
typedef uint8_t gkPinValue;
typedef uint8_t gkPort;
typedef uint8_t gkPinMode;
typedef uint8_t gkPinAction;
typedef unsigned long gkTime;

typedef void gkPinModeSetter(gkPin, gkPinMode, gkPinAction);
typedef void gkPinWriter(gkPin, gkPinAction);
typedef gkPinValue gkPinReader(gkPin);

#define GK_NUM_PINS NUM_DIGITAL_PINS
#define GK_NOT_A_PIN 255

#define GK_PIN_MODE_PASS 0
#define GK_PIN_MODE_INPUT 1
#define GK_PIN_MODE_OUTPUT 2

#define GK_PIN_SET_PASS 0
#define GK_PIN_SET_OFF 1
#define GK_PIN_SET_ON 2
#define GK_PIN_SET_TOGGLE 3
#define GK_PIN_PULLUP_OFF 1
#define GK_PIN_PULLUP_ON 2
#define GK_PIN_LEVEL_NC 0
#define GK_PIN_LEVEL_LOW 1
#define GK_PIN_LEVEL_HIGH 2

// Define number of ports based on the highest numbered one available.
// The actual number of ports may be lower, e.g., PINA/PORTA is not defined
// for ATmega328P. But the actual number of ports should be no higher than this.
#if defined(PINL)
#define GK_NUM_PORTS 12
#elif defined(PINK)
#define GK_NUM_PORTS 11
#elif defined(PINJ)
#define GK_NUM_PORTS 10
#elif defined(PINI)
#define GK_NUM_PORTS 9
#elif defined(PINH)
#define GK_NUM_PORTS 8
#elif defined(PING)
#define GK_NUM_PORTS 7
#elif defined(PINF)
#define GK_NUM_PORTS 6
#elif defined(PINE)
#define GK_NUM_PORTS 5
#elif defined(PIND)
#define GK_NUM_PORTS 4
#elif defined(PINC)
#define GK_NUM_PORTS 3
#else
#define GK_NUM_PORTS 2
#endif

void gk_setup(void);
void gk_protect_serial_pins(void);

void gk_pin_set_mode(gkPin, gkPinMode, gkPinAction);
void gk_pin_write(gkPin, gkPinAction);
gkPinValue gk_pin_read(gkPin);

// Default pin handlers
void gk_pin_set_mode_simple(gkPin, gkPinMode, gkPinAction);
void gk_pin_write_simple(gkPin, gkPinAction);
gkPinValue gk_pin_read_simple(gkPin);

// Tables of pin handlers
EXTERN gkPinModeSetter* gk_pin_mode_setters[NUM_DIGITAL_PINS];
EXTERN gkPinWriter* gk_pin_writers[NUM_DIGITAL_PINS];
EXTERN gkPinReader* gk_pin_readers[NUM_DIGITAL_PINS];

inline
gkPinModeSetter* gk_pin_set_mode_setter(gkPin pin, gkPinModeSetter* setter) {
    gkPinModeSetter* orig_setter = gk_pin_mode_setters[pin];
    gk_pin_mode_setters[pin] = setter;
    return orig_setter;
}

inline
gkPinWriter* gk_pin_set_writer(gkPin pin, gkPinWriter* writer) {
    gkPinWriter* orig_writer = gk_pin_writers[pin];
    gk_pin_writers[pin] = writer;
    return orig_writer;
}

inline
gkPinReader* gk_pin_set_reader(gkPin pin, gkPinReader* reader) {
    gkPinReader* orig_reader = gk_pin_readers[pin];
    gk_pin_readers[pin] = reader;
    return orig_reader;
}
#ifdef GKUTIL_GLOBAL
// Non-inlined declarations to force the compiler to produce symbols, per
// the recommendation of Modern C
gkPinModeSetter* gk_pin_set_mode_setter(gkPin, gkPinModeSetter*);
gkPinWriter* gk_pin_set_writer(gkPin, gkPinWriter*);
gkPinReader* gk_pin_set_reader(gkPin, gkPinReader*);
#endif

// Utility functions to flexibly change register values
typedef void gkRegSetter(volatile uint8_t*, uint8_t);
void gk_reg_on(volatile uint8_t* reg, uint8_t bits);
void gk_reg_off(volatile uint8_t* reg, uint8_t bits);
void gk_reg_toggle(volatile uint8_t* reg, uint8_t bits);
void gk_reg_pass(volatile uint8_t* reg, uint8_t bits);

#ifndef GKUTIL_GLOBAL
extern gkRegSetter*const gk_reg_setters[4];
#else
// Expose the definition of this const array here in the header for readability
gkRegSetter *const gk_reg_setters[] = {
    &gk_reg_pass,
    &gk_reg_off,
    &gk_reg_on,
    &gk_reg_toggle,
};
#endif

void gk_crc8_update(void*, uint8_t);

#ifdef __cplusplus
}
#endif
#endif
