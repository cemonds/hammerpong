import pygame
import serial
import time

pygame.mixer.pre_init(44100, -16, 1, 512)
pygame.mixer.init()
hit = pygame.mixer.Sound("sounds/sound1.wav")
miss = pygame.mixer.Sound("sounds/sound3.wav")
start = pygame.mixer.Sound("sounds/sound2.wav")
end = pygame.mixer.Sound("sounds/sound4.wav")
port = serial.Serial('/dev/ttyUSB0', baudrate=115200, timeout=1)
print "initialized"

print "play start"
start.play()

print "play end"
end.play()

print "play hit"
hit.play()

print "play miss"
miss.play()

	
while True:
    try:
        command = port.read(4)
 
#        if len(data) >= 4:
        if len(command) > 0:
            print "received command "+''.join(format(ord(x), '02x') for x in command)

            if ord(command[0]) == 1:
                print "play start"
                start.play()
            elif ord(command[0]) == 2:
                print "play end"
                end.play()
            elif ord(command[0]) == 4:
                print "play hit"
                hit.play()
            elif ord(command[0]) == 5:
                print "play miss" 
                miss.play()
         
    except serial.SerialException:
        pass
