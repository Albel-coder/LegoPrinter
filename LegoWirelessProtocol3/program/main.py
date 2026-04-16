"""
Pybricks Klipper-style Firmware v6 (Stable)
- Two motors (Port A = x, Port B = Y)
- Binary protocol with CRC8
- Command queue per axis (16 slots)
- Continuous motion using track_target (or run_target fallback)
- Watchdog, status reporter
"""

from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import multitask, run_task, wait, StopWatch
from usys import stdin, stdout

# Configuration
SYNC_BYTE = 0xAA
BUFFER_SIZE = 16
DEFAULT_SPEED = 800
DEFAULT_ACCEL = 600
STATUS_INTERVAL_MS = 20
WATCHDOG_TIMEOUT_MS = 2000

# Hardware
hub = TechnicHub()
motors = {}
try:
    motors[0] = Motor(Port.A)   # X
    motors[1] = Motor(Port.B)   # Y
    for m in motors.values():
        m.control.limits(DEFAULT_SPEED, DEFAULT_ACCEL)
        m.reset_angle(0)
    hub.light.on(Color.GREEN)
except Exception as e:
    # Fatal motor init error
    err = b"FATAL: Motor init failed: " + str(e).encode + b"\n"
    stdout.buffer.write(err)
    stdout.flush()
    while True:
        hub.light.on(Color.RED)
        wait(200)
        hub.light.off()
        wait(200)

# Protocol Commands
CMD_UPDATE_TARGET = 0x10 # target(int32), speed(uint16), time_ms(uint16)
CMD_SET_VELOCITY = 0x11 # speed(int32)
CMD_STOP = 0x12 # stop_type (uint8)
CMD_SET_LIMITS = 0x20 # max_speed(uint32), max_accel(uint32)
CMD_RESET_POS = 0x21 # position(int32)
CMD_GET_STATUS = 0x30
CMD_EMERGENCY_STOP = 0x40
CMD_PING = 0x41
CMD_CLEAR_BUFFER = 0x42
CMD_ENABLED_WATCHDOG = 0x50

REPLY_STATUS = 0x80
REPLY_PONG = 0x81
REPLY_ERROR = 0xFF

STOP_COAST = 0
STOP_HOLD = 1

FLAG_MOVING = 0x01
FLAG_BUFFER_LOW = 0x02
FLAG_WATCHDOG = 0x04

# CRC8
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

# Command Queue
class CommandQueue:
    def __init__(self, maxSize):
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

# Packing Helpers
def pack_i32(val):
    return bytes([val & 0xFF, (val>>8)&0xFF, (val>>16)&0xFF, (val>>24)&0xFF])

def pack_u16(val):
    return bytes([val & 0xFF, (val>>8)&0xFF])

def unpack_i32(data, off=0):
    return int.from_bytes(data[off:off+4], 'little', True)

def unpack_u16(data, off=0):
    return int.from_bytes(data[off:off+2], 'little', False)

def unpack_u32(data, off=0):
    return int.from_bytes(data[off:off+4], 'little', False)

# Motion Executor
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
                # try using track_target, if not - run_target
                try:
                    motor.track_target(target, speed)
                except AttributeError:
                    motor.run_target(speed, target)
                next_time = watch.time()
            elif typ == 'velocity':
                motor.run(cmd[1])
            elif typ == 'stop':
                if cmd[1] == STOP_HOLD:
                    motor.hold()
                else:
                    motor.stop()
            await wait(1)
        except Exception as e:
            err = b"EXECUTOR %d ERROR: %s\n" % (axis, str(e).encode())
            stdout.buffer.write(err)
            stdout.flush()

# UART Receiver
async def read_exact(n):
    data = b''
    while len(data) < n:
        chunk = stdin.buffer.read(n - len(data))
        if chunk:
            data += chunk
        await wait(1)
    return data

watchdog_watch = StopWatch()
last_activity = watchdog_watch.time()
watchdog_enabled = True
watchdog_timeout = WATCHDOG_TIMEOUT_MS

async def uart_receiver():
    global last_activity, watchdog_enabled, watchdog_timeout
    while True:
        try:
            b = await read_exact(1)
            if b[0] != SYNC_BYTE:
                continue
            last_activity = watchdog_watch.time()

            length = (await read_exact(1))[0]
            if length < 2:
                continue
            
            frame = await read_exact(length + 1)
            axis, cmd = frame[0], frame[1]
            payload = frame[2:-1]
            crc_rcvd = frame[-1]

            crc_calc = crc8(bytes([SYNC_BYTE, length]) + frame[:-1])
            if crc_calc != crc_rcvd:
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
            if cmd == CMD_ENABLED_WATCHDOG:
                if len(payload) >= 2:
                    watchdog_timeout = unpack_u16(payload)
                    watchdog_enabled = (watchdog_timeout > 0)
                continue
            
            if axis not in (0, 1): continue

            q = queues[axis]
            motor = motors[axis]

            if cmd == CMD_CLEAR_BUFFER:
                q.clear()
            elif cmd == CMD_SET_LIMITS:
                if len(payload) >= 8:
                    max_spd = unpack_u32(payload, 0)
                    max_acc = unpack_u32(payload, 4)
                    motor.control.limits(max_spd, max_acc)
            elif cmd == CMD_RESET_POS:
                pos = unpack_i32(payload) if len(payload)>=4 else 0
                motor.reset_angle(pos)
            elif cmd == CMD_GET_STATUS:
                pos = motor.angle()
                spd = motor.speed()
                flags = 0
                if not motor.control.done(): flags |= FLAG_MOVING
                if q.free() < 2: flags |= FLAG_BUFFER_LOW
                if watchdog_enabled: flags |= FLAG_WATCHDOG
                reply = bytes([REPLY_STATUS, axis]) + pack_i32(pos) + pack_i32(spd) + bytes([flags, q.free(), BUFFER_SIZE])
                stdout.buffer.write(reply)
                stdout.flush()
            elif cmd == CMD_UPDATE_TARGET:
                if len(payload) >= 8:
                    target = unpack_i32(payload, 0)
                    speed = unpack_u16(payload, 4)
                    time_ms = unpack_u16(payload, 6)
                    if speed == 0: speed = DEFAULT_SPEED
                    q.put(('target', target, speed, time_ms))
            elif cmd == CMD_SET_VELOCITY:
                if len(payload) >= 4:
                    speed = unpack_i32(payload)
                    q.put(('velocity', speed))
            elif cmd == CMD_STOP:
                stop_type = payload[0] if payload else STOP_COAST
                q.put(('stop', stop_type))
        except Exception as e:
            err = b"RECEIVER ERROR: %s\n" % str(e).encode()
            stdout.buffer.write(err)
            stdout.flush()
        await wait(1)

# Watchdog
async def watchdog_task():
    global watchdog_enabled, watchdog_timeout, last_activity
    while True:
        if watchdog_enabled and watchdog_timeout > 0:
            now = watchdog_watch.time()
            if (now - last_activity) > watchdog_timeout:
                for m in motors.values():
                    m.stop()
                for q in queues.values():
                    q.clear()
                hub.light.on(Color.RED)
                watchdog_enabled = False
        await wait(100)

# Status Reporter
async def status_reporter(interval_ms):
    while True:
        try:
            for axis in (0, 1):
                motor = motors[axis]
                q = queues[axis]
                pos = motor.angle()
                spd = motor.speed()
                flags = 0
                if not motor.control.done(): flags |= FLAG_MOVING
                if q.free() < 2: flags |= FLAG_BUFFER_LOW
                if watchdog_enabled: flags |= FLAG_WATCHDOG
                reply = bytes([REPLY_STATUS, axis]) + pack_i32(pos) + pack_i32(spd) + bytes([flags, q.free(), BUFFER_SIZE])
                stdout.buffer.write(reply)
            stdout.flush()
        except Exception as e:
            pass
        await wait(interval_ms)

# Main
async def main():
    task_x = motion_executor(0, motors[0], queues[0])
    task_y = motion_executor(1, motion[1], queues[1])
    receiver = uart_receiver()
    reporter = status_reporter(STATUS_INTERVAL_MS)
    wd = watchdog_task()
    await multitask(task_x, task_y, receiver, reporter, wd)

try:
    run_task(main())
except Exception as e:
    err = b"MAIN FATAL: %s\n" % str(e).encode()
    stdout.buffer.write(err)
    stdout.flush()
    while True:
        hub.light.on(Color.RED)
        wait(200)
        hub.light.off()
        wait(200)