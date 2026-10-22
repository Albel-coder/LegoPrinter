from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import multitask, run_task, wait
import usys
import uselect
import ustruct as struct
import gc

from micropython import kbd_intr
kbd_intr(-1)

hub = TechnicHub()
hub.light.on(Color.GREEN)

# ============================================================
# VERSION
# ============================================================
# Меняется при каждой значимой правке протокола/логики.
# Полезно видеть в логе при запуске
FIRMWARE_VERSION = "fw-2026-10-15-zwatermark"

# ============================================================
# XY RING BUFFER
# ============================================================
BUFFER_SIZE = 1024
buf_x = [0] * BUFFER_SIZE
buf_y = [0] * BUFFER_SIZE
buf_dur = [0] * BUFFER_SIZE

head = 0
tail = 0

def buffer_put(dx, dy, duration):
    global head
    next_head = (head + 1) % BUFFER_SIZE
    if next_head == tail:
        return False
    buf_x[head] = dx
    buf_y[head] = dy
    buf_dur[head] = duration
    head = next_head
    return True

def buffer_get_count():
    # Возвращает индекс следующего элемента и сдвигает tail, или -1 если пусто.
    global tail
    if tail == head:
        return -1
    t = tail
    tail = (tail + 1) % BUFFER_SIZE
    return t

def buffer_count():
    # Текущее количество элементов в буфере.
    if head >= tail:
        return head - tail
    else:
        return BUFFER_SIZE - (tail - head)

# ============================================================
# Z-ОЧЕРЕДЬ С BACKPRESSURE
# ============================================================
# Z_QUEUE_SIZE: сколько Z-команд хаб может держать в памяти.
# Z_HIGH_WATERMARK: при достижении - шлет Z_STOP, host копит Z у себя
# Z_LOW_WATERMARK: при падении - шлет Z_RESUME, host снова шлет Z.
#
# Гистерезис (48 -> 16) не дает "дребезга" STOP/RESUME когда
# z_count колеблится около порога.

Z_QUEUE_SIZE = 64
Z_HIGH_WATERMARK = 48
Z_LOW_WATERMARK = 16

z_angles = [0] * Z_QUEUE_SIZE
z_after_xy = [0] * Z_QUEUE_SIZE
z_head = 0
z_tail = 0
z_count = 0

# Флаг состояния backpressure:
# False = RESUMED (host шлет Z свободно)
# True = STOPPED (host держит Z локально)
z_stop_sent = False

def z_put(angle, after_xy):
    # Кладет Z-команду в очередь. Возвращает False при переполнении
    global z_head, z_count
    if z_count >= Z_QUEUE_SIZE:
        return False
    z_angles[z_head] = angle
    z_after_xy[z_head] = after_xy
    z_head = (z_head + 1) % Z_QUEUE_SIZE
    z_count += 1
    return True

def z_get():
    # Забирает Z-команду из очереди. Возвращает (angle, after_xy) или (None, None).
    global z_tail, z_count
    if z_count == 0:
        return None, None
    angle = z_angles[z_tail]
    after_xy = z_after_xy[z_tail]
    z_tail = (z_tail + 1) % Z_QUEUE_SIZE
    z_count -= 1
    return angle, after_xy

def z_check_high():
    # Проверка на переход RESUMED -> STOPPED. Вызывать после успешного z_put.
    global z_stop_sent
    if (not z_stop_sent) and z_count >= Z_HIGH_WATERMARK:
        usys.stdout.buffer.write(b"Z_STOP\n")
        usys.stdout.flush()
        z_stop_sent = True

def z_check_low():
    # Проверка на переход STOPPED -> RESUMED. Вызывать после z_get.
    global z_stop_sent
    if z_stop_sent and z_count <= Z_LOW_WATERMARK:
        usys.stdout.buffer.write(b"Z_RESUME\n")
        usys.stdout.flush()
        z_stop_sent = False

# -----------------------------------------------------------
# Z-ось и её очередь
# -----------------------------------------------------------
motor_z = Motor(Port.C)
motor_z.control.limits(speed=600, acceleration=2000)
motor_z.reset_angle(0)

Z_UP_ANGLE = 10
Z_DOWN_ANGLE = 0
Z_SPEED = 400

# -----------------------------------------------------------
# Глобальное состояние задания
# -----------------------------------------------------------
total_received_xy = 0
executed_xy = 0
start_threshold = 35
start_gate_open = False

last_x = 0
last_y = 0

def reset_job_state():
    # Полный сброс перед новым заданием (по CMD_SET_START_THRESHOLD).
    global total_received_xy, executed_xy, start_gate_open
    global z_head, z_tail, z_count, z_stop_sent
    global last_x, last_y
    global head, tail

    total_received_xy = 0
    executed_xy = 0
    start_gate_open = False

    z_head = 0
    z_tail = 0
    z_count = 0
    z_stop_sent = False

    last_x = 0
    last_y = 0
    head = 0
    tail = 0

# -----------------------------------------------------------
# Протокол (stdin, после удаления Pybricks 0x06)
# -----------------------------------------------------------
CMD_LINE = 0x01 # Одиночный XY
CMD_MOTION_BLOCK = 0x02 # Пакет XY
CMD_Z = 0x05 # Z-команда
CMD_SET_START_THRESHOLD = 0x06 # старт нового job

WAIT_SYNC = 0
IN_PACKET = 1
state = WAIT_SYNC

packet_buf = bytearray(256)
packet_idx = 0
expected_len = 0

poll = uselect.poll()
poll.register(usys.stdin, uselect.POLLIN)

motor_x = Motor(Port.A)
motor_y = Motor(Port.B)
motor_x.control.limits(speed=2000, acceleration=6000)
motor_y.control.limits(speed=2000, acceleration=6000)
motor_x.reset_angle(0)
motor_y.reset_angle(0)

# ============================================================
# RECEIVER
# ============================================================
async def receiver():
    global state, packet_idx, expected_len, packet_buf
    global last_x, last_y, total_received_xy, start_threshold

    usys.stdout.buffer.write(b"receiver ready, version=%s\n" % FIRMWARE_VERSION)
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
            # Принимаем только известные команды (0x06 уже удалён Pybricks)
            if b == CMD_LINE or b == CMD_MOTION_BLOCK or b == CMD_Z or b == CMD_SET_START_THRESHOLD:
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

        # Определение длины пакета после получения второго байта
        if packet_idx == 2:
            cmd = packet_buf[0]
            if cmd == CMD_LINE:
                expected_len = 12
            elif cmd == CMD_MOTION_BLOCK:
                count = packet_buf[1]
                if count == 0:
                    state = WAIT_SYNC
                    packet_idx = 0
                    expected_len = 0
                    continue
                expected_len = 2 + count * 6
                if expected_len > len(packet_buf):
                    state = WAIT_SYNC
                    packet_idx = 0
                    expected_len = 0
                    continue
            elif cmd == CMD_Z:
                expected_len = 7
            elif cmd == CMD_SET_START_THRESHOLD:
                expected_len = 3
            else:
                state = WAIT_SYNC
                packet_idx = 0
                expected_len = 0
                continue

        # Проверка полного пакета
        if expected_len > 0 and packet_idx == expected_len:
            cmd = packet_buf[0]

            if cmd == CMD_LINE:
                tx = struct.unpack_from('<i', packet_buf, 1)[0]
                ty = struct.unpack_from('<i', packet_buf, 5)[0]
                dur = struct.unpack_from('<H', packet_buf, 9)[0]
                if not buffer_put(tx, ty, dur):
                    usys.stdout.buffer.write(b"BUFFER_FULL\n")
                    usys.stdout.flush()
                else:
                    last_x = tx
                    last_y = ty
                    total_received_xy += 1

            elif cmd == CMD_MOTION_BLOCK:
                count = packet_buf[1]
                offset = 2
                for _ in range(count):
                    dx, dy, dur = struct.unpack_from('<hhH', packet_buf, offset)
                    offset += 6
                    last_x += dx
                    last_y += dy
                    if not buffer_put(last_x, last_y, dur):
                        usys.stdout.buffer.write(b"BUFFER_FULL\n")
                        usys.stdout.flush()
                        break
                    total_received_xy += 1

            elif cmd == CMD_Z:
                angle = struct.unpack_from('<h', packet_buf, 1)[0]
                after_xy = struct.unpack_from('<I', packet_buf, 3)[0]
                if not z_put(angle, after_xy):
                    usys.stdout.buffer.write(b"Z_QUEUE_FULL\n")
                    usys.stdout.flush()

            elif cmd == CMD_SET_START_THRESHOLD:
                new_threshold = struct.unpack_from('<H', packet_buf, 1)[0]
                reset_job_state()
                start_threshold = new_threshold
                usys.stdout.write("START_THRESHOLD_SET %d\n" % start_threshold)
                usys.stdout.flush()

            state = WAIT_SYNC
            packet_idx = 0
            expected_len = 0

        await wait(1)

# ============================================================
# EXECUTOR
# ============================================================
async def smooth_executor():
    usys.stdout.buffer.write(b"executor ready\n")
    usys.stdout.flush()

    current_x = 0
    current_y = 0
    segment_count = 0
    UPDATE_MS = 4
    flow_stopped = False
    global executed_xy, start_gate_open

    while True:
        # --- 1. Z-команды (с проверкой после каждого z_get) ---
        if z_count > 0:
            angle = z_angles[z_tail]
            after_xy = z_after_xy[z_tail]
            if executed_xy >= after_xy:
                z_get()
                z_check_low() # можем отправлять Z_RESUME, если ниже watermark
                motor_z.run_target(Z_SPEED, angle, then=Stop.HOLD, wait=False)
                while not motor_z.done():
                    await wait(2)
                usys.stdout.write("Z_EXEC angle=%d after=%d executed=%d\n" % (angle, after_xy, executed_xy))
                usys.stdout.flush()
                continue

        # --- 2. Стартовый порог ---
        if not start_gate_open:
            if buffer_count() >= start_threshold:
                start_gate_open = True
                usys.stdout.buffer.write(b"start printing\n")
                usys.stdout.flush()
            else:
                await wait(10)
                continue

        # --- 3. Flow control ---
        count = buffer_count()
        if not flow_stopped and count > 900:
            usys.stdout.buffer.write(b"FLOW_STOP\n")
            usys.stdout.flush()
            flow_stopped = True
        elif flow_stopped and count < 400:
            usys.stdout.buffer.write(b"FLOW_RESUME\n")
            usys.stdout.flush()
            flow_stopped = False

        # --- 4. Получаем XY ---
        idx = buffer_get_count()
        if idx == -1:
            await wait(1)
            continue

        target_x = buf_x[idx]
        target_y = buf_y[idx]
        duration_ms = buf_dur[idx]

        if duration_ms <= UPDATE_MS:
            motor_x.track_target(target_x)
            motor_y.track_target(target_y)
            current_x = target_x
            current_y = target_y
        else:
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

        executed_xy += 1
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

# ============================================================
# MAIN
# ============================================================
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
