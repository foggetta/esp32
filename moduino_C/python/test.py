import t
import gsm
import machine

t.modemOnOffNew()
#gsm.start(tx=5, rx=35, apn='internet', connect=False)
gsm.start(tx=5, rx=35, apn='internet', connect=False, user='login', password='pass')
gsm.connect()
gsm.status()
gsm.stop()
uart = machine.UART(1, tx=5, rx=35)
#uart.write('AT\r\n')
recipient = "+393406440781"
message = "Hello, F1neye!"

try:
    time.sleep(0.5)
    #uart.write(b'ATZ\r\n')
    uart.write('ATZ\r\n')
    time.sleep(0.5)
    uart.write('AT+CMGF=1\r\n')
    time.sleep(0.5)
    uart.write('AT+CMGS="' + recipient + '"\r\n')
    time.sleep(0.5)
    uart.write(message + "\r\n")
    time.sleep(0.5)
    uart.write(bytes([26]))
    time.sleep(0.5)
finally:
    uart.close()

machine.deepsleep(10000)

