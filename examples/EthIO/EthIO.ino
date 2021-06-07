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

uint8_t pin_output_inverted[GK_NUM_PINS] = {0};

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

// <pulse_train> <pin> <count> <on1> <on2> [<off1> <off2> <on1> <on2>]*
// Immediately turn pin on, then schedule it to be turned off in word(on1,on2)
// ms. If count>1, then schedule count-1 additional pulses, each starting
// word(off1,off2) ms after the previous one ends and lasting for word(on1,on2)
// ms.
bool cmd_pulse_train();

// <pulse_after> <pin> <delay1> <delay2> <dur1> <dur2>
// After a delay of word(<delay1>,<delay2>) ms, pulse <pin> for
// word(<dur1>,<dur2>) ms. If any actions are already scheduled for <pin>,
// the delay begins following the last scheduled action for that pin.
bool cmd_pulse_after();

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
    cmd_pulse_after,
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
gkTime command_time_received;
gkTime command_time_initiated;
gkTime command_time_last_scheduled;
gkTime command_time_completed;

void setup() {
    gk_setup();
    //gk_protect_serial_pins();
    gk_modulation_setup();
    //gk_listeners_setup();
    Serial.begin(BAUD_RATE);
}

void loop() {
    static CommandDispatcher* active_command = NULL;

    // Perform any outputs according to the schedule
    gk_schedule_execute();

    // If no command already in progress, check for a new command
    if (!active_command && Serial.available()) {
        // There is a command byte ready to read
        command_time_received = millis();
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
        pin_output_inverted[pin] = 1;
        gk_configure_pin(pin, true);
        gk_pin_set_mode(pin, GK_PIN_MODE_OUTPUT, GK_PIN_SET_OFF);
        command_time_initiated = millis();
        command_time_completed = command_time_initiated;
        command_time_last_scheduled = command_time_initiated;
        return true;
    }
    return false;
}

bool cmd_config_output_low() {
    uint8_t pin;
    if (Serial.available()) {
        pin = Serial.read();
        pin_output_inverted[pin] = 0;
        gk_configure_pin(pin, false);
        gk_pin_set_mode(pin, GK_PIN_MODE_OUTPUT, GK_PIN_SET_OFF);
        command_time_initiated = millis();
        command_time_completed = command_time_initiated;
        command_time_last_scheduled = command_time_initiated;
        return true;
    }
    return false;
}

bool cmd_pulse() {
    static uint8_t pin;
    static uint8_t step = 0;

    if (step == 0 && Serial.available()) {
        // Read which pin to pulse, and immediately turn it on
        pin = Serial.read();
        gk_pin_write(pin, GK_PIN_SET_ON);
        // Update scheduling time steps
        command_time_initiated = millis();
        command_time_last_scheduled = command_time_initiated;
        ++step;
    }
    if (step == 1 && Serial.available() >= 2) {
        // Read for how long the pin is to remain on, and schedule turning
        // it off
        short duration = word(Serial.read(), Serial.read());
        command_time_last_scheduled += duration;
        gk_schedule_add(command_time_last_scheduled, pin, GK_PIN_SET_OFF);
        // Update the time at which the last event from this command will be
        // completed, reset the state of this command reader, and report
        // that the command has been completely read.
        command_time_completed = command_time_last_scheduled;
        step = 0;
        return true;
    }
    return false;
}

bool cmd_pulse_after() {
    static uint8_t pin;
    static uint8_t step = 0;

    if (step == 0 && Serial.available()) {
        // Read which pin to pulse
        pin = Serial.read();
        ++step;
    }
    if (step == 1 && Serial.available() >= 2) {
        // Read for how long to delay before turning the pin on, then determine
        // whether that delay begins from when this command was received or
        // from the end of an already-scheduled action on that pin.
        short delay = word(Serial.read(), Serial.read());
        // Check for the last scheduled event on this pin
        uint8_t schedule_length = gk_schedule_size();
        command_time_initiated = command_time_received;
        for (int i = gk_schedule_size(); i >= 0; i--) {
            // Starting from the last scheduled action, check to see if any
            // of them refer to this pin. If one does then work from it.
            if (gk_schedule_get(i)->pin == pin) {
                command_time_initiated = gk_schedule_get(i)->time;
                break;
            }
        }
        command_time_initiated += delay;
        gk_schedule_add(command_time_initiated, pin, GK_PIN_SET_ON);
        // Update the scheduling time steps
        command_time_last_scheduled = command_time_initiated;
        ++step;
    }
    if (step == 2 && Serial.available() >= 2) {
        // Read for how long to delay before turning the pin off, and schedule
        // that following the turn on.
        short duration = word(Serial.read(), Serial.read());
        command_time_last_scheduled += duration;
        gk_schedule_add(command_time_last_scheduled, pin, GK_PIN_SET_OFF);
        // Clean up and report that the command has been processed.
        command_time_completed = command_time_last_scheduled;
        step = 0;
        return true;
    }
    return false;
}

bool cmd_pulse_train() {
    static uint8_t pin, num_to_process;
    static uint8_t step = 0;

    if (step == 0 && Serial.available()) {
        // Read which pin we're scheduling a train for and begin by turning it
        // on.
        pin = Serial.read();
        gk_pin_write(pin, GK_PIN_SET_ON);
        command_time_initiated = millis();
        command_time_last_scheduled = command_time_initiated;
        ++step;
    }
    if (step == 1 && Serial.available()) {
        // Read how many pulses this train consists of.
        uint8_t num_pulses = Serial.read();
        if (num_pulses) {
            // For N pulses there are 2N-1 delay intervals between scheduled
            // actions
            num_to_process = 2*num_pulses - 1;
            ++step;
        } else {
            // 0 pulses? Weird. We'll just end now.
            command_time_completed = command_time_initiated;
            step = 0;
            return true;
        }
    }
    if (step == 2) {
        // Read a total of 2N-1 delay intervals, each 2 bytes
        while (num_to_process && Serial.available() >= 2) {
            short delay = word(Serial.read(), Serial.read());
            // If num_to_process is odd, we are scheduling the pin to turn off;
            // if even, we're scheduling it on.
            gkPinAction action =
                (num_to_process % 2) ? GK_PIN_SET_OFF : GK_PIN_SET_ON;
            command_time_last_scheduled += delay;
            gk_schedule_add(command_time_last_scheduled, pin, action);
            --num_to_process;
        }
        if (!num_to_process) {
            // We have now processed all of the delays
            command_time_completed = command_time_last_scheduled;
            step = 0;
            return true;
        }
        // If there are still some to process, we remain on step 2 until all
        // have been processed.
    }
    return false;
}

bool cmd_config_input_pullup() {
    uint8_t pin;
    if (Serial.available()) {
        pin = Serial.read();
        // Make sure we're not using inverted logic:
        gk_configure_pin(pin, false);
        gk_pin_set_mode(pin, GK_PIN_MODE_INPUT, GK_PIN_PULLUP_ON);
        return true;
    }
    return false;
}

bool cmd_config_input_nopullup() {
    uint8_t pin;
    if (Serial.available()) {
        pin = Serial.read();
        gk_configure_pin(pin, false);
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
    serial_write_bigendian(
        (uint8_t*)&command_time_received,
        sizeof(command_time_received)
    );
}

bool cmd_get_last_clock() {
    serial_write_bigendian(
        (uint8_t*)&command_time_initiated,
        sizeof(command_time_initiated)
    );
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
