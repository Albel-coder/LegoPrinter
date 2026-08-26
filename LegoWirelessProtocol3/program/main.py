from pybricks.hubs import TechnicHub
from pybricks.pupdevices import Motor
from pybricks.parameters import Port, Stop, Color
from pybricks.tools import multitask, run_task, wait

import usys
import uselect
import ustruct as struct

from micropython import kbd_intr

# micropython setup
kbd_intr(-1)

hub = TechnicHub()
hub.light.on(Color.GREEN)

# protocol
MOTION_TRANSPORT_PREFIX = 0x06

CMD_LINE = 0x01
CMD_MOTION_BLOCK = 0x02

CMD_Z = 0x05
CMD_SET_START_THRESHOLD = 0x06

# XY buffer
BUFFER_SIZE = 1024

buf_x = [0] * BUFFER_SIZE
buf_y = [0] * BUFFER_SIZE
buf_dur = [0] * BUFFER_SIZE

head = 0
tail = 0

def buffer_put(x, y, duration):
    global head

    next_head = (head + 1) % BUFFER_SIZE

    if next_head == tail:
        return False

    buf_x[head] = x
    buf_y[head] = y
    buf_dur[head] = duration

    head = next_head

    return True

def buffer_get_index():
    global tail

    if tail == head:
        return -1
    
    index = tail
    tail = (tail + 1) % BUFFER_SIZE

    return index

def buffer_count():
    if head >= tail:
        return head - tail
    
    return BUFFER_SIZE - (tail - head)

# Z axis logic

motor_z = Motor(Port.C)

motor_z.control.limits(speed=600, acceleration=2000)

motor_z.reset_angle(0)

Z_UP_ANGLE = 90
Z_DOWN_ANGLE = 0
Z_SPEED = 600

# Z queue

Z_QUEUE_SIZE = 4

z_angles = [0] * Z_QUEUE_SIZE
z_after_xy = [0] * Z_QUEUE_SIZE

z_head = 0
z_tail = 0
z_count = 0

def z_put(angle, after_xy):
    global z_head
    global z_count

    if z_count >= Z_QUEUE_SIZE:
        return False

    z_angles[z_head] = angle
    z_after_xy[z_head] = after_xy

    z_head = (z_head + 1) % Z_QUEUE_SIZE

    z_count += 1

    return True

def z_get():
    global z_tail
    global z_count

    if z_count == 0:
        return None, None
    
    angle = z_angles[z_tail]
    after_xy = z_after_xy[z_tail]

    z_tail = (z_tail + 1) % Z_QUEUE_SIZE

    z_count += 1

    return angle, after_xy

# job state

total_received_xy = 0
executed_xy = 0

# default value
# normally overwritten by CMD_SET_START_THRESHOLD
start_threshold = 35

start_gate_open = False

# absolute planned position used to decode delta packets
last_x = 0
last_y = 0

def reset_job_state():
    global total_received_xy
    global executed_xy
    global start_gate_open

    global z_head
    global z_tail
    global z_count

    global last_x
    global last_y

    global head
    global tail

    total_received_xy = 0
    executed_xy = 0

    start_gate_open = False

    z_head = 0
    z_tail = 0
    z_count = 0

    last_x = 0
    last_y = 0

    head = 0
    tait = 0

# simple packet parser

WAIT_SYNC = 0
IN_PACKET = 1

state = WAIT_SYNC

packet_buf = bytearray(256)
packet_idx = 0
expected_len = 0

# input polling

poll = uselect.poll()
poll.register(usys.stdin, uselect.POLLIN)

# motors xy

motor_x = Motor(Port.A)
motor_y = Motor(Port.B)

motor_x.control.limits(speed=2000, acceleration=6000)
motor_y.control.limits(speed=2000, acceleration=6000)

motor_x.reset_angle(0)
motor_y.reset_angle(0)

async def receiver():
    global state
    global packet_idx
    global expected_len

    global last_x
    global last_y

    global total_received_xy
    global start_threshold

    usys.stdout.buffer.write(b"receiver ready\n")
    usys.stdout.flush()

    while True:

        events = poll.poll(0)

        if not events:
            await wait(1)
            continue
        
        data = usys.stdin.buffer.read(1)

        if not data:
            await wait(1)
            continue
        
        b = data[0]

        if state == WAIT_SYNC:

            if b != MOTION_TRANSPORT_PREFIX:
                continue

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

        # command received
        if packet_idx == 2:

            cmd = packet_buf[1]

            if cmd == CMD_LINE:

                # 06 01
                # int32 X
                # int32 Y
                # uint16 duration
                expected_len = 12
            elif cmd == CMD_MOTION_BLOCK:
                # Count is the third byte
                expected_len = 0
            elif cmd == CMD_Z:
                # 06 05
                # int16 angle
                # uint32 afterXY
                expected_len = 8
            elif cmd == CMD_SET_START_THRESHOLD:
                # 06 06
                # uint16 threshold
                expected_len = 4
            else:
                state = WAIT_SYNC
                packet_idx = 0
                expected_len = 0
                continue
        
        # motion block count
        if (packet_idx == 3 and packet_buf[1] == CMD_MOTION_BLOCK):
            count = packet_buf[2]

            if count == 0:
                state = WAIT_SYNC
                packet_idx = 0
                expected_len = 0
                continue
            
            expected_len = (3 + count * 6)

            if expected_len > len(packet_buf):
                state = WAIT_SYNC
                packet_idx = 0
                expected_len = 0
                continue

        # full packet
        if (expected_len > 0 and packet_idx == expected_len):
            cmd = packet_buf[1]

            if cmd == CMD_LINE:
                tx = struct.unpack_from('<i', packet_buf, 2)[0]
                ty = struct.unpack_from('<i', packet_buf, 6)[0]
                duration = struct.unpack_from('<H', packet_buf, 10)[0]

                if buffer_put(tx, ty, duration):
                    last_x = tx
                    last_y = ty

                    total_received_xy += 1
                else:
                    usys.stdout.buffer.write(b"BUFFER_FULL\n")
                    usys.stdout.flush()

            elif cmd == CMD_MOTION_BLOCK:
                count = packet_buf[2]
                offset = 3

                for _ in range(count):
                    dx, dy, duration = \ struct.unpack_from('hhH', packet_buf, offset)

                    offset += 6

                    next_x = last_x + dx
                    next_y = last_y + dy

                    if not buffer_put(next_x, next_y, duration):
                        usys.stdout.buffer.write(b"BUFFER_FULL")
                        usys.stdout.flush()
                        break
                    
                    last_x = next_x
                    last_y = next_y

                    total_received_xy += 1
            
            elif cmd == CMD_Z:
                angle = struct.unpack_from('<h', packet_buf, 2)[0]
                after_xy = struct.unpack_from('<I', packet_buf, 4)[0]

                if not z_put(angle, after_xy):
                    usys.stdout.buffer.write(b"Z_QUEUE_FULL")
                    usys.stdout.flush()
            
            elif cmd == CMD_SET_START_THRESHOLD:
                new_threshold = \ struct.unpack_from('<H', packet_buf, 2)[0]

                reset_job_state()

                start_threshold = new_threshold

                usys.stdout.buffer.write(b"START_THRESHOLD_SET %d\n" % start_threshold)
                usys.stdout.flush()
            
            # reset parser
            state = WAIT_SYNC
            packet_idx = 0
            expected_len = 0

            await wait(1)

# XY + Z executor

async def smooth_executor():
    global executed_xy
    global start_gate_open

    usys.stdout.buffer.write(b"executor ready\n")
    usys.stdout.flush()

    current_x = 0
    current_y = 0

    segment_count = 0

    UPDATE_MS = 4

    flow_stopped = False

    while True:
        if z_count > 0:
            angle = z_angles[z_tail]
            after_xy = z_after_xy[z_tail]

            if executed_xy >= after_xy:
                z_get()

                motor_z.run_target(Z_SPEED, angle, then=Stop.HOLD, wait=False)

                while not motor_z.done():
                    await wait(2)
                
                usys.stdout.write(b"Z_EXEC angle = %d after = %d executed = %d" % (angle, after_xy, executed_xy))
                usys.stdout.flush()

                continue
            
        if not start_gate_open:
            if buffer_count() >= start_threshold:
                start_gate_open = True

                usys.stdout.buffer.write(b"start printing\n")
                usys.stdout.flush()
            else:
                await wait(10)
                continue
        
        count = buffer_count()

        if (not flow_stopped and count > 900):
            usys.stdout.buffer.write(b"FLOW_STOP\n")
            usys.stdout.flush()
            flow_stopped = True
        elif (flow_stopped and count < 400):
            usys.stdout.buffer.write(b"FLOW_RESUME")
            usys.stdout.flush()
            flow_stopped = False
        
        idx = buffer_get_index()

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

            executed_xy += 1
            segment_count += 1

            continue

        steps = max(1, duration_ms // UPDATE_MS)

        dx = target_x - current_x
        dy = target_y - current_y

        for i in range(1, steps + 1):
            x = (current_x + (dx * i) // steps)
            y = (current_y + (dy * i) // steps)

            motor_x.track_target(x)
            motor_y.track_target(y)

            await wait(UPDATE_MS)
        
        # final target
        motor_x.track_target(target_x)
        motor_y.track_target(target_y)

        current_x = target_x
        current_y = target_y

        executed_xy += 1
        segment_count += 1

        if segment_count % 20 == 0:
            usys.stdout.write("SEG")
            usys.stdout.write(str(segment_count))
            usys.stdout.write(": X = ")
            usys.stdout.write(str(motor_x.angle()))
            usys.stdout.write(" Y = ")
            usys.stdout.write(str(motor_y.angle()))
            usys.stdout.write("\n")
            usys.stdout.flush()

async def main():
    try:
        await multitask(receiver(), smooth_executor())
    except Exception as e:
        try:
            usys.stdout.write("CRITICAL_ERROR:")
            usys.stdout.write(str(e))
            usys.stdout.write("\n")
            usys.stdout.flush()
        except:
            pass

        hub.light.on(Color.RED)

        while True:
            await wait(100)

run_task(main())        
