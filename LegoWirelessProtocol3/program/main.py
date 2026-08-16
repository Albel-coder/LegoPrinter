from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import multitask, run_task, wait
import umath
import usys
import uselect
import ustruct as struct
import gc

from micropython import kbd_intr
kbd_intr(-1)

hub = TechnicHub()
hub.light.on(Color.GREEN)

BUFFER_SIZE = 1024
segments = [(0, 0, 0)] * BUFFER_SIZE
head = 0
tail = 0

def buffer_put(dx, dy, duration):
    global head
    next_head = (head + 1) % BUFFER_SIZE
    if next_head == tail:
        return False
    segments[head] = (dx, dy, duration)
    head = next_head
    return True

def buffer_get():
    global tail
    if tail == head:
        return None
    seg = segments[tail]
    tail = (tail + 1) % BUFFER_SIZE
    return seg

def buffer_count():
    if head >= tail:
        return head - tail
    else:
        return BUFFER_SIZE - (tail - head)

def buffer_space():
    return BUFFER_SIZE - buffer_count() - 1

poll = uselect.poll()
poll.register(usys.stdin, uselect.POLLIN)

motor_x = Motor(Port.A)
motor_y = Motor(Port.B)
motor_x.control.limits(speed=1000, acceleration=2000)
motor_y.control.limits(speed=1000, acceleration=2000)

motor_x.reset_angle(0)
motor_y.reset_angle(0)

START_MARKER = b'\x01'
CMD_LINE = 0x01
PACKET_SIZE = 12

WAIT_SYNC = 0
IN_PACKET = 1
state = WAIT_SYNC

packet_buf = bytearray(PACKET_SIZE)
packet_idx = 0

async def receiver():
    global state, packet_idx

    usys.stdout.buffer.write(b"receiver ready\n")
    usys.stdout.flush()

    while True:
        events = poll.poll(0)
        if not events:
            await wait(1)
            continue

        chunk = usys.stdin.buffer.read(1)
        if not chunk:
            await wait(1)
            continue

        if state == WAIT_SYNC:
            if chunk == START_MARKER:
                packet_buf[0] = chunk[0]
                packet_idx = 1
                state = IN_PACKET
        else:
            packet_buf[packet_idx] = chunk[0]
            packet_idx += 1
            if packet_idx == PACKET_SIZE:
                cmd = packet_buf[1]
                if cmd == CMD_LINE:
                    tx = struct.unpack('<i', packet_buf[2:6])[0]
                    ty = struct.unpack('<i', packet_buf[6:10])[0]
                    dur = struct.unpack('<H', packet_buf[10:12])[0]
                    buffer_put(tx, ty, dur)
                state = WAIT_SYNC

        if head % 200 == 0:
            gc.collect()

async def smooth_executor():
    usys.stdout.buffer.write(b"executor ready\n")
    usys.stdout.flush()

    current_x = 0
    current_y = 0
    segment_count = 0
    UPDATE_MS = 5

    while True:
        seg = buffer_get()
        if seg is None:
            await wait(1)
            continue

        target_x, target_y, duration_ms = seg

        if duration_ms <= UPDATE_MS:
            motor_x.track_target(target_x)
            motor_y.track_target(target_y)
            current_x = target_x
            current_y = target_y
            continue

        steps = max(1, duration_ms // UPDATE_MS)
        dx = target_x - current_x
        dy = target_y - current_y

        for i in range(1, steps + 1):
            x = current_x + (dx * i) // steps
            y = current_y + (dy * i) // steps
            motor_x.track_target(x)
            motor_y.track_target(y)
            await wait(UPDATE_MS)

        motor_x.track_target(target_x)
        motor_y.track_target(target_y)
        current_x = target_x
        current_y = target_y

        segment_count += 1
        if segment_count % 20 == 0:
            usys.stdout.buffer.write(
                "SEG %d: X = %d Y = %d\n" %
                (segment_count, motor_x.angle(), motor_y.angle())
            )
            usys.stdout.flush()

async def main():
    try:
        await multitask(receiver(), smooth_executor())
    except Exception as e:
        hub.light.on(Color.RED)
        while True:
            await wait(100)

run_task(main())
