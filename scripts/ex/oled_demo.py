from jumperless import oled_connect, oled_print
import time

oled_connect()

word = "JUMPERLESS"
text = ""

for ch in word:
    text += ch
    
    oled_print(text + "")
    
    time.sleep(0.25)


oled_print(text)
