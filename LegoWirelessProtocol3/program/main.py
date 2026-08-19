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

BUFFER_SIZE = 4096
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
motor_x.control.limits(speed=1500, acceleration=3000)
motor_y.control.limits(speed=1500, acceleration=3000)

motor_x.reset_angle(0)
motor_y.reset_angle(0)

# Команды протокола
CMD_LINE = 0x01
CMD_MOTION_BLOCK = 0x02

WAIT_SYNC = 0
IN_PACKET = 1
state = WAIT_SYNC

packet_buf = bytearray(256)
packet_idx = 0
expected_len = 0

last_x = 0
last_y = 0

async def receiver():
    global state, packet_idx, expected_len, last_x, last_y

    usys.stdout.buffer.write(b"receiver ready\n")
    usys.stdout.flush()

    while True:
        events = poll.poll(0)
        if not events:
            await wait(5)
            continue

        data = usys.stdin.buffer.read(1)
        if not data:
            await wait(5)
            continue

        b = data[0]

        if state == WAIT_SYNC:
            # Ждём начало пакета: 0x01 (CMD_LINE) или 0x02 (CMD_MOTION_BLOCK)
            if b == CMD_LINE or b == CMD_MOTION_BLOCK:
                packet_buf[0] = b
                packet_idx = 1
                expected_len = 0
                state = IN_PACKET
            continue

        # IN_PACKET
        if packet_idx >= len(packet_buf):
            state = WAIT_SYNC
            packet_idx = 0
            expected_len = 0
            continue

        packet_buf[packet_idx] = b
        packet_idx += 1

        # После второго байта определяем длину пакета
        if packet_idx == 2:
            if packet_buf[0] == CMD_LINE:
                # Формат: [0x01][axis][tx(4)][ty(4)][dur(2)] = 12 байт
                expected_len = 12
            elif packet_buf[0] == CMD_MOTION_BLOCK:
                # Формат: [0x02][count][count*(dx,dy,dur)] = 2 + count*6 байт
                count = packet_buf[1]
                if count == 0:
                    state = WAIT_SYNC
                    packet_idx = 0
                    expected_len = 0
                    continue
                expected_len = 2 + count * 6
            else:
                state = WAIT_SYNC
                packet_idx = 0
                expected_len = 0
                continue

        # Полный пакет?
        if expected_len > 0 and packet_idx == expected_len:
            if packet_buf[0] == CMD_LINE:
                # Читаем без создания срезов
                tx = struct.unpack_from('<i', packet_buf, 2)[0]
                ty = struct.unpack_from('<i', packet_buf, 6)[0]
                dur = struct.unpack_from('<H', packet_buf, 10)[0]
                buffer_put(tx, ty, dur)
                last_x = tx
                last_y = ty
                usys.stdout.buffer.write(b"LINE %d %d %d\n" % (tx, ty, dur))
                usys.stdout.flush()

            elif packet_buf[0] == CMD_MOTION_BLOCK:
                count = packet_buf[1]
                offset = 2
                for _ in range(count):
                    # Читаем сразу три поля одной операцией, без срезов
                    dx, dy, dur = struct.unpack_from('<hhH', packet_buf, offset)
                    offset += 6
                    last_x += dx
                    last_y += dy
                    buffer_put(last_x, last_y, dur)

            state = WAIT_SYNC
            packet_idx = 0
            expected_len = 0
            gc.collect()          # принудительная сборка мусора
            await wait(5)         # даём поработать другим корутинам

async def smooth_executor():
    usys.stdout.buffer.write(b"executor ready\n")
    usys.stdout.flush()

    current_x = 0
    current_y = 0
    segment_count = 0
    UPDATE_MS = 5
    flow_stopped = False

    # Ждём первоначального наполнения буфера
    while buffer_count() < 300:
        await wait(10)

    usys.stdout.buffer.write(b"start printing\n")
    usys.stdout.flush()

    while True:
        # Flow control с гистерезисом
        count = buffer_count()
        if not flow_stopped and count > 3000:
            usys.stdout.buffer.write(b"FLOW_STOP\n")
            usys.stdout.flush()
            flow_stopped = True
        elif flow_stopped and count < 2000:
            usys.stdout.buffer.write(b"FLOW_RESUME\n")
            usys.stdout.flush()
            flow_stopped = False

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
        usys.stdout.buffer.write(b"CRITICAL_ERR: %s\n" % str(e).encode())
        usys.stdout.flush()
        hub.light.on(Color.RED)
        while True:
            await wait(100)

run_task(main())
