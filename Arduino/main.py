#Example of playing an .mp3 file with Python4Android

import android

BT_DEVICE_ID = '20:14:08:27:05:27'

LAST_SONG = 7
currentSong = 0
paused = False

droid = android.Android() #initiates module

droid.toggleBluetoothState(True)
"""The first parameter is the service UUID for SerialPortServiceClass.
The second one is the address of your bluetooth module.
If the second one is ommited, Android shows you a selection at program start.
When this function succeeds the little led on the bluetooth module should stop blinking.
"""
droid.bluetoothConnect('00001101-0000-1000-8000-00805F9B34FB', BT_DEVICE_ID)

while True:
    command = droid.bluetoothReadLine().result  # read the line with the sensor value from arduino.
    print(command)
    droid.eventClearBuffer()  # workaround for a bug in SL4A r4.
    
    if command == '0' or command == '4':
        # play next music
        droid.mediaPlay('/storage/emulated/0/DedMoroz/' + str(currentSong) + '.mp3')
        currentSong += 1
        if LAST_SONG < currentSong:
            currentSong = 0
    elif command == '5':
        # stop everything
        currentSong = 0
        droid.mediaPlayClose()
    elif command == '1':
        if not paused:
            # pause music
            paused = True
            print("pause")
            droid.mediaPlayPause()
        else:
            # resume playing
            paused = False
            print("resume")
            droid.mediaPlayStart()
    if command == '2':
        # yes sound
        droid.mediaPlay('/storage/emulated/0/DedMoroz/yes.mp3')
    if command == '3':
        # yes sound
        droid.mediaPlay('/storage/emulated/0/DedMoroz/no.mp3')