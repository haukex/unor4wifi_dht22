Arduino Uno R4 WiFi with DHT22 Sensor
=====================================

This project is a test of using the Arduino Uno R4 WiFi to serve DHT22 sensor data
over a simple website based on some modern web tools like TypeScript and Parcel.js.

<https://github.com/haukex/unor4wifi_dht22>

The firmware will read out the DHT22 and report its values on the Arduino's LED matrix
as well as the USB serial port (note a WiFi configuration is required). WiFi can be
configured either as an Access Point or a client by sending the command ``RECONF``
over the USB serial port to the Arduino; configuration is stored in EEPROM.


Development Environment
-----------------------

I developed this on Windows (Git Bash and VSCode), but I think it should work with other IDEs, command line only, and on Linux as well.

### Arduino

- Download the latest Arduino CLI (64 bit Windows exe) from <https://arduino.github.io/arduino-cli/latest/installation/#download>
  - Unpack and place `arduino-cli.exe` into a dir in `PATH`, e.g. `~/bin`
  - If needed: `arduino-cli config init` and edit the generated config file to include the line `locale: en_US`
  - `arduino-cli core update-index`
  - `arduino-cli core install arduino:renesas_uno`

### Web Page

The following in the `webpage` subdir:

- Setting up dev env:
  - Set up Node and Typescript as per
    <https://github.com/haukex/toolshed/blob/eef5bb57/notes/JavaScript.md#nodejs-and-typescript>
    (or later versions as appropriate)
  - `npm install` (uses `package.json`)
- Can run dev server via `npm start`

### Building

Just run `./build.sh`, or look into that file for the individual build steps.
To just run the serial port monitor, use `./ardu.py monitor`.


Author, Copyright, and License
------------------------------

This work © 2023 by Hauke Dämpfling (haukex@zero-g.net) is licensed under Attribution-ShareAlike 4.0
International. To view a copy of this license, visit <http://creativecommons.org/licenses/by-sa/4.0/>
