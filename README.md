# Real-Time LED Controller

A sophisticated embedded real-time control system for high-brightness LED current regulation using a buck converter on the NXP FRDM-KL25Z development board.

![Platform](https://img.shields.io/badge/Platform-FRDM--KL25Z-blue)
![MCU](https://img.shields.io/badge/MCU-Cortex--M0+-green)
![RTOS](https://img.shields.io/badge/RTOS-CMSIS--RTX5-orange)
![License](https://img.shields.io/badge/License-MIT-yellow)

## 📋 Overview

This project implements a digital closed-loop control system for precise LED current regulation through a DC-DC buck converter. It features multiple control algorithms, real-time waveform visualization, touchscreen interface, and accelerometer-based parameter control.

### Key Features

- **Multiple Control Algorithms**: Open Loop, Bang-Bang, Incremental, Proportional, PID (floating & fixed-point)
- **Real-Time OS**: CMSIS-RTOS2 (RTX5) with 5 concurrent threads
- **Graphical Interface**: TFT LCD (ST7789/ILI9341) with touchscreen control
- **Waveform Display**: Real-time oscilloscope showing setpoint vs measured current
- **Accelerometer Integration**: Tilt-based flash parameter control (MMA8451)
- **Performance Profiling**: Built-in code profiling and timing analysis
- **Thread Synchronization**: Priority-based scheduling with mutex protection

## 🛠️ Hardware Requirements

### Development Board
- **NXP FRDM-KL25Z** (MKL25Z128xxx4)
  - ARM Cortex-M0+ @ 48 MHz
  - 128 KB Flash, 16 KB RAM

### Peripherals
- TFT LCD Display (240x320, ST7789 or ILI9341 controller)
- Resistive Touchscreen
- MMA8451 3-axis Accelerometer (I2C)
- Buck Converter Circuit (external)
- Current Sense Resistor (2.2Ω)
- High-Brightness LED

### Pin Connections
- **PWM Output**: PTE31 (TPM0_CH4) - Buck converter drive
- **DAC Output**: PTE30 - Current setpoint reference
- **ADC Input**: PTB0 (ADC0_SE8) - Current sensing
- **I2C**: MMA8451 Accelerometer
- **SPI**: LCD Display
- **Touch**: ADC channels for resistive touchscreen

## 📊 System Architecture

### Control Loop
```
DAC → Set Current → Buck Converter → LED → Current Sensor (2.2Ω) → ADC → Controller
                     ↑                                                      ↓
                     └────────────── PWM Duty Cycle ──────────────────────┘
```

### RTOS Thread Structure

| Thread | Priority | Period | Function |
|--------|----------|--------|----------|
| Update_Setpoint | High | 1 ms | Generate LED flash pattern |
| Read_Accelerometer | Above Normal | 50 ms | Read tilt, adjust parameters |
| Draw_Waveforms | Above Normal | 200 ms | Update scope display |
| Draw_UI_Controls | Normal | 210 ms | Refresh UI elements |
| Read_Touchscreen | Normal | 100 ms | Process touch input |

### Control Specifications
- **PWM Frequency**: ~32 kHz (configurable: 30-96 kHz)
- **ADC Resolution**: 16-bit
- **DAC Resolution**: 12-bit
- **Control Update Rate**: Synchronized with PWM (interrupt-driven)
- **Sample Buffer**: 960 samples for waveform display

## 🚀 Getting Started

### Prerequisites
- [Keil MDK](https://www.keil.com/download/) (µVision IDE)
- ARM Compiler 6 (included with Keil MDK)
- CMSIS Packs (installed via Pack Installer):
  - ARM::CMSIS 6.1.0
  - ARM::CMSIS-RTX 5.9.0
  - Keil::Kinetis_KLxx_DFP 1.15.1

### Building the Project

1. Clone the repository:
   ```bash
   git clone https://github.com/siddharthbc/Real-Time-LED-Controller.git
   cd Real-Time-LED-Controller
   ```

2. Open `Project_Base.uvprojx` in Keil µVision

3. Build the project:
   - Click **Project → Build Target** or press **F7**

4. Flash to FRDM-KL25Z:
   - Connect the board via USB
   - Click **Flash → Download** or press **F8**

### Configuration

Edit `Include/config.h` to customize:

```c
#define USE_LCD_MUTEX_LEVEL  1      // Mutex granularity (1-3)
#define USE_ADC_SERVER       1      // Enable ADC interrupt handling
#define READ_FULL_XYZ        1      // 16-bit accelerometer mode
```

Control mode selection in `Source/control.h`:
```c
#define DEF_CONTROL_MODE (PID_FX)   // OpenLoop, BangBang, Incremental, PID, PID_FX
```

## 🎮 Usage

### User Interface

The LCD displays:
- **Top Section**: Real-time waveform scope (setpoint in blue, measured in orange)
- **Control Panel**: Adjustable parameters
- **Bottom**: Slider for value adjustment

### Touchscreen Controls

1. **Tap a field** to select it (highlighted in red)
2. **Use slider** at bottom to adjust the selected value
3. **Available fields**:
   - Duty Cycle (read-only when controller enabled)
   - Enable Controller (on/off)
   - Flash Period (ms)
   - Flash Duration (ms)
   - Set Current (mA)
   - Peak Current (mA)
   - Measured Current (read-only)

### Accelerometer Control

Tilt the board to dynamically adjust LED flash period:
- **Roll angle** affects flash timing
- **Range**: 2-180 ms based on tilt

## 📁 Project Structure

```
Project_Base/
├── Include/              # Header files
│   ├── config.h         # Main configuration
│   ├── control.h        # Control system definitions
│   ├── threads.h        # RTOS thread definitions
│   └── ...
├── Source/              # Source files
│   ├── main.c          # Initialization & startup
│   ├── control.c       # PID algorithms, ADC/DAC
│   ├── threads.c       # RTOS thread implementations
│   ├── UI.c            # User interface logic
│   ├── MMA8451.c       # Accelerometer driver
│   ├── I2C.c           # I2C communication
│   ├── LCD/            # Display drivers
│   └── Profiler/       # Performance profiling
├── RTE/                # CMSIS Runtime Environment
├── Scripts/            # Build scripts
└── Project_Base.uvprojx # Keil project file
```

## 🔧 Control Algorithms

### 1. Open Loop
Manual duty cycle control without feedback.

### 2. Bang-Bang
Binary control: full on when below setpoint, off when above.

### 3. Incremental
Step-wise duty cycle adjustments based on error.

### 4. Proportional
Duty cycle proportional to current error.

### 5. PID (Floating-Point)
Full PID control with configurable gains:
```c
P_GAIN = 0.006 * CTL_PERIOD
I_GAIN = 0.000 * CTL_PERIOD
D_GAIN = 0.000 * CTL_PERIOD
```

### 6. PID_FX (Fixed-Point) ⭐ Default
Optimized fixed-point implementation for faster execution:
```c
P_GAIN = 87.5 * CTL_PERIOD
I_GAIN = 0.625 * CTL_PERIOD
D_GAIN = 0.0 * CTL_PERIOD
```

## 📈 Performance Profiling

Enable profiling to analyze code execution:

1. Built-in PIT-based PC sampling
2. Thread-specific profiling
3. Code region tracking
4. Optional LCD display of results

## 🐛 Debugging

### Debug Signals
GPIO pins toggle to visualize thread execution timing (connect oscilloscope):
- Can measure thread execution time
- Identify blocking/mutex contention
- Verify real-time performance

### Error Codes
If initialization fails, the onboard RGB LED flashes red with error codes:
- **2 flashes**: LCD font bitmaps not found
- **3 flashes**: Accelerometer initialization failed

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📝 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 👨‍💻 Author

**Siddharth BC**
- GitHub: [@siddharthbc](https://github.com/siddharthbc)

## 🎓 Acknowledgments

- Developed for ECE 4/560 Embedded Systems coursework
- Based on NXP FRDM-KL25Z reference designs
- Uses ARM CMSIS-RTOS2 framework

## 📚 References

- [FRDM-KL25Z User's Guide](https://www.nxp.com/design/development-boards/freedom-development-boards/mcu-boards/freedom-development-platform-for-kinetis-kl14-kl15-kl24-kl25-mcus:FRDM-KL25Z)
- [CMSIS-RTOS2 Documentation](https://arm-software.github.io/CMSIS_5/RTOS2/html/index.html)
- [MMA8451Q Datasheet](https://www.nxp.com/docs/en/data-sheet/MMA8451Q.pdf)
- NXP AN3461: Tilt Sensing Using Linear Accelerometers

---

**⭐ If you find this project useful, please consider giving it a star!**
