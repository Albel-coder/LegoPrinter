from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop
from pybricks.tools import wait
from usys import stdin, stdout
from uselect import poll
import ustruct

# Configuring Ports
# Mapping Numbers (from C++) to Real LEGO Portals
PORT_MAP = {
    0: Port.A,
    1: Port.B,
    2: Port.C,
    3: Port.D
}

# Create motor objects only for the ports where they are connected.
# (If the port is empty, Motor() will throw an exception - catch it and set it to None)
motors = {}
for port_number, port_enum in PORT_MAP.items():
    try:
        motors[port_number] = Motor(port_enum)
    except:
        motors[port_number] = None # No motor
        
# BLE setup (stdin/stdout)
hub = TechnicHub()
keyboard = poll()
keyboard.register(stdin)

# Command codes (must match C++ code)
CMD_MOVE = 0x01    # move with parameters
CMD_STOP = 0x02    # stop all motors
CMD_STATUS = 0x04  # request status (position/speed)
CMD_RESET = 0x05   # reset encoder to zero
CMD_PING = 0x06    # check connection

# Ready signal
stdout.buffer.write(b"ready\n")

def send_status():
    """Sends the status of all motors (simplified: only ports 0 and 1)"""
    position0 = motors.get(0).angle() if motors.get(0) else 0
    position1 = motors.get(1).angle() if motors.get(1) else 0
    speed0 = motors.get(0).speed() if motors.get(0) else 0
    speed1 = motors.get(1).speed() if motors.get(1) else 0
    packet = ustruct.pack("<Biiii", CMD_STATUS, position0, position1, speed0, speed1)
    stdout.buffer.write(packet)
    
while True:
    while not keyboard.poll(0):
        wait(10)
    
    data = stdin.buffer.read(128)
    if not data:
        continue
        
    try:
        cmd = data[0]
        
        if cmd == CMD_MOVE and len(data) >= 2:
            # Format: [COMMAND_MOVE][count][command1][command2]...
            count = data[1]
            offset = 2
            for _ in range(count):
                if len(data) < offset + 10:
                    break
                port, speed, angle, flags = ustruct.unpack("<BiiB", data[offset:offset+10])
                offset += 10
                
                motor = motors.get(port)
                if motor:
                    then = Stop.HOLD if (flags & 1) else Stop.NONE
                    motor.run_angle(speed, angle, then=then, wait=False)
            
            stdout.buffer.write(b"ack\n")
            
        elif cmd == CMD_STOP:
            for motor in motors.values():
                if motor:
                    motor.stop()
            stdout.buffer.write(b"ack\n")
            
        elif cmd == CMD_STATUS:
            send_status()
        
        elif cmd == CMD_RESET:
            for motor in motors.values():
                if motor:
                    motor.reset_angle(0)
            stdout.buffer.write(b"ack\n")
            
        elif cmd == CMD_PING:
            stdout.buffer.write(b"pong\n")
    
    except Exception as e:
        stdout.buffer.write(f"ERROR:{e}\n".encode())