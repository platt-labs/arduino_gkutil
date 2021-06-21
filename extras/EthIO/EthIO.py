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

def require_ready(f):
    def _require_ready(self, *args, **kwargs):
        if not self._is_ready:
            raise NoResponseError
        return f(self, *args, **kwargs)
    return _require_ready

class NoResponseError(Exception):
    """
    Raised when trying to access the EthIO device without first receiving a
    READY message, or when reading the value of an EthIOResponse before the
    device has replied. If the connection is good, then trying the access again
    after a few milliseconds should work.
    """
    pass

class EthIO:
    def __init__(self, port=None, baudrate=115200, timeout=0.1):
        self._io = serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=timeout
        )
        self._responders = []
        self._is_ready = False
        self._ready_message = ""

    @property
    def port(self):
        return self._io.port
    @port.setter
    def port(self, new_port):
        self._io.port = new_port

    def open(self):
        self._io.open()

    def close(self):
        self._io.close()
        self._is_ready = False
        self._ready_message = ""
        self._responders = []

    @property
    def is_open(self):
        return self._io.is_open

    @property
    def is_ready(self):
        if self._is_ready:
            return True
        # Check if the device has reported in
        self._ready_message += self._io.read_until().decode()
        if self._ready_message and self._ready_message[-1] == "\n":
            self._is_ready = True
        return self._is_ready

    @require_ready
    def config_output(self, pin, invert=False):
        if invert:
            msg = msg_start['config_output_inverted']
        else:
            msg = msg_start['config_output']
        msg += pin.to_bytes(1, byteorder='big')
        self._io.write(msg)

    @require_ready
    def pulse(self, pin, duration=10):
        msg = msg_start['pulse']
        msg += pin.to_bytes(1, byteorder='big')
        msg += duration.to_bytes(2, byteorder='big')
        self._io.write(msg)

    @require_ready
    def pulse_after(self, pin, duration=10, delay=1):
        msg = msg_start['pulse_after']
        msg += pin.to_bytes(1, byteorder='big')
        msg += delay.to_bytes(2, byteorder='big')
        msg += duration.to_bytes(2, byteorder='big')
        self._io.write(msg)

    @require_ready
    def pulse_train(self, pin, intervals):
        pass # Not yet implemented

    @require_ready
    def config_input(self, pin, pullup=False):
        if pullup:
            msg = msg_start['config_input_pullup']
        else:
            msg = msg_start['config_input_nopullup']
        msg += pin.to_bytes(1, byteorder='big')
        self._io.write(msg)

    @require_ready
    def read_pin(self, pin):
        msg = msg_start['read_pin']
        msg += pin.to_bytes(1, byteorder='big')
        self._io.write(msg)
        new_response = EthIOResponse(self, 1, convert_pin_input)
        self._responders.append(new_response)
        return new_response

    @require_ready
    def get_clock(self):
        msg = msg_start['get_clock']
        self._io.write(msg)
        new_response = EthIOResponse(self, 4, convert_time_ms)
        self._responders.append(new_response)
        return new_response

    @require_ready
    def get_last_clock(self):
        msg = msg_start['get_last_clock']
        self._io.write(msg)
        new_response = EthIOResponse(self, 4, convert_time_ms)
        self._responders.append(new_response)
        return new_response

    @require_ready
    def get_schedule_size(self):
        msg = msg_start['get_schedule_size']
        self._io.write(msg)
        new_response = EthIOResponse(self, 1, convert_int)
        self._responders.append(new_response)
        return new_response

# Not sure this is the best way to implement this; seems a bit sketchy to let
# the EthIOResponse control the EthIO's queue and its siblings.
class EthIOResponse:
    def __init__(self, ethio, num_bytes, converter):
        self.ethio = ethio
        self.num_bytes = num_bytes
        self.converter = converter
        self.raw_data = bytes()
        self._value = None
        self._is_ready = False
        self._is_defunct = False

    @property
    def is_ready(self):
        if self._is_ready:
            return True
        if self._is_defunct:
            return False
        try:
            while self.ethio._responders[0] is not self:
                # There are other EthIOReponses queued ahead of this one. Check
                # if the first one in the queue is ready until we either find
                # one that's not, or we exhaust the queue ahead of this one.
                if not self.ethio._responders[0].is_ready:
                    return False
        except IndexError:
            # If we hit this, it means we made it to the end of the _responders
            # queue without encountering this EthIOResponse. That means
            # something went wrong (maybe the connection was closed) and this
            # EthIOResponse is defunct.
            self._is_defunct = True
            return False
        # This EthIOResponse is the next in the queue. Check if it's available.
        bytes_remaining = self.num_bytes - len(self.raw_data)
        self.raw_data += self.ethio._io.read(bytes_remaining)
        if len(self.raw_data) == self.num_bytes:
            # We have now read the entire reply for this one
            self._value = self.converter(self.raw_data)
            self._is_ready = True
            self.ethio._responders.pop(0)
            return True
        else:
            # There is still data remaining to be read
            return False

    @property
    @require_ready
    def value(self):
        return self._value
