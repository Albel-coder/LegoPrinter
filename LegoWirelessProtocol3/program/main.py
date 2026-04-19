"""
Pybricks Klipper-Level v11 - Advanced Time-Based Motion
- True Klipper architecture: time is the master, all axes synchronized.
- Receives Move objects with trapezoidal profiles.
- High-frequency (1ms loop) computes instantaneous speed.
- Global start time synchronized.
- Buffer overflow feedback.
- Graceful handling of empty queue (holds last speed)
"""

from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import multitask, run_task, wait, StopWatch
from usys import stdin, stdout
import ustruct as struct

# Configuration
SYNC_BYTE = 0xAA
BUFFER_SIZE = 32
PLANNER_LOOP_MS = 1 # 1 kHz update rate for smooth motion
STATUS_INTERVAL_MS = 20
WATCHDOG_TIMEOUT_MS = 2000
UART_READ_TIMEOUT_MS = 500

# Hardware
hub = TechnicHub()
motors = {}
try:
    motors[0] = Motor(Port.A) # X
    motors[1] = Motor(Port.B) # Y
    # Set conservative limits (host will override via CMD_SET_LIMITS if needed)
    for m in motors.values():
        m.control.limits(2000, 2000)
        m.reset_angle(0)
    hub.light.on(Color.GREEN)
except Exception as e:
    err_msg = "FATAL: Motor init failed: " + str(e) + "\n"
    stdout.buffer.write(bytes(err_msg, 'utf-8'))
    stdout.flush()
    while True:
        hub.light.on(Color.RED)
        wait(200)
        hub.light.off()
        wait(200)

# Protocol Commands
CMD_ADD_MOVE = 0x10 #payload: duration_ms(uint16), start_speed(int16), cruise_speed(int16), end_speed(int16), accel(uint16)
CMD_SYNC_START = 0x11 # payload: start_time_ms(uint32) - global start time for all axes
CMD_STOP = 0x12 # stop_type(uint8)
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

# Status flags
FLAG_MOVING = 0x01
FLAG_BUFFER_LOW = 0x02
FLAG_BUFFER_FULL = 0x04
FLAG_WATCHDOG = 0x08

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

# Packing Helpers
def pack_i32(val):
    return struct.pack('<i', val)

def pack_u32(val):
    return struct.pack('<I', val)

def pack_u16(val):
    return struct.pack('<H', val)

def pack_i16(val):
    return struct.pack('<h', val)

def unpack_i32(data, off=0):
    return struct.unpack('<i', data[off:off+4])[0]

def unpack_u32(data, off=0):
    return struct.unpack('<I', data[off:off+4])[0]

def unpack_u16(data, off=0):
    return struct.unpack('<H', data[off:off+2])[0]

def unpack_i16(data, off=0):
    return struct.unpack('<h', data[off:off+2])[0]

# Move definition (Klipper-Level)
class Move:
    def __init__(self, duration_ms, start_speed, cruise_speed, end_speed, accel):
        self.duration_ms = duration_ms
        self.start_speed = start_speed
        self.cruise_speed = cruise_speed
        self.end_speed = end_speed
        self.accel = accel

        # Pre-calculate phase timings
        if accel > 0:
            self.t_accel = abs(cruise_speed - start_speed) / accel * 1000.0
            self.t_decel = abs(end_speed - cruise_speed) / accel * 1000.0
        else:
            # Zero acceleration instant speed changes
            self.t_accel = 0.0
            self.t_decel = 0.0
        
        total_ramp = self.t_accel + self.t_decel
        if total_ramp < duration_ms:
            self.t_cruise = duration_ms - total_ramp
        else:
            # Not enough time for full acceleration - triangular profiles
            ratio = duration_ms / total_ramp if total_ramp > 0 else 1.0
            self.t_accel *= ratio
            self.t_decel *= ratio
            self.t_cruise = 0.0
        
    def get_speed_at_time(self, elapsed_ms):
        """Calculate desired speed according to trapezoidal profile"""
        if self.t_accel and elapsed_ms <= self.t_accel:
            if self.t_accel > 0:
                progress = elapsed_ms / self.t_accel
            else:
                progress = 1.0
            return self.start_speed + (self.cruise_speed - self.start_speed) * progress
        elif self.t_cruise and elapsed_ms <= self.t_accel + self.t_cruise:
            return self.cruise_speed
        elif elapsed_ms <= self.duration_ms:
            decel_elapsed = elapsed_ms - (self.t_accel + self.t_cruise)
            if self.t_decel > 0:
                progress = decel_elapsed / self.t_decel
            else:
                progress = 1.0
            return self.cruise_speed + (self.end_speed - self.cruise_speed) * progress
        else:
            return self.end_speed

# Commands Queue
class MoveQueue:
    def __init__(self, maxSize):
        self.maxsize = maxSize
        self._queue = []

    def full(self):
        return len(self._queue) >= self.maxsize

    def empty(self):
        return len(self._queue) == 0
    
    def put(self, move):
        if len(self._queue) < self.maxsize:
            self._queue.append(move)
            return True
        return False
    
    def peek(self):
        return self._queue[0] if self._queue else None

    def pop(self):
        if self._queue:
            return self._queue.pop(0)
        return None
    
    def clear(self):
        self._queue.clear()

    def free(self):
        return self.maxsize - len(self._queue)

queues = {0: MoveQueue(BUFFER_SIZE), 1: MoveQueue(BUFFER_SIZE)}

# Global Time Synchronization
class GlobalTimer:
    """Provides a common time base for all axes"""
    def __init__(self):
        self.watch = StopWatch()
        self._offset = 0 # Allows to adjust time (unused currently)
    
    def time_ms(self):
        return self.watch.time()

    def reset(self):
        self.watch.reset()

global_timer = GlobalTimer()
scheduled_start_time = None # Absolute time (ms) when all axes should start

# Time-based Move Executor (per motor)
class MoveExecutor:
    def __init__(self, motor, queue):
        self.motor = motor
        self.queue = queue
        self.current_move = None
        self.move_start_time = 0
        self.active = False
        self.last_speed = 0

    def start_move(self, move, start_time):
        self.current_move = move
        self.move_start_time = start_time
        self.active = True
        self.last_speed = move.start_speed
        self.motor.run(int(self.last_speed))
    
    def update(self, now_ms):
        if not self.active:
            # Try to fetch next move
            self.current_move = self.queue.pop()
            if self.current_move:
                # Wait for global start signal
                if scheduled_start_time is not None and now_ms < scheduled_start_time:
                    return
                self.start_move(self.current_move, now_ms)
            else:
                # No move - keep last speed (graceful)
                if abs(self.last_speed) > 0:
                    self.motor.run(int(self.last_speed))
                return

        elapsed_ms = now_ms - self.move_start_time
        if elapsed_ms >= self.current_move.duration_ms:
            # Move finished
            self.last_speed = self.current_move.end_speed
            # Fetch next move immediately (may be None)
            self.current_move = self.queue.pop()
            if self.current_move:
                self.start_move(self.current_move, now_ms)
            else:
                self.active = False
                self.motor.run(int(self.last_speed)) # keep speed
            return
        
        desired_speed = self.current_move.get_speed_at_time(elapsed)
        self.last_speed = desired_speed
        self.motor.run(int(desired_speed))

    def reset(self):
        self.active = False
        self.current_move = None
        self.motor.stop()
        self.last_speed = 0

executors = {
    0: MoveExecutor(motors[0], queues[0]),
    1: MoveExecutor(motors[1], queues[1])
}

# High-Frequency Planner Task
async def planner_task():
    while True:
        try:
            now = global_timer.time_ms()
            # Check global start condition
            global scheduled_start_time
            if scheduled_start_time is not None and now >= scheduled_start_time:
                scheduled_start_time = None # Start window passed

            for executor in executors.values():
                executor.update(now)
        except Exception as e:
            err_msg = "PLANNER ERROR: %s\n" % str(e)
            stdout.buffer.write(bytes(err_msg, 'utf-8'))
            stdout.flush()
        await wait(PLANNER_LOOP_MS)

# UART Receiver
async def read_exact(n):
    data = b''
    start = StopWatch()
    while len(data) < n:
        try:
            chunk = stdin.buffer.read(n - len(data))
        except KeyboardInterrupt:
            data += b'\x03'
            start.reset()
            continue
        if chunk:
            data += chunk
            start.reset()
        else:
            if start.time() > UART_READ_TIMEOUT_MS:
                raise EOFError("UART read error")
        await wait(1)
    return data

watchdog_watch = StopWatch()
last_activity = watchdog_watch.time()
watchdog_enabled = True
watchdog_timeout = WATCHDOG_TIMEOUT_MS

async def uart_receiver():
    global last_activity, watchdog_enabled, watchdog_timeout, scheduled_start_time
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
                for e in executors.values(): e.reset()
                for q in queues.values(): q.clear()
                scheduled_start_time = None
                continue
            if cmd == CMD_PING:
                stdout.buffer.write(bytes([REPLY_PONG]));
                stdout.flush()
                continue
            if cmd == CMD_ENABLED_WATCHDOG:
                if len(payload) >= 2:
                    watchdog_timeout = unpack_u16(payload)
                    watchdog_enabled = (watchdog_timeout > 0)
                continue
            if cmd == CMD_SYNC_START:
                if len(payload) >= 4:
                    start_ms = unpack_u32(payload, 0)
                    scheduled_start_time = start_ms
                continue
            
            # Axis-specific commands
            if axis not in (0,1): continue

            q = queues[axis]
            executor = executors[axis]

            if cmd == CMD_CLEAR_BUFFER:
                q.clear()
                executor.reset()
            elif cmd == CMD_SET_LIMITS:
                if len(payload) >= 8:
                    max_spd = unpack_u32(payload, 0)
                    max_acc = unpack_u32(payload, 4)
                    motors[axis].control.limits(max_spd, max_acc)
            elif cmd == CMD_RESET_POS:
                pos = unpack_i32(payload) if len(payload)>=4 else 0
                motors[axis].reset_angle(pos)
                executor.reset()
            elif cmd == CMD_GET_STATUS:
                motor = motors[axis]
                pos = motor.angle()
                spd = motor.speed()
                flags = 0
                if executor.active or spd != 0:
                    flags |= FLAG_MOVING
                if q.free() < 2:
                    flags |= FLAG_BUFFER_LOW
                if q.full():
                    flags |= FLAG_BUFFER_FULL
                if watchdog_enabled:
                    flags |= FLAG_WATCHDOG
                reply = bytes([REPLY_STATUS, axis] + pack_i32(pos) + pack_i32(spd))
                stdout.buffer.write(reply)
                stdout.flush()
            elif cmd == CMD_ADD_MOVE:
                err_msg = "Call add move"
                stdout.buffer.write(bytes(err_msg, 'utf-8'))
                stdout.flush()
                if len(payload) >= 10:
                    duration = unpack_u16(payload, 0)
                    start_spd = unpack_i16(payload, 2)
                    cruise_spd = unpack_i16(payload, 4)
                    end_spd = unpack_i16(payload, 6)
                    accel = unpack_u16(payload, 8)
                    move = Move(duration, start_spd, cruise_spd, end_spd, accel)
                    if not q.put(move):
                        # Buffer full - host should slow down
                        pass
            elif cmd == CMD_STOP:
                stop_type = payload[0] if payload else STOP_COAST
                if stop_type == STOP_HOLD:
                    motors[axis].hold()
                else:
                    motors[axis].stop()
                executor.reset()
        except EOFError as e:
            err_msg = "UART read timeout, waiting for reconnect..."
            stdout.buffer.write(bytes(err_msg, 'utf-8'))
            stdout.flush()
            await wait(1000)
            try:
                stdin.read()
            except:
                pass
        except Exception as e:
            err_msg = "RECEIVER ERROR: %s" % str(e)
            stdout.buffer.write(bytes(err_msg, 'utf-8'))
            stdout.flush()
            await wait(100)
        await wait(1)

# Watchdog
async def watchdog_task():
    global watchdog_enabled, watchdog_timeout, last_activity
    while True:
        if watchdog_enabled and watchdog_timeout > 0:
            now = watchdog_watch.time()
            if (now - last_activity) > watchdog_timeout:
                for e in executors.values(): e.reset()
                for q in queues.values(): q.clear()
                hub.light.on(Color.ORANGE)
                watchdog_enabled = False
        await wait(100)

# Status Reporter
async def status_reporter(interval_ms):
    while True:
        try:
            for axis in (0,1):
                motor = motors[axis]
                q = queues[axis]
                executor = executors[axis]
                pos = motor.angle()
                spd = motor.speed()
                flags = 0
                if executor.active or spd != 0:
                    flags |= FLAG_MOVING
                if q.free() < 2:
                    flags |= FLAG_BUFFER_LOW
                if q.full():
                    flags |= FLAG_BUFFER_FULL
                if watchdog_enabled:
                    flags |= FLAG_WATCHDOG
                reply = bytes([REPLY_STATUS, axis] + pack_i32(pos) + pack_i32(spd) + bytes([flags, q.free(), BUFFER_SIZE]))
                stdout.buffer.write(reply)
            stdout.flush()
        except Exception:
            pass
        await wait(interval_ms)

# Main
async def main():
    while True:
        try:
            task_planner = planner_task()
            receiver = uart_receiver()
            reporter = status_reporter(STATUS_INTERVAL_MS)
            wd = watchdog_task()
            await multitask(task_planner, receiver, reporter, wd)
        except KeyboardInterrupt:
            err_msg = "Program interrupted by host, restarting tasks..."
            stdout.buffer.write(bytes(err_msg, 'utf-8'))
            stdout.flush()
            for executor in executors.values():
                executor.reset()
            
            hub.light.on(Color.GREEN)
            await wait(1000)
        except Exception as e:
            write_error("MAIN FATAL: " + str(e))
            break

    for executor in executors.values():
        executor.reset()
    hub.light.off()

try:
    run_task(main())
except Exception as e:
    err_msg = "MAIN FATAL: %s\n" % str(e)
    stdout.buffer.write(bytes(err_msg, 'utf-8'))
    stdout.flush()
    while True:
        hub.light.on(Color.RED)
        wait(200)
        hub.light.off()
        wait(200)