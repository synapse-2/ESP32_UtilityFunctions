# UtilityFunctions

A robust, thread-safe system utility library optimized for ESP32 and ESP32-S3 microcontrollers on arduino esp32 framework. This library provides foundational services like:
 1. Loggig
 2. Restart on button push
 3. On board led control
 4. An isolated thread-safe rolling **log buffer** featuring non-volatile flash memory persistence across system restarts
 5. Comprehensive runtime hardware diagnostic reporting
 6. **NON Bocking wait/delay**
 7. Software ESP32 restart

---

## Key Features

* **Thread-Safe RGB LED Signals**: Control built-in addressable status lights (e.g., WS2812B) using color-coded profiles and asynchronous blinking patterns protected by FreeRTOS Mutex Semaphores.
* **Persistent Web Log Buffer**: An inline, auto-popping character buffer (`AutoPopCharBuffer`) that caps memory allocation while acting as a live diagnostic log window.
* **NVS Crash/Restart Protection**: Automatically flushes the diagnostic string window to the ESP32's Non-Volatile Storage (NVS via `Preferences`) before software restarts, allowing crash logs to survive reboot cycles.
* **Non-Blocking Execution Delays**: Implements safe, task-yielding busy delays leveraging `vTaskDelay` to avoid starving lower-priority FreeRTOS processing loops or triggering the hardware Watchdog Timer (WDT).
* **Hardware Debouncing & Matrix Control**: State tracking for critical physical interfaces like the `BOOT` button, maintaining press metrics and millisecond timestamps.
* **Environment Diagnostics**: Quick-access reporting string engines for active tasks, hardware partitions, silicon revision specs, and `LEDC` configurations.

---

## Directory Structure

```text
UtilityFunctions/
├── src/
│   ├── UtilityFunctions.h        # Configuration definitions and public API namespace
│   └── UtilityFunctions.cpp      # Implementation (rolling circular buffer, NVS access, RTOS locks)
└── README.md                     # Library documentation and usage guide
```

---

## Installation

Add the folling lines to Platformio.ini, look at exmaple/simple workspace it has a fully working example 

```text

lib_deps =
	fastled/FastLED @ ^3.10.1
	https://github.com/synapse-2/ESP32_magic_enum.git
	tzapu/WiFiManager @ ^2.0.17
	arduino-libraries/Arduino_DebugUtils @ ^1.4.0
	https://github.com/synapse-2/ESP32_UtilityFunctions.git#1.0.0

board_build.partitions = partitions_NVM_PHY_OTA_16M.csv

```

And note build flags 

```text
build_unflags = -std=gnu++11 -std=gnu++2b -std=gnu++2a
build_flags = 
	-std=gnu++23 
	-MMD 
	-c 
	-g 
	-Og
	-D BOARD_HAS_PSRAM
	-D USE_ESP_IDF_LOG
	-D CCACHE_ENABLE=ON
  	-D WM_NOHELP						;do not show wifi manager help on the info page

```

If you want to fork the lib and do LIB dev then note the Platformio.ini file for the lib test build this is provided in the main Workspace file 

```text
src_dir = examples/LIB_bild_test/src 

lib_deps = 
	https://github.com/synapse-2/ESP32_magic_enum.git
	fastled/FastLED @ ^3.10.1
	tzapu/WiFiManager @ ^2.0.17
	arduino-libraries/Arduino_DebugUtils @ ^1.4.0
	symlink://.	
  
```
---

## Configuration Flags

You can customize the underlying pinouts, buffer boundaries, and performance configurations by modifying these preprocessor tokens globally inside your environment or directly before inclusion:

| Preprocessor Macro | Default Value | Description |
| :--- | :--- | :--- |
| `ResetButton` | `GPIO_NUM_0` | Hardware pin mapping for tracking system reset or configuration input loops. |
| `LED_BUILTINIO` | `GPIO_NUM_48` | Targeted RGB pixel hardware pin out data pipeline line. |
| `RGBCHIP` | `WS2812B` | Driver standard used by FastLED to interact with the target indicator matrix. |
| `NUMPIXELS` | `1` | Total structural count of addressable status pixels attached inline. |
| `LED_MUTEX_WAIT_MS` | `1000` | Block-time cutoff configuration threshold for cross-thread layout ownership. |
| `WEB_STATUS_LOG_BUFFER` | `8192` | Absolute window limit size in bytes reserved for live runtime diagnostics logging. |
| `NVRAM_PERFS` | `"registry"` | Identifier label designating the NVS namespace context used for storage. |

---

## API Reference

### System Core & Thread Orchestration
* `void UtilityFunctionsInit()`
  Initializes hardware configurations, setups up button debouncing, constructs synchronization mutex blocks, and triggers FastLED mappings.
* `void waitTillInitComplete()`
  Locks execution in a safe, non-blocking polling sequence until internal library structures complete activation.
* `void delay(long waitMills)`
  Executes a non-blocking delay loop using task slices to remain cooperative with the FreeRTOS processing scheduler.

### Safe Matrix Signaling Indicators
* `void ledRed()`, `void ledGreen()`, `void ledYellow()`, `void ledBlue()`, `void ledWhite()`, `void ledBrown()`, `void ledStop()`
  Changes color profiles safely across threads via Mutex locking mechanisms.
* `void ledBlinkGreen()`, `void ledBlinkBlue()`, `void ledBlinkRed()`, `void ledBlinkYellow()`
  Triggers a momentary visual interrupt sequence, blinking the pixel profile before restoring the prior state thread footprint.
* `void ledBlinkGreenLong()`, `void ledBlinkRedLong()`
  Triggers prolonged flash alert sequences representing deeper state transformations.

### Persistent Logging Framework
* `void debugLog(String message)`
  Appends information strings immediately onto your local trace stream and live text logging arrays.
* `int webLogPrintf(const char *format, va_list args)`
  Acts as an input hook allowing format parsing to pipe string streams directly into your operational window buffer.
* `String webLog()`
  Exposes active historical tracking metrics inside your window buffer layout structures.
* `String getPreBootWebLog()`
  Exposes historical execution text retrieved out of non-volatile sector blocks generated prior to the most recent reset flag hook.
* `void ESP32Restart()`
  Gracefully flushes the current debug history records down to flash blocks before executing a standard hard system reboot.

### Advanced Inspection Maps
* `String chipInfo()`
  Extracts core architectural indicators including silicon revisions, internal clock limits, and core footprints.
* `String taskInfo()`
  Runs standard runtime state dumps parsing task priority tiers, execution allocations, and memory safety margins.
* `String partitionInfo()`
  Returns the physical partitioning profile layout mapped across the embedded storage memory.

---

## Comprehensive Integration Example

Below is a typical framework configuration implementing cross-boot error tracking and manual flash restoration checking using your button arrays:

```cpp
#include "UtilityFunctions.h"

void setup() {
    Serial.begin(115200);
    
    // Initialize standard registers, locks, and pixel interfaces
    UtilityFunctions::UtilityFunctionsInit();
    
    // Retrieve tracking traces written immediately prior to the previous software restart event
    String fallbackHistory = UtilityFunctions::getPreBootWebLog();
    if (fallbackHistory.length() > 0) {
        Serial.println("[NVS Recovery] Previous execution records located:");
        Serial.println(fallbackHistory);
    }
    
    UtilityFunctions::debugLog("Operational loop initialized safely.");
}

void loop() {
    // Process input states from your mapped physical button structures safely
    if (UtilityFunctions::isResetPressed()) {
        UtilityFunctions::ledBlinkRedLong();
        UtilityFunctions::debugLog("[System Warning] Manual hardware trigger detected! Flushing tracking windows...");
        
        UtilityFunctions::unpressRest();
        
        // Commits live diagnostic frames down to NVS allocations and commands a system restart
        UtilityFunctions::ESP32Restart();
    }

    // Standard baseline operation heartbeat signaling
    UtilityFunctions::ledBlinkGreen();
    UtilityFunctions::delay(5000); 
}
```
