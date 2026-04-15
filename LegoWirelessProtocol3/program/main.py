"""
Pybricks Klipper-style Firmware v5 (Debug)
- Added error catching and logging
- Queue overflow detection
- Safer track_target calls
"""

from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import multitask, run_task, wait, StopWatch
from usys import stdin, stdout

# ----------------------------------------------------------------------
# 1. Configuration
# ----------------------------------------------------------------------
SYNC_BYTE = 0xAA
BUFFER_SIZE = 16
DEFAULT_SPEED = 800
DEFAULT_ACCEL = 600
STATUS_INTERVAL_MS = 20

# ----------------------------------------------------------------------
# 2. Hardware Init
# ----------------------------------------------------------------------
hub = TechnicHub()
motors = {}
try:
    motors[0] = Motor(Port.A)   # X
    motors[1] = Motor(Port.B)   # Y
    for m in motors.values():
        m.control.limits(speed=DEFAULT_SPEED, acceleration=DEFAULT_ACCEL)
        m.reset_angle(0)
    all_motors_ok = True
except Exception as e:
    all_motors_ok = False
    err = b"MOTOR INIT ERROR: " + str(e).encode() + b"\n"
    stdout.buffer.write(err)
    stdout.flush()
    while True:
        hub.light.on(Color.RED)
        wait(200)
        hub.light.off()
        wait(200)

# ----------------------------------------------------------------------
# 3. Protocol Commands
# ----------------------------------------------------------------------
CMD_UPDATE_TARGET   = 0x10
CMD_SET_VELOCITY    = 0x11
CMD_STOP            = 0x12
CMD_SET_LIMITS      = 0x20
CMD_RESET_POS       = 0x21
CMD_GET_STATUS      = 0x30
CMD_EMERGENCY_STOP  = 0x40
CMD_PING            = 0x41
CMD_CLEAR_BUFFER    = 0x42
CMD_ENABLE_WATCHDOG = 0x50

REPLY_STATUS = 0x80
REPLY_PONG   = 0x81
REPLY_ERROR  = 0xFF

STOP_COAST = 0
STOP_HOLD  = 1

FLAG_MOVING      = 0x01
FLAG_BUFFER_LOW  = 0x02
FLAG_WATCHDOG    = 0x04

# ----------------------------------------------------------------------
# 4. CRC8 Table
# ----------------------------------------------------------------------
CRC8_TABLE = [0] * 256
def _init_crc8_table():
    for i in range(256):
        crc = i
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x31
            else:
                crc <<= 1
            crc &= 0xFF
        CRC8_TABLE[i] = crc
_init_crc8_table()

def crc8(data):
    crc = 0
    for byte in data:
        crc = CRC8_TABLE[crc ^ byte]
    return crc

# ----------------------------------------------------------------------
# 5. Simple Command Queue
# ----------------------------------------------------------------------
class CommandQueue:
    def __init__(self, maxsize):
        self.maxsize = maxsize
        self._queue = []

    def full(self):
        return len(self._queue) >= self.maxsize

    def empty(self):
        return len(self._queue) == 0

    def put(self, item):
        if len(self._queue) < self.maxsize:
            self._queue.append(item)
            return True
        return False

    def get(self):
        if self._queue:
            return self._queue.pop(0)
        return None

    def clear(self):
        self._queue.clear()

    def free(self):
        return self.maxsize - len(self._queue)

queues = {0: CommandQueue(BUFFER_SIZE), 1: CommandQueue(BUFFER_SIZE)}

# ----------------------------------------------------------------------
# 6. Manual byte packing
# ----------------------------------------------------------------------
def pack_i32_le(val):
    return bytes([
        val & 0xFF,
        (val >> 8) & 0xFF,
        (val >> 16) & 0xFF,
        (val >> 24) & 0xFF
    ])

def pack_u16_le(val):
    return bytes([val & 0xFF, (val >> 8) & 0xFF])

def pack_u32_le(val):
    return bytes([
        val & 0xFF,
        (val >> 8) & 0xFF,
        (val >> 16) & 0xFF,
        (val >> 24) & 0xFF
    ])

def unpack_i32_le(data, offset=0):
    return int.from_bytes(data[offset:offset+4], 'little', signed=True)

def unpack_u16_le(data, offset=0):
    return int.from_bytes(data[offset:offset+2], 'little', signed=False)

def unpack_u32_le(data, offset=0):
    return int.from_bytes(data[offset:offset+4], 'little', signed=False)

# ----------------------------------------------------------------------
# 7. Motion Executor (with error catching)
# ----------------------------------------------------------------------
async def motion_executor(axis, motor, queue):
    watch = StopWatch()
    next_time = watch.time()
    while True:
        try:
            cmd = queue.get()
            if cmd is None:
                await wait(1)
                continue

            typ = cmd[0]
            if typ == 'target':
                target, speed, delay_ms = cmd[1], cmd[2], cmd[3]
                now = watch.time()
                target_time = next_time + delay_ms
                diff = target_time - now
                if diff > 0:
                    await wait(diff)
                # Безопасный вызов track_target
                try:
                    motor.track_target(target, speed)
                except Exception as e:
                    err = b"TRACK_TARGET ERROR axis=" + bytes([axis]) + b": " + str(e).encode() + b"\n"
                    stdout.buffer.write(err)
                    stdout.flush()
                next_time = watch.time()
            elif typ == 'velocity':
                speed = cmd[1]
                motor.run(speed)
            elif typ == 'stop':
                stop_type = cmd[1]
                if stop_type == STOP_HOLD:
                    motor.hold()
                else:
                    motor.stop()
                next_time = watch.time()
            await wait(1)
        except Exception as e:
            err = b"EXECUTOR ERROR axis=" + bytes([axis]) + b": " + str(e).encode() + b"\n"
            stdout.buffer.write(err)
            stdout.flush()

# ----------------------------------------------------------------------
# 8. UART Receiver (with debug)
# ----------------------------------------------------------------------
async def read_exact(n):
    data = b''
    while len(data) < n:
        chunk = stdin.buffer.read(n - len(data))
        if chunk:
            data += chunk
        await wait(1)
    return data

watchdog_watch = StopWatch()
last_activity_time = watchdog_watch.time()
watchdog_timeout = 0
watchdog_enabled = False

async def uart_receiver():
    global last_activity_time, watchdog_enabled, watchdog_timeout
    while True:
        try:
            b = await read_exact(1)
            if b[0] != SYNC_BYTE:
                continue
            # Визуальная индикация получения байта (для отладки)
            hub.light.on(Color.ORANGE)
            await wait(20)
            hub.light.on(Color.GREEN)

            last_activity_time = watchdog_watch.time()
            len_byte = await read_exact(1)
            length = len_byte[0]
            if length < 2:
                continue

            frame = await read_exact(length + 1)
            axis, cmd = frame[0], frame[1]
            payload = frame[2:-1]
            crc_received = frame[-1]

            crc_calc = crc8(bytes([SYNC_BYTE, length]) + frame[:-1])
            if crc_calc != crc_received:
                stdout.buffer.write(bytes([REPLY_ERROR, axis, cmd, 0x01]))
                stdout.flush()
                continue

            # Global commands
            if cmd == CMD_EMERGENCY_STOP:
                for m in motors.values():
                    m.stop()
                for q in queues.values():
                    q.clear()
                continue

            if cmd == CMD_PING:
                stdout.buffer.write(bytes([REPLY_PONG]))
                stdout.flush()
                continue

            if cmd == CMD_ENABLE_WATCHDOG:
                if len(payload) >= 2:
                    watchdog_timeout = unpack_u16_le(payload, 0)
                    watchdog_enabled = (watchdog_timeout > 0)
                continue

            if axis not in (0, 1):
                continue

            q = queues[axis]
            motor = motors[axis]

            if cmd == CMD_CLEAR_BUFFER:
                q.clear()
                continue

            if cmd == CMD_SET_LIMITS:
                if len(payload) >= 8:
                    max_speed = unpack_u32_le(payload, 0)
                    max_accel = unpack_u32_le(payload, 4)
                    motor.control.limits(speed=max_speed, acceleration=max_accel)
                continue

            if cmd == CMD_RESET_POS:
                if len(payload) >= 4:
                    pos = unpack_i32_le(payload, 0)
                    motor.reset_angle(pos)
                else:
                    motor.reset_angle(0)
                continue

            if cmd == CMD_GET_STATUS:
                pos = motor.angle()
                spd = motor.speed()
                flags = 0
                if not motor.control.done():
                    flags |= FLAG_MOVING
                if q.free() < 2:
                    flags |= FLAG_BUFFER_LOW
                if watchdog_enabled:
                    flags |= FLAG_WATCHDOG
                reply = bytes([REPLY_STATUS, axis]) + \
                        pack_i32_le(pos) + \
                        pack_i32_le(spd) + \
                        bytes([flags, q.free(), BUFFER_SIZE])
                stdout.buffer.write(reply)
                stdout.flush()
                continue

            # Motion commands
            if cmd == CMD_UPDATE_TARGET:
                if len(payload) >= 8:
                    target = unpack_i32_le(payload, 0)
                    speed = unpack_u16_le(payload, 4)
                    time_ms = unpack_u16_le(payload, 6)
                    if speed == 0:
                        speed = DEFAULT_SPEED
                    if not q.put(('target', target, speed, time_ms)):
                        stdout.buffer.write(b"ERROR: queue full axis=" + bytes([axis]) + b"\n")
                        stdout.flush()
            elif cmd == CMD_SET_VELOCITY:
                if len(payload) >= 4:
                    speed = unpack_i32_le(payload, 0)
                    if not q.put(('velocity', speed)):
                        stdout.buffer.write(b"ERROR: queue full axis=" + bytes([axis]) + b"\n")
                        stdout.flush()
            elif cmd == CMD_STOP:
                stop_type = payload[0] if len(payload) >= 1 else STOP_COAST
                if not q.put(('stop', stop_type)):
                    stdout.buffer.write(b"ERROR: queue full axis=" + bytes([axis]) + b"\n")
                    stdout.flush()
        except Exception as e:
            err = b"RECEIVER ERROR: " + str(e).encode() + b"\n"
            stdout.buffer.write(err)
            stdout.flush()
        await wait(1)

# ----------------------------------------------------------------------
# 9. Watchdog Task
# ----------------------------------------------------------------------
async def watchdog_task():
    global watchdog_enabled, watchdog_timeout, last_activity_time
    while True:
        try:
            if watchdog_enabled and watchdog_timeout > 0:
                now = watchdog_watch.time()
                if (now - last_activity_time) > watchdog_timeout:
                    for m in motors.values():
                        m.stop()
                    for q in queues.values():
                        q.clear()
                    hub.light.on(Color.RED)
                    watchdog_enabled = False
        except Exception as e:
            pass
        await wait(100)

# ----------------------------------------------------------------------
# 10. Status Reporter
# ----------------------------------------------------------------------
async def status_reporter(interval_ms):
    while True:
        try:
            for axis in (0, 1):
                motor = motors[axis]
                q = queues[axis]
                pos = motor.angle()
                spd = motor.speed()
                flags = 0
                if not motor.control.done():
                    flags |= FLAG_MOVING
                if q.free() < 2:
                    flags |= FLAG_BUFFER_LOW
                if watchdog_enabled:
                    flags |= FLAG_WATCHDOG
                reply = bytes([REPLY_STATUS, axis]) + \
                        pack_i32_le(pos) + \
                        pack_i32_le(spd) + \
                        bytes([flags, q.free(), BUFFER_SIZE])
                stdout.buffer.write(reply)
        except Exception as e:
            pass
        stdout.flush()
        await wait(interval_ms)

# ----------------------------------------------------------------------
# 11. Main with error catching
# ----------------------------------------------------------------------
async def main():
    hub.light.on(Color.GREEN)
    await wait(500)
    hub.light.off()

    task_x = motion_executor(0, motors[0], queues[0])
    task_y = motion_executor(1, motors[1], queues[1])
    receiver = uart_receiver()
    reporter = status_reporter(STATUS_INTERVAL_MS)
    wd = watchdog_task()

    try:
        await multitask(task_x, task_y, receiver, reporter, wd)
    except Exception as e:
        err = b"MAIN FATAL ERROR: " + str(e).encode() + b"\n"
        stdout.buffer.write(err)
        stdout.flush()
        while True:
            hub.light.on(Color.RED)
            await wait(200)
            hub.light.off()
            await wait(200)

# ----------------------------------------------------------------------
# 12. Entry Point
# ----------------------------------------------------------------------
try:
    run_task(main())
except Exception as e:
    err = b"BOOT FATAL ERROR: " + str(e).encode() + b"\n"
    stdout.buffer.write(err)
    stdout.flush()
    while True:
        hub.light.on(Color.RED)
        wait(100)
        hub.light.off()
        wait(100)