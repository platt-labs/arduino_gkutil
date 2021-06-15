import serial

commands = [
    'config_output',
    'config_output_inverted',
    'pulse',
    'pulse_train',
    'pulse_after',
    'config_input_pullup',
    'config_input_nopullup',
    'read_pin',
    'get_clock',
    'get_last_clock',

    'get_schedule_size',
]

msg_start = {
    cmd: (ind+1).to_bytes(1, byteorder='big')
    for (ind, cmd) in enumerate(commands)
}

def convert_pin_input(raw_bytes):
    return bool.from_bytes(raw_bytes, byteorder='big')

def convert_int(raw_bytes):
    return int.from_bytes(raw_bytes, byteorder='big')

def convert_time_ms(raw_bytes):
    return int.from_bytes(raw_bytes, byteorder='big')

class EthIO:
    def __init__(self, port, baudrate=115200, timeout=0.1):
        self.device = serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=timeout
        )
        self._responders = []
        self._is_ready = False
        self._ready_message = ""

    @property
    def is_ready(self):
        if self._is_ready:
            return True
        # Check if the device has reported in
        self._ready_message += self.device.read_until().decode()
        if self._ready_message and self._ready_message[-1] == "\n":
            self._is_ready = True
        return self._is_ready

    def config_output(self, pin, invert=False):
        if invert:
            msg = msg_start['config_output_inverted']
        else:
            msg = msg_start['config_output']
        msg += pin.to_bytes(1, byteorder='big')
        self.device.write(msg)

    def pulse(self, pin, duration):
        msg = msg_start['pulse']
        msg += pin.to_bytes(1, byteorder='big')
        msg += duration.to_bytes(2, byteorder='big')
        self.device.write(msg)

    def pulse_after(self, pin, duration, after=10):
        msg = msg_start['pulse_after']
        msg += pin.to_bytes(1, byteorder='big')
        msg += after.to_bytes(2, byteorder='big')
        msg += duration.to_bytes(2, byteorder='big')
        self.device.write(msg)

    def pulse_train(self, pin, intervals):
        pass # Not yet implemented

    def config_input(self, pin, pullup=False):
        if pullup:
            msg = msg_start['config_input_pullup']
        else:
            msg = msg_start['config_input_nopullup']
        msg += pin.to_bytes(1, byteorder='big')
        self.device.write(msg)

    def read_pin(self, pin):
        msg = msg_start['read_pin']
        msg += pin.to_bytes(1, byteorder='big')
        self.device.write(msg)
        new_response = EthIOResponse(self, 1, convert_pin_input)
        self._responders.append(new_response)
        return new_response

    def get_clock(self):
        msg = msg_start['get_clock']
        self.device.write(msg)
        new_response = EthIOResponse(self, 4, convert_time_ms)
        self._responders.append(new_response)
        return new_response

    def get_last_clock(self):
        msg = msg_start['get_last_clock']
        self.device.write(msg)
        new_response = EthIOResponse(self, 4, convert_time_ms)
        self._responders.append(new_response)
        return new_response

    def get_schedule_size(self):
        msg = msg_start['get_schedule_size']
        self.device.write(msg)
        new_response = EthIOResponse(self, 1, convert_int)
        self._responders.append(new_response)
        return new_response

class EthIOResponse:
    def __init__(self, ethio, num_bytes, converter):
        self.ethio = ethio
        self.num_bytes = num_bytes
        self.converter = converter
        self.raw_data = bytes()
        self.value = None
        self.finished = False

    @property
    def is_ready(self):
        if self.finished:
            return True
        if not (
                self.ethio.is_ready and
                self.ethio._responders and
                self.ethio._responders[0] == self
                ):
            # This response hasn't even started yet
            return False
        bytes_remaining = self.num_bytes - len(self.raw_data)
        self.raw_data += self.ethio.device.read(bytes_remaining)
        if len(self.raw_data) == self.num_bytes:
            self.value = self.converter(self.raw_data)
            self.finished = True
            self.ethio._responders.pop(0)
            return True
        else:
            return False
