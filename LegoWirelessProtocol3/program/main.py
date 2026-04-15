"""
Pybricks Klipper-style Firmware v2
- Continuous motion with per-segmnet speed control
- Time-synchronized target updates
- Unified command queue per axis
- Reliable watchdog and enhanced status reporting
"""

from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import multitask, run_task, wait
from usys import stdin, stdout
import asyncio
import struct
import time

# Configuration
SYNC_BYTE = 0xAA
BUFFER_SIZE = 16
DEFAULT_SPEED = 800
DEFAULT_ACCEL = 600
STATUS_INTERVAL_MS = 20    # 50 Hz

# Hardware Init
hub = TechnicHub()
motors = {
    0: Motor(Port.A),   # X
    1: Motor(Port.B)   # Y
}

for motor in motors.values():
    motor.control.limits(speed=DEFAULT_SPEED, acceleration=DEFAULT_ACCEL)
    motor.reset_angle(0)

# Protocol commands
CMD_UPDATE_TARGET = 0x10 # payload: target(int32), speed(uint16), time_ms(uint16)
CMD_SET_VELOCITY = 0x11 # payload: speed(int32)
CMD_STOP = 0x12 # payload: stop_type(uint8)
CMD_SET_LIMITS = 0x20 # payload: max_speed(uint32), max_accel(uint32)
CMD_RESET_POS = 0x21 # payload: position(int32)
CMD_GET_STATUS = 0x30
CMD_EMERGENCY_STOP = 0x40
CMD_PING = 0x41
CMD_CLEAR_BUFFER = 0x42
CMD_ENABLE_WATCHBOG = 0x50 # payload: timeout_ms (uint16)

REPLY_STATUS = 0x80
REPLY_PONG = 0x81
REPLY_ERROR = 0xFF

STOP_COAST = 0
STOP_HOLD = 1

FLAG_MOVING = 0x01
FLAG_BUFFER_LOW = 0x02
FLAG_WATCHDOG = 0x04

# CRC8 with Lookup Table
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

# Unified Command Queue (per axis)
class CommandQueue:
    def __init__(self, maxsize):
        self.maxsize = maxsize
        self._queue = []
        self._lock = asyncio.Lock()
    
    def full(self):
        return len(self._queue) == 0
    
    async def put(self, item):
        async with self._lock:
            if len(self._queue) < self.maxsize:
                self._queue.append(item)
                return True
            return False
    
    async def clear(self):
        async with self._lock:
            self._queue.clear()
    
    def free(self):
        return self.maxsize - len(self._queue)

queues = {0: CommandQueue(BUFFER_SIZE), 1: CommandQueue(BUFFER_SIZE)}

# Motion Executor (Unified)
async def motion_executor(axis, motor, queue):
    """
    Execute commands sequentially.
    Supported command types:
    - 'target': (target_angle, speed, rel_time_ms)
    - 'velocity': speed
    - 'stop': stop_type
    """
    next_time = time.time_ms() # base time for scheduling
    while True:
        cmd = await queue.get()
        if cmd is None:
            await asyncio.sleep_ms(1)
            continue
        
        typ = cmd[0]
        if typ == 'target':
            target, speed, delay_ms = cmd[1], cmd[2], cmd[3]
            # Wait until the scheduled moment
            now = time.ticks_ms()
            target_time = next_time + delay_ms
            if time.ticks_diff(target_time, now) > 0:
                await asyncio.sleep_ms(time.ticks_diff(target_time, now))
            motor.track_target(target, speed)
            next_time = time.ticks_ms() # reset base for next segment
        elif typ == 'velocity':
            speed = cmd[1]
            motor.run(speed)
        elif typ == 'stop':
            stop_type = cmd[1]
            if stop_type == STOP_HOLD:
                motor.hold()
            else:
                motor.stop()
            next_time = time.ticks_ms()
        await asyncio.sleep(0)

# UART Receiver with Frame Parsing
async def read_exact(n):
    data = b''
    while len(data) < n:
        chunk = stdin.buffer.read(n - len(data))
        if chunk:
            data += chunk
        await asyncio.sleep(0)
    return data

last_activity_ticks = time.ticks_ms()
watchdog_timeout = 0
watchdog_enabled = False

async def uart_reciver():
    global last_activity_ticks, watchdog_enabled, watchdog_timeout
    while True:
        # Wait for sync byte
        b = await read_exact(1)
        if b[0] != SYNC_BYTE:
            continue
        last_activity_ticks = time.ticks_ms() # activity detected

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
            # Send error but keep connection
            stdout.buffer.write(bytes([REPLY_ERROR, axis, cmd, 0x01]))
            stdout.flush()
            continue
        
        # Global commands
        if cmd == CMD_EMERGENCY_STOP:
            for m in motors.values():
                m.stop()
            for q in queues.values():
                await q.clear()
            continue
        
        if cmd == CMD_PING:
            stdout.buffer.write(bytes([REPLY_PONG]))
            stdout.flush()
            continue
        
        if cmd == CMD_ENABLE_WATCHBOG:
            if len(payload) >= 2:
                watchdog_timeout = struct.unpack('<H', payload[:2])[0]
                watchdog_enabled = (watchdog_timeout > 0)
            continue
        
        if axis not in (0, 1):
            continue
        
        q = queues[axis]
        motor = motors[axis]

        if cmd == CMD_CLEAR_BUFFER:
            await q.clear()
            continue
        
        if cmd == CMD_SET_LIMITS:
            if len(payload) >= 8:
                max_speed, max_accel = struct.unpack('<II', payload[:8])
                motor.control.limits(speed=max_speed, acceleration=max_accel)
            continue
        
        if cmd == CMD_RESET_POS:
            pos = struct.unpack('<i', payload[:4][0] if len(payload) >= 4 else 0)
            motor.reset_angle(pos)
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
            reply = struct.pack('<BBiiBBB', REPLY_STATUS, axis, pos, spd, flags, q.free(), BUFFER_SIZE)
            stdout.buffer.write(reply)
            stdout.flush()
            continue
        
        # Motion commands (placed in queue)
        if cmd == CMD_UPDATE_TARGET:
            if len(payload) >= 8:
                target, speed, time_ms = struct.unpack('<iHH', payload[:8])
                if speed == 0:
                    speed = DEFAULT_SPEED
                await q.put(('target', target, speed, time_ms))
            elif cmd == CMD_SET_VELOCITY:
                if len(payload) >= 4:
                    speed = struct.unpack('<i', payload[:4])[0]
                    await q.put(('velocity', speed))
            elif cmd == CMD_STOP:
                stop_type = payload[0] if len(payload) >= 1 else STOP_COAST
                await q.put(('stop', stop_type))

            await asyncio.sleep(0)

# Watchdog Task
async def watchdog_task():
    global watchdog_enabled, watchdog_timeout, last_activity_ticks
    while True:
        if watchdog_enabled and watchdog_timeout > 0:
            now = time.ticks_ms()
            if time.ticks_diff(now, last_activity_ticks) > watchdog_timeout:
                # Timeout - emergency stop
                for m in motors.values():
                    m.stop()
                for q in queues.values():
                    await q.clear()
                hub.light.on(Color.RED)
                watchdog_enabled = False
        await asyncio.sleep_ms(100)

# Status Reporter
async def status_reporter(interval_ms):
    while True:
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
            reply = struct.pack('<BBiiBBB', REPLY_STATUS, axis, pos, spd, flags, q.free(), BUFFER_SIZE)
            stdout.buffer.write(reply)
        stdout.flush()
        await asyncio.sleep_ms(interval_ms)

# Main function
async def main():
    hub.light.on(Color.GREEN)
    await asyncio.sleep_ms(500)
    hub.light.off()

    task_x = motion_executor(0, motors[0], queues[0])
    task_y = motion_executor(1, motors[1], queues[1])
    receiver = uart_reciver()
    reporter = status_reporter(STATUS_INTERVAL_MS)
    wd = watchdog_task()

    await multitask(task_x, task_y, receiver, reporter, wd)

# Entry point
try:
    run_task(main())
except Exception as e:
    while True:
        hub.light.on(Color.RED)
        wait(100)
        hub.light.off()
        wait(100)