from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import multitask, run_task, wait, StopWatch
import usys
import uselect
import ustruct as struct
import gc

from micropython import kbd_intr
kbd_intr(-1)

hub = TechnicHub()
hub.light.on(Color.GREEN)

clock = StopWatch()

# ============================================================
# VERSION
# ============================================================
FIRMWARE_VERSION = "fw-2026-10-15-nonblocking"

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
    global tail
    if tail == head:
        return -1
    t = tail
    tail = (tail + 1) % BUFFER_SIZE
    return t

def buffer_count():
    if head >= tail:
        return head - tail
    else:
        return BUFFER_SIZE - (tail - head)

# ============================================================
# Z-ОЧЕРЕДЬ С BACKPRESSURE
# ============================================================
Z_QUEUE_SIZE     = 128
Z_HIGH_WATERMARK = 32
Z_LOW_WATERMARK  = 8

z_angles   = [0] * Z_QUEUE_SIZE
z_after_xy = [0] * Z_QUEUE_SIZE
z_head = 0
z_tail = 0
z_count = 0

z_stop_sent = False
z_stop_pending = False
z_resume_pending = False

# ============================================================
# START THRESHOLD CLAMP
# ============================================================
START_THRESHOLD_MAX_SAFE = 200

# ============================================================
# FLOW CONTROL
# ============================================================
FLOW_STOP_AT        = 900
FLOW_RESUME_AT      = 400
FLOW_REPORT_MIN_MS  = 100

_buffer_full_state     = False
_last_buffer_report_ms = 0


def report_buffer_state():
    """Внутренняя: шлёт FLOW_STOP / FLOW_RESUME по фронту.
    Rate-limit 100 мс. Не вызывать напрямую — только через process_flow_state.
    """
    global _buffer_full_state, _last_buffer_report_ms
    now = clock.time()
    if now - _last_buffer_report_ms < FLOW_REPORT_MIN_MS:
        return
    count = buffer_count()
    if not _buffer_full_state and count > FLOW_STOP_AT:
        _buffer_full_state = True
        _last_buffer_report_ms = now
        usys.stdout.buffer.write(b"FLOW_STOP\n")
        usys.stdout.flush()
    elif _buffer_full_state and count < FLOW_RESUME_AT:
        _buffer_full_state = False
        _last_buffer_report_ms = now
        usys.stdout.buffer.write(b"FLOW_RESUME\n")
        usys.stdout.flush()


def process_flow_state():
    """ЕДИНАЯ точка flow control.
    Вызывается:
      - из receiver после каждого пакета (ловит рост буфера как можно раньше)
      - из executor в начале каждой итерации
      - из inner-loop интерполяции XY на каждом шаге (закрывает дыру длинных сегментов)
    """
    global z_stop_sent, z_stop_pending, z_resume_pending

    # --- Z backpressure ---
    if z_stop_pending:
        usys.stdout.buffer.write(b"Z_STOP\n")
        usys.stdout.flush()
        z_stop_sent = True
        z_stop_pending = False
    if z_resume_pending:
        usys.stdout.buffer.write(b"Z_RESUME\n")
        usys.stdout.flush()
        z_stop_sent = False
        z_resume_pending = False

    # --- XY flow ---
    report_buffer_state()


def z_put(angle, after_xy):
    global z_head, z_count, z_stop_pending
    if z_count >= Z_QUEUE_SIZE:
        return False
    z_angles[z_head] = angle
    z_after_xy[z_head] = after_xy
    z_head = (z_head + 1) % Z_QUEUE_SIZE
    z_count += 1
    if (not z_stop_sent) and z_count >= Z_HIGH_WATERMARK:
        z_stop_pending = True
    return True


def z_get():
    global z_tail, z_count, z_resume_pending
    if z_count == 0:
        return None, None
    angle = z_angles[z_tail]
    after_xy = z_after_xy[z_tail]
    z_tail = (z_tail + 1) % Z_QUEUE_SIZE
    z_count -= 1
    if z_stop_sent and z_count <= Z_LOW_WATERMARK:
        z_resume_pending = True
    return angle, after_xy

# -----------------------------------------------------------
# Z-ось
# -----------------------------------------------------------
motor_z = Motor(Port.C)
motor_z.control.limits(speed=600, acceleration=2000)
motor_z.reset_angle(0)

Z_UP_ANGLE   = 10
Z_DOWN_ANGLE = 0
Z_SPEED      = 400

# -----------------------------------------------------------
# XY-оси
# -----------------------------------------------------------
motor_x = Motor(Port.A)
motor_y = Motor(Port.B)
motor_x.control.limits(speed=2000, acceleration=6000)
motor_y.control.limits(speed=2000, acceleration=6000)
motor_x.reset_angle(0)
motor_y.reset_angle(0)

# -----------------------------------------------------------
# Глобальное состояние задания
# -----------------------------------------------------------
total_received_xy = 0
executed_xy = 0
start_threshold = 35
start_gate_open = False

last_x = 0
last_y = 0

_z_move_active       = False
_z_move_started_ms   = 0
_z_move_target_angle = 0
_z_move_after_xy     = 0


def reset_job_state():
    global total_received_xy, executed_xy, start_gate_open
    global z_head, z_tail, z_count, z_stop_sent
    global z_stop_pending, z_resume_pending
    global last_x, last_y
    global head, tail
    global _buffer_full_state, _last_buffer_report_ms
    global _z_move_active, _z_move_started_ms
    global _z_move_target_angle, _z_move_after_xy

    total_received_xy = 0
    executed_xy = 0
    start_gate_open = False

    z_head = 0
    z_tail = 0
    z_count = 0
    z_stop_sent = False
    z_stop_pending = False
    z_resume_pending = False

    last_x = 0
    last_y = 0
    head = 0
    tail = 0

    _buffer_full_state = False
    _last_buffer_report_ms = 0

    _z_move_active = False
    _z_move_started_ms = 0
    _z_move_target_angle = 0
    _z_move_after_xy = 0

# -----------------------------------------------------------
# Протокол
# -----------------------------------------------------------
CMD_LINE                 = 0x01
CMD_MOTION_BLOCK         = 0x02
CMD_Z                    = 0x05
CMD_SET_START_THRESHOLD  = 0x06

WAIT_SYNC = 0
IN_PACKET = 1
state = WAIT_SYNC

packet_buf = bytearray(256)
packet_idx = 0
expected_len = 0

poll = uselect.poll()
poll.register(usys.stdin, uselect.POLLIN)

# ============================================================
# TELEMETRY
# ============================================================
def _fmt_pid(motor, tag):
    try:
        vals = motor.control.pid()
        names = ("kp", "ki", "kd", "idead", "irange", "imax")
        parts = []
        for name, v in zip(names, vals):
            try:
                parts.append("%s=%.5f" % (name, float(v)))
            except Exception:
                parts.append("%s=%s" % (name, v))
        if not parts:
            parts = ["<empty>"]
        return "LOG_PID %s %s" % (tag, " ".join(parts))
    except Exception as e:
        return "LOG_PID %s error=%s" % (tag, e)


def _fmt_limits(motor, tag):
    try:
        vals = motor.control.limits()
        names = ("speed", "accel", "torque")
        parts = ["%s=%s" % (n, v) for n, v in zip(names, vals)]
        return "LOG_LIMITS %s %s" % (tag, " ".join(parts))
    except Exception as e:
        return "LOG_LIMITS %s error=%s" % (tag, e)


def _fmt_tolerances(motor, tag):
    for name in ("target_tolerances", "target_tolerance"):
        try:
            vals = getattr(motor.control, name)()
            names = ("speed", "position")
            parts = ["%s=%s" % (n, v) for n, v in zip(names, vals)]
            return "LOG_TOL %s %s" % (tag, " ".join(parts))
        except Exception:
            continue
    return "LOG_TOL %s error=unavailable" % tag


def log_motor_config(reason="init"):
    lines = [
        "LOG_CONFIG_BEGIN reason=%s" % reason,
        _fmt_pid(motor_x, "X"),
        _fmt_pid(motor_y, "Y"),
        _fmt_pid(motor_z, "Z"),
        _fmt_limits(motor_x, "X"),
        _fmt_limits(motor_y, "Y"),
        _fmt_limits(motor_z, "Z"),
        _fmt_tolerances(motor_x, "X"),
        _fmt_tolerances(motor_y, "Y"),
        _fmt_tolerances(motor_z, "Z"),
        "LOG_CONFIG_END",
    ]
    blob = "\n".join(lines) + "\n"
    try:
        usys.stdout.write(blob)
        usys.stdout.flush()
    except Exception:
        pass

# ============================================================
# RECEIVER
# ============================================================
async def receiver():
    global state, packet_idx, expected_len, packet_buf
    global last_x, last_y, total_received_xy, start_threshold

    usys.stdout.buffer.write(b"receiver ready, version=%s\n" % FIRMWARE_VERSION)
    usys.stdout.flush()

    rx_byte_count = 0

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
        rx_byte_count += 1

        if state == WAIT_SYNC:
            if b == CMD_LINE or b == CMD_MOTION_BLOCK or b == CMD_Z \
                    or b == CMD_SET_START_THRESHOLD:
                packet_buf[0] = b
                packet_idx = 1
                expected_len = 0
                state = IN_PACKET
            if rx_byte_count >= 16:
                await wait(1)
                rx_byte_count = 0
            continue

        if packet_idx >= len(packet_buf):
            state = WAIT_SYNC
            packet_idx = 0
            expected_len = 0
            if rx_byte_count >= 16:
                await wait(1)
                rx_byte_count = 0
            continue

        packet_buf[packet_idx] = b
        packet_idx += 1

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

        if expected_len > 0 and packet_idx == expected_len:
            cmd = packet_buf[0]

            if cmd == CMD_LINE:
                tx = struct.unpack_from('<i', packet_buf, 1)[0]
                ty = struct.unpack_from('<i', packet_buf, 5)[0]
                dur = struct.unpack_from('<H', packet_buf, 9)[0]
                if buffer_put(tx, ty, dur):
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
                    if buffer_put(last_x, last_y, dur):
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
                start_threshold = min(new_threshold, START_THRESHOLD_MAX_SAFE)
                usys.stdout.write("START_THRESHOLD_SET %d (req=%d)\n" %
                                  (start_threshold, new_threshold))
                usys.stdout.flush()
                log_motor_config(reason="job_start")

            # === КРИТИЧНО ===
            # После каждого пакета проверяем flow state.
            # Именно здесь счётчик растёт — значит FLOW_STOP уйдёт
            # максимум через 100 мс (rate-limit) после пересечения порога.
            process_flow_state()

            state = WAIT_SYNC
            packet_idx = 0
            expected_len = 0

        if rx_byte_count >= 16:
            await wait(1)
            rx_byte_count = 0

# ============================================================
# EXECUTOR
# ============================================================
async def smooth_executor():
    global executed_xy, start_gate_open
    global _z_move_active, _z_move_started_ms
    global _z_move_target_angle, _z_move_after_xy

    usys.stdout.buffer.write(b"executor ready\n")
    usys.stdout.flush()

    current_x = 0.0
    current_y = 0.0
    segment_count = 0
    UPDATE_MS = 8

    max_lag_x = 0.0
    max_lag_y = 0.0
    diag_counter = 0

    Z_MOVE_TIMEOUT_MS = 5000

    while True:
        # --- Flow control + Z backpressure (единая точка) ---
        process_flow_state()

        # --- Non-blocking Z ---
        if _z_move_active:
            if motor_z.done():
                _z_move_active = False
                usys.stdout.write(
                    "Z_EXEC angle=%d after=%d executed=%d plan=%.1f,%.1f act=%d,%d\n" %
                    (_z_move_target_angle, _z_move_after_xy, executed_xy,
                     current_x, current_y,
                     motor_x.angle(), motor_y.angle()))
                usys.stdout.flush()
            elif clock.time() - _z_move_started_ms > Z_MOVE_TIMEOUT_MS:
                motor_z.stop()
                _z_move_active = False
                usys.stdout.write(
                    "Z_TIMEOUT angle=%d after=%d executed=%d\n" %
                    (_z_move_target_angle, _z_move_after_xy, executed_xy))
                usys.stdout.flush()

        # --- Запуск нового Z ---
        if not _z_move_active and z_count > 0:
            angle = z_angles[z_tail]
            after_xy = z_after_xy[z_tail]
            if executed_xy >= after_xy:
                catchup_start = clock.time()
                while clock.time() - catchup_start < 200:
                    err_x = current_x - motor_x.angle()
                    err_y = current_y - motor_y.angle()
                    if abs(err_x) < 0.5 and abs(err_y) < 0.5:
                        break
                    await wait(2)

                z_get()
                motor_z.run_target(Z_SPEED, angle, then=Stop.HOLD, wait=False)
                _z_move_active = True
                _z_move_started_ms = clock.time()
                _z_move_target_angle = angle
                _z_move_after_xy = after_xy

        if _z_move_active:
            await wait(2)
            continue

        # --- Стартовый порог ---
        if not start_gate_open:
            if buffer_count() >= start_threshold:
                start_gate_open = True
                usys.stdout.buffer.write(b"start printing\n")
                usys.stdout.flush()
            else:
                await wait(10)
                continue

        # --- XY ---
        idx = buffer_get_count()
        if idx == -1:
            await wait(1)
            continue

        target_x    = buf_x[idx]
        target_y    = buf_y[idx]
        duration_ms = buf_dur[idx]

        steps        = max(1, int(round(duration_ms / UPDATE_MS)))
        step_time_ms = duration_ms / steps
        dx = target_x - current_x
        dy = target_y - current_y

        seg_start = clock.time()
        for i in range(1, steps + 1):
            x = current_x + dx * i / steps
            y = current_y + dy * i / steps
            motor_x.track_target(x)
            motor_y.track_target(y)

            target_tick = seg_start + int(round(i * step_time_ms))
            now = clock.time()
            delta = target_tick - now
            if delta > 0:
                await wait(delta)

            # === КРИТИЧНО ===
            # Закрываем дыру «executor занят длинным сегментом».
            # На каждом шаге интерполяции проверяем flow state.
            # Функция внутри rate-limited 100 мс — реальный stdout
            # только при смене состояния.
            process_flow_state()

        current_x = target_x
        current_y = target_y
        executed_xy += 1
        segment_count += 1

        lag_x = current_x - motor_x.angle()
        lag_y = current_y - motor_y.angle()
        if abs(lag_x) > abs(max_lag_x):
            max_lag_x = lag_x
        if abs(lag_y) > abs(max_lag_y):
            max_lag_y = lag_y

        diag_counter += 1
        if diag_counter >= 200:
            usys.stdout.write("SEG %d maxlag=(%.1f,%.1f) buf=%d zq=%d\n" %
                              (segment_count, max_lag_x, max_lag_y,
                               buffer_count(), z_count))
            usys.stdout.flush()
            diag_counter = 0
            max_lag_x = 0.0
            max_lag_y = 0.0

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
