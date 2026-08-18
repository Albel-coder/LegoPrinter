from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import multitask, run_task, wait
import umath
import usys
import uselect
import ustruct as struct
import gc
import utime

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

last_x = 0
last_y = 0

START_MARKER = b'\x01'

WAIT_SYNC = 0
IN_PACKET = 1
state = WAIT_SYNC

packet_buf = bytearray(256)
packet_idx = 0
expected_len = 0

CMD_LINE = 0x01
CMD_MOTION_BLOCK = 0x02

async def receiver():
    global state, packet_idx, expected_len

    usys.stdout.buffer.write(b"receiver ready\n")
    usys.stdout.flush()

    while True:
        events = poll.poll(0)
        if not events:
            await wait(1)
            continue

        data = usys.stdin.buffer.read()
        if not data:
            await wait(1)
            continue

        for b in data:
            if state == WAIT_SYNC:
                if b == START_MARKER[0]:  # 0x01
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

            # Получили cmd
            if packet_idx == 2:
                cmd = packet_buf[1]
                if cmd == CMD_LINE:
                    expected_len = 12
                elif cmd == CMD_MOTION_BLOCK:
                    # ждём байт count
                    expected_len = 0
                else:
                    state = WAIT_SYNC
                    packet_idx = 0
                    expected_len = 0
                    continue

            # Получили count для блока
            elif packet_idx == 3 and packet_buf[1] == CMD_MOTION_BLOCK:
                count = packet_buf[2]
                if count == 0:
                    state = WAIT_SYNC
                    packet_idx = 0
                    expected_len = 0
                    continue
                expected_len = 3 + count * 6  # теперь delta: 2+2+2 = 6 байт
                if expected_len > len(packet_buf):
                    state = WAIT_SYNC
                    packet_idx = 0
                    expected_len = 0
                    continue

            # Полный пакет
            if expected_len > 0 and packet_idx == expected_len:
                cmd = packet_buf[1]

                if cmd == CMD_LINE:
                    tx = struct.unpack('<i', packet_buf[2:6])[0]
                    ty = struct.unpack('<i', packet_buf[6:10])[0]
                    dur = struct.unpack('<H', packet_buf[10:12])[0]
                    buffer_put(tx, ty, dur)
                    # обновляем базовую точку для последующих delta
                    last_x = tx
                    last_y = ty

                elif cmd == CMD_MOTION_BLOCK:
                    count = packet_buf[2]
                    offset = 3
                    for _ in range(count):
                        dx = struct.unpack('<h', packet_buf[offset:offset+2])[0]
                        dy = struct.unpack('<h', packet_buf[offset+2:offset+4])[0]
                        dur = struct.unpack('<H', packet_buf[offset+4:offset+6])[0]
                        offset += 6

                        # delta -> абсолютные координаты
                        last_x += dx
                        last_y += dy
                        if not buffer_put(last_x, last_y, dur):
                            break

                state = WAIT_SYNC
                packet_idx = 0
                expected_len = 0

async def smooth_executor():
    usys.stdout.buffer.write(b"executor ready\n")
    usys.stdout.flush()

    UPDATE_MS = 10
    START_BUFFER = 30

    # Плановая траектория (идеальные координаты)
    trajectory_x = 0
    trajectory_y = 0

    flow_stopped = False

    # Ждём начального наполнения буфера
    while buffer_count() < START_BUFFER:
        await wait(10)

    current = buffer_get()
    if current is None:
        return

    segment_start_x = trajectory_x
    segment_start_y = trajectory_y
    segment_target_x = current[0]
    segment_target_y = current[1]
    segment_duration = current[2]
    if segment_duration < UPDATE_MS:
        segment_duration = UPDATE_MS

    segment_start_time = utime.ticks_ms()

    while True:
        count = buffer_count()

        # Flow control с гистерезисом
        if not flow_stopped and count > 3000:
            usys.stdout.buffer.write(b"FLOW_STOP\n")
            usys.stdout.flush()
            flow_stopped = True
        elif flow_stopped and count < 2000:
            usys.stdout.buffer.write(b"FLOW_RESUME\n")
            usys.stdout.flush()
            flow_stopped = False

        now = utime.ticks_ms()
        elapsed = utime.ticks_diff(now, segment_start_time)

        # Переход к следующим сегментам, если текущий завершён
        while elapsed >= segment_duration:
            # Фиксируем конечную точку сегмента как новую стартовую
            trajectory_x = segment_target_x
            trajectory_y = segment_target_y

            elapsed -= segment_duration
            segment_start_time = now - elapsed

            next_seg = buffer_get()
            if next_seg is None:
                # Данных временно нет – удерживаем последнюю целевую точку
                motor_x.track_target(trajectory_x)
                motor_y.track_target(trajectory_y)
                await wait(UPDATE_MS)
                break

            segment_start_x = trajectory_x
            segment_start_y = trajectory_y
            segment_target_x = next_seg[0]
            segment_target_y = next_seg[1]
            segment_duration = next_seg[2]
            if segment_duration < UPDATE_MS:
                segment_duration = UPDATE_MS

        else:
            # Integer-интерполяция внутри сегмента
            dx = segment_target_x - segment_start_x
            dy = segment_target_y - segment_start_y

            target_x = segment_start_x + (dx * elapsed) // segment_duration
            target_y = segment_start_y + (dy * elapsed) // segment_duration

            motor_x.track_target(target_x)
            motor_y.track_target(target_y)

            await wait(UPDATE_MS)
            continue

        await wait(UPDATE_MS)

async def main():
    try:
        await multitask(receiver(), smooth_executor())
    except Exception as e:
        hub.light.on(Color.RED)
        while True:
            await wait(100)

run_task(main())
