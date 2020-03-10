

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

#define LISTENER_EVENT_BUFFER_SIZE 20

#define LISTENER_PIN_UNCHANGED 0
#define LISTENER_PIN_SET_LOW   1
#define LISTENER_PIN_SET_HIGH  2

typedef uint8_t gkListener(gkPin, gkPinValue, gkTime);

void gk_listeners_setup(void);
void gk_listener_set(gkPin, gkListener*);
void gk_listener_clear(gkPin);
// Update but do not execute listeners, return the number currently queued
uint8_t gk_listeners_update(void);
uint8_t gk_listeners_queued(void);
// Execute currently queued listeners
void gk_listeners_execute(void);

#ifdef __cplusplus
}
#endif
#endif
