// filepath: d:\Documents\XGIMI_ALEXA_INTEGRATION\src\UtilityFunctions.h
#pragma once // The compiler will ignore this file if it is included again

#ifndef UtilityFunctions_H
#define UtilityFunctions_H

// #include "defaults.h"
#include <FastLED.h>
#include <Print.h>

#ifdef CONFIG_ESP_WIFI_ENABLED
#include <WiFiManager.h>

#ifndef WIFIDEBUG
#define WIFIDEBUG WM_DEBUG_VERBOSE
#endif

#ifndef AP_CONNECT_TIMEOUT
#define AP_CONNECT_TIMEOUT 120 // seconds or 2 mins
#endif

#ifndef WIFI_DISCONNET_TIMEOUT_SEC
#define WIFI_DISCONNET_TIMEOUT_SEC 60 // in seconds time duration when disconneccted from wifi will cause the system to reboot.
#endif

#endif

// specific to our ESP32S3 chip

#ifndef ResetButton
#define ResetButton GPIO_NUM_0 // the boot button on the ESP32
#endif

#ifndef RGBCHIP
#define RGBCHIP WS2812B
#endif

#ifndef LED_BUILTINIO
#define LED_BUILTINIO GPIO_NUM_48
#endif

#ifndef RGB_DATA_ORDER
#define RGB_DATA_ORDER GRB
#endif

#ifndef NUMPIXELS
#define NUMPIXELS 1
#endif

#ifndef LED_MUTEX_WAIT_MS
#define LED_MUTEX_WAIT_MS 1000
#endif

// #ifndef FASTLED_RMT_MAX_CHANNELS
// #define FASTLED_RMT_MAX_CHANNELS 1
// #endif

// #ifndef FASTLED_ESP32_I2S
// #define FASTLED_ESP32_I2S
// #endif

#ifndef I2C_SCLK
#define I2C_SCLK GPIO_NUM_12
#endif

#ifndef I2C_SDA
#define I2C_SDA GPIO_NUM_21
#endif

#ifndef I2C_FREQ
#define I2C_FREQ 100000 // 100 kHz
#endif

#ifndef I2C_SLAVE_ADDR
#define I2C_SLAVE_ADDR 0x3f // Slave address for I2C
#endif

#ifndef WEB_ESP_RESTART_DELAY
#define WEB_ESP_RESTART_DELAY 2000 // mills 2 mins
#endif

#ifndef WEB_STATUS_LOG_BUFFER
#define WEB_STATUS_LOG_BUFFER 8192 /// buffer for log to be shon in the web page
#endif

#ifndef NVRAM_PERFS
#define NVRAM_PERFS "registry" // references registry name
#endif

#ifndef NVRAM_PERFS_HOSTNAME_LOCAL_PROP
#define NVRAM_PERFS_HOSTNAME_LOCAL_PROP "hostname"
#endif

#ifndef NVRAM_PERFS_HOSTNAME_LOCAL_DEFAULT
#define NVRAM_PERFS_HOSTNAME_LOCAL_DEFAULT "UtilityFunctionsHost" // only use nuber alphabets and - and dot . NO .local It is added automtically
#endif

#ifndef NVRAM_PERFS_WEB_STATUS_LOG_BUFFER_PROP
#define NVRAM_PERFS_WEB_STATUS_LOG_BUFFER_PROP "weblogOld"
#endif

#ifndef NVRAM_PERFS_WEB_STATUS_LOG_BUFFER_SIZE_PROP
#define NVRAM_PERFS_WEB_STATUS_LOG_BUFFER_SIZE_PROP "weblogSize"
#endif

#ifndef STRINGIFY
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
#endif

namespace UtilityFunctions
{

  // have the wifi managwer log to the web logger
#ifdef CONFIG_ESP_WIFI_ENABLED
  extern WiFiManager wm;
  extern uint64_t Wifi_Disconnect_Start_Time;

  String getSSID();
  String getPSK();

  void setupWiFiAndConnect();
  void rebootIfWiFiDisconnected();

#endif

  void delay(long waitMills);
  void waitTillInitComplete();

  void UtilityFunctionsInit();
  bool isMaster();

  int findI2cOtherAddress();
  bool isResetPressed();
  int numTimesResetPressed();
  void unpressRest();
  unsigned long resetMills();

  void ledRed();
  void ledGreen();
  void ledYellow();
  void ledBlue();
  void ledWhite();
  void ledBrown();
  void ledStop();

  void ledBlinkGreen();
  void ledBlinkBlue();
  void ledBlinkRed();
  void ledBlinkYellow();
  void ledBlinkGreenLong();
  void ledBlinkRedLong();

  void debugLog(String message);
  void debugLog();
  void debugLogf(const char *format, ...);
  void finalLog(char *temp, bool timestamp = true);
  void finalLog(char temp, bool timestamp = true);

  // used for arduino cloud and wifi manager log
  int webLogPrintf(const char *format, va_list args);

  String chipInfo();
  String taskInfo();
  String ledCInfo();
  String partitionInfo();
  String webLog();

  // check if reset was pressed
  void checkResetPressed();

  // get RTC time
  String getDateTimeUTC();

  // save the old log in nvram and restart
  void ESP32Restart();

  // get the log from last boot
  String getPreBootWebLog();

  // Load hostname from NVRAM
  String loadLocalHostname();

  // Save hostname to NVRAM
  String saveLocalHostname(String newHostname);
}

#endif