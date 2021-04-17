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
#define SCHEDULE_BUFFER_SIZE 100

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
void gk_schedule_add(gkTime time, gkPin pin, gkPinAction action);
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
