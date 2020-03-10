// EthIO
// Use an Arduino as an input/output device for supporting
// behavioral research. This sketch is designed to facilitate
// synchronization between multiple acquisition devices, control
// simple devices via logic pulses, etc. Primary design goals are
// low latency and at least millisecond-level precision for timing.
// The Arduino is also intended to handle most of the timing
// functionality, allowing the controlling PC to dispatch instructions for
// a desired pulse or sequence of pulses to the Arduino, then resume
// execution immediately while the Arduino handles adjusting the pins when
// needed.

// Terminology:
//   "stable high" versus "stable low" refers to the logic level a pin is
// configured to be held at when no other commands are active.
//   "pulse" refers to inverting the logic level of a pin for a specified
// period of time

#include <gkutil.h>
#include <modulation.h>
#include <schedule.h>
#include <listener.h>

#define BAUD_RATE 115200

// A command dispatcher is a function taking no arguments and returning a
// boolean true (if the command is completed) or false (if the command needs to
// remain active)
typedef bool CommandDispatcher(void);

bool cmd_config_output_on();
bool cmd_config_output_off();
bool cmd_pulse();
bool cmd_pulse_train();
bool cmd_config_input_pullup();
bool cmd_config_input_nopullup();
bool cmd_read_pin();
bool cmd_get_clock();
bool cmd_get_last_clock();
bool cmd_start_listening();
bool cmd_stop_listening();
bool cmd_set_data_rate();
bool cmd_send_bytes();
bool cmd_send_clock();

CommandDispatcher*const dispatchers[] = {
    NULL,
    cmd_config_output_on,
    cmd_config_output_off,
    cmd_pulse,
    cmd_pulse_train,
    cmd_config_input_pullup,
    cmd_config_input_nopullup,
    cmd_read_pin,
    cmd_get_clock,
    cmd_get_last_clock,
    cmd_start_listening,
    cmd_stop_listening,
    cmd_set_data_rate,
    cmd_send_bytes,
    cmd_send_clock,
};
const byte num_commands = sizeof(dispatchers) / sizeof(dispatchers[0]);

byte command_byte_count = 0;
gkTime command_received_time;
gkTime command_initiated_time;
gkTime command_completed_time;
gkTime command_last_time_scheduled;

void setup() {
    gk_setup();
    gk_protect_serial_pins();
    gk_modulation_setup();
    gk_listeners_setup();
    Serial.begin(BAUD_RATE);

}

void loop() {
    byte cmd_byte;
    static CommandDispatcher active_command = NULL;

    // Perform any outputs according to the schedule
    gk_schedule_check();

    // Handle commands
    if (!active_command && Serial.available()) {
        // There is a command byte ready to read
        command_received_time = millis();
        cmd_byte = Serial.read();
        active_command = command_dispatchers[cmd_byte];
    }
    if (active_command && active_command())
        // There is an active command to execute, and it finishes its work
        active_command = NULL;

    // Handle listeners

}

bool cmd_config_output_on() {
    uint8_t pin;
    if (Serial.available()) {
        pin = Serial.read();
        config_pin(pin, MODE_OUT, PIN_ON);
        command_initiated_time = millis();
        command_completed_time = command_initiated_time;
        command_last_time_scheduled = command_initiated_time;
        return true;
    }
    return false;
}

bool cmd_config_output_off() {
    uint8_t pin;
    if (Serial.available()) {
        pin = Serial.read();
        config_pin(pin, MODE_OUT, PIN_OFF);
        command_initiated_time = millis();
        command_completed_time = command_initiated_time;
        command_last_time_scheduled = command_initiated_time;
        return true;
    }
    return false;
}

bool cmd_pulse() {
    static uint8_t pin;
    static uint8_t step = 0;

    if (processing_step == 0 && Serial.available()) {
        pin = Serial.read();
        set_pin(pin, PIN_TOGGLE);
        command_initiated_time = millis();
        command_last_time_scheduled = command_initiated_time;
        ++step;
    }
    if (processing_step == 1) {
        if (!subcmd_pulses(pin, 1)) {
            command_completed_time = command_last_time_scheduled;
            step = 0;
            return true;
        }
    }
    return false;
}

bool cmd_pulse_train() {
    static uint8_t pin, num;
    static uint8_t step = 0;

    if (processing_step == 0 && Serial.available()) {
        pin = Serial.read();
        set_pin(pin, PIN_TOGGLE);
        command_initiated_time = millis();
        command_last_time_scheduled = command_initiated_time;
        ++step;
    }
    if (processing_step == 1 && Serial.available()) {
        num = Serial.read();
        ++step;
    }
    if (processing_step == 2) {
        num -= subcmd_pulses(pin, num);
        if (num == 0) {
            // All pulses were processed
            command_completed_time = command_last_time_scheduled;
            step = 0;
            return true;
        }
    }
    return false;
}

uint8_t subcmd_pulses(uint8_t pin, uint8_t count) {
    byte dur1, dur2;
    uint8_t num_processed = 0;
    msTime next_pulse_time;

    while (count && Serial.available() >= 2) {
        dur1 = Serial.read();
        dur2 = Serial.read();
        next_pulse_time = command_last_time_scheduled + word(dur1, dur2);
        schedule_add(next_pulse_time, pin);
        command_last_time_scheduled = next_pulse_time;
        count--;
        num_processed++;
    }

    return num_processed;
}

bool cmd_config_input_pullup() {
    uint8_t pin;
    if (Serial.available()) {
        pin = Serial.read();
        config_pin(pin, MODE_IN, PULLUP_ON);
        return true;
    }
    return false;
}

bool cmd_config_input_nopullup() {
    uint8_t pin;
    if (Serial.available()) {
        pin = Serial.read();
        config_pin(pin, MODE_IN, PULLUP_OFF);
        return true;
    }
    return false;
}

bool cmd_read_pin() {
    uint8_t pin;
    bool value;
    if (Serial.available()) {
        pin = Serial.read();
        value = read_pin(pin);
        Serial.write(value);
        return true;
    }
    return false;
}

bool cmd_get_clock() {
    msTime time = millis();
    Serial.write((uint8_t*)&time, sizeof(time));
}

bool cmd_get_last_clock() {
    Serial.write(
        (uint8_t*)&command_initiated_time,
        sizeof(command_initiated_time) );
}

bool cmd_start_listening();
bool cmd_stop_listening();
bool cmd_set_data_rate();
bool cmd_send_bytes();
bool cmd_send_clock();
