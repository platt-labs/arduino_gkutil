
#include "gkutil.h"
#define LISTENER_GLOBAL
#include "listener.h"
#undef LISTENER_GLOBAL

typedef struct PortListeners {
    gkPinValue last_inputs;
    uint8_t listeners;
    gkPin pins[8];
} PortListeners;

typedef struct InputEvent {
    gkTime timestamp;
    gkPort port;
    gkPinValue changes;
    gkPinValue value;
} InputEvent;

PortListeners port_listeners[NUM_PORTS] = {0};
InputEvent event_buffer[LISTENER_EVENT_BUFFER_SIZE] = {0};
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
    for (gkPin pin=0; pin < NUM_DIGITAL_PINS; ++pin) {
        gkPort port = digitalPinToPort(pin);
        uint8_t bit_mask = digitalPinToBitMask(pin);
        for (uint8_t bit=0; bit < 8; ++bit) {
            if (bit_mask & (1<<bit)) {
                port_listeners[port].pins[bit] = pin;
                break;
            }
        }
    }
    for (gkPort port=0; port < NUM_PORTS; ++port) {
        uint8_t* reg = portInputRegister(port);
        if (reg) {
            port_listeners[port].last_inputs = *reg;
        }
    }
}

void gk_listener_set(gkPin pin) {
    uint8_t port = digitalPinToPort(pin);
    uint8_t bit_mask = digitalPinToBitMask(pin);
    port_listeners[port].listeners |= bit_mask;
    if ((*portInputRegister(port)) & bit_mask)
        port_listeners[port].last_inputs |= bit_mask;
    else
        port_listeners[port].last_inputs &= ~bit_mask;
}

void gk_listener_clear(gkPin pin) {
    uint8_t port = digitalPinToPort(pin);
    uint8_t bit_mask = digitalPinToBitMask(pin);
    port_listeners[port].listeners &= ~bit_mask;
}

uint8_t gk_listeners_poll(void) {
    for (gkPort port = 0; port < NUM_PORTS; ++port) {
        if (port_listeners[port].listeners) {
            uint8_t new_inputs = *portInputRegister(port);
            uint8_t input_change =
                port_listeners[port].listeners
                & ( port_listeners[port].last_inputs ^ new_inputs );
            if (input_change) {
                InputEvent new_event = {
                    .timestamp = millis(),
                    .port = port,
                    .changes = input_change,
                    .value = new_inputs,
                };
                event_buffer[EVENT_BUFFER_TAIL] = new_event;
                if (event_buffer_occupancy < LISTENER_EVENT_BUFFER_SIZE)
                    ++event_buffer_occupancy;
                else
                    event_buffer_head = EVENT_BUFFER_IND(1);
            }
            port_listeners[port].last_inputs = new_inputs;
        }
    }
    return event_buffer_occupancy;
}

uint8_t gk_listeners_events_available(void) {
    return event_buffer_occupancy;
}

uint8_t gk_listeners_get_event(gkTime* when, gkPinValue* listener_value) {
    if (!event_buffer_occupancy)
        return 0;
    InputEvent* cur_event = &(event_buffer[event_buffer_head]);
    if (when)
        *when = cur_event->timestamp;
    if (listener_value) {
        for (uint8_t bit = 0; bit < 8; ++bit) {
            uint8_t bitmask = 1<<bit;
            if (cur_event->changes & bitmask) {
                uint8_t pin = port_listeners[cur_event->port].pins[bit];
                listener_value[pin] = (cur_event->value & bitmask) ?
                    GK_PIN_LEVEL_HIGH : GK_PIN_LEVEL_LOW;
            }
        }
    }
    --event_buffer_occupancy;
    event_buffer_head = EVENT_BUFFER_IND(1);
    return event_buffer_occupancy;
}
