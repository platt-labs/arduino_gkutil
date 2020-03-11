
#define SCHEDULE_GLOBAL
#include "schedule.h"
#undef SCHEDULE_GLOBAL

#define SCHEDULE_BUFFER_IND(x) ((sched.head + (x)) % SCHEDULE_BUFFER_SIZE)

void gk_schedule_add(gkTime time, gkPin pin, gkPinAction action) {
    ScheduledEvent new_item = {time, pin, action};
    uint8_t current_length = sched.length;

    if (current_length) {
        // Insertion sort based on scheduled time
        uint8_t insertion_point = sched.head;
        uint8_t i = current_length-1;
        for (i; i >= 0; i--) {
            uint8_t buffer_test_ind = SCHEDULE_BUFFER_IND(i);
            uint8_t buffer_next_ind = SCHEDULE_BUFFER_IND(i+1);
            if (sched.buffer[buffer_test_ind].time <= time) {
                // We've found our position
                insertion_point = buffer_next_ind;
                break;
            } else {
                // Move the last item backwards and keep going
                sched.buffer[buffer_next_ind] = sched.buffer[buffer_test_ind];
            }
        }
        sched.buffer[insertion_point] = new_item;
        sched.length += 1;
    } else {
        // No items scheduled, this is the new start
        sched.buffer[0] = new_item;
        sched.head = 0;
        sched.length = 1;
    }
}

void gk_schedule_check() {
    ScheduledEvent* next_item = &(sched.buffer[sched.head]);

    while (sched.length && next_item->time <= millis()) {
        gk_pin_write(next_item->pin, next_item->action);
        if (sched.length > 1) {
            sched.length--;
            sched.head = SCHEDULE_BUFFER_IND(1);
        } else {
            sched.length = 0;
            sched.head = 0;
        }
        next_item = &(sched.buffer[sched.head]);
    }
}

void gk_schedule_write_byte(
        gkTime when,
        gkPin pin,
        gkTime bit_interval,
        gkTime bit_width,
        uint8_t value) {
    gk_schedule_write_bytes(when, pin, bit_interval, bit_width, 1, &value);
}

void gk_schedule_write_bytes(
        gkTime when,
        gkPin pin,
        gkTime bit_interval,
        gkTime bit_width,
        uint8_t count,
        uint8_t* value ) {
    gk_schedule_write_bytes_with_checksum(
        when, pin, bit_interval, bit_width, count, value, /* generic checksum?*/
}

// Write a series of bits to communicate information, using a simplistic
// protocol. Each bit consists of either a 1 waveform (bit_width ms ON
// followed by bit_interval-bit_width ms OFF) or a 0 waveform (bit_interval ms
// OFF). Here, ON is defined as departing from the intially set value, OFF as
// remaining unchanged. The sequence of bits is preceded and followed by a 1
// waveform.
void gk_schedule_write_bytes(
        gkTime when,
        gkPin pin,
        gkTime bit_interval,
        gkTime bit_width,
        uint8_t count,
        uint8_t* value ) {
    if (!when) {
        // Immediate write
        gk_pin_write(pin, GK_PIN_SET_TOGGLE);
        when = millis();
    } else {
        gk_schedule_add(when, pin, GK_PIN_SET_TOGGLE);
    }
    gk_schedule_add(when+bit_width, pin, GK_PIN_SET_TOGGLE);
    when += bit_interval;
    for (uint8_t byte_ind=0; byte_ind < count; byte_ind++) {
        for (uint8_t bit_ind=0; bit_ind<8; bit_ind++) {
            if ( *(value+byte_ind) & (1 << bit_ind) ) {
                gk_schedule_add(when, pin, GK_PIN_SET_TOGGLE);
                gk_schedule_add(when+bit_width, pin, GK_PIN_SET_TOGGLE);
            }
            when += bit_interval;
        }
    }
    gk_schedule_add(when, pin, GK_PIN_SET_TOGGLE);
    gk_schedule_add(when+bit_width, pin, GK_PIN_SET_TOGGLE);
}
