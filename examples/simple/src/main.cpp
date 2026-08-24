#include <Arduino.h>
#include <Arduino_DebugUtils.h>
#include <UtilityFunctions.h>
#include "WebLogPrint.h"

// auto matically include wifi manager if wifi is enabled
#ifdef CONFIG_ESP_WIFI_ENABLED
#include <WiFiManager.h>
#include <WiFiType.h>
#endif

#include "magic_enum/magic_enum.hpp"
#include "magic_enum/magic_enum_iostream.hpp"

template <typename E>
auto to_integer(magic_enum::Enum<E> value) -> int
{
  // magic_enum::Enum<E> - C++17 Concept for enum type.
  return static_cast<magic_enum::underlying_type_t<E>>(value);
}

void setup()
{

  Serial.begin(115200);
  // Wait for the serial console to be ready. This is a blocking spin-wait
  // that exits once `Serial` becomes available (host opens serial terminal).
  // Exit condition: `Serial` evaluates true.
  while (!Serial)
    ; // wait for serial attach

  UtilityFunctions::debugLog("Initializing EXAMPLE...");
  UtilityFunctions::UtilityFunctionsInit(); // Initialize utility functions

  // Check if the device is in master or slave mode
  // If device is master: initialize cloud/WiFi functionality, otherwise
  // run in  (slave) mode. Exit from this block when setup
  // completes or after a restart is triggered on failure.
  if (UtilityFunctions::isMaster())
  {

    /**
     * @brief Setup (what happens once when the BluetoothESP32 device wakes up)
     *
     * Plain words: This function runs one time when the  device starts. It
     * turns on the console (so we can see messages), sets up WiFi (if we are
     * the boss/master), starts the little web server that helps configure
     * the BluetoothESP32 device, and gets everything ready for the repeating work in
     * `loop()`.
     *
     * Important steps:
     * - Start serial console for debug messages
     * - Redirect ESP logs to the web logger so logs are viewable remotely
     * - If master: start WiFiManager to connect to WiFi or create an AP
     * - Create the web server so users can interact through a browser
     *
     * Loops: this function does not contain repeated loops except possible
     * short LED blink loops to show activity.
     */

#ifndef CONFIG_ESP_WIFI_ENABLED
    UtilityFunctions::debugLog("WIFI is truned off");
#else

    // reset settings - wipe stored credentials for testing
    //  these are stored by the esp library
    //  wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name (
    // "AutoConnectAP"), if empty will auto generate SSID, if password is blank
    // it will be anonymous AP (wm.autoConnect()) then goes into a blocking loop
    // awaiting configuration and will return success result

    UtilityFunctions::setupWiFiAndConnect();

#endif
  }
  else
  {
    /* slave code */
  }
}

// this shoud run on core 1
void loop()
{
  // yield(); // for the watchdog timer on core 0
  // UtilityFunctions::delay(1000);

  UtilityFunctions::debugLog("LOOP TASK Running...");

  // Check if the device is in master or slave mode
  if (UtilityFunctions::isMaster())
  {

    // Main worker loop: continuously polls for  commands and
    // WiFi status, and handles timeouts. Intended to run indefinitely.
    // Exit condition: only stops when the device is reset or powered off.
    for (;;) // infinite loop
    {

      /// do work  handle
      UtilityFunctions::ledBlinkBlue();

      /// do work
      UtilityFunctions::delay(2000);  // update delay as needed
      // other updates such as BLE, arduinoIot, web server etc are to be put here
#ifdef CONFIG_ESP_WIFI_ENABLED
    // put wifi dependent code here for the loop 
#endif
      // work done
      UtilityFunctions::ledStop();

      UtilityFunctions::checkResetPressed(); // Check if the reset button has been pressed

#ifdef CONFIG_ESP_WIFI_ENABLED
      UtilityFunctions::rebootIfWiFiDisconnected(); // check for wifi disconet due to router issues
#endif
    }
  }
}
