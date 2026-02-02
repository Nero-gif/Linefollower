from machine import Pin, PWM
import time

# Doporučuji GPIO 2 (na desce označen jako G2 nebo jen 2)
# Je to bezpečný pin, který nekoliduje s OLED displejem (I2C)
servo_pin = Pin(2, Pin.OUT)
pwm = PWM(servo_pin)

# Nastavení frekvence na 50Hz (standard pro serva)
pwm.freq(50)

print("Startuji PWM na GPIO 2...")

# Inicializace na střed (cca 1.5ms puls)
# Pro 16-bit (0-65535): 1ms = 3277, 1.5ms = 4915, 2ms = 6553
pwm.duty_u16(4915)
time.sleep(2)

try:
    while True:
        # Pomalu zvyšuj (pohyb tam)
        for duty in range(3277, 6553, 50):
            pwm.duty_u16(duty)
            time.sleep(0.02) # Trochu rychlejší odezva

        # Pomalu snižuj (pohyb zpět)
        for duty in range(6553, 3277, -50):
            pwm.duty_u16(duty)
            time.sleep(0.02)

except KeyboardInterrupt:
    # Bezpečné ukončení a vypnutí PWM
    pwm.deinit()
    print("PWM vypnuto")