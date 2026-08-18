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

START_MARKER = b'\x01'
CMD_LINE = 0x01
PACKET_SIZE = 12

WAIT_SYNC = 0
IN_PACKET = 1
state = WAIT_SYNC

packet_buf = bytearray(256)
packet_idx = 0

CMD_LINE = 0x01
CMD_MOTION_BLOCK = 0x02

async def receiver():
    global state, packet_idx

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
                if b == 0x01:  # START_MARKER
                    packet_buf[0] = b
                    packet_idx = 1
                    state = IN_PACKET
            else:  # IN_PACKET
                packet_buf[packet_idx] = b
                packet_idx += 1

                # Определяем ожидаемую длину пакета после получения cmd
                if packet_idx == 2:
                    cmd = packet_buf[1]
                    if cmd == CMD_LINE:
                        expected_len = 12  # 1 (marker) + 1 (cmd) + 10 (data)
                    elif cmd == CMD_MOTION_BLOCK:
                        # ждём ещё байт count
                        if packet_idx < 3:
                            continue
                        count = packet_buf[2]
                        expected_len = 3 + count * 10  # 1 marker + 1 cmd + 1 count + N*10
                    else:
                        state = WAIT_SYNC
                        continue

                if packet_idx >= expected_len:
                    cmd = packet_buf[1]
                    if cmd == CMD_LINE:
                        tx = struct.unpack('<i', packet_buf[2:6])[0]
                        ty = struct.unpack('<i', packet_buf[6:10])[0]
                        dur = struct.unpack('<H', packet_buf[10:12])[0]
                        buffer_put(tx, ty, dur)
                    elif cmd == CMD_MOTION_BLOCK:
                        count = packet_buf[2]
                        offset = 3
                        for _ in range(count):
                            tx = struct.unpack('<i', packet_buf[offset:offset+4])[0]
                            ty = struct.unpack('<i', packet_buf[offset+4:offset+8])[0]
                            dur = struct.unpack('<H', packet_buf[offset+8:offset+10])[0]
                            offset += 10
                            buffer_put(tx, ty, dur)
                    state = WAIT_SYNC

async def smooth_executor():
    usys.stdout.buffer.write(b"executor ready\n")
    usys.stdout.flush()

    current_x = 0
    current_y = 0
    segment_count = 0
    UPDATE_MS = 10  # можно оставить 5, если хватает ресурсов

    TARGET_LOOKAHEAD = 50  # сколько сегментов загружать за раз
    local_segments = []

    # Ждём первоначального наполнения буфера
    while buffer_count() < 30:
        await wait(10)

    while True:
        # --- Flow control ---
        if buffer_count() < 10:
            usys.stdout.buffer.write(b"COMMAND_PAUSE\n")
            usys.stdout.flush()
        elif buffer_count() > 100:
            usys.stdout.buffer.write(b"COMMAND_RESUME\n")
            usys.stdout.flush()

        # --- Загрузка новой партии, если текущая исчерпана ---
        if not local_segments:
            while len(local_segments) < TARGET_LOOKAHEAD:
                seg = buffer_get()
                if seg is None:
                    break
                local_segments.append(seg)

            if not local_segments:
                # Буфер пуст, ждём
                await wait(UPDATE_MS)
                continue

            # Начинаем новую партию
            segment_index = 0
            segment_elapsed = 0
            prev_target_x = motor_x.angle()
            prev_target_y = motor_y.angle()
            start_time = utime.ticks_ms()

        # --- Определяем текущий сегмент и целевую точку ---
        now = utime.ticks_ms() - start_time

        # Переходим к нужному сегменту
        while segment_index < len(local_segments):
            seg = local_segments[segment_index]
            seg_dur = seg[2]
            if segment_elapsed + seg_dur > now:
                break
            # сегмент завершён
            segment_elapsed += seg_dur
            prev_target_x = seg[0]
            prev_target_y = seg[1]
            segment_index += 1

        if segment_index >= len(local_segments):
            # Партия закончилась, в следующей итерации загрузим новую
            local_segments = []
            continue

        # Мы внутри сегмента segment_index
        seg = local_segments[segment_index]
        seg_dur = seg[2]
        progress = (now - segment_elapsed) / seg_dur
        if progress < 0:
            progress = 0
        elif progress > 1:
            progress = 1

        target_x = prev_target_x + (seg[0] - prev_target_x) * progress
        target_y = prev_target_y + (seg[1] - prev_target_y) * progress

        # Отправляем цель моторам
        motor_x.track_target(int(round(target_x)))
        motor_y.track_target(int(round(target_y)))

        segment_count += 1
        if segment_count % 100 == 0:  # реже логируем
            usys.stdout.buffer.write(
                "SEG %d: X = %d Y = %d\n" %
                (segment_count, motor_x.angle(), motor_y.angle())
            )
            usys.stdout.flush()

        await wait(UPDATE_MS)

async def main():
    try:
        await multitask(receiver(), smooth_executor())
    except Exception as e:
        hub.light.on(Color.RED)
        while True:
            await wait(100)

run_task(main())
