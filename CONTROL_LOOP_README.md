# Motor Controller - 400Hz Control Loop

## Overview

This project implements a real-time 400Hz motor control loop on the ESP32 with high-frequency keyboard input capture via a Python serial bridge.

## Architecture

### Firmware (main.c)

The firmware runs two concurrent FreeRTOS tasks:

1. **Control Loop Task (400Hz)**
   - Runs at precisely 400Hz (2.5ms period)
   - Reads the control state register
   - Updates motor GPIO pins to match register state
   - Independent of input latency

2. **Serial Input Task**
   - Monitors serial port for keyboard commands
   - Updates control state register thread-safely using a mutex
   - Commands:
     - `W` / `w` - Forward (direction = +1)
     - `S` / `s` - Reverse (direction = -1)
     - `SPACE` - Stop (direction = 0)

### Control State Register

```c
typedef struct {
    int8_t direction;      // Motor direction: 1 (forward), -1 (reverse), 0 (stop)
    uint32_t timestamp_ms; // Timestamp of last update
} control_state_t;
```

Thread-safe access is protected by a binary semaphore (mutex).

## Python Utility (motor_controller.py)

The Python utility captures keyboard input in real-time and streams it to the ESP32 serial port.

### Features
- Real-time keyboard listening using `pynput`
- Non-blocking serial communication
- Echoes ESP32 responses for visual feedback
- ~1000Hz polling rate for responsive control

### Installation

```bash
pip install -r requirements.txt
```

### Usage

```bash
# Default: COM3 at 115200 baud
python motor_controller.py

# Custom port and/or baudrate
python motor_controller.py COM4 115200
```

### Controls
- **W** - Motor forward
- **S** - Motor reverse
- **SPACE** - Motor stop
- **ESC** - Exit program

## Data Flow

```
Keyboard Input
    ↓
Python motor_controller.py (listening at ~1000Hz)
    ↓
Serial Port (115200 baud)
    ↓
ESP32 Serial Input Task
    ↓
Control State Register (protected by mutex)
    ↓
ESP32 Control Loop Task (400Hz)
    ↓
Motor GPIO Control (MOTOR_IN1_PIN=32, MOTOR_IN2_PIN=33)
    ↓
L298N Motor Driver
    ↓
Motor
```

## Timing & Performance

- **Control Loop Frequency**: 400Hz (2.5ms period)
- **Python Poll Rate**: ~1000Hz (1ms), ensuring input latency < 3ms
- **Serial Baud Rate**: 115200 (max ~11,520 characters/sec)
- **Expected Input Latency**: < 5ms (one control loop period)

## Building & Flashing

```bash
# Build the project
idf.py build

# Flash to device (replace /dev/ttyUSB0 with your port)
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor
```

## Thread Safety

- Control state is protected by a binary semaphore
- `update_control_state()` and `get_control_direction()` are thread-safe
- Timeout on semaphore prevents control loop from blocking on serial tasks

## Future Enhancements

- Add PWM speed control (currently digital on/off)
- Add analog joystick input via Python
- Implement telemetry feedback from ESP32
- Add PID loop for speed control
- Multi-motor support
