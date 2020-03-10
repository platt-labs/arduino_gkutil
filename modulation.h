/* modulation.h

Alternatives to the Arduino pinMode, digitalRead, and digitalWrite i/o functions
for (possible) use with a modulated oscillator pin.

*/
#ifndef MODULATION_H
#define MODULATION_H

#include "gkutil.h"

#ifdef __cplusplus
extern "C" {
#endif

// Default values. Override these with compiler flags if desired.
#ifndef MODULATION_TIMER
#define MODULATION_TIMER TIMER2B // pin 3 on the Uno and other ATMega328 boards
#endif
#ifndef CARRIER_FREQUENCY_KHZ
#define CARRIER_FREQUENCY_KHZ 38
#endif
#ifndef CARRIER_DUTY_DIVISOR
#define CARRIER_DUTY_DIVISOR 3 // Duty cycle of the carrier wave
#endif

#define NO_MODULATED_PIN 255

#ifdef MODULATION_GLOBAL
uint8_t modulated_pin = NO_MODULATED_PIN;
#else
extern uint8_t modulated_pin;
#endif

void gk_modulation_setup(void);

void gk_pin_set_mode_modulator(gkPin, gkPinMode, gkPinAction);
void gk_pin_write_modulator(gkPin, gkPinAction);

#ifdef __cplusplus
}
#endif
#endif //ifndef MODULATION_H
