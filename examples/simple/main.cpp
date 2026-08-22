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

// Define the LED_BUILTIN pin for the ESP32
// This is typically GPIO 48 on many ESP32 boards, but can vary by board.

// have the wifi managwer log to the web logger
#ifdef CONFIG_ESP_WIFI_ENABLED
WiFiManager wm = WiFiManager(*(new WebLogPrint()));
uint64_t Wifi_Disconnect_Start_Time = 0;

String getSSID() { return wm.getWiFiSSID(); }
String getPSK() { return wm.getWiFiPass(); }
#endif

void setup()
{

  Serial.begin(115200);
  // Wait for the serial console to be ready. This is a blocking spin-wait
  // that exits once `Serial` becomes available (host opens serial terminal).
  // Exit condition: `Serial` evaluates true.
  while (!Serial)
    ; // wait for serial attach

  // also log the esp 32 errors to the log
  // esp_log_level_set("*", ESP_LOG_VERBOSE);

  // have the ESP logs go to weblog
  esp_log_set_vprintf(UtilityFunctions::webLogPrintf);

  // set the arduino cloud debug to weblogPrint stream
  Debug.setDebugOutputStream(new WebLogPrint());

  Serial.setDebugOutput(true);
  UtilityFunctions::debugLog("Initializing EXAMPLE...");
  UtilityFunctions::UtilityFunctionsInit(); // Initialize utility functions

  // Check if the device is in master or slave mode
  // If device is master: initialize cloud/WiFi functionality, otherwise
  // run in BLE-only (slave) mode. Exit from this block when setup
  // completes or after a restart is triggered on failure.
  if (UtilityFunctions::isMaster())
  {

    /**
     * @brief Setup (what happens once when the BluetoothESP32 device wakes up)
     *
     * Plain words: This function runs one time when the BluetoothESP32 device starts. It
     * turns on the console (so we can see messages), sets up WiFi (if we are
     * the boss/master), starts the little web server that helps configure
     * the BluetoothESP32 device, and gets everything ready for the repeating work in
     * `loop()`.
     *
     * Important steps:
     * - Start serial console for debug messages
     * - Redirect ESP logs to the web logger so logs are viewable remotely
     * - Initialize utility code and the command ring buffer
     * - If master: start WiFiManager to connect to WiFi or create an AP
     * - Create the web server so users can interact through a browser
     *
     * Loops: this function does not contain repeated loops except possible
     * short LED blink loops to show activity.
     */

#ifndef CONFIG_ESP_WIFI_ENABLED
    UtilityFunctions::debugLog(
        "WIFI is truned off");
#else

    // reset settings - wipe stored credentials for testing
    //  these are stored by the esp library
    //  wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name (
    // "AutoConnectAP"), if empty will auto generate SSID, if password is blank
    // it will be anonymous AP (wm.autoConnect()) then goes into a blocking loop
    // awaiting configuration and will return success result

    bool res;
    UtilityFunctions::ledRed();

    UtilityFunctions::debugLog("Starting WiFiManager...");
    wm.setDebugOutput(true, WIFIDEBUG);
    wm.setConfigPortalBlocking(true);
    wm.setHostname(UtilityFunctions::loadLocalHostname());
    wm.setShowInfoErase(false);  // no erase settings on info page
    wm.setDarkMode(true);        // show in black background
    wm.setShowInfoUpdate(false); // no OTA mode
    wm.setConfigPortalTimeout(
        AP_CONNECT_TIMEOUT); // Set the timeout for the configuration portal

    // AcloudIOT_Decoder::LogWifiDebugInfo();
    res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    // res = wm.autoConnect("AutoConnectAP","password"); // password protected
    // ap

    // If connection to WiFi failed after the config portal timeout,
    // indicate failure (long red blink) and restart to retry the
    // initialization flow. Else, continue normal startup.
    if (!res)
    {
      UtilityFunctions::debugLogf("Failed to connect to wifi in startup init, and no one connected to AP in sec:%i\n", AP_CONNECT_TIMEOUT);
      UtilityFunctions::ledBlinkRedLong();
      UtilityFunctions::debugLog("Failed to connect to wifi ssid in start up init: RESTARTING");
      UtilityFunctions::ESP32Restart();
    }
    else
    {
      // if you get here you have connected to the WiFi
      WiFi.setAutoReconnect(true);
      UtilityFunctions::debugLog("Connected to WIFI Network...yeey :)");
      UtilityFunctions::ledStop();
      UtilityFunctions::ledBlinkGreenLong();
    }

#endif
  }
}

// this shoud run on core 1
void loop()
{
  //yield(); // for the watchdog timer on core 0
  //UtilityFunctions::delay(1000);

  UtilityFunctions::debugLog("LOOP TASK Running...");

  // Check if the device is in master or slave mode
  // If this device is configured as master, start AIoT cloud services and
  // register callbacks; otherwise operate in BLE-only mode.
  if (UtilityFunctions::isMaster())
  {
    UtilityFunctions::debugLog(" Starting WIFI Connext ");

    // timer for the wifi disconnect reboot.
    Wifi_Disconnect_Start_Time = 0;

    // Main worker loop: continuously polls for BLE commands, cloud and
    // WiFi status, and handles timeouts. Intended to run indefinitely.
    // Exit condition: only stops when the device is reset or powered off.
    for (;;) // infinite loop
    {

      /// bluetooth handle
      UtilityFunctions::ledBlinkBlue();
      UtilityFunctions::delay(2000);
      UtilityFunctions::ledStop();

      // Check WiFi connection: if disconnected, start/track a disconnect
      // timer and reboot the device if it remains disconnected longer than
      // `WIFI_DISCONNET_TIMEOUT_SEC`. Exit: when connected the timer resets.
      if (WiFi.status() != WL_CONNECTED)
      {
        // we are disconnected.
        if (Wifi_Disconnect_Start_Time == 0)
        {
          // this is the first time we are disconnected
          Wifi_Disconnect_Start_Time = esp_timer_get_time(); // set this to current time
          UtilityFunctions::debugLogf("Wifi is NOT CONNECTED(State =3); current state:%i and current time:%llu\n", WiFi.status(), Wifi_Disconnect_Start_Time);
        }
        else
        {
          // we have been disconnected for some time find how long
          uint64_t time_elapsed = (esp_timer_get_time() - Wifi_Disconnect_Start_Time);
          if (time_elapsed > (WIFI_DISCONNET_TIMEOUT_SEC * 1000000))
          {
            // greater than s secs (s * 1000 * 1000)
            UtilityFunctions::debugLogf("Wifi is NOT CONNECTED for atleast %i secs, REBOOTING time elapsed:%llu and start time:%llu\n", WIFI_DISCONNET_TIMEOUT_SEC, time_elapsed, Wifi_Disconnect_Start_Time);
            UtilityFunctions::ESP32Restart();
          }
        }
      }
      else
      {
        // we are connected so reset the disconenct time
        Wifi_Disconnect_Start_Time = 0;
      }

      UtilityFunctions::checkResetPressed(); // Check if the reset button has been pressed
    }
  }
}
