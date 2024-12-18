# Magic (GPT)8 Ball

🎱

## Overview

The Magic (GPT)8 Ball is an interactive project that simulates the classic Magic 8 Ball toy. It uses an accelerometer and gyroscope to detect shaking, displays responses on a screen, and integrates with ChatGPT for dynamic responses. The project is built using an ESP32 microcontroller and various sensors and peripherals.

## Features

- Shake detection using accelerometer and gyroscope
- Display responses on a screen
- Integration with ChatGPT for dynamic responses
- Analog microphone input using Electret Microphone Amplifier - MAX9814

## Hardware Requirements

- ESP32 microcontroller
- Accelerometer and gyroscope sensor (e.g., LSM6DS3TR-C)
- Display (e.g., TFT LCD)
- Electret Microphone Amplifier - MAX9814
- Other necessary components (e.g., resistors, capacitors, wires)

## Software Requirements

- PlatformIO IDE
- Arduino framework
- LVGL library for display handling
- ChatGPT API for dynamic responses

## Getting Started

### Hardware Setup

1. Connect the accelerometer and gyroscope sensor to the ESP32.
2. Connect the display to the ESP32.
3. Connect the Electret Microphone Amplifier - MAX9814 to the ESP32 analog input pin.
4. Ensure all connections are secure and powered correctly.

### Software Setup

1. Clone the repository:

   ```sh
   git clone <repository-url>
   cd <repository-directory>
    ```

2. Open the project in PlatformIO IDE.
3. Install the necessary libraries:

`pio lib install`

4. Configure the `platformio.ini` file with your board and environment settings.
5. Build and upload the firmware to the ESP32:

   ```sh
   pio run --target upload
   ```

## Usage

1. Power on the ESP32.
2. Shake the device to trigger the Magic 8 Ball response.
3. View the response on the display.
4. Use the ChatGPT integration for dynamic responses.

## To-Do List

- [ ] Move the animation depending on the gyroscope
- [ ] PDM Microphone working
- [ ] ChatGPT integration
- [ ] Decrease the text size by 10% or make it dynamic

## Contributing

1. Fork the repository.
2. Create a new branch (`git checkout -b feature-branch`).
3. Make your changes.
4. Commit your changes (`git commit -am 'Add new feature'`).
5. Push to the branch (`git push origin feature-branch`).
6. Create a new Pull Request.

## License

This project is licensed under the MIT License - see the

LICENSE

 file for details.

## Acknowledgements

- [LVGL](https://lvgl.io/) for the display library
- [PlatformIO](https://platformio.org/) for the development environment
```