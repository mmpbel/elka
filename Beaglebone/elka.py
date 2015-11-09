#!/usr/bin/python

# play audio
import sys
import math
import threading
import pygame

from time import sleep

fBtn=open("/sys/ebb/gpio2/btnPressedBitMask", 'r+', 0)

ledEvent = threading.Event()
ledEvent.clear()
led0 = "P9_21"
led1 = "P9_16"

class ledThread (threading.Thread):
    def __init__(self, threadID, name, counter):
        threading.Thread.__init__(self)

    def run(self):
    
        print "Thread start"

        # have to do this line here because sometimes (don't know why) if fails during initialization
        # so let's do PWM initialization when we start LED thread
        import Adafruit_BBIO.PWM as PWM
        PWM.start(led0, 0, 30000)
        PWM.start(led1, 0, 30000)

        i=0

        while True:
            if ledEvent.is_set():
                sleep(0.2)
                try:
                    PWM.set_duty_cycle(led0, 45 * math.sin(i) + 55)
                    PWM.set_duty_cycle(led1, 45 * math.cos(i) + 55)
                except Exception as inst:
                    print type(inst)     # the exception instance
                    print inst.args      # arguments stored in .args
                    print "Unexpected error:", sys.exc_info()[0]
                    break
                i += math.pi/2
            else:
                PWM.start(led0, 0, 30000)
                PWM.start(led1, 0, 30000)
                ledEvent.wait()

def readButtons():
    fBtn.flush()
    fBtn.seek(0)
    return int(fBtn.readline())

def resetButtons():
    fBtn.write("0")

if __name__ == "__main__":

    LAST_SONG = 7
    currentSong = 0
    paused = False

    pygame.mixer.init()
    pygame.mixer.music.set_volume(1)

    # Create new threads
    thread1 = ledThread(1, "ledThread", 1)
    # thread dies when main thread exits.
    thread1.daemon = True

    try:
        while True:
            sleep(1)

            buttonMask = readButtons()

            if buttonMask:
                print "btn: " + str(buttonMask)
                for command in xrange(4):
                    print "try cmd: " + str(command)
                    if buttonMask & (1 << command):
                        resetButtons()
                        print "cmd: " + str(command)
                        pygame.mixer.music.unpause()    #there is some issue with pause+play so let's unpause before any command
                        if not thread1.isAlive():
                            thread1.start()
                            
                        ledEvent.set()

                        if command == 3 or command == 4:
                            print "next"
                            if pygame.mixer.music.get_busy() == True:
                                pygame.mixer.music.fadeout(1)
                            else:
                                pygame.mixer.music.stop()
                            # play next music
                            pygame.mixer.music.load('/home/elka/music/' + str(currentSong) + '.mp3')
                            pygame.mixer.music.play()
                            currentSong += 1
                            if LAST_SONG < currentSong:
                                currentSong = 0

                        elif command == 5:
                            # stop everything
                            print "stop"
                            currentSong = 0
                            ledEvent.clear()
                            pygame.mixer.music.stop()

                        elif command == 1:
                            if buttonMask & (1 << 3):
                                print "stop"
                                currentSong = 0
                                ledEvent.clear()
                                pygame.mixer.music.stop()
                            if not paused:
                                # pause music
                                paused = True
                                print "pause"
                                pygame.mixer.music.pause()
                            else:
                                # resume playing
                                paused = False
                                print "resume"
                                #pygame.mixer.music.unpause()

                        elif command == 2:
                            print "yes"
                            if pygame.mixer.music.get_busy() == True:
                                pygame.mixer.music.fadeout(1)
                            # yes sound
                            pygame.mixer.music.load('/home/elka/music/yes.mp3')
                            pygame.mixer.music.play()

                        elif command == 0:
                            print "no"
                            if pygame.mixer.music.get_busy() == True:
                                pygame.mixer.music.fadeout(1)
                            # no sound
                            pygame.mixer.music.load('/home/elka/music/no.mp3')
                            pygame.mixer.music.play()

                        break

    except KeyboardInterrupt:
        pygame.quit()
        PWM.stop(led0)
        PWM.stop(led1)
        PWM.cleanup()
        sys.exit()
    except Exception as inst:
        print type(inst)     # the exception instance
        print inst.args      # arguments stored in .args
        print "Unexpected error:", sys.exc_info()[0]

