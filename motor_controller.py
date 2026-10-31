#!/usr/bin/env python3
"""
Motor Controller - Keyboard to Serial Bridge
Captures keyboard input and sends commands to ESP32 immediately.
"""

import sys
import time
import serial
import logging
from pathlib import Path
from datetime import datetime
from typing import Optional

try:
    import pynput
    from pynput.keyboard import Listener, Key
except ImportError:
    print("Error: pynput not installed. Install with: pip install pynput pyserial")
    sys.exit(1)


class MotorController:
    def __init__(self, port: str = "COM3", baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.ser: Optional[serial.Serial] = None
        self.running = True
        self.listener = None
        self.rx_buffer = ""

        # Setup logging
        self.setup_logging()

    def setup_logging(self):
        """Setup timestamped logging to runs directory."""
        runs_dir = Path("runs")
        runs_dir.mkdir(exist_ok=True)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_file = runs_dir / f"motor_control_{timestamp}.log"

        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(message)s',
            datefmt='%H:%M:%S',
            handlers=[
                logging.FileHandler(log_file)
            ]
        )
        self.logger = logging.getLogger(__name__)
        self.logger.info("=" * 60)
        self.logger.info("Motor Control Log Started")
        self.logger.info("=" * 60)
        self.logger.info(f"Port: {self.port}, Baudrate: {self.baudrate}")
        self.logger.info(f"Log file: {log_file}")
        self.logger.info("")

    def connect(self) -> bool:
        """Establish serial connection."""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            self.logger.info(f"✓ Connected to {self.port} at {self.baudrate} baud")
            time.sleep(0.5)
            return True
        except serial.SerialException as e:
            self.logger.error(f"✗ Failed to connect: {e}")
            return False

    def disconnect(self):
        """Close serial connection."""
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.logger.info("Disconnected from serial port")

    def on_press(self, key):
        """Callback when a key is pressed - immediately send to serial."""
        try:
            if hasattr(key, 'char') and key.char:
                char = key.char
                if char in ['w', 'W', 's', 'S', ' ']:
                    if self.ser and self.ser.is_open:
                        self.ser.write(char.encode())
                        self.ser.flush()
                        cmd_name = {'w': 'FORWARD', 'W': 'FORWARD',
                                   's': 'REVERSE', 'S': 'REVERSE',
                                   ' ': 'STOP'}.get(char, 'UNKNOWN')
                        self.logger.info(f"[TX] {cmd_name:8s} ('{char}')")
        except AttributeError:
            pass

    def on_release(self, key):
        """Callback when a key is released."""
        try:
            if key == Key.esc:
                self.running = False
                return False
        except AttributeError:
            pass

    def process_serial_response(self):
        """Read and process data from ESP32."""
        if self.ser and self.ser.is_open and self.ser.in_waiting:
            response = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
            if response:
                sys.stdout.write(response)
                sys.stdout.flush()

                self.rx_buffer += response

                # Check if we have a complete status block
                if "==================" in self.rx_buffer:
                    lines = self.rx_buffer.split('\n')
                    status_block = []
                    for line in lines:
                        if line.strip():
                            status_block.append(line)
                        if "==================" in line:
                            break

                    if status_block:
                        self.logger.info("[ESP32 Status]")
                        for line in status_block:
                            self.logger.info(f"  {line}")
                        self.logger.info("")

                    # Clear the buffer of processed data
                    remaining_lines = lines[len(status_block):]
                    self.rx_buffer = '\n'.join(remaining_lines)
                else:
                    # Log other output immediately
                    for line in response.strip().split('\n'):
                        if line and not any(x in line for x in ['Wake count:', '===']):
                            self.logger.info(f"[ESP32] {line}")

    def run(self):
        """Start the motor controller."""
        if not self.connect():
            return

        print("\n=== Motor Controller ===")
        print("Controls:")
        print("  W - Forward")
        print("  S - Reverse")
        print("  SPACE - Stop")
        print("  ESC - Exit")
        print("========================\n")

        self.logger.info("Controls: W=FORWARD, S=REVERSE, SPACE=STOP, ESC=EXIT")
        self.logger.info("")

        self.listener = Listener(on_press=self.on_press, on_release=self.on_release)
        self.listener.start()

        try:
            while self.running:
                self.process_serial_response()
                time.sleep(0.01)
        except KeyboardInterrupt:
            self.logger.info("Shutdown requested (KeyboardInterrupt)")
        finally:
            self.running = False
            if self.listener:
                self.listener.stop()
            self.disconnect()
            self.logger.info("=" * 60)
            self.logger.info("Motor Control Log Ended")
            self.logger.info("=" * 60)


def main():
    """Main entry point."""
    port = "COM7"
    baudrate = 115200

    if len(sys.argv) > 1:
        port = sys.argv[1]
    if len(sys.argv) > 2:
        baudrate = int(sys.argv[2])

    controller = MotorController(port, baudrate)
    controller.run()


if __name__ == "__main__":
    main()

