#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

static TaskHandle_t LEDFlicker = NULL;
static TaskHandle_t Button = NULL;

// LED pin list
static const uint8_t LED_PINS[] = {26, 13, 27, 33, 32, 22, 23, 18, 19, 25};
static const size_t LED_COUNT = sizeof(LED_PINS) / sizeof(LED_PINS[0]);

// Button pin list
static const uint8_t BUTTON_PINS[] = {12, 14};
static const size_t BUTTON_COUNT = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);

// Command queue to communicate button presses to the LED task
static QueueHandle_t commandQueue = NULL;

enum Command : int
{
  CMD_NONE = 0,
  CMD_FORWARD = 1,
  CMD_REVERSE = 2
};

void flicker(void *parameter)
{
  int cmd;
  for (;;)
  {
    // Wait for a command from the button task
    if (xQueueReceive(commandQueue, &cmd, portMAX_DELAY) == pdTRUE)
    {
      if (cmd == CMD_FORWARD)
      {
        for (size_t i = 0; i < LED_COUNT + 2; ++i)
        {
          // led count = 10 + 2
          if (i < LED_COUNT)
            digitalWrite(LED_PINS[i], HIGH);
          // Turn off 2 LEDs behind the current one to leave a trail
          if (i >= 2)
            digitalWrite(LED_PINS[i - 2], LOW);
          vTaskDelay(130 / portTICK_PERIOD_MS);
        }
      }
      else if (cmd == CMD_REVERSE)
      {
        // N-1 -> 0
        for (int i = LED_COUNT - 1; i > -3 ; --i)
        {
          // led count = 10 
          // i=9 
          if (i >= 0 && i < LED_COUNT)
            digitalWrite(LED_PINS[i], HIGH);
          
          // Turn off 2 LEDs ahead (which are behind visually in reverse)
          if (i + 2 < LED_COUNT && i + 2 >= 0)
            digitalWrite(LED_PINS[i + 2], LOW);
          vTaskDelay(130 / portTICK_PERIOD_MS);
        }
      }
      // After finishing sequence, continue waiting for next command
    }
  }
}

void readButtons(void *parameter)
{
  // Simple debounce parameters
  const TickType_t debounceDelay = 20 / portTICK_PERIOD_MS;

  for (;;)
  {
    for (size_t i = 0; i < BUTTON_COUNT; ++i)
    {
      int state = digitalRead(BUTTON_PINS[i]); // using INPUT_PULLUP: HIGH = not pressed, LOW = pressed
      if (state == LOW)
      {
        // debounce
        vTaskDelay(debounceDelay);
        if (digitalRead(BUTTON_PINS[i]) == LOW)
        {
          // Send command depending on which button
          int cmdToSend = (i == 0) ? CMD_FORWARD : CMD_REVERSE;
          // Use overwrite to ensure latest command is stored (queue length 1)
          if (commandQueue != NULL)
          {
            xQueueOverwrite(commandQueue, &cmdToSend);
          }

          // wait for release to avoid multiple triggers
          while (digitalRead(BUTTON_PINS[i]) == LOW)
          {
            vTaskDelay(10 / portTICK_PERIOD_MS);
          }
        }
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); // poll interval
  }
}
void setup()
{
  Serial.begin(115200);

  for (size_t i = 0; i < LED_COUNT; ++i)
  {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW); // Ensure all LEDs are off initially
  }
  for (size_t i = 0; i < BUTTON_COUNT; ++i)
  {
    pinMode(BUTTON_PINS[i], INPUT); // Using external pull-down resistors
  }

  // Create command queue (length 1) used to notify LED task which sequence to run
  commandQueue = xQueueCreate(1, sizeof(int));
  if (commandQueue == NULL)
  {
    Serial.println("Failed to create command queue");
  }

  xTaskCreatePinnedToCore(
      flicker,     // Function to implement the task
      "LEDTask",   // Name of the task
      2048,        // Stack size in words
      NULL,        // Task input parameter
      1,           // Priority of the task
      &LEDFlicker, // Task handle
      app_cpu      // Core where the task should run
  );

  xTaskCreatePinnedToCore(
      readButtons,  // Function to implement the task
      "ButtonTask", // Name of the task
      2048,         // Stack size in words
      NULL,         // Task input parameter
      1,            // Priority of the task
      &Button,      // Task handle
      app_cpu       // Core where the task should run
  );
}

void loop()
{
  vTaskDelay(100 / portTICK_PERIOD_MS);
}