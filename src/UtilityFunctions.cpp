
#include "UtilityFunctions.h"
#include "WebLogPrint.h"
#include "driver/ledc.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>
#include <cstddef>
#include <cstring>
#include <esp_chip_info.h>
#include <format>
#include <magic_Enum/magic_enum.hpp>
#include <magic_Enum/magic_enum_iostream.hpp>
#include <ranges>
#include <string>
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include <Arduino_DebugUtils.h>

#ifdef CONFIG_ESP_WIFI_ENABLED
#include <WiFiManager.h>
#endif

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

template <typename E>
auto to_integer(magic_enum::Enum<E> value) -> int
{
  // magic_enum::Enum<E> - C++17 Concept for enum type.
  return static_cast<magic_enum::underlying_type_t<E>>(value);
}

// CRGB UtilityFunctions::leds[NUMPIXELS];

namespace UtilityFunctions
{
  CRGB leds[NUMPIXELS];
  bool newLineSeenForESPLog = true;

  // have the wifi managwer log to the web logger
#ifdef CONFIG_ESP_WIFI_ENABLED
  WiFiManager wm = WiFiManager(*(new WebLogPrint()));
  uint64_t Wifi_Disconnect_Start_Time = 0;

  String getSSID() { return wm.getWiFiSSID(); }
  String getPSK() { return wm.getWiFiPass(); }

  void setupWiFiAndConnect()
  {
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
    wm.setShowInfoErase(false);             // no erase settings on info page
    wm.setShowInfoUpdate(false);            // no OTA update button
    wm.setTitle("WiFi Connection Manager"); // set title
    wm.setDarkMode(true);                   // show in black background

    // custom menu via array or vector
    //
    // menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" (sep is seperator) (if param is in menu, params will not show up in wifi page!)
    // const char* menu[] = {"wifi","info","param","sep","restart","exit"};
    std::vector<const char *> menu = {"wifi", "info", "param", "sep", "restart", "exit"};
    wm.setMenu(menu);
    wm.setConfigPortalTimeout(AP_CONNECT_TIMEOUT); // Set the timeout for the configuration portal

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

  void rebootIfWiFiDisconnected()
  {
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
  }

#endif

  namespace
  {

    // This is a private variable to keep track of the master/slave mode
    // It is not exposed outside this namespace.
    bool masterMode = false; // true for master, false for slave
    struct Button
    {
      const uint8_t PIN;
      uint32_t numberKeyPresses;
      bool pressed;
    };

    // a string buffer that holdas max string content like a window looking at a train
    class AutoPopCharBuffer
    {
    public:
      // Constructor initializes the buffer with a fixed capacity
      AutoPopCharBuffer(size_t capacity)
          : capacity_(capacity), size_(0)
      {
        buffer_ = (char *)malloc(capacity);
      }

      // Destructor to free dynamically allocated memory
      ~AutoPopCharBuffer() { delete[] buffer_; }

      String peekFullBuffer()
      {
        if (isEmpty())
        {
          return ""; // Buffer is empty, nothing to pop
        }

        return String(buffer_, size_);
      }

      // Add a char to the buffer, overwriting if necessary
      void pushChar(const char charIN)
      {
        int len = 1;

        if (len > capacity_)
        {
          // we do  not have space
          return;
        }

        //  do we have space in the buffer
        if ((size_ + len + 1) > capacity_)
        {
          // we are over capacity so find out how many bytes in the front have to be discarded
          int discardNum = (size_ + len + 1) - capacity_; // one more byte for the num string
          strcpy(buffer_, &buffer_[discardNum]);          // discard num contains the one byte for null
          buffer_[capacity_ - len - 1] = charIN;
          buffer_[capacity_ - len] = 0;
          size_ = capacity_ - 1;
        }
        else
        {
          // we are with capacity so cp the bytes in the buffer
          buffer_[size_ + 1] = charIN;
          buffer_[size_ + 2] = 0;
          size_ = size_ + len;
        }
      }

      // Add a string to the buffer, overwriting if necessary
      void pushString(const char *str)
      {
        int len = strlen(str);
        if (len == 0)
        {
          return; // no need to add a zero length sting
        }

        if (len > capacity_)
        {
          strcpy(buffer_, &str[len + 1 - capacity_]); // fill up the buffer wiith the ending bytes that are in the string minus 1 for null char
          size_ = capacity_ - 1;
          return;
        }

        // Serial.printf("Buffer capacity:%i size:%i asked to add %i\n",capacity_, size_,len);
        //  do we have space in teh buffer
        if ((size_ + len + 1) > capacity_)
        {
          // we are over capacity so find out how many bytes in the front have to be discarded
          int discardNum = (size_ + len + 1) - capacity_; // one more byte for the num string
          strcpy(buffer_, &buffer_[discardNum]);          // discard num contains the one byte for null
          strcpy(&buffer_[capacity_ - len - 1], str);
          size_ = capacity_ - 1;
        }
        else
        {
          // we are with capacity so cp the bytes in the buffer
          strcpy(&buffer_[size_ + 1], str);
          size_ = size_ + len;
        }
        // Serial.printf("Buffer size:%i Now afer adding to add %i\n",size_,len);
      }

      void pushString(String str) { pushString(str.c_str()); }

      void pushString(std::string str) { pushString(str.c_str()); }

      // Check if the buffer is empty
      bool isEmpty() const { return (size_ == 0); }

      // Check if the buffer is full
      bool isFull() const { return (size_ == capacity_); }

      // Get the current number of elements in the buffer
      size_t size() const { return size_; }

      // Get the maximum capacity of the buffer
      size_t capacity() const { return capacity_; }

      char *getBuffer()
      {
        return buffer_;
      }

    protected:
      char *buffer_;
      int capacity_;
      int size_;
    };

    // variables to keep track of the timing of recent interrupts
    unsigned long last_buttonReset_time = 0;

    Button buttonReset = {ResetButton, 0, false};
    bool initPerformed = false;

    SemaphoreHandle_t xLedMutex;
    AutoPopCharBuffer webLogBuffer(WEB_STATUS_LOG_BUFFER);

  } // namespace

  // waits till init is completed; the execution is blocked
  void waitTillInitComplete()
  {

    for (; !initPerformed;)
    {
      UtilityFunctions::delay(30);
    }
  }
  void delay(long waitMills)
  {
    // non blocking delay i.e busy wait
    long cuurentMillis = millis();
    for (; millis() - cuurentMillis <= waitMills;)
    {

      //__asm__("nop"); // saver battery/power
      // yield();        // tell watchdog timer do other level 0 idle tasks
      vTaskDelay(waitMills * portTICK_PERIOD_MS);
    }
  }
  void ledRed()
  {
#ifdef UTILFUNC_DEBUG_LED_ON
    debugLog("Turning LED red");
#endif
    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = CRGB::Red;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledGreen()
  {
#ifdef UTILFUNC_DEBUG_LED_ON
    debugLog("Turning LED green");
#endif
    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = CRGB::Green;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledYellow()
  {
#ifdef UTILFUNC_DEBUG_LED_ON
    debugLog("Turning LED yellow");
#endif
    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = CRGB::Yellow3;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledBlue()
  {
#ifdef UTILFUNC_DEBUG_LED_ON
    debugLog("Turning LED blue");
#endif
    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = CRGB::Blue;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledWhite()
  {
#ifdef UTILFUNC_DEBUG_LED_ON
    debugLog("Turning LED white");
#endif
    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = CRGB::White;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledBrown()
  {
#ifdef UTILFUNC_DEBUG_LED_ON
    debugLog("Turning LED brown");
#endif
    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = CRGB::Brown;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledStop()
  {
#ifdef UTILFUNC_DEBUG_LED_ON
    debugLog("Turning LED off");
#endif
    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = CRGB::Black;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledBlinkGreen()
  {

    CRGB currColor = leds[0];

    UtilityFunctions::ledGreen(); // Turn on the LED to indicate a change has
                                  // been received
    UtilityFunctions::delay(30);
    UtilityFunctions::ledStop(); // Turn off the LED after processing the change

    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = currColor;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledBlinkRed()
  {

    CRGB currColor = leds[0];

    UtilityFunctions::ledRed(); // Turn on the LED to indicate a change has been
                                // received
    UtilityFunctions::delay(30);
    UtilityFunctions::ledStop(); // Turn off the LED after processing the change

    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = currColor;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledBlinkYellow()
  {

    CRGB currColor = leds[0];

    UtilityFunctions::ledYellow(); // Turn on the LED to indicate a change has
                                   // been received
    UtilityFunctions::delay(30);
    UtilityFunctions::ledStop(); // Turn off the LED after processing the change

    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = currColor;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledBlinkBlue()
  {

    CRGB currColor = leds[0];
    UtilityFunctions::ledBlue(); // Turn on the LED to indicate a change has
                                 // been received
    UtilityFunctions::delay(30);
    UtilityFunctions::ledStop(); // Turn off the LED after processing the change

    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = currColor;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledBlinkGreenLong()
  {
    CRGB currColor = leds[0];

    for (int i = 0; i < 10; i++)
    {
      UtilityFunctions::ledGreen();
      UtilityFunctions::delay(30);
      UtilityFunctions::ledStop();
      UtilityFunctions::delay(30);
    }
    UtilityFunctions::ledStop(); // Turn off the LED after processing the change

    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = currColor;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void ledBlinkRedLong()
  {
    CRGB currColor = leds[0];

    for (int i = 0; i < 10; i++)
    {
      UtilityFunctions::ledRed();
      UtilityFunctions::delay(30);
      UtilityFunctions::ledStop();
      UtilityFunctions::delay(30);
    }
    UtilityFunctions::ledStop(); // Turn off the LED after processing the change

    if (xSemaphoreTake(xLedMutex, pdMS_TO_TICKS(LED_MUTEX_WAIT_MS)) == pdTRUE)
    {
      leds[0] = currColor;
      FastLED.show();
      xSemaphoreGive(xLedMutex); // Release the mutex
    }
  }

  void IRAM_ATTR isr()
  {
    // only work for master
    if (!isMaster())
    {
      // we are slave and this ISR is not valid for us
      return;
    }
    unsigned long buttonReset_time = millis();

    if (buttonReset_time < last_buttonReset_time)
    {
      // we have overflowed the mills re set the lasst button time
      last_buttonReset_time = buttonReset_time;
    }
    unsigned long diff = buttonReset_time - last_buttonReset_time;
    if ((diff > 250) && (diff < 3000))
    {
      buttonReset.numberKeyPresses++;
      buttonReset.pressed = true;
      last_buttonReset_time = buttonReset_time;
    }
    if (diff > 3000)
    {
      buttonReset.numberKeyPresses = 0; // Reset the count if over 3 secs
      buttonReset.pressed = false;      // Unpress the button
      last_buttonReset_time = buttonReset_time;
    }
  }

  bool isResetPressed() { return buttonReset.pressed; }

  int numTimesResetPressed() { return buttonReset.numberKeyPresses; }
  unsigned long resetMills() { return last_buttonReset_time; }

  void unpressRest() { buttonReset.pressed = false; }

  /**
   * @brief Check whether the physical reset/boot button was pressed.
   *
   * Plain words: if the BluetoothESP32 device is the boss (master) and someone presses the
   * tiny boot button 3 times, the BluetoothESP32 device will forget saved WiFi and settings
   * and then restart itself.  This is useful when you want to make it like
   * new again.
   *
   * Algorithm (simple):
   * - If we are the master device, check the reset button state.
   * - If pressed, count how many times it was pressed recently.
   * - If fewer than 3 presses: ignore (clear the press). If 3 or more:
   *   - Erase WiFi settings and NVRAM, blink an LED a few times, wait, then
   *     restart the board.
   *
   * Loops: a small for-loop blinks an LED 5 times to show the reset action.
   */
  void checkResetPressed()
  {
    // If this device is configured as the master, check whether the
    // physical reset/boot button was pressed and handle a factory reset
    // sequence (erase settings, blink LED, restart).
    if (UtilityFunctions::isMaster())
    {
      // only check the boot button if we are the master device

      if (UtilityFunctions::isResetPressed())
      {
        UtilityFunctions::debugLogf(
            "Boot pressed %i times, need 3 to reset system count goees to "
            "zero after 3 secs reset detected at mills %i\n",
            UtilityFunctions::numTimesResetPressed(),
            UtilityFunctions::resetMills());

        if (UtilityFunctions::numTimesResetPressed() < 3)
        {
          // Not enough presses yet: clear and wait for more
          UtilityFunctions::unpressRest();
          return;
        }

// Enough presses: erase settings and restart to factory-like state
#ifdef CONFIG_ESP_WIFI_ENABLED
        WiFiManager wm = WiFiManager(*(new WebLogPrint()));
        wm.resetSettings(); // Reset WiFi settings
#endif

        nvs_flash_erase();
        UtilityFunctions::debugLog("Resetting ALL NVRAM settings...");
        // Blink the yellow LED 5 times to indicate an imminent full reset.
        // Purpose: provide a visible warning before erasing settings.
        // Exit condition: loop ends after 5 iterations.
        for (int i = 0; i < 5; i++)
        {
          UtilityFunctions::ledYellow();
          UtilityFunctions::delay(30);
          UtilityFunctions::ledStop();
          UtilityFunctions::delay(30);
        }
        UtilityFunctions::delay(1000); // short pause before restart
        UtilityFunctions::debugLog("Restarting ESP...");
        UtilityFunctions::ESP32Restart();
      }
    }
  }

  String getDateTimeUTC()
  {
    struct tm timeinfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);

    char s[51];

    strftime(s, 50, "%m-%d-%y %H:%M:%S", &timeinfo);

    return String(s);
  }

  String taskInfo()
  {

    std::string str;
#ifdef configUSE_TRACE_FACILITY
    str = "";
    // Get the total number of tasks
    UBaseType_t numberOfTasks = uxTaskGetNumberOfTasks();

    // Allocate memory for the TaskStatus_t array
    TaskStatus_t *taskStatusArray = new TaskStatus_t[numberOfTasks];

    // Get the system state and fill the array

#ifdef configGENERATE_RUN_TIME_STATS

    configRUN_TIME_COUNTER_TYPE ulTotalRunTime, ulStatsAsPercentage;

    /* For percentage calculations. */
    ulTotalRunTime = 100;
    if (uxTaskGetSystemState(taskStatusArray, numberOfTasks, &ulTotalRunTime) > 0)
#else
    if (uxTaskGetSystemState(taskStatusArray, numberOfTasks, NULL) > 0)
#endif
    {

      str = str + std::format(
                      "{: <" STRINGIFY(
                          CONFIG_FREERTOS_MAX_TASK_NAME_LEN) "}{: <10}{: <4}{: "
                                                             "<2}{: <3}\n",
                      "Task Name", "State", "Pri ", "Core ", "%CPU");

      for (int pri = ESP_TASK_PRIO_MAX - 1; pri >= ESP_TASK_PRIO_MIN;
           pri = pri - 1)
      {
        for (int i = 0; i < numberOfTasks; i++)
        {

          if (taskStatusArray[i].uxCurrentPriority == pri)
          {
#ifdef configGENERATE_RUN_TIME_STATS
            /* What percentage of the total run time has the task used?
                This will always be rounded down to the nearest integer.
            ulTotalRunTimeDiv100 has already been divided by 100. */
            if (ulTotalRunTime != 0)
            {
              ulStatsAsPercentage =
                  (taskStatusArray[i].ulRunTimeCounter / ulTotalRunTime);
            }
            else
            {
              ulStatsAsPercentage = taskStatusArray[i].ulRunTimeCounter;
            }
#else
            ulStatsAsPercentage = 0;
#endif

            const char *taskName = taskStatusArray[i].pcTaskName;

            // Get task state as a readable string
            const char *state;
            switch (taskStatusArray[i].eCurrentState)
            {
            case eRunning:
              state = "Running";
              break;
            case eReady:
              state = "Ready";
              break;
            case eBlocked:
              state = "Blocked";
              break;
            case eSuspended:
              state = "Suspended";
              break;
            case eDeleted:
              state = "Deleted";
              break;
            default:
              state = "Unknown";
              break;
            }

            UBaseType_t priority = taskStatusArray[i].uxCurrentPriority;

            UBaseType_t coreID = 0;
#ifdef configTASKLIST_INCLUDE_COREID
            coreID = taskStatusArray[i].xCoreID; // Specific to ESP-IDF
#endif

            str = str +
                  std::format(
                      "{: <" STRINGIFY(
                          CONFIG_FREERTOS_MAX_TASK_NAME_LEN) "}{: <10}{: <4}{: "
                                                             "<4}{: <3}\n",
                      taskName, state, priority, coreID, ulStatsAsPercentage);
          }
        }
      }

      // Free the dynamically allocated memory
      delete[] taskStatusArray;
#else
    str = "Free RTOS TASKS NOT CONFIGURED :\n";
#endif
      return String(str.c_str());
    }
  } // namespace UtilityFunctions
  String chipInfo()
  {
    /* Print chip information */

    unsigned major_rev = ESP.getChipRevision() / 100;
    unsigned minor_rev = ESP.getChipRevision() % 100;
    uint32_t flash_size = ESP.getFlashChipSize();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    std::string str = std::format(
        "System {} chip with {} CPU core(s) Clock Feq {} MHz, {}{}{}{}, silicon "
        "revision v{}.{}, {} MB {} flash \n",
        ESP.getChipModel(), ESP.getChipCores(), ESP.getCpuFreqMHz(),
        (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
        (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
        (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
        (chip_info.features & CHIP_FEATURE_IEEE802154)
            ? ", 802.15.4 (Zigbee/Thread)"
            : "",
        major_rev, minor_rev, flash_size / (uint32_t)(1024 * 1024),
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    str = str + std::format("Minimum free heap size: {} KB \n",
                            esp_get_minimum_free_heap_size() / 1024);
    str = str + std::format("Total heap: {} KB \n", ESP.getHeapSize() / 1024);
    str = str + std::format("Free heap: {} KB \n", ESP.getFreeHeap() / 1024);
    str = str + std::format("Total Flash Chip Mode: {} \n",
                            magic_enum::enum_name(ESP.getFlashChipMode()));
    str = str + std::format("Fash Chip Speed {} MHz \n",
                            ESP.getFlashChipSpeed() / (1000 * 1000));
    str = str + std::format("Total PSRAM: {} KB \n", ESP.getPsramSize() / 1024);
    str = str + std::format("Free PSRAM: {} KB\n", ESP.getFreePsram() / 1024);

    str = str + std::format("SDK version: {} \n", ESP.getSdkVersion());
    str = str + std::format("Core version: {} \n", ESP.getCoreVersion());

    str = str + std::format("Sketch Size: {} KB \n", ESP.getSketchSize() / 1024);
    str = str + std::format("Sketch Free Space: {} KB\n",
                            ESP.getFreeSketchSpace() / 1024);

    uint64_t macID = ESP.getEfuseMac(); // Get the MAC address
    str = str +
          std::format(
              "Device MAC Address: {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}\n",
              (uint16_t)(macID & 0x0000000000FF),
              (uint16_t)(macID >> 8) & 0x0000000000FF,
              (uint16_t)(macID >> 16) & 0x0000000000FF,
              (uint16_t)(macID >> 24) & 0x0000000000FF,
              (uint16_t)(macID >> 32) & 0x0000000000FF,
              (uint16_t)(macID >> 40) & 0x0000000000FF);

#ifdef ESP_WIFI_ENABLED
    WiFiManager wm;
    // can contain gargbage on esp32 if wifi is not ready yet
    str = str + "[WIFI] WIFI_INFO DEBUG \n";
    str = str + std::format("[WIFI] MODE: {} \n",
                            wm.getModeString(WiFi.getMode()).c_str());
    str = str + std::format("[WIFI] SAVED: {} \n",
                            (wm.getWiFiIsSaved() ? "YES" : "NO"));
    str = str + std::format("[WIFI] SSID: {} \n", wm.getWiFiSSID().c_str());
    str = str + std::format("[WIFI] CHANNEL: {} \n", WiFi.channel());
    str = str + std::format("[WIFI] RSSI: {} \n", WiFi.RSSI());
    str = str + std::format("[WIFI] PASS: {} \n", wm.getWiFiPass().c_str());
    str = str + std::format("[WIFI] HOSTNAME: {} \n", WiFi.getHostname());
#endif

    str = str + "\n\nIn order to RESET and ERASE NVRAM press BOOT key 3 times within "
                "3 seconds";

    return String(str.c_str());
  }

  String ledCInfo()
  {

    std::string str = std::format("");

    // Iterate through high-speed mode channels
#if SOC_LEDC_SUPPORT_HS_MODE
    str = str + std::format("High-Speed Mode Channels:\n");
    for (int channel = 0; channel < LEDC_CHANNEL_MAX; channel++)
    {
      uint32_t duty = ledc_get_duty(ledc_mode_t::LEDC_LOW_SPEED_MODE,
                                    (ledc_channel_t)channel);

      str = str + std::format("Channel {}: Duty={} {}\n", channel, ((duty == 259) ? "Not Init" : ""), duty);
    }
    for (int timer = 0; timer < LEDC_TIMER_MAX; timer++)
    {
      uint32_t freq =
          ledc_get_freq(ledc_mode_t::LEDC_LOW_SPEED_MODE, (ledc_timer_t)timer);
      str = str + std::format("Timer {}: Freq={} {} Hz\n", timer, ((freq == 259) ? "Not Init" : ""), freq);
    }
#else
  str = str + std::format("High-Speed Mode Channels: NA \n");
#endif

    // Iterate through low-speed mode channels
    str = str + std::format("Low-Speed Mode Channels:\n");
    // for (int channel = 0; channel < LEDC_CHANNEL_MAX; channel++)
    // do only one channel for now to reduce the amt of error logs from the esp framework
    for (int channel = 0; channel < 1; channel++)
    {
      uint32_t duty = ledc_get_duty(ledc_mode_t::LEDC_LOW_SPEED_MODE,
                                    (ledc_channel_t)channel);

      str = str + std::format("Channel {}: Duty={} {}\n", channel, ((duty == 259) ? "Not Init" : ""), duty);
    }

    // for (int timer = 0; timer < LEDC_TIMER_MAX; timer++)
    // do only one tomer for now to reduce the amt of error logs from the esp framework
    for (int timer = 0; timer < 1; timer++)
    {
      uint32_t freq =
          ledc_get_freq(ledc_mode_t::LEDC_LOW_SPEED_MODE, (ledc_timer_t)timer);
      str = str + std::format("Timer {}: Freq={} {} Hz\n", timer, ((freq == 259) ? "Not Init" : ""), freq);
    }

    return String(str.c_str());
  }

  String partitionInfo()
  {

    std::string str = std::format("");

    // Iterate through the partitions
    str = str + std::format(
                    "{: <17}{: <4} {: <4}\n{: <10}{: <10} {: <4}{: <2} {: <2}\n",
                    "Name", "Type", "Sub", "Offset", "Size", "Size(KB)", "Enc", "RO");

    // Find an iterator for all partition types and subtypes
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);

    while (it != NULL)
    {
      const esp_partition_t *p = esp_partition_get(it);
      str = str + std::format("{: <17}{:#04X} {:#04X}\n{:#010X} {:#010X} {:0>4}Kb {: <1} {: <1}\n",
                              p->label, (uint32_t)p->type, (uint32_t)p->subtype, p->address, p->size, p->size / 1024, p->encrypted, p->readonly);
      it = esp_partition_next(it);
    }

    // Release the iterator to avoid memory leaks
    esp_partition_iterator_release(it);

    str = str + std::format("\nBoot Partition :\n");
    const esp_partition_t *p = esp_ota_get_boot_partition();
    str = str + std::format("{: <17}{:#04X} {:#04X}\n{:#010X} {:#010X} {:0>4}Kb {: <1} {: <1}\n",
                            p->label, (uint32_t)p->type, (uint32_t)p->subtype, p->address, p->size, p->size / 1024, p->encrypted, p->readonly);

    str = str + std::format("\nRunning Partition :\n");
    p = esp_ota_get_running_partition();
    str = str + std::format("{: <17}{:#04X} {:#04X}\n{:#010X} {:#010X} {:0>4}Kb {: <1} {: <1}\n",
                            p->label, (uint32_t)p->type, (uint32_t)p->subtype, p->address, p->size, p->size / 1024, p->encrypted, p->readonly);

    return String(str.c_str());
  }

  void UtilityFunctionsInit()
  {
    // only init once
    if (initPerformed)
    {
      return;
    }

    // have the ESP logs go to weblog
    esp_log_set_vprintf(UtilityFunctions::webLogPrintf);

    // set the arduino cloud debug to weblogPrint stream
    Debug.setDebugOutputStream(new WebLogPrint());

    Serial.setDebugOutput(true);

    xLedMutex = xSemaphoreCreateMutex();
    if (xLedMutex == NULL)
    {
      // Handle mutex creation error
      UtilityFunctions::debugLog("Failed to create LED mute restarting...");
      UtilityFunctions::ESP32Restart();
    }
    FastLED.addLeds<RGBCHIP, LED_BUILTINIO, RGB_DATA_ORDER>(leds, NUMPIXELS);
    // Initialize the LED array to off (black)
    for (int i = 0; i < NUMPIXELS; ++i)
    {
      leds[i] = CRGB::Black;
    }
    FastLED.show();

    // start debug log
    debugLog();

    debugLog(chipInfo());

    // Extract the last two bytes
    // uint16_t lastByte = (uint16_t)(macID & 0x00FF);

    // debugLogf("Last Byte: 0x%04X\n", lastByte);

    // UtilityFunctions::delay(lastByte);

    // debugLog("Initializing I2C...");
    // bool masterSuccess = Wire1.begin(I2C_SDA, I2C_SCLK, I2C_FREQ); //
    // Initialize I2C with specified pins and frequency

    // debugLog(String("We are master: ") + (masterSuccess ? "true" : "false"));

    masterMode = true; // Set the mode based on the success of Wire1.begin

    if (masterMode)
    {
      // if we are mastwer we need the boot button to reset the saved WIFI info
      pinMode(buttonReset.PIN, INPUT_PULLUP);
      attachInterrupt(buttonReset.PIN, isr, FALLING);
    }
    else
    {
      // we are in slave mode
      // Wire1.begin(I2C_SLAVE_ADDR, I2C_SDA, I2C_SCLK, I2C_FREQ);
      debugLog(String("were slave: ") +
               (masterMode ? "false" : "true")); // opposite of master
    }

    // set init was cmpleted ok
    initPerformed = true;
  }

  bool isMaster() { return masterMode; }

  // int findI2cOtherAddress()
  // {

  //     byte error, address;
  //     byte lowestDevADDR;

  //     Serial.println("Scanning...");

  //     lowestDevADDR = 0;
  //     for (address = 1; address < 127; address++)
  //     {
  //         // The i2c_scanner uses the return value of
  //         // the Write.endTransmisstion to see if
  //         // a device did acknowledge to the address.
  //         Wire1.beginTransmission(address);
  //         error = Wire1.endTransmission();

  //         if (error == 0)
  //         {
  //             Serial.print("I2C device found at address 0x");
  //             if (address < 16)
  //                 Serial.print("0");
  //             Serial.print(address, HEX);
  //             Serial.println("  !");

  //             if (lowestDevADDR == 0)
  //             {
  //                 lowestDevADDR = address; // Store the first found address
  //             }
  //             if (address < lowestDevADDR)
  //             {
  //                 lowestDevADDR = address; // Store the lowest address found
  //                 Serial.print("Lowest address found so far: 0x");
  //                 Serial.println(lowestDevADDR, HEX);
  //             }
  //         }
  //         else if (error == 4)
  //         {
  //             Serial.print("Unknown error at address 0x");
  //             if (address < 16)
  //                 Serial.print("0");
  //             Serial.println(address, HEX);
  //         }
  //     }

  //     Serial.println("done\n");
  //     return lowestDevADDR;
  // }

  void debugLog()
  {
    Serial.println();
    webLogBuffer.pushString("\n");
  }

  String webLog() { return String(webLogBuffer.peekFullBuffer()); }

  void debugLog(String message)
  {

    std::string str =
        std::format("{}:CORE:{}:{}\n", getDateTimeUTC().c_str(), xPortGetCoreID(), message.c_str());
    Serial.printf(str.c_str());
    webLogBuffer.pushString(str);
  }
  void debugLogf(const char *format, ...)
  {
    char loc_buf[512];
    char *temp = loc_buf;
    va_list args;
    va_start(args, format);
    int len = vsnprintf(temp, sizeof(loc_buf), format, args);
    va_end(args);
    if (len < 0)
    {
      return;
    }
    if (len >=
        (int)sizeof(loc_buf))
    { // comparation of same sign type for the compiler
      temp = (char *)malloc(len + 1);
      if (temp == NULL)
      {
        return;
      }
      len = vsnprintf(temp, len + 1, format, args);
    }

    finalLog(temp);

    // len = Serial.write((uint8_t *)temp, len);
    if (temp != loc_buf)
    {
      free(temp);
    }
    return;
  }

  void finalLog(char *temp, bool timestamp)
  {
    if (timestamp)
    {
      std::string str;
      str = std::format("{}:C{}:{}", getDateTimeUTC().c_str(), xPortGetCoreID(), temp);
      Serial.printf(str.c_str());
      webLogBuffer.pushString(str);
    }
    else
    {
      Serial.print(temp);
      webLogBuffer.pushString(temp);
    }
  }

  void finalLog(char temp, bool timestamp)
  {
    if (timestamp)
    {
      std::string str;
      str = std::format("{}:C{}:{}", getDateTimeUTC().c_str(), xPortGetCoreID(), temp);
      Serial.printf(str.c_str());
      webLogBuffer.pushString(str);
    }
    else
    {
      Serial.print(temp);
      // Serial.printf(":%x:",temp);
      webLogBuffer.pushChar(temp);
    }
  }

  // used for arduino esp logs
  int webLogPrintf(const char *format, va_list args)
  {
    char loc_buf[512];
    char *temp = loc_buf;
    int len = vsnprintf(temp, sizeof(loc_buf), format, args);
    if (len > 0)
    {

      // see if there is a new line if so set newLineSeenForESPLog to true else set to false
      if (newLineSeenForESPLog)
      {
        // shows timestamp
        debugLogf("%s", temp);
        newLineSeenForESPLog = false;
      }
      else
      {
        // no timestamp log
        UtilityFunctions::finalLog(temp, false);
      }
      for (int i = 0; i < len; i++)
      {
        if (temp[i] == '\n')
        {
          newLineSeenForESPLog = true;
          break;
        }
      }
    }
    return len;
  }

  String getBuildTimeVersion()
  {
    std::string str = std::format("Build Time:{} {}", __DATE__, __TIME__);
    return String(str.c_str());
  }

  // save the old log in nvram and restart
  void ESP32Restart()
  {

    UtilityFunctions::debugLog("......REBOOTING.....EOF.");
    Preferences _preferences;
    _preferences.begin(NVRAM_PERFS, false);
    uint size = webLogBuffer.size();
    _preferences.putUInt(NVRAM_PERFS_WEB_STATUS_LOG_BUFFER_SIZE_PROP, size);
    _preferences.putBytes(NVRAM_PERFS_WEB_STATUS_LOG_BUFFER_PROP, webLogBuffer.getBuffer(), size);
    _preferences.end();
    ESP.restart();
  }

  String getPreBootWebLog()
  {

    Preferences _preferences;
    _preferences.begin(NVRAM_PERFS, false);
    int size = _preferences.getUInt(NVRAM_PERFS_WEB_STATUS_LOG_BUFFER_SIZE_PROP, 0);
    if (size == 0)
    {
      _preferences.end();
      return "";
    }

    char *buffer = (char *)malloc(size);

    _preferences.getBytes(NVRAM_PERFS_WEB_STATUS_LOG_BUFFER_PROP, buffer, size);

    _preferences.end();
    return String(buffer, size);
    ;
  }

  // Load hostname from NVRAM
  String loadLocalHostname()
  {
    Preferences _preferences;
    _preferences.begin(NVRAM_PERFS, false);
    String _localHostname = _preferences.getString(
        NVRAM_PERFS_HOSTNAME_LOCAL_PROP, NVRAM_PERFS_HOSTNAME_LOCAL_DEFAULT);
    _preferences.end();
    // UtilityFunctions::debugLogf("loaded local hostname from NVRAM. %s\n", _localHostname.c_str());

    return _localHostname;
  }

  // Save hostname to NVRAM
  String saveLocalHostname(String newHostname)
  {
    size_t bytesWritten;
    if (!newHostname.isEmpty() && (newHostname.length() < 32) &&
        (!newHostname.endsWith(".local")))
    {
      Preferences _preferences;
      _preferences.begin(NVRAM_PERFS, false);
      bytesWritten = _preferences.putString(NVRAM_PERFS_HOSTNAME_LOCAL_PROP, newHostname);
      _preferences.end();

      if (bytesWritten == 0)
      {
        std::string str = "Unkown Error Invalid hostnname, must be less than 32 chars;not "
                          "empty; and not .local in the end";
        String Astr = String(str.c_str());
        debugLog(Astr);
        return Astr;
      }
      UtilityFunctions::debugLog("hostname updated and saved to NVRAM.");
      return "";
    }
    else
    {
      std::string str = "Invalid hostnname, must be less than 32 chars;not "
                        "empty; and not .local in the end";
      String Astr = String(str.c_str());
      debugLog(Astr);
      return Astr;
    }
  }

} // namespace UtilityFunctions
