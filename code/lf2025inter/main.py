# Raspberry Pi Pico based Line Following Robot
from machine import Pin,PWM #importing PIN and PWM
import time #importing time
import utime
# Defining motor pins
motor1=Pin(3,Pin.OUT)
motor2=Pin(2,Pin.OUT)
motor3=Pin(4,Pin.OUT)
motor4=Pin(5,Pin.OUT)
# Defining enable pins and PWM object
global enable1
global enable2
enable1=PWM(Pin(11))
enable2=PWM(Pin(6))
# Defining  right and left IR digital pins as input
IR_middleright = machine.Pin(8, machine.Pin.IN)
IR_middle = machine.Pin(10, machine.Pin.IN)
IR_right = machine.Pin(9, machine.Pin.IN)
IR_middleleft = machine.Pin(13, machine.Pin.IN)
IR_left = machine.Pin(12, machine.Pin.IN)
# Defining frequency for enable pins
enable1.freq(1000)
enable2.freq(1000)
# Setting maximum duty cycle for maximum speed
enable1.duty_u16(10000)#left
enable2.duty_u16(10000)
global nasobek_rychlosti
nasobek_rychlosti = 1.5 
global midd_deleni
midd_deleni = 3
# Forward
def move_forward(speed1, speed2):
    enable1.duty_u16(int(speed1*nasobek_rychlosti))#left
    enable2.duty_u16(int(speed2*nasobek_rychlosti))
    motor1.high()
    motor2.low()
    motor3.high()
    motor4.low() 
# Backward
def move_backward(speed1, speed2):
    enable1.duty_u16(int(speed1*nasobek_rychlosti))#left
    enable2.duty_u16(int(speed2*nasobek_rychlosti))
    motor1.high()
    motor2.low()
    motor3.high()
    motor4.low() 
#Turn Right
def turn_right(speed1, speed2):
    enable1.duty_u16(int(speed1*nasobek_rychlosti))#left
    enable2.duty_u16(int(speed2*nasobek_rychlosti))
    motor1.high()
    motor2.low()
    motor3.low()
    motor4.low() 
#Turn Left
def turn_left(speed1, speed2):
    enable1.duty_u16(int(speed1*nasobek_rychlosti))#left
    enable2.duty_u16(int(speed2*nasobek_rychlosti))
    motor1.low()
    motor2.low()
    motor3.high()
    motor4.low()
#Stop
def turn_middleleft(speed1, speed2):
    enable1.duty_u16(int(int(speed1*nasobek_rychlosti) / midd_deleni))#left
    enable2.duty_u16(int(speed2*nasobek_rychlosti))
    motor1.high()
    motor2.low()
    motor3.high()
    motor4.low()
def turn_middleright(speed1, speed2):
    enable1.duty_u16(int(speed1*nasobek_rychlosti))#left
    enable2.duty_u16(int(int(speed2*nasobek_rychlosti) / midd_deleni))
    motor1.high()
    motor2.low()
    motor3.high()
    motor4.low()
#Stop
def stop(speed1, speed2):
    enable1.duty_u16(int(speed1*nasobek_rychlosti))#left
    enable2.duty_u16(int(speed2*nasobek_rychlosti))
    motor1.high()
    motor2.high()
    motor3.high()
    motor4.high()
global R_mot_def_speed
R_mot_def_speed =40000
global L_mot_def_speed
L_mot_def_speed =96000

global memory
memory = 0

time.sleep(5)
for i in range(1,1001):
    move_forward(int(L_mot_def_speed*(i/1000)), int(R_mot_def_speed*(i/500)))
    time.sleep(0.001)
    
while True:
    right_val=IR_right.value() #Getting right IR value(0 or 1)
    middle_val=IR_middle.value()
    left_val=IR_left.value() #Getting left IR value(0 or 1)
    middleleft_val=IR_middleleft.value()
    middleright_val=IR_middleright.value()
    #print(right_val, middleright_val, middle_val, middleleft_val, left_val)
    #time.sleep(1)
    if middleright_val==0 and middle_val==0 and middleleft_val==0:
        move_forward(L_mot_def_speed, R_mot_def_speed)
        time.sleep(0.5)
        stop(1, 1)
        quit()
    elif right_val==1 and left_val==0:
        turn_left(L_mot_def_speed, R_mot_def_speed)
        mamory = 12220
    elif right_val==0 and left_val==1:
        turn_right(L_mot_def_speed, R_mot_def_speed)
        memory = 02221
    elif right_val==1 and middleright_val==1 and middle_val==0 and middleleft_val==1 and left_val==1:
        move_forward(L_mot_def_speed, R_mot_def_speed)
        memory = 11011
    elif middleright_val==1 and middleleft_val==0:
        turn_middleleft(L_mot_def_speed, R_mot_def_speed)
        memory = 20210
    elif middleright_val==0 and middleleft_val==1:
        turn_middleright(L_mot_def_speed, R_mot_def_speed)
        memory = 21202
    elif memory == 12220:
        turn_left(L_mot_def_speed, R_mot_def_speed)
    elif memory == 02221:
        turn_right(L_mot_def_speed, R_mot_def_speed)        
# #    else:
# #       stop()
