# The Jive Stick

This repo contains the firmware for The Jive Stick. The required functionality is:

```
[X] Set-up the repo with GIT and compnents structure
[X] Create an event handler
[X] Create a button handler
[/] Serial Input
    [ ] Serial Input component
    [ ] Handle passing in the time
    [ ] Update the flash script to pass in the current time
[X] Partition to allow OTA and Audio Files
[X] Saving Audio Files
[ ] Real Time Clock handler
[ ] Sleep management
[ ] Audio Playing
[ ] Bluetooth Settings
```

# Setup

## Start Of Day Setup

```
gotoEspV5
rm -rf jive-stick-firmware
git clone git@github.com:josh-buildingyouridea/jive-stick-firmware.git
cd jive-stick-firmare
idf.py build flash monitor


cd jive_stick
code .
. ../esp-idf-v5/export.sh
echo $IDF_PATH
idf.py build flash monitor
```

- Set the target: `idf.py set-target esp32c6`
- Set the port: `/dev/tty.usbmodem[########]`

## Initial Repo Set-up

- Create a new project: `idf.py create-project jive_stick`
- Copy in .vscode files below
- Create git: `git init`
- Start this readme: `touch README.md`

### IDE

- v5.5 is being used since 6.0 is still in beta and I don't want to run into any issues
- File structure should be:

```
v5
├── esp-idf-v5
└── jive_stick
```

# Features

## Button Input

Created a button interrupts on each pin. It then triggers a button event to a local handler on release (short press) or when the long press timer is reached (long press). The local button press handler can then call the main handler as needed.

## Neopixel

Add depencency:

```
idf.py add-dependency "espressif/led_strip"
idf.py fullclean
idf.py build
```

## Partitions

1. Create a partitions.csv file with 2x OTA slots and a storage slot.
2. Menuconfig → Component config → Partition Table → Custom partition table CSV
3. Set flash to 8MB: Menuconfig → Serial flasher config → Flash size → 8 MB

- 1.5MB for main code
- 1.5MB for OTA. Since there is no factory, the rollback will be to the last stable firmware
- ~5MB (Remaining) space will be used for storage of Audio files.

## Audio

### Creating a littlefs image and flashing to the device:

Add the littlefs managed component:

```
idf.py add-dependency joltwallet/littlefs==1.20.4
idf.py fullclean
```

Create [/audio](audio) folder and place the audio files in there.  
Add `littlefs_create_partition_image(storage ../audio FLASH_IN_PROJECT)` to CMakeLists.txt in main to flash the audio files to the partition.
NOTE: Comment out the CMakeLists.txt file to make flashing quicker once those files are on the device.

### Creating 2min audio files:

Downloads:

- [Fur Elise](https://archive.org/download/WoO59PocoMotoBagatelleInAMinorFurElise)
- [Beethoven - Symphony No. 5 in C Minor](https://freesound.org/people/GregorQuendel/sounds/719388/)
- [The Young Rascals - Groovin](https://www.youtube.com/watch?v=4JIq8Zn0AJE)
- [Old Time Rock and Roll - Bob Seger ](https://www.youtube.com/watch?v=W1LsRShUPtY)
- For the last 2 I used [tubeRipper](https://tuberipper.net/73/) These will need to be replaced before production.

Download the MP3
Convert to 2 minutes, mono, 16kHz, (WAV /codec stuff)

```zsh
ffmpeg -ss 00:00:01 -i FrEliseWoo59.mp3 \
  -t 120 -ac 1 -ar 16000 \
  -c:a adpcm_ima_wav -f wav \
  FrEliseWoo59_120s_16k_adpcm_01.wav
```

### Creating Custom Audio Files

say -v "Samantha (Enhanced)" "I need help. Please help" -o help.aiff

```zsh
ffmpeg -i help.aiff \
  -af "volume=-6dB" -ac 1 -ar 16000 \
  -c:a adpcm_ima_wav -f wav \
  help_16k_adpcm_6db.wav
```

## Debug/Set-Up Input

In order to send the current timestamp, the device needs to be able to listen to serial inputs and handle them. This is also used for debugging during development.

Map console output:  
idf.py menuconfig → Component config → ESP System Settings → Channel for console output → USB Serial/JTAG Controller

## Time

- This system uses an external RTC (PCF8523)
  - This stores the UNIX time as a structure with Binary Coded Decimal notation
  - The MSB of the seconds register has an error bit to say if this has been corrputed
- The RTC time is converted to unix seconds and saved as the system time with `settimeofday`
- The system timezone is stored with:

```C
setenv("TZ", tz, 1);
tzset();
```

- The local time string is shown with `time()` to get the unix then `ctime` for the converted string
- The local time structure is called with `localtime_r()`
- The initial RTC time can be set by sending: `T:1770921313` (use actual current unix seconds)
- Timezone can be set with `L:EST5EDT,M3.2.0/2,M11.1.0/2`

## BLE/Serial Commanmds

### System Time

Reading:

- Serial: `t`
- BLE:

Writing:

- Serial: `T:[unix seconds]`
- BLE:

### Location (Timezone)

Reading:

- Serial: `l`
- BLE:

Writing:

- Serial: `L:EST5EDT,M3.2.0/2,M11.1.0/2`
- BLE:

## BLE

- idf.py menuconfig → Component config → Bluetooth → Enable
  - Host → NimBLE - BLE only
  - Enable BLE (Not needed on C6?)
