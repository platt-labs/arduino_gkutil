

#ifndef LISTENER_H
#define LISTENER_H

#ifdef LISTENER_GLOBAL
#define EXTERN
#else
#define EXTERN extern
#endif

#include <Arduino.h>
#include "gkutil.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_PORTS 11

#define LISTENER_EVENT_BUFFER_SIZE 20

#define LISTENER_PIN_UNCHANGED 0
#define LISTENER_PIN_SET_LOW   1
#define LISTENER_PIN_SET_HIGH  2

void gk_listeners_setup(void);
void gk_listener_set(gkPin);
void gk_listener_clear(gkPin);
uint8_t gk_listeners_poll(void);
uint8_t gk_listeners_events_available(void);
uint8_t gk_listeners_get_event(gkTime*, gkPinValue*);

#ifdef __cplusplus
}
#endif
#endif
