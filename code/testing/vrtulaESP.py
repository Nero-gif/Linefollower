import machine
import ssd1306
import math
import time

# Nastavení displeje
SCREEN_WIDTH = 128
SCREEN_HEIGHT = 64
OLED_ADDR = 0x3C

# Definice I2C (SDA=5, SCL=6)
i2c = machine.I2C(0, sda=machine.Pin(5), scl=machine.Pin(6))
display = ssd1306.SSD1306_I2C(SCREEN_WIDTH, SCREEN_HEIGHT, i2c, addr=OLED_ADDR)

# Definice viditelné oblasti (podle tvého zadání)
VISIBLE_X_OFFSET = 28
VISIBLE_Y_OFFSET = 24
VISIBLE_WIDTH = 72
VISIBLE_HEIGHT = 40

# Výpočet středu
CENTER_X = VISIBLE_X_OFFSET + VISIBLE_WIDTH // 2
CENTER_Y = VISIBLE_Y_OFFSET + VISIBLE_HEIGHT // 2
RADIUS = 15

angle = 0.0

while True:
    # Smazat displej
    display.fill(0)

    # Obrys viditelné části
    display.rect(VISIBLE_X_OFFSET, VISIBLE_Y_OFFSET, VISIBLE_WIDTH, VISIBLE_HEIGHT, 1)

    # Čtyři lopatky vrtule
    for i in range(4):
        # Úhel pro každou lopatku (i * PI/2)
        a = angle + i * (math.pi / 2)

        # Výpočet koncového bodu úsečky
        # x = střed_x + cos(úhel) * poloměr
        # y = střed_y + sin(úhel) * poloměr
        x = int(CENTER_X + math.cos(a) * RADIUS)
        y = int(CENTER_Y + math.sin(a) * RADIUS)

        # Vykreslení čáry (střed -> konec lopatky)
        display.line(CENTER_X, CENTER_Y, x, y, 1)

    # Středový bod (MicroPython ssd1306 nemá fillCircle, simulujeme malým čtvercem nebo pixely)
    display.fill_rect(CENTER_X - 1, CENTER_Y - 1, 3, 3, 1)

    # Odeslat data na displej
    display.show()

    # Aktualizace úhlu (otáčení)
    angle += 0.25
    if angle > 2 * math.pi:
        angle = 0

    # Krátká pauza (v sekundách)
    time.sleep_ms(30)