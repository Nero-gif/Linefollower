from machine import Pin
import time




enc_a = Pin(1, Pin.IN, Pin.PULL_UP)
enc_b = Pin(0, Pin.IN, Pin.PULL_UP)

positionMotoru = 0

def encoder_irq(pin):
    global position
    if enc_b.value() == 0:
        positionMotoru += 1



# ------ Sensor pins ------
sensorR4 = Pin(17, Pin.IN)
sensorR3 = Pin(16, Pin.IN)
sensorR2 = Pin(18, Pin.IN)
sensorR1 = Pin(19, Pin.IN)
sensorL4 = Pin(15, Pin.IN)
sensorL3 = Pin(14, Pin.IN)
sensorL2 = Pin(13, Pin.IN)
sensorL1 = Pin(12, Pin.IN)

# ------ Motor control pins ------
motorL1 = Pin(11, Pin.OUT)
motorL2 = Pin(10, Pin.OUT)
motorR1 = Pin(20, Pin.OUT)
motorR2 = Pin(21, Pin.OUT)

# ------ Enable pins (PWM) ------
motorPWMLeft = PWM(Pin(22))
motorPWMRight = PWM(Pin(9))
motorPWMLeft.freq(1000)
motorPWMRight.freq(1000)

# ------ PID parameters ------
global base_speed
base_speed = 18000
Kp = 9700.0
Ki = 1.0
Kd = 36000.0

error = 0.0
previous_error = 0.0
integral = 0.0

time.sleep(3)

# ====== Helper functions ======
def set_motor(left_speed, right_speed, sensor_state):
    left_speed = max(min(int(left_speed), 65535), -65535)
    right_speed = max(min(int(right_speed), 65535), -65535)
    # ------ direction control ------
    if left_speed >= 0:
        motorL1.high()
        motorL2.low()
    else:
        motorL1.low()
        motorL2.high()

    if right_speed >= 0:
        motorR1.high()
        motorR2.low()
    else:
        motorR1.low()
        motorR2.high()
    motorPWMLeft.duty_u16(abs(left_speed))
    motorPWMRight.duty_u16(abs(right_speed))
# ====== position reading function ======
def read_position():
    vsensorL1, vsensorL2, vsensorL3, vsensorL4 = sensorL1.value(), sensorL2.value(), sensorL3.value(), sensorL4.value()
    vsensorR1, vsensorR2, vsensorR3, vsensorR4 = sensorR1.value(), sensorR2.value(), sensorR3.value(), sensorR4.value()
    total = vsensorL1 + vsensorL2 + vsensorL3 + vsensorL4 + vsensorR1 + vsensorR2 + vsensorR3 + vsensorR4
    if total == 0:
        return None
    weighted_sum = (
        vsensorL1*-3.5 + vsensorL2*-2.5 + vsensorL3*-1.5 + vsensorL4*-0.5 +
        vsensorR1*0.5 + vsensorR2*1.5 + vsensorR3*2.5 + vsensorR4*3.5
    )
    return weighted_sum / total

# ====== main loop ======
motorL1.high()
motorL2.low()
motorR1.high()
motorR2.low()
        
# ===== start =====
i = 0
while i < base_speed:
    motorPWMLeft.duty_u16(int(i))
    motorPWMRight.duty_u16(int(i))
    i += .5
# ===== main loop =====
while True:
    enc_a.irq(trigger=Pin.IRQ_RISING, handler=encoder_irq)
    if(positionMotoru > 16000 and positionMotoru < 17000):
        if(base_speed > 18000):
            base_speed -= 5
    if(positionMotoru > 17000):
        if(base_speed < 18000):
            base_speed += 5
    position = read_position()
    sensor_state = f"{sensorL1.value()} {sensorL2.value()} {sensorL3.value()} {sensorL4.value()} {sensorR1.value()} {sensorR2.value()} {sensorR3.value()} {sensorR4.value()}"

    
    if all(v == 1 for v in[sensorL4.value(), sensorL3.value(), sensorL2.value(), sensorL1.value()]):
        motorL1.low()
        motorL2.high()
        motorR1.high()
        motorR2.low()
           
        set_angle_X(20)
        motorPWMLeft.duty_u16(8000)
        motorPWMRight.duty_u16(18000)
       
    elif all(v == 1 for v in[sensorR4.value(), sensorR3.value(), sensorR2.value(), sensorR1.value()]):
        motorL1.high()
        motorL2.low()
        motorR1.low()
        motorR2.high()
        
        set_angle_X(-20)
        motorPWMLeft.duty_u16(18000)
        motorPWMRight.duty_u16(8000)
        
    if all(v == 1 for v in[sensorL4.value(), sensorL3.value(), sensorR1.value(), sensorR2.value()]):
        break
    elif position is not None:
        # PID regulation
        error = position
        integral += error
        integral = max(min(integral, 100), -100)
        derivative = error - previous_error
        correction = Kp * error + Ki * integral + Kd * derivative

        left_speed = base_speed + correction
        right_speed = base_speed - correction

        set_motor(left_speed, right_speed, sensor_state)
        previous_error = error
        
    # ------ anti fire fuse ------
    time.sleep(0.01)

# ------ motor stop ------
while base_speed > 0:
    motorPWMRight.duty_u16(base_speed)
    motorPWMLeft.duty_u16(base_speed)
    base_speed -= 2
    time.sleep(.0002)
print("stop")