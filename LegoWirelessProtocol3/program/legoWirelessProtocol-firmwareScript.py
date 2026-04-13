from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import wait

hub = TechnicHub()
hub.light.on(Color.RED)        # Старт – красный

try:
    motor = Motor(Port.A)      # Попытка инициализации мотора
    hub.light.on(Color.GREEN)  # Мотор найден – зелёный
    wait(500)
    motor.run_angle(500, 360, then=Stop.HOLD, wait=False)
except Exception as e:
    # Мигаем красным 5 раз при ошибке
    for _ in range(5):
        hub.light.on(Color.RED)
        wait(200)
        hub.light.off()
        wait(200)
    # Дальше скрипт просто висит, но светодиод не горит

while True:
    wait(1000)