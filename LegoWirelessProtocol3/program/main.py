from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import multitask, run_task, wait
import usys
import uselect
import ustruct as struct
import gc
from array import array  # Импортируем эффективные плоские массивы

from micropython import kbd_intr
kbd_intr(-1)

hub = TechnicHub()
hub.light.on(Color.GREEN)

# --- ОПТИМИЗАЦИЯ БУФЕРА (ZERO-ALLOCATION) ---
BUFFER_SIZE = 1024
# array('i') — массив 32-битных signed int, array('H') — 16-битных unsigned short
buf_x = array('i', [0] * BUFFER_SIZE)
buf_y = array('i', [0] * BUFFER_SIZE)
buf_dur = array('H', [0] * BUFFER_SIZE)

head = 0
tail = 0

def buffer_put(dx, dy, duration):
    global head
    next_head = (head + 1) % BUFFER_SIZE
    if next_head == tail:
        return False
    # Записываем атомарные значения напрямую в массивы без создания кортежей!
    buf_x[head] = dx
    buf_y[head] = dy
    buf_dur[head] = duration
    head = next_head
    return True

def buffer_get_count():
    # Возвращаем количество элементов, не создавая новые объекты
    global tail
    if tail == head:
        return -1 # Буфер пуст
    t = tail
    tail = (tail + 1) % BUFFER_SIZE
    return t # Возвращаем только индекс, откуда исполнителю читать данные

def buffer_count():
    if head >= tail:
        return head - tail
    else:
        return BUFFER_SIZE - (tail - head)

# --------------------------------------------

poll = uselect.poll()
poll.register(usys.stdin, uselect.POLLIN)

motor_x = Motor(Port.A)
motor_y = Motor(Port.B)
motor_x.control.limits(speed=1500, acceleration=3000)
motor_y.control.limits(speed=1500, acceleration=3000)

motor_x.reset_angle(0)
motor_y.reset_angle(0)

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
            if b == CMD_LINE or b == CMD_MOTION_BLOCK:
                packet_buf[0] = b
                packet_idx = 1
                expected_len = 0
                state = IN_PACKET
            continue

        if packet_idx >= len(packet_buf):
            state = WAIT_SYNC
            packet_idx = 0
            expected_len = 0
            continue

        packet_buf[packet_idx] = b
        packet_idx += 1

        if packet_idx == 2:
            if packet_buf[0] == CMD_LINE:
                expected_len = 12
            elif packet_buf[0] == CMD_MOTION_BLOCK:
                count = packet_buf[1]
                if count == 0 or (2 + count * 6) > len(packet_buf):
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

        if expected_len > 0 and packet_idx == expected_len:
            if packet_buf[0] == CMD_LINE:
                tx = struct.unpack_from('<i', packet_buf, 2)[0]
                ty = struct.unpack_from('<i', packet_buf, 6)[0]
                dur = struct.unpack_from('<H', packet_buf, 10)[0]
                buffer_put(tx, ty, dur)
                last_x = tx
                last_y = ty

            elif packet_buf[0] == CMD_MOTION_BLOCK:
                count = packet_buf[1]
                offset = 2
                for _ in range(count):
                    dx, dy, dur = struct.unpack_from('<hhH', packet_buf, offset)
                    offset += 6
                    last_x += dx
                    last_y += dy
                    buffer_put(last_x, last_y, dur)

            state = WAIT_SYNC
            packet_idx = 0
            expected_len = 0
            # Сборка мусора больше не обязательна каждую итерацию, так как память стабильна
            await wait(1) 

async def smooth_executor():
    usys.stdout.buffer.write(b"executor ready\n")
    usys.stdout.flush()

    current_x = 0
    current_y = 0
    segment_count = 0
    UPDATE_MS = 5
    flow_stopped = False

    # Ждём наполнения буфера (1000 элементов для буфера 1024 — это предел, лучше 800)
    while buffer_count() < 1000:
        await wait(10)

    usys.stdout.buffer.write(b"start printing\n")
    usys.stdout.flush()

    while True:
        # Flow control скорректирован под размер 1024
        count = buffer_count()
        if not flow_stopped and count > 900:
            usys.stdout.buffer.write(b"FLOW_STOP\n")
            usys.stdout.flush()
            flow_stopped = True
        elif flow_stopped and count < 400:
            usys.stdout.buffer.write(b"FLOW_RESUME\n")
            usys.stdout.flush()
            flow_stopped = False

        # Получаем индекс из буфера
        idx = buffer_get_count()
        if idx == -1:
            await wait(1)
            continue

        # Читаем значения напрямую из плоских массивов по индексу
        target_x = buf_x[idx]
        target_y = buf_y[idx]
        duration_ms = buf_dur[idx]

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

        # БЕЗОПАСНЫЙ ОТЛАДОЧНЫЙ ВЫВОД БЕЗ ФОРМАТИРОВАНИЯ СТРОК
        segment_count += 1
        if segment_count % 20 == 0:
            usys.stdout.write("SEG ")
            usys.stdout.write(str(segment_count))
            usys.stdout.write(": X=")
            usys.stdout.write(str(motor_x.angle()))
            usys.stdout.write(" Y=")
            usys.stdout.write(str(motor_y.angle()))
            usys.stdout.write("\n")
            usys.stdout.flush()

async def main():
    try:
        await multitask(receiver(), smooth_executor())
    except Exception as e:
        try:
            err_msg = "CRITICAL_ERR: " + str(e) + "\n"
            usys.stdout.write(err_msg)
            usys.stdout.flush()
        except:
            pass
        
        hub.light.on(Color.RED)
        while True:
            await wait(100)

run_task(main())
