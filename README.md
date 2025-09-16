# Blindman_location_buzzer_ultrasonic_ir

## Project Overview

This project is a smart assistive device for visually impaired individuals. It uses an ultrasonic sensor and infrared (IR) sensors to detect obstacles and provides auditory feedback via a buzzer. The system helps users navigate their environment more safely by alerting them to nearby obstacles.

## Features

- **Obstacle Detection:** Uses ultrasonic and IR sensors to detect objects in the user's path.
- **Audio Feedback:** Emits buzzer sounds to alert the user to detected obstacles.
- **Easy to Build:** Designed for use with Arduino-compatible boards.

## Pin Connections

| Component          | Arduino Pin   |
|--------------------|--------------|
| Ultrasonic Trigger | D2           |
| Ultrasonic Echo    | D3           |
| IR Sensor 1        | D4           |
| IR Sensor 2        | D5           |
| Buzzer             | D6           |
| VCC                | 5V           |
| GND                | GND          |

*(Update pin numbers if your code is different.)*

## What You'll Need

- 1 x Arduino board (Uno, Nano, etc.)
- 1 x Ultrasonic distance sensor (e.g., HC-SR04)
- 2 x IR obstacle avoidance sensors
- 1 x Piezo buzzer
- Jumper wires
- Breadboard or PCB

## How It Works

1. **Power on the device.**
2. The ultrasonic sensor continuously measures the distance to obstacles ahead.
3. The IR sensors detect objects to the left and right sides.
4. When an obstacle is detected within a certain range, the buzzer sounds to warn the user.
5. The frequency or pattern of the buzzer may change based on obstacle proximity or sensor location.

## Setup & Installation

1. **Download the Arduino IDE:**  
   [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)

2. **Download the Code:**  
   - Click the green "Code" button in this repository and select "Download ZIP", or clone with Git:  
     ```
     git clone https://github.com/godragun/Blindman_location_buzzer_ultrasonic_ir.git
     ```

3. **Connect the Hardware:**  
   - Wire the sensors and buzzer according to the pin table above.

4. **Upload the Code:**  
   - Open the `.ino` file in Arduino IDE.
   - Select your board and port under "Tools".
   - Click "Upload".

## Usage

- Power the Arduino with USB or battery.
- The device will start detecting obstacles and alert the user with buzzer sounds.

## License

This project is open source. See [LICENSE](LICENSE) for details.
