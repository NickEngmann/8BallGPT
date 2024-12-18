# Magic (GPT)8 Ball
🎱

## Overview

The Magic (GPT)8 Ball is an interactive project that simulates the classic Magic 8 Ball toy. It uses an accelerometer and gyroscope to detect shaking, displays responses on a screen, and integrates with ChatGPT for dynamic responses. The project is built using an ESP32 microcontroller and various sensors and peripherals.

## Features

- Shake detection using accelerometer and gyroscope
- Display responses on a screen
- Integration with ChatGPT for dynamic responses
- PDM microphone input
- Vibrational feedback

## Hardware Requirements

- ESP32S3 Waveshare 1.28 LCD Display + Microcontroller
- Adafruit PDM Microphone Breakout - SPH0645LM4H
- Other necessary components (e.g., wires)
- Vibration Mini Motor Disk

## Software Requirements

- PlatformIO IDE
- Arduino framework
- LVGL library for display handling
- ChatGPT API for dynamic responses

## Getting Started

### Hardware Setup
1. Connect the PDM to the ESP32 analog input pins
2. Connect the Vibrational Motor Disks to the Correct GPIO pins.
3. Ensure all connections are secure and powered correctly.

### Software Setup

1. Clone the repository:

   ```sh
   git clone github.com/NickEngmann/MagicGPT8Ball.git
   cd MagicGPT8Ball
    ```

2. Open the project in PlatformIO IDE. (Visual Studio Code with PlatformIO extension)
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
- [ ] PDM Microphone working
- [ ] ChatGPT integration for responses
- [ ] Clean Up Displayed Text
- [ ] Better Animations (Nice to have)
- [ ] Vibrational Feedback (Nice to have)

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
