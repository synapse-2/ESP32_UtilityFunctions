# UtilityFunctions

**Latest release: `3.0.0`**

A robust, thread-safe system utility library optimized for ESP32 and ESP32-S3 microcontrollers on arduino esp32 framework. This library provides foundational services like:
 1. Loggig
 2. Restart on button push
 3. On board led control
 4. An isolated thread-safe rolling **log buffer** featuring non-volatile flash memory persistence across system restarts
 5. Comprehensive runtime hardware diagnostic reporting
 6. **NON Bocking wait/delay**
 7. Software ESP32 restart
 8. Turn on and off wach dog timers jobs on each core
 9. Enable NTP server time sync for lwip based programs

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
  https://github.com/synapse-2/ESP32_UtilityFunctions.git#3.0.0

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
* `bool isMaster()`
  Reports whether the library is operating in master mode.
* `void waitTillInitComplete()`
  Locks execution in a safe, non-blocking polling sequence until internal library structures complete activation.
* `void delay(long waitMills)`
  Executes a non-blocking delay loop using task slices to remain cooperative with the FreeRTOS processing scheduler.
* `bool disableTWDTimeronIdleTaskOnCore(int xCoreID)` / `bool enableTWDTimeronIdleTaskOnCore(int xCoreID)`
  Disables or enables the task watchdog timer for the idle task on the selected CPU core. Pass the ESP32 core ID.

### Wi-Fi & Network Time
The Wi-Fi functions are available when `CONFIG_ESP_WIFI_ENABLED` is enabled. The NTP functions are available when `CONFIG_LWIP_IPV4` or `CONFIG_LWIP_IPV6` is enabled.

* `String getSSID()` / `String getPSK()`
  Returns the Wi-Fi credentials currently stored by WiFiManager.
* `void setupWiFiAndConnect()`
  Connects using saved Wi-Fi settings or opens the WiFiManager configuration portal. The device restarts if the portal times out without a connection.
* `void rebootIfWiFiDisconnected()`
  Tracks a Wi-Fi outage and restarts the device after `WIFI_DISCONNET_TIMEOUT_SEC` seconds without a connection. Call it periodically from the application.
* `bool enableNTPTimeServer(String server)`
  Configures the specified NTP server before connecting to Wi-Fi.
* `bool isNTPTimeSynced()`
  Reports whether the NTP clock synchronization has completed.

### Reset Button & Hardware State
* `bool isResetPressed()`
  Returns the debounced BOOT/reset button state.
* `int numTimesResetPressed()`
  Returns the number of button presses recorded by the reset-button handler.
* `unsigned long resetMills()`
  Returns the timestamp, in milliseconds, recorded for the most recent reset-button event.
* `void unpressRest()`
  Clears the current reset-button pressed state and press count.
* `void checkResetPressed()`
  Processes the reset-button state and performs the configured multi-press NVRAM reset flow.

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
* `void debugLog()`
  Writes the default startup log entry.
* `void debugLogf(const char *format, ...)`
  Appends a `printf`-style formatted message to the log buffer.
* `void finalLog(char *temp, bool timestamp = true)` / `void finalLog(char temp, bool timestamp = true)`
  Appends raw log data, optionally adding the UTC timestamp and CPU core ID. These overloads are primarily used by logging adapters.
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
* `String ledCInfo()`
  Returns the current LEDC channel duty-cycle and timer-frequency diagnostics.
* `String partitionInfo()`
  Returns the physical partitioning profile layout mapped across the embedded storage memory.
* `String getDateTimeUTC()`
  Returns the current RTC time formatted as `MM-DD-YY HH:MM:SS`.

### Hostname Persistence
* `String loadLocalHostname()`
  Loads the local hostname from NVS, returning `NVRAM_PERFS_HOSTNAME_LOCAL_DEFAULT` when no hostname has been saved.
* `String saveLocalHostname(String newHostname)`
  Saves a hostname to NVS. The value must be non-empty, fewer than 32 characters, and must not end in `.local`. An empty string indicates success; otherwise the returned string describes the validation or storage error.

---

## Comprehensive Integration Example

This example initializes the library, restores the previous boot log, optionally connects to Wi-Fi with NTP time synchronization, and services reset and connection monitoring from the main loop:

```cpp
#include <Arduino.h>
#include "UtilityFunctions.h"

void setup() {
    Serial.begin(115200);

    UtilityFunctions::UtilityFunctionsInit();

  // The empty return value means the hostname was saved successfully.
  String hostnameError = UtilityFunctions::saveLocalHostname("utility-device");
  if (hostnameError.length() > 0) {
    UtilityFunctions::debugLog(hostnameError);
  }

    String fallbackHistory = UtilityFunctions::getPreBootWebLog();
    if (fallbackHistory.length() > 0) {
        Serial.println("[NVS Recovery] Previous execution records located:");
        Serial.println(fallbackHistory);
    }

  UtilityFunctions::debugLogf("Boot time: %s\n",
                UtilityFunctions::getDateTimeUTC().c_str());
  UtilityFunctions::debugLog(UtilityFunctions::chipInfo());

  if (UtilityFunctions::isMaster()) {
#if defined(CONFIG_LWIP_IPV4) || defined(CONFIG_LWIP_IPV6)
    // Configure NTP before the network connection is established.
    UtilityFunctions::enableNTPTimeServer("pool.ntp.org");
#endif
#ifdef CONFIG_ESP_WIFI_ENABLED
    UtilityFunctions::setupWiFiAndConnect();
#endif
  }
}

void loop() {
  UtilityFunctions::checkResetPressed();
    UtilityFunctions::ledBlinkGreen();
  UtilityFunctions::debugLogf("Reset presses: %d, last event: %lu ms\n",
                UtilityFunctions::numTimesResetPressed(),
                UtilityFunctions::resetMills());

#ifdef CONFIG_ESP_WIFI_ENABLED
  UtilityFunctions::rebootIfWiFiDisconnected();
#endif

  // Cooperative delay that yields to other FreeRTOS tasks.
  UtilityFunctions::delay(5000);
}
```
