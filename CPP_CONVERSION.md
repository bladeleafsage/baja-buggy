# C++ Motor Controller Conversion

This document describes the conversion from C to C++ of the motor controller application.

## Key Changes

### 1. Class-Based Architecture

**MotorController Class**
- Encapsulates GPIO motor control
- Stores pin configuration
- Methods: `init()`, `set_direction()`
- Type-safe direction enum

**ControlState Class**
- Manages shared control state thread-safely
- Encapsulates mutex and state struct
- Methods: `update()`, `get_direction()`
- Automatic cleanup via destructor

### 2. Type Safety Improvements

```cpp
enum class MotorDirection : int8_t {
    STOP = 0,
    FORWARD = 1,
    REVERSE = -1
};
```

- Uses `enum class` instead of magic numbers
- Compile-time type checking
- Self-documenting code

### 3. Modern C++ Features

- `constexpr` for compile-time constants instead of `#define`
- RAII pattern (Resource Acquisition Is Initialization) for mutex/semaphore cleanup
- Class constructors for initialization
- Type-safe enumerations

### 4. Maintained FreeRTOS Compatibility

- All FreeRTOS C APIs wrapped appropriately
- `extern "C"` for `app_main()` to work with ESP-IDF bootloader
- Pointers to classes instantiated in `app_main()`
- Static pointers for FreeRTOS task access

## Compilation

The ESP-IDF build system automatically detects `.cpp` files. You can:

1. Keep both `main.c` and `main.cpp` (only one will be built depending on CMakeLists.txt)
2. Replace `main.c` with `main.cpp` by updating `CMakeLists.txt`:
   ```cmake
   idf_component_register(SRCS "main.cpp" INCLUDE_DIRS ".")
   ```

## Object Layout

```
app_main()
  ├─ motor_controller (MotorController instance)
  │  └─ GPIO pins 32, 33
  ├─ control_state (ControlState instance)
  │  └─ Mutex (thread-safe)
  ├─ control_loop_task (FreeRTOS task)
  │  └─ Reads control_state, drives motor
  └─ serial_input_task (FreeRTOS task)
     └─ Updates control_state on input
```

## Benefits

1. **Type Safety** - Compiler catches more errors
2. **Encapsulation** - Clear ownership of resources
3. **Maintainability** - Easier to extend with more motors/features
4. **Memory Safety** - Classes manage lifetime of resources
5. **Code Clarity** - Self-documenting through strong typing

## Backwards Compatibility

The C++ version maintains identical behavior to the C version:
- Same GPIO pins and control logic
- Same thread priorities and timing
- Same input timeout (100ms)
- Same serial protocol (W/S/SPACE commands)
