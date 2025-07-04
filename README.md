# ESP32-S3 USB Composite Device

## Description

This device, based on the ESP32-S3 microcontroller, acts as a mass storage device (pendrive) and additionally simulates a USB keyboard. When the **BOOT** button on the board is pressed, the device sends the **Win + R** keyboard shortcut to Windows, opens the Run dialog, and executes a predefined command that searches all mounted drives for a `c.exe` file and runs it.

The device works only on Windows systems. Since it has no control over which drive letter is assigned, the program searches all drives to find and execute the specified file.

The `c.exe` program accepts the path where it is located on the device as its first argument.

## SD card connection

The ESP32-S3 board connects to the SD card using the SDMMC interface in 1-line mode, utilizing only the following lines: D0, CLK, and CMD. This simplifies wiring and requires fewer pins.

**ESP32-S3 pin configuration:**

```c
slot_config.clk = GPIO_NUM_36;
slot_config.cmd = GPIO_NUM_35;
slot_config.d0  = GPIO_NUM_37;
```

Connection diagram available in:

```
./assets/schema.png
```

## Setup and build

The device uses the **tinyUSB** library to simulate a composite device (MSC + HID). After building and flashing the program to the board, disconnect and reconnect it to the computer. A new external drive should appear.

On this drive, create a folder named `c` and place the `c.exe` file inside. This file will be launched when the **BOOT** button is pressed.

## Compiling the Python program to .exe

The repository includes an example Python script that changes the mouse cursor to a cat image and toggles it back to the default on subsequent runs.

To compile the Python program with images into a single executable, use the **PyInstaller** library.

### Example compilation command

```bash
cd test-program
pyinstaller --onefile --add-data "cursor.cur:." --add-data "pointer.cur:." --add-data "state:state" --name c.exe test-program.py
```

## Example operation

Screen recording demonstrating device operation (cursor replacement):

```
./assets/example.mp4
```

## Final notes

After proper configuration and file preparation, the device is ready for use. The `c.exe` program must be compiled and placed in the `c` folder on the drive to allow execution via the **BOOT** button.
