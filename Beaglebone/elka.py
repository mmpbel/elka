#!/usr/bin/python

# play audio
import pygame
from time import sleep

f=open("/sys/ebb/gpio115/numberPresses")

def readButtons():
  f.flush()
  sleep(1)
  f.seek(0)
  return int(f.readline())

button = readButtons()
newBtn = button

while button == newBtn:
  newBtn = readButtons()
  print newBtn

button = newBtn

pygame.mixer.init()
pygame.mixer.music.load("/home/elka/music/3.mp3")
pygame.mixer.music.play()
while pygame.mixer.music.get_busy() == True:
  continue
