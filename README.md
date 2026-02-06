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
[ ] Saving Audio Files
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

Add `littlefs_create_partition_image(storage ../audio FLASH_IN_PROJECT)` to CMakeLists.txt in main to flash the audio files to the partition
Create /audio folder and place the audio files in there.

NOTE: Comment out the CMakeLists.txt file to make flashing quicker once those files are on the device.

### Creating 2min audio files:

https://archive.org/download/WoO59PocoMotoBagatelleInAMinorFurElise

Download the MP3
Convert to 2 minutes, mono, 16kHz, (WAV /codec stuff)
`ffmpeg -i FrEliseWoo59.mp3 -t 120 -ac 1 -ar 16000 -c:a adpcm_ima_wav FrElise_120s_16k_adpcm.wav`

## Debug/Set-Up Input

In order to send the current timestamp, the device needs to be able to listen to serial inputs and handle them. This is also used for debugging during development.

Map console output:  
idf.py menuconfig → Component config → ESP System Settings → Channel for console output → USB Serial/JTAG Controller

# OLD STUFF

- Install ESP-IDF >= V5.4 and install tools
- Pull in the project `git clone git@github.com:glamcor/hosted_simple.git`
- Open in VS Code: `code hosted_simple`
- Update the setting.json as needed. This is my code:
  - Note: Intellisence may need: Cmd+Shift+P -> "CMake: Reset CMake Tools Extension State" or restarting VS Code
  - Note: You can keep the idf.pythonInstallPath that was created on VS Code open
  - Note: The esp-idf path may be different

```
{
  "idf.pythonInstallPath": "/opt/homebrew/opt/python@3.11/Frameworks/Python.framework/Versions/3.11/bin/python3",
  "idf.espIdfPath": "/Volumes/Data_Int/_Documents/ESP/_CODE/v5/esp-idf-v5",
  "C_Cpp.intelliSenseEngine": "default",
  "C_Cpp.default.configurationProvider": "ms-vscode.cpptools",
  "[c]": {
    "editor.defaultFormatter": "ms-vscode.cpptools"
  },
  "[cpp]": {
    "editor.defaultFormatter": "ms-vscode.cpptools"
  },
  "editor.formatOnSave": true,
  "files.associations": {
    "crush_wifi.h": "c"
  }
}
```

- Open a new terminal in the VS Code Editor
- Activate the IDF: `source ../esp-idf-v5/export.sh`
  - Note: Your esp-idf folder may have a different name
- Set the target to esp32p4: `idf.py set-target esp32p4`
  - Note: This will also build the project
- idf.py flash monitor
  - Build, Flash and Monitor the code
  - To close out of monitor: Control+`]`

## Creating the project

- Create a new project: `idf.py create-project hosted_simple`
- Add the remove/hosted dependencies. These will be added to the main/idf_component.yml file

```
idf.py add-dependency "espressif/esp_wifi_remote"
idf.py add-dependency "espressif/esp_hosted"
```

### Git info

The `managed_components` and `build` folders are ignored from git becuse they are pulled in when compiled.

### Menuconfig

Here are some changes to menuconfig that are suggested but I didn't use them:  
https://github.com/espressif/esp-hosted-mcu/blob/main/docs/esp32_p4_function_ev_board.md#32-configuring-defaults
