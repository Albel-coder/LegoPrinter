from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Color
from pybricks.tools import wait
from usys import stdin, stdout

ACCELERATION = 800
SPEED_LIMIT = 1000

hub = TechnicHub()
try:
    motor = Motor(Port.A)
    motor.control.limits(speed=SPEED_LIMIT, acceleration=ACCELERATION)
    motor.reset_angle(0)
    hub.light.on(Color.GREEN)
except Exception as e:
    while True:
        hub.light.on(Color.RED)
        wait(200)
        hub.light.off()
        wait(200)

SYNC_BYTE = 0xAA
CMD_UPDATE_TARGET = 0x10

def read_exact(n):
    data = b''
    while len(data) < n:
        chunk = stdin.buffer.read(n - len(data))
        if chunk:
            data += chunk
    return data

while True:
    while True:
        b = read_exact(1)
        if b[0] == SYNC_BYTE:
            break

    length = read_exact(1)[0]
    if length < 2:
        continue

    frame = read_exact(length + 1)
    axis = frame[0]
    cmd = frame[1]
    payload = frame[2:-1]

    if axis != 0 or cmd != CMD_UPDATE_TARGET:
        continue

    if len(payload) >= 8:
        # Позиционные аргументы: little-endian, signed
        target = int.from_bytes(payload[0:4], 'little', True)
        # speed = int.from_bytes(payload[4:6], 'little', False)

        try:
            motor.run_target(SPEED_LIMIT, target)
            stdout.buffer.write(b"OK\n")
            stdout.flush()
        except Exception as e:
            err = b"ERROR: " + str(e).encode() + b"\n"
            stdout.buffer.write(err)
            stdout.flush()