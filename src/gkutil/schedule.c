
#define SCHEDULE_GLOBAL
#include "schedule.h"
#undef SCHEDULE_GLOBAL

/*
The event schedule is implemented as a dynamically-allocated singly-linked list.
Initially I implemented it as a ring buffer, but when I realized that it would
sometimes be necessary to remove items from the middle of the list rather than
only from the front, it seemed easier and less error-prone to just use a linked
list. Not sure if in the end, using a ring buffer is a better idea for
performance reasons, but the linked list approach has worked for me on the
Arduino in the past so hopefully it will hold up.
*/

typedef struct gkScheduleNode gkScheduleNode;

struct gkScheduleNode {
    gkScheduledEvent event;
    gkScheduleNode *next;
    gkScheduleNode *prev;
};

struct Schedule {
    gkScheduleNode *head;
    gkScheduleNode *tail;
    uint8_t length;
} sched = {0};

uint8_t gk_schedule_add(gkTime time, gkPin pin, gkPinAction action) {
    gkScheduleNode *new_node = (gkScheduleNode*)malloc(sizeof(gkScheduleNode));
    if (!new_node) {
        // Out of memory!
        return 255;
    }
    new_node->event.time = time;
    new_node->event.pin = pin;
    new_node->event.action = action;
    new_node->next = 0;

    if (sched.head) {
        // This is not the only item in the schedule
        gkScheduleNode *before = 0;
        gkScheduleNode *after = sched.head;
        while (after && after->event.time < time) {
            before = after;
            after = before->next;
        }
        // `after` is now the first node with a time greater than the new node's
        // time, or null if the new node should be at the end, and `before` is
        // the last node with a time less than the new node's time, or null if
        // the new node should be at the start
        new_node->next = after;
        new_node->prev = before;
        if (before) {
            before->next = new_node;
        } else {
            sched.head = new_node;
        }
        if (after) {
            after->prev = new_node;
        } else {
            sched.tail = new_node;
        }
        ++sched.length;
    } else {
        // This is now the only item in the schedule
        sched.head = new_node;
        sched.tail = new_node;
        sched.length = 1;
    }
    return sched.length;
}

uint8_t gk_schedule_size() {
    return sched.length;
}

gkScheduleIterator gk_schedule_head() {
    return sched.head;
}

gkScheduleIterator gk_schedule_tail() {
    return sched.tail;
}

gkScheduleIterator gk_schedule_next(gkScheduleIterator iter) {
    if (iter)
        return iter->next;
    else
        return 0;
}

gkScheduleIterator gk_schedule_prev(gkScheduleIterator iter) {
    if (iter)
        return iter->prev;
    else
        return 0;
}

gkScheduledEvent *const gk_schedule_get(gkScheduleIterator iter) {
    if (iter)
        return &(iter->event);
    else
        return 0;
}

void gk_schedule_remove(gkScheduleIterator iter) {
    if (!iter)
        return;
    if (iter->prev) {
        iter->prev->next = iter->next;
    } else {
        sched.head = iter->next;
    }
    if (iter->next) {
        iter->next->prev = iter->prev;
    } else {
        sched.tail = iter->prev;
    }
    free(iter);
}

void gk_schedule_execute() {
    gkScheduleNode *current = sched.head;
    while (current && current->event.time <= millis()) {
        gk_pin_write(current->event.pin, current->event.action);

        sched.head = current->next;
        --sched.length;
        free(current);
        current = sched.head;
    }
    if (sched.head)
        sched.head->prev = 0;
    else
        sched.tail = 0;
}

void gk_schedule_write_byte(
        gkTime when,
        gkPin pin,
        gkTime bit_interval,
        gkTime bit_width,
        uint8_t value) {
    gk_schedule_write_bytes(when, pin, bit_interval, bit_width, 1, &value);
}

//void gk_schedule_write_bytes(
//        gkTime when,
//        gkPin pin,
//        gkTime bit_interval,
//        gkTime bit_width,
//        uint8_t count,
//        uint8_t* value ) {
//    gk_schedule_write_bytes_with_checksum(
//        when, pin, bit_interval, bit_width, count, value, /* generic checksum?*/
//}

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
        gk_pin_write(pin, GK_PIN_WRITE_TOGGLE);
        when = millis();
    } else {
        gk_schedule_add(when, pin, GK_PIN_WRITE_TOGGLE);
    }
    gk_schedule_add(when+bit_width, pin, GK_PIN_WRITE_TOGGLE);
    when += bit_interval;
    for (uint8_t byte_ind=0; byte_ind < count; byte_ind++) {
        for (uint8_t bit_ind=0; bit_ind<8; bit_ind++) {
            if ( *(value+byte_ind) & (1 << bit_ind) ) {
                gk_schedule_add(when, pin, GK_PIN_WRITE_TOGGLE);
                gk_schedule_add(when+bit_width, pin, GK_PIN_WRITE_TOGGLE);
            }
            when += bit_interval;
        }
    }
    gk_schedule_add(when, pin, GK_PIN_WRITE_TOGGLE);
    gk_schedule_add(when+bit_width, pin, GK_PIN_WRITE_TOGGLE);
}
