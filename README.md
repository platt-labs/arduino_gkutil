# Arduino utilities for behavioral experiments

When conducting behavioral experiments, it is often necessary to employ specialized custom electronic hardware to coordinate devices, provide environmental feedback, etc. For example, in studying animal behavior it is often necessary to trigger some kind of reward-delivery device in response to a programmed condition. In studying the physiology of behavior, it is often necessary to provide some form of synchronization between devices recording behavior and devices recording physiological signals. This library provides utilities for using the Arduino as a tool for meeting these needs for the behavioral researcher.

# The EthIO device

The most straightforward way to use this library is to use the built-in EthIO "example" sketch running on an Arduino. An Arduino running this code will act as a simple digital input-output device under the control of an attached PC, while providing millisecond-level timing coordination. The controlling PC sends various commands to trigger input and output actions by the Arduino on its digital I/O pins, and to retrieve information from the Arduino. In addition to the code to run on the Arduino, Python code to run on the PC is provided. However, the Arduino can be controlled using any software that can write to a serial port.

## Installation
1. Install the `arduino_gkutil` library into the `libraries` subdirectory of your Arduino software installation. For example, on Windows this might be `Documents\Arduino\libraries`.

2. Upload the EthIO "example" sketch to your Arduino
    1. From the menu select `File` > `Examples` > `GKUtil` > `EthIO`
    2. From the `Tools` menu, make sure the board and port are correct for your device
    3. Click the "Upload" icon.

3. If using the provided Python library to interact with the Arduino, copy the file `extras/EthIO/EthIO.py` to your desired Python source or library directory. Install the `PySerial` module (e.g., `pip install pyserial`).

## Usage with Python

The provided `EthIO` module provides a class, `EthIO`, for handling interactions with the EthIO Arduino code via a serial port (physical or USB-emulated). Responses from the EthIO device are handled using a helper class, `EthIOResponse`.

### *class* `EthIO.EthIO`
#### `__init__(port, baudrate=115200, timeout=0.1)`
Establish a new connection to the Arduino EthIO device. `port` is the name of the serial port used: on Windows this will be something like `COM1`; on Linux something like `/dev/ttyUSB0`. Check the Arduino documentation for the best ways to identify the serial port that the Arduino is connected to on your system. `timeout` is the amount of time, in seconds, to wait for read/write commands to finish. `baudrate` is the connection speed, and must match the speed used in the Arduino code. This can be changed by setting the value of the `BAUDRATE` constant in the `EthIO` "example" sketch before uploading it to the Arduino.

#### `is_ready`
On opening a new connection, the `EthIO` device will send a ready message over the serial link. Prior to receiving this, `is_ready` is False, and calling any command methods will raise `NoResponseError`. After ready message has been read, `is_ready` is True.

#### `config_output(pin, invert=False)`
Configure the digital I/O pin `pin` for output. If `invert`ed, the resting state of the pin is set to logic level high (5 or 3.3 volts, depending on the device), and pulses will pull the pin low; otherwise the pin is set to logic level low (ground), and pulses will pull the pin high. Note that pins 0 and 1 are reserved for serial port communication.

#### `pulse(pin, duration=10)`
Cause the device to immediately turn output `pin` on, then after `duration` milliseconds, turn it off again. `duration` must be less than 65536. This behavior is entirely asynchronous: the function returns immediately after sending the command, and the Arduino handles the pulse timing. If `pulse` is called on the same pin while the Arduino is still processing a previous pulse, unexpected behavior may result; see `pulse_after` for one alternative.

If using `pulse` as a synchronizing event between multiple devices, ensure that the `duration` of the pulse is long enough to be detected by the effective sampling rate of all devices. For example, if synchronizing to a device that reads an input at 100 Hz, the pulse duration should be at least 20 ms.

#### `pulse_after(pin, duration=10, delay=1)`
Cause the device to wait `delay` milliseconds, then pulse output `pin` for `duration` milliseconds. If other write actions are already scheduled on `pin` (e.g., from a previous call to `pulse_after` or `pulse`), the `delay` interval begins only after the last scheduled write action. For example, if `pulse_after` is called twice in quick succession, the result will be two pulses separated by (at least) `delay` milliseconds, even if the second call occurred less than `duration` milliseconds after the first.

#### `pulse_train(pin, intervals)`
Cause the device to send a series of pulses on output `pin`, scheduled according to `intervals`. **Not yet implemented.**

#### `config_input(pin, pullup=False)`
Configure the digital I/O pin `pin` for input. The device's internal pullup resistor is set according to `pullup`.

#### `read_pin(pin)`
Request the current status of input `pin`. Returns an `EthIOResponse` where `value` will be either `True` (for logic level high) or `False` (for logic level low).

#### `get_clock()`
Request the current value of the device's clock, in milliseconds. This value is set the moment the device receives the 1-byte command transmission. Returns an `EthIOResponse`.

#### `get_last_clock()`
Request the value of the device's clock at the moment the last pin write command was scheduled. For `pulse` and `pulse_after`, this is the moment that the digital pin was actually turned on (or off, for inverted output). For `pulse_train`, this is the leading edge of the first pulse in the train. Returns an `EthIOResponse`.

Note that because this is scheduled time, it is possible to receive a value that is actually in the future relative to the method call. For example, consider the following sequence of calls for an `EthIO` stored in `io`:

```
io.pulse(13, duration=1000)
resp1 = io.get_last_clock()
io.pulse_after(13, duration=1000, delay=100)
resp2 = io.get_last_clock()
while not resp2.is_ready:
    pass # Wait until we've received both responses
print(resp2.value - resp1.value)
```

In this case, assuming the Python interpreter and the serial connection are running at normal speeds, this should print a value of 1100 *before the first pulse has even finished*.

#### `get_schedule_size()`
Request the number of items currently in the EthIO device's output scheduling buffer, intended primarily for debugging purposes. Returns an `EthIOResponse`.

### *class* `EthIOResponse`
Objects of this class are intended only to be constructed by an `EthIO` object. They are essentially "promise-like" objects which present data sent by the EthIO device once it has been fully received over the serial link.

#### `is_ready`
Before the data expected by this object has been received, `False`. Only once *all* of the data expected by the object has been received will it become `True`.

#### `value`
The value received in response to the request that generated this `EthIOResponse` object. If the data has not yet been received, attempting to access `value` will raise `NoResponseError`.

#### `num_bytes`
The number of bytes expected (or received) for this response.

#### `raw_data`
A `bytes` object containing the raw data received from the EthIO device.

#### `converter`
The function used to convert the raw data into the processed value.

#### `ethio`
The `EthIO` object that "owns" this `EthIOResponse`.

### Exceptions
#### `NoResponseError`

## Usage with other systems
The `EthIO` Arduino sketch implements a simple protocol for dispatching a set of commands, each represented by a single byte followed by zero or more "argument" bytes. The function of each command is as described above for the Python module. Multi-byte arguments are big-endian.

### Commands
#### `0x00(no-op)`
No arguments; does nothing.

#### `0x01(config_output) pin`
Configure `pin` for output.

* `pin`: 1 byte

#### `0x02(config_output_inverted) pin`
Configure `pin` for output, using inverted logic levels.

* `pin`: 1 byte

#### `0x03(pulse) pin duration`
Immediately toggle a pin, then after a delay toggle it back.

* `pin`: 1 byte
* `duration`: 2 bytes

#### `0x04(pulse_train) pin count duration_0 [delay_k duration_k]*`
Send a sequence of pulses on a pin. There must be `count`-1 pairs of `delay_k duration_k` arguments.

* `pin`: 1 byte
* `count`: 1 byte; number of pulses to send
* `duration_k`: 2 bytes; width of the k'th pulse
* `delay_k`: 2 bytes; interval preceding the k'th pulse

#### `0x05(pulse_after) pin delay duration`
Initiate a pulse after a delay.

* `pin`: 1 byte
* `delay`: 2 bytes
* `duration`: 2 bytes

#### `0x06(config_input_pullup) pin`
Configure `pin` for input and activate its pullup resistor

* `pin`: 1 byte

#### `0x07(config_input_nopullup) pin`
Configure `pin` for input and deactivate its pullup resistor

* `pin`: 1 byte

#### `0x08(read_pin) pin`
Request the current value of input `pin`

* `pin`: 1 byte

Writes the following response:

* `value`: 1 byte; a 0 or 1.

#### `0x09(get_clock)`
Request the current time on the device's millisecond-precision clock. No arguments. Writes the following response:

* `time`: 4 bytes

#### `0x0A(get_last_clock)`
Request the time that the last write command was initiated. No arguments. Writes the following response:

* `time`: 4 bytes

#### `0x0B(get_schedule_size)`
Request the number of items in the output schedule queue. No arguments. Writes the following response:

* `size`: 1 byte


# The GKUtil Library

The library provides a number of functions to assist with writing Arduino code for the specialized purposes of running behavioral experiments. For many simple uses, the built-in Arduino libraries are more than adequate, but the tools provided here may be helpful when writing customized Arduino code when knowing the timing at which various events are being coordinated is required to be both accurate and precise. The library is split into several parts.

## `gkutil.h`
This header provides the basic functionality of the library, replacing the standard Arduino I/O functions with more specialized ones that are designed to be extensible and customizable.

### Data types
#### `uint8_t gkPin`
Type alias for pin identifiers.

#### `uint8_t gkPinMode`
Type alias with two associated constants:

* `GK_PIN_MODE_INPUT` = 1
* `GK_PIN_MODE_OUTPUT` = 2

#### `uint8_t gkPinAction`
Type alias for identifying "actions" to take on a pin, with associated constants:

* `GK_PIN_WRITE_PASS` = 0: Do nothing
* `GK_PIN_WRITE_OFF` = 1: Turn the output off
* `GK_PIN_WRITE_ON` = 2: Turn the output on
* `GK_PIN_WRITE_TOGGLE` = 3: Toggle the output from its current state
* `GK_PIN_PULLUP_OFF` = 1: Turn off an input's pullup resistor
* `GK_PIN_PULLUP_ON` = 2: Turn on an input's pullup resistor

#### `unsigned long gkTime`
Type alias for the millisecond-precision clock values.

#### `void gkPinModeSetter(gkPin, gkPinMode, gkPinAction)`
Function pointer type for functions that are used to set the mode (input/output) of a pin.

#### `void gkPinWriter(gkPin, gkPinAction)`
Function pointer type for functions that are used to write digital output on a pin.

#### `gkPinValue gkPinReader(gkPin)`
Function pointer type for functions that are used to read digital input on a pin.

### Functions
#### `void gk_setup(void)`
Sets up the GKUtil library. Call once during the `setup()` function of the sketch.

#### `void gk_pin_configure(gkPin, gkPinModeSetter*, gkPinWriter*, gkPinReader*)`
Configure a pin with functions to change its mode, write outputs, and read inputs.

#### `void gk_pin_configure_simple(gkPin)`
Configure a pin for standard "simple" behavior, using the functions `gk_pin_set_mode_simple`, `gk_pin_write_simple`, and `gk_pin_read_simple`. *(Implemented as a macro calling `gk_pin_configure`.)*

#### `void gk_pin_disable(gkPin)`
Disable a pin. Subsequent attempts to write, or set the mode of the pin will have no effect, and reads will always return 0. *(Implemented as a macro calling `gk_pin_configure`.)*

#### `void gk_pin_set_mode(gkPin, gkPinMode, gkPinAction)`
Change the mode of a pin, and immediately take some action, using the `gkPinModeSetter` for the pin.

#### `void gk_pin_write(gkPin, gkPinAction)`
Perform a digital write using the `gkPinWriter` configured for the pin.

#### `gkPinValue gk_pin_read(gkPin)`
Perform a digital read using the `gkPinReader` configured for the pin.

#### `void gk_crc8_update(void*, uint8_t)`

## `gkutil/schedule.h`
This header provides functions for schedule digital output. The schedule is functionally a priority queue of actions to be performed at specified times on specified output pins. The schedule is maintained in sorted order by time, and whenever `schedule_execute()` is called, any actions that are "due" are performed and the completed items are cleared from the schedule.

### Data types
#### `struct gkScheduledEvent`
An event in the schedule, having the following fields:
* `gkTime time`: when the action should be taken
* `gkPin pin`: which pin is to be manipulated
* `gkPinAction action`: what action to take on the pin

#### `gkScheduleIterator`
An opaque data type used to iterate through the schedule.

### Functions
#### `uint8_t gk_schedule_add(gkTime, gkPin, gkPinAction)`
Add an event to the schedule. Once `millis()` is greater than the provided time, the specified action will be taken on the specified pin.

#### `uint8_t gk_schedule_size()`
Get the number of events currently in the schedule.

#### `gkScheduleIterator gk_schedule_head()`
Get an iterator corresponding to the first (lowest time) item in the schedule, or null if the schedule is empty.

#### `gkScheduleIterator gk_schedule_tail()`
Get an iterator corresponding to the last (greatest time) item in the schedule, or null if the schedule is empty.

#### `gkScheduleIterator gk_schedule_next(gkScheduleIterator)`
Get the next iterator after the current one.

#### `gkScheduleIterator gk_schedule_prev(gkScheduleIterator)`
Get the previous iterator before the current one.

#### `gkScheduledEvent *const gk_schedule_get(gkScheduleIterator)`
Get the event referred to by the current iterator.

#### `void gk_schedule_remove(gkScheduleIterator)`
Remove the item corresponding to the current iterator from the schedule.

#### `void gk_schedule_execute()`
Check the schedule for actions that are due to be performed, execute them if any, and remove them from the schedule.

#### `void gk_schedule_write_byte( [...] )`
Perform a low-bitrate serial write over any digital output pin by scheduling a series of on/off writes, using a very simple protocol. Each bit consists of either a 1-waveform (bit_width ms ON followed by bit_interval-bit_width ms OFF) or a 0-waveform (bit_interval ms OFF). Here, ON is defined as departing from the intially set value, OFF as remaining unchanged. The sequence of bits is preceded and followed by a 1-waveform.

Has the following parameters:

1. `gkTime time`: when to begin the write, corresponding to the leading edge of the first 1-waveform
2. `gkPin pin`: which pin to perform the write on
3. `gkTime bit_interval`: the time, in ms, between leading edges of sequential 1-waveforms (or when that leading edge would have occurred for 0-waveforms)
4. `gkTime bit_width`: the time, in ms, that 1-waveforms are on. Must be less than `bit_interval`.
5. `uint8_t value`: the byte to write

#### `void gk_schedule_write_bytes( [...] )`
A multi-byte version of `gk_serial_write_byte`. The 1-waveforms are inserted at the beginning and end of the multi-byte sequence, with no extra "bits" between bytes.

Has the following parameters:

1. `gkTime time`
2. `gkPin pin`
3. `gkTime bit_interval`
4. `gkTime bit_width`
5. `uint8_t count`: the number of bytes to write
6. `uint8_t *value`: A pointer to the start of a buffer containing the data to write

## `gkutil/modulation.h`
This header provides functions to put a pin into "modulation mode", such that when it is written using `gk_pin_write`, a logical "on" causes the pin to oscillate at a fixed frequency and duty cycle. This is particularly useful for using infrared receiver chips to wirelessly synchronize devices. IR receivers typically do background rejection by looking for signals modulated at a specific frequency, often 38 kHz. An Arduino is capable of producing such a modulated signal on some of its pins with no extra hardware required. This header uses the flexible `gkutil` interface to allow such a modulated pin to be configured once and then simply treated as any other digital I/O pin.

This header provides one global variable: `modulated_pin`, which identifies which pin is available for modulation.

### Functions

#### `void gk_modulation_setup(void)`
Call this function once during the `setup()` function of the main program.

#### `void gk_pin_set_mode_modulator(gkPin, gkPinMode, gkPinAction)`
A `gkPinModeSetter` for a modulated pin.

#### `void gk_pin_write_modulator(gkPin, gkPinAction)`
A `gkPinWriter` for a modulated pin.

## `gkutil/listener.h`
A header providing utilities to scan inputs for changes and take actions based on those changes. **The implementation of this section is not yet complete.**
