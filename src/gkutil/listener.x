
#include "gkutil.h"
#define LISTENER_GLOBAL
#include "listener.h"
#undef LISTENER_GLOBAL

typedef struct PortListeners {
    uint8_t last_input;
    uint8_t listeners_mask;
    gkListener listeners[8];
    gkPins pins[8];
} PortListeners;

typedef struct QueuedEvent {
    gkTime timestamp;
    gkPort port;
    uint8_t input;
    uint8_t change;
} QueuedEvent;

PortListeners port_listeners[GK_NUM_PORTS] = {0};
QueuedEvent event_buffer[LISTENER_EVENT_BUFFER_SIZE] = {0};
uint8_t event_buffer_head = 0;
uint8_t event_buffer_occupancy = 0;

#define EVENT_BUFFER_TAIL (                         \
    ( event_buffer_head + event_buffer_occupancy )  \
    % LISTENER_EVENT_BUFFER_SIZE                    \
)
#define EVENT_BUFFER_IND(i) (       \
    ( event_buffer_head + (i) )     \
    % LISTENER_EVENT_BUFFER_SIZE    \
)

void gk_listeners_setup(void) {

}

void gk_listener_set(gkPin pin, gkListener listener) {
    uint8_t port = digitalPinToPort(pin);
    uint8_t bit_mask = digitalPinToBitMask(pin);
    for (uint8_t bit = 0; bit < 8; ++bit) {
        if (bit_mask & (1<<bit)) {
            port_listeners[port].listeners[bit] = listener;
            port_listeners[port].pin[bit] = pin;
            break;
        }
    }
    port_listeners[port].listeners_mask |= bit_mask;
    if ((*portInputRegister(port)) & bit_mask)
        port_listeners[port].last_input |= bit_mask;
    else
        port_listeners[port].last_input &= ~bit_mask;
}

void gk_listener_clear(gkPin pin) {
    uint8_t port = digitalPinToPort(pin);
    uint8_t bit_mask = digitalPinToBitMask(pin);
    port_listeners[port].listeners_mask &= ~bit_mask;
    for (uint8_t bit = 0; bit < 8; ++bit) {
        if (bit_mask & (1<<bit)) {
            port_listeners[port].listeners[bit] = (void*)0;
            break;
        }
    }
}

uint8_t gk_listeners_update(void) {
    for (gkPort port=0; port < NUM_PORTS; ++port) {
        uint8_t new_input = *portInputRegister(port);
        uint8_t input_change =
            port_listeners[port].listeners_mask
            & (port_listeners[port].last_input ^ new_input);

        if (input_change) {
            QueuedEvent new_event = {
                .timestamp = millis(),
                .port = port,
                .input = new_input;
                .change = input_change;
            };
            event_buffer[EVENT_BUFFER_TAIL] = new_event;
            if (event_buffer_occupancy < LISTENER_EVENT_BUFFER_SIZE)
                ++event_buffer_occupancy;
            else // Overflow of ring buffer by overwriting old elements
                event_buffer_head = EVENT_BUFFER_IND(1);
        }
    }
    return event_buffer_occupancy;
}

uint8_t gk_listeners_queued(void) {
    return event_buffer_occupancy;
}

void gk_listeners_execute(void) {
    while (event_buffer_occupancy) {
        // For each event currently in the queue...
        QueuedEvent* event = &(event_buffer[event_buffer_head]);
        for (uint8_t bit = 0; bit < 8; ++bit) {
            // Check whether each input of the associated port changed, and
            // whether it still has an active listener
            uint8_t bitmask = 1<<bit;
            gkPin pin = port_listeners[event->port].pins[bit];
            gkListener listener = port_listeners[event->port].listeners[bit];
            if ( (event->changed & bitmask) && listener ) {
                gkPinValue value = (event->input & bitmask) ?
                    GK_PIN_LEVEL_HIGH : GK_PIN_LEVEL_LOW;
                // Call the listener callback. If it returns true, unset the
                // listener.
                if (listener(pin, value, event->timestamp)) {
                    port_listeners[event->port].listeners[bit] = (void*)0;
                    port_listeners[event->port].listeners_mask &= ~bitmask;
                }
            }
        }
        --event_buffer_occupancy;
        event_buffer_head = EVENT_BUFFER_IND(1);
    }
    event_buffer_head = 0;
}
