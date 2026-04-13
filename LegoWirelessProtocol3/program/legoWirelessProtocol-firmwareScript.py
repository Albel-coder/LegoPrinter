from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import wait
from usys import stdin, stdout
from uselect import poll
import ustruct

stdout.buffer.write(b"boot\n")
hub = TechnicHub()
keyboard = poll()
keyboard.register(stdin)

# Попытка инициализации мотора с индикацией
try:
    motor = Motor(Port.A)
    motor_ok = True
    hub.light.on(Color.GREEN)    # Зелёный - мотор найден
except:
    motor_ok = False
    for _ in range(3):
        hub.light.on(Color.RED)  # Мигает красным 3 раза
        wait(200)
        hub.light.off()
        wait(200)
    stdout.buffer.write(b"ERROR: Motor init failed: " + str(e).encode() + b"\n")

COMMAND_MOVE = 0x01
COMMAND_STOP = 0x02
COMMAND_STATUS = 0x04
COMMAND_RESET = 0x05
COMMAND_PING = 0x06

# Сигнал готовности
stdout.buffer.write(b"ready\n")

while True:
    while not keyboard.poll(0):
        wait(10)    
    data = stdin.buffer.read(128)
    if not data:
        continue    
    command = data[0]
    stdout.buffer.write(b"got command: " + bytes([command]) + b"\n")
    if command == COMMAND_PING:
        stdout.buffer.write(b"pong\n")
    elif commad == COMMAND_MOVE and motor_ok:
        if len(data) >= 11:
            port = data[1]
            speed = int.from_bytes(data[2:6], 'little')
            angle = int.from_bytes(data[6:10], 'little')
            hold = data[10]
            motor.run_angle(speed, angle, then=Stop.HOLD if hold else Stop.NONE, wait=False)
            stdout.buffer.write(b"ack\n")
    elif command == COMMAND_STOP and motor_ok:
        motor.stop()
        stdout.buffer.write(b"ack\n")
    elif command == COMMAND_RESET and motor_ok:
        motor.reset_angle(0)
        stdout.buffer.write(b"ack\n")