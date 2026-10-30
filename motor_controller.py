#!/usr/bin/env python3
"""
Motor Controller - Keyboard to Serial Bridge
Captures keyboard input and streams commands to ESP32 at high frequency.
"""

import sys
import time
import serial
import threading
from queue import Queue
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
        self.input_queue: Queue = Queue(maxsize=100)
        self.running = True
        self.listener = None

    def connect(self) -> bool:
        """Establish serial connection."""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            print(f"✓ Connected to {self.port} at {self.baudrate} baud")
            time.sleep(0.5)
            return True
        except serial.SerialException as e:
            print(f"✗ Failed to connect: {e}")
            return False

    def disconnect(self):
        """Close serial connection."""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("Disconnected from serial port")

    def on_press(self, key):
        """Callback when a key is pressed."""
        try:
            if hasattr(key, 'char') and key.char:
                char = key.char
                print(char)
                if char in ['w', 'W', 's', 'S', ' ']:
                    self.input_queue.put(char)
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

    def serial_writer_thread(self):
        """Thread that writes queued input to serial port."""
        while self.running:
            try:
                if not self.input_queue.empty():
                    char = self.input_queue.get_nowait()
                    if self.ser and self.ser.is_open:
                        self.ser.write(char.encode())
                        self.ser.flush()

                if self.ser and self.ser.is_open and self.ser.in_waiting:
                    response = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
                    if response:
                        sys.stdout.write(response)
                        sys.stdout.flush()

                time.sleep(0.001)
            except serial.SerialException as e:
                print(f"Serial error: {e}")
                break
            except Exception as e:
                print(f"Error: {e}")

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

        writer_thread = threading.Thread(target=self.serial_writer_thread, daemon=True)
        writer_thread.start()

        self.listener = Listener(on_press=self.on_press, on_release=self.on_release)
        self.listener.start()

        try:
            while self.running:
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\nShutdown requested")
        finally:
            self.running = False
            if self.listener:
                self.listener.stop()
            self.disconnect()


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
