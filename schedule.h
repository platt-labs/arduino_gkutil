/* schedule.h

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

void gk_schedule_add(gkTime, gkPin, gkPinAction);
void gk_schedule_check();
void gk_schedule_write_byte(gkTime, gkPin, gkTime, gkTime, uint8_t);
void gk_schedule_write_bytes(gkTime, gkPin, gkTime, gkTime, uint8_t, uint8_t*);

#ifdef __cplusplus
}
#endif
#endif
