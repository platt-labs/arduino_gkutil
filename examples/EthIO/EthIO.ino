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
#include <gkutil/modulation.h>
#include <gkutil/schedule.h>
//#include <gkutil/listener.h>

#define BAUD_RATE 115200

int serial_write_bigendian(uint8_t* value, int size);

// A command dispatcher is a function taking no arguments and returning a
// boolean true (if the command is completed) or false (if the command needs to
// remain active)
typedef bool CommandDispatcher(void);

// <config_output_high> <pin>
// Configure <pin> for output, and set its "resting" value to high
bool cmd_config_output_high();
// <config_output_low> <pin>
// Configure <pin> for output, and set its "resting" value to low
bool cmd_config_output_low();
// <pulse> <pin> <dur1> <dur2>
// Immediately toggle <pin>, then after word(<dur1>,<dur2>) ms, toggle it back
bool cmd_pulse();
// <pulse_train> <pin> <count> [<dur1> <dur2>]*
// Immediately toggle <pin>, then schedule <count> more toggles to occur at
// intervals defined by each pair of subsequent <dur> arguments, each pair
// interpreted as word(<dur1>,<dur2>) ms
bool cmd_pulse_train();
// <config_input_pullup> <pin>
// Configure <pin> for input, and activate its pullup resistor
bool cmd_config_input_pullup();
// <config_input_nopullup> <pin>
// Configure <pin> for input, and deactivate its pullup resistor
bool cmd_config_input_nopullup();
// <read_pin> <pin>
// Immediately check the input value on <pin> and send it to serial output
bool cmd_read_pin();
// <get_clock>
// Send the current time of millis() to serial output.
bool cmd_get_clock();
// <get_last_clock>
// Send the time at which the last pin write command was initiated to
// serial output. This will correspond to the moment of the first pin value
// toggle produced by the command (e.g., rising edge of pulse).
bool cmd_get_last_clock();
//bool cmd_start_listening();
//bool cmd_stop_listening();
//bool cmd_set_data_rate();
//bool cmd_send_bytes();
//bool cmd_send_clock();
bool cmd_get_schedule_size();

CommandDispatcher*const dispatchers[] = {
    NULL,
    cmd_config_output_high,
    cmd_config_output_low,
    cmd_pulse,
    cmd_pulse_train,
    cmd_config_input_pullup,
    cmd_config_input_nopullup,
    cmd_read_pin,
    cmd_get_clock,
    cmd_get_last_clock,
//    cmd_start_listening,
//    cmd_stop_listening,
//    cmd_set_data_rate,
//    cmd_send_bytes,
//    cmd_send_clock,
    cmd_get_schedule_size,
};
const byte num_commands = sizeof(dispatchers) / sizeof(dispatchers[0]);

// State used to track the number of bytes that have been received in a
// multi-byte command, and track time offsets for pulse scheduling
byte command_byte_count = 0;
gkTime command_received_time;
gkTime command_initiated_time;
gkTime command_completed_time;
gkTime command_last_time_scheduled;

void setup() {
    gk_setup();
    gk_protect_serial_pins();
    gk_modulation_setup();
    //gk_listeners_setup();
    Serial.begin(BAUD_RATE);
    Serial.write("Reporting for duty");
}

void loop() {
    static CommandDispatcher* active_command = NULL;

    // Perform any outputs according to the schedule
    gk_schedule_execute();

    // If no command already in progress, check for a new command
    if (!active_command && Serial.available()) {
        // There is a command byte ready to read
        command_received_time = millis();
        byte cmd_byte = Serial.read();
        if (cmd_byte < num_commands)
            active_command = dispatchers[cmd_byte];
        else
            active_command = NULL;
    }
    // Run the currently active command, if it exists
    if (active_command && active_command())
        // The command finished and can now be deactivated
        active_command = NULL;

    // Handle listeners

}

bool cmd_config_output_high() {
    uint8_t pin;
    if (Serial.available()) {
        pin = Serial.read();
        gk_pin_set_mode(pin, GK_PIN_MODE_OUTPUT, GK_PIN_SET_ON);
        command_initiated_time = millis();
        command_completed_time = command_initiated_time;
        command_last_time_scheduled = command_initiated_time;
        return true;
    }
    return false;
}

bool cmd_config_output_low() {
    uint8_t pin;
    if (Serial.available()) {
        pin = Serial.read();
        gk_pin_set_mode(pin, GK_PIN_MODE_OUTPUT, GK_PIN_SET_OFF);
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

    if (step == 0 && Serial.available()) {
        pin = Serial.read();
        gk_pin_write(pin, GK_PIN_SET_TOGGLE);
        command_initiated_time = millis();
        command_last_time_scheduled = command_initiated_time;
        ++step;
    }
    if (step == 1) {
        if (subcmd_pulses(pin, 1)) {
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

    if (step == 0 && Serial.available()) {
        pin = Serial.read();
        gk_pin_write(pin, GK_PIN_SET_TOGGLE);
        command_initiated_time = millis();
        command_last_time_scheduled = command_initiated_time;
        ++step;
    }
    if (step == 1 && Serial.available()) {
        num = Serial.read();
        ++step;
    }
    if (step == 2) {
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
    uint8_t num_processed = 0;
    //Serial.print("subcmd_pulses ");
    //Serial.print(pin);
    //Serial.print(" ");
    //Serial.println(count);

    while (count && Serial.available() >= 2) {
        byte dur1 = Serial.read();
        byte dur2 = Serial.read();
        gkTime next_pulse_time = command_last_time_scheduled + word(dur1, dur2);
        gk_schedule_add(next_pulse_time, pin, GK_PIN_SET_TOGGLE);
        //Serial.print("add schedule ");
        //Serial.println(sched.buffer[sched.head].time);
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
        gk_pin_set_mode(pin, GK_PIN_MODE_INPUT, GK_PIN_PULLUP_ON);
        return true;
    }
    return false;
}

bool cmd_config_input_nopullup() {
    uint8_t pin;
    if (Serial.available()) {
        pin = Serial.read();
        gk_pin_set_mode(pin, GK_PIN_MODE_INPUT, GK_PIN_PULLUP_OFF);
        return true;
    }
    return false;
}

bool cmd_read_pin() {
    uint8_t pin;
    bool value;
    if (Serial.available()) {
        pin = Serial.read();
        value = gk_pin_read(pin);
        Serial.write(value);
        return true;
    }
    return false;
}

bool cmd_get_clock() {
    gkTime time = millis();
    //Serial.write((uint8_t*)&time, sizeof(time));
    serial_write_bigendian((uint8_t*)&time, sizeof(time));
}

bool cmd_get_last_clock() {
    //Serial.write(
    //    (uint8_t*)&command_initiated_time,
    //    sizeof(command_initiated_time)
    //);
    serial_write_bigendian((uint8_t*)&command_initiated_time, sizeof(command_initiated_time));
}

bool cmd_get_schedule_size() {
    Serial.write(gk_schedule_size());
}

//bool cmd_start_listening();
//bool cmd_stop_listening();
//bool cmd_set_data_rate();
//bool cmd_send_bytes();
//bool cmd_send_clock();

int serial_write_bigendian(uint8_t* value, int size) {
    for (int i = size-1; i >= 0; i--) {
        Serial.write(value[i]);
    }
    return size;
}
