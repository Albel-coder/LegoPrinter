"""
Программа управления мотором через Pybricks BLE UART.
Использует сырой ввод/вывод stdin/stdout для обмена командами с ПК.
Поддерживает команды:
  0x01 MOVE   - вращение мотора с заданной скоростью и углом
  0x02 STOP   - остановка мотора
  0x04 STATUS - (зарезервировано)
  0x05 RESET  - сброс энкодера мотора в 0
  0x06 PING   - проверка связи (отвечает "pong")
"""

from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import wait
from usys import stdin, stdout

# ------------------------------------------------------------
# 1. Инициализация оборудования
# ------------------------------------------------------------
hub = TechnicHub()

# Сообщаем о старте программы (драйвер ожидает "boot\n")
stdout.buffer.write(b"boot\n")
stdout.flush()  # Гарантированная отправка данных в BLE

# Пытаемся инициализировать мотор на порту A
try:
    motor = Motor(Port.A)
    motor_ok = True
    hub.light.on(Color.GREEN)   # Зелёный = мотор готов
except Exception as e:
    motor_ok = False
    # Мигаем красным три раза при ошибке
    for _ in range(3):
        hub.light.on(Color.RED)
        wait(200)
        hub.light.off()
        wait(200)
    # Отправляем сообщение об ошибке драйверу
    err_msg = b"ERROR: Motor init failed: " + str(e).encode() + b"\n"
    stdout.buffer.write(err_msg)
    stdout.flush()

# ------------------------------------------------------------
# 2. Коды команд (должны совпадать с драйвером)
# ------------------------------------------------------------
COMMAND_MOVE   = 0x01
COMMAND_STOP   = 0x02
COMMAND_STATUS = 0x04   # Не используется в этой версии
COMMAND_RESET  = 0x05
COMMAND_PING   = 0x06

# ------------------------------------------------------------
# 3. Основной цикл обработки команд
# ------------------------------------------------------------
while True:
    # Сообщаем драйверу, что хаб готов принимать команды
    stdout.buffer.write(b"ready\n")
    stdout.flush()

    # БЛОКИРУЮЩЕЕ ЧТЕНИЕ ПЕРВОГО БАЙТА КОМАНДЫ
    # Программа будет ждать здесь, пока не получит хотя бы 1 байт
    cmd_byte = stdin.buffer.read(1)
    if not cmd_byte:
        # Если по какой-то причине прочитали пустой байт, пропускаем
        continue

    command = cmd_byte[0]

    # Отладочное эхо для драйвера (можно удалить в production)
    stdout.buffer.write(b"got command: " + bytes([command]) + b"\n")
    stdout.flush()

    # --------------------------------------------------------
    # Обработка команды PING
    # --------------------------------------------------------
    if command == COMMAND_PING:
        stdout.buffer.write(b"pong\n")
        stdout.flush()

    # --------------------------------------------------------
    # Обработка команды MOVE (требуется 10 дополнительных байт)
    # --------------------------------------------------------
    elif command == COMMAND_MOVE and motor_ok:
        # Читаем оставшиеся 10 байт (порт, скорость, угол, флаг удержания)
        remaining = stdin.buffer.read(10)
        if len(remaining) == 10:
            port  = remaining[0]                     # Порт мотора (пока игнорируем, у нас только A)
            speed = int.from_bytes(remaining[1:5], 'little')   # Скорость (град/сек)
            angle = int.from_bytes(remaining[5:9], 'little')   # Угол поворота (град)
            hold  = remaining[9]                     # 1 = удерживать позицию, 0 = свободный выбег

            # Запускаем асинхронное вращение (wait=False, чтобы не блокировать цикл)
            motor.run_angle(speed, angle,
                            then=Stop.HOLD if hold else Stop.NONE,
                            wait=False)
            stdout.buffer.write(b"ack\n")
        else:
            stdout.buffer.write(b"ERROR: MOVE command incomplete\n")
        stdout.flush()

    # --------------------------------------------------------
    # Обработка команды STOP
    # --------------------------------------------------------
    elif command == COMMAND_STOP and motor_ok:
        motor.stop()
        stdout.buffer.write(b"ack\n")
        stdout.flush()

    # --------------------------------------------------------
    # Обработка команды RESET (сброс угла энкодера)
    # --------------------------------------------------------
    elif command == COMMAND_RESET and motor_ok:
        motor.reset_angle(0)
        stdout.buffer.write(b"ack\n")
        stdout.flush()

    # --------------------------------------------------------
    # Неизвестная команда или мотор не инициализирован
    # --------------------------------------------------------
    else:
        if not motor_ok:
            stdout.buffer.write(b"ERROR: Motor not available\n")
        else:
            stdout.buffer.write(b"ERROR: Unknown command\n")
        stdout.flush()