#!/usr/bin/python

# play audio
import pygame
from time import sleep

fBtn=open("/sys/ebb/gpio51/btnPressedBitMask", 'r+', 0)
fLed=open("/sys/ebb/gpio51/ledStateBitMask", 'r+', 0)

def readButtons():
    global fBtn
    fBtn.flush()
    fBtn.seek(0)
    return int(fBtn.readline())

def resetButtons():
    global fBtn
    fBtn.write("0")

if __name__ == "__main__":

    i = 0
    while True:
        sleep(1)

        fLed.write(str(++i & 3))

        #print str(i & 3)
        i += 1
        
        buttonMask = readButtons()

        if buttonMask:
            print buttonMask
            for i in xrange(4):
                if buttonMask & (1 << i):
                    resetButtons()

                    pygame.mixer.init()
                    pygame.mixer.music.load("/home/elka/music/3.mp3")
                    pygame.mixer.music.play()
                    while pygame.mixer.music.get_busy() == True:
                      continue
