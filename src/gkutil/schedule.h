/* schedule.h
Allow digital outputs to be "scheduled" to a millisecond-precision clock.
*/

#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "gkutil.h"
#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of scheduled digital output events to queue
#define SCHEDULE_BUFFER_SIZE 256

#ifdef SCHEDULE_GLOBAL
#define EXTERN
#else
#define EXTERN extern
#endif

typedef struct ScheduledEvent {
    gkTime time;
    gkPin pin;
    gkPinAction action;
} ScheduledEvent;

EXTERN struct OutputSchedule {
    ScheduledEvent buffer[SCHEDULE_BUFFER_SIZE];
    uint8_t head;
    uint8_t length;
} sched;

// Schedule a digital write action to be executed when millis()>=time.
// Returns the (current) index of the scheduled event, but note that this may
// change after calling gk_schedule_execute
uint8_t gk_schedule_add(gkTime time, gkPin pin, gkPinAction action);
// Get the number of actions currently scheduled
uint8_t gk_schedule_size();
// Get the n'th event scheduled
ScheduledEvent*const gk_schedule_get(uint8_t n);
// Remove the n'th event scheduled
void gk_schedule_remove(uint8_t n);
// Check the schedule and perform any write actions that are due
void gk_schedule_execute();
// Perform a low-bitrate serial write, to begin when millis()>=time
void gk_schedule_write_byte(
    gkTime time,
    gkPin pin,
    gkTime bit_interval,
    gkTime bit_width,
    uint8_t value
);
void gk_schedule_write_bytes(
    gkTime time,
    gkPin pin,
    gkTime bit_interval,
    gkTime bit_width,
    uint8_t count,
    uint8_t* value);

#ifdef __cplusplus
}
#endif
#endif
