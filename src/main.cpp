#include <M5StickCPlus.h>
#include "FastLED.h"

#define Neopixel_PIN 32
#define NUM_LEDS 60

// LEDs
CRGB leds[NUM_LEDS];

// Rainbow effect
uint8_t gHue = 0;

// Tasks
// static TaskHandle_t FastLEDshowTaskHandle = 0;
// static TaskHandle_t CycleEffectTaskHandle = 0;

// For cycle
uint8_t selected = 0;
CRGB colors[3] = {CRGB::Red, CRGB::Green, CRGB::Blue};

// For fade
uint8_t fade = 0;
bool going_up = true;

// For wave
uint8_t wave_offset = 0;
uint8_t wave_width = 10;
bool wave_direction = true; // true == up, false == down

// Effects
const char *EFFECT_RAINBOW = "rainbow";
const char *EFFECT_CYCLE = "cycle";
const char *EFFECT_BREATHING = "breathing";
const char *EFFECT_WAVE = "wave";
uint8_t num_effects = 4;

// current effect state
static SemaphoreHandle_t effect_state_mutex;
RTC_DATA_ATTR int effect = 0;

void do_rainbow()
{
  fill_rainbow(leds, NUM_LEDS, gHue, 7); // rainbow effect

  FastLED.show(); // must be executed for neopixel becoming effective
  EVERY_N_MILLISECONDS(20) { gHue++; }
}

void do_cycle()
{
  fill_solid(leds, NUM_LEDS, colors[selected]); // solid color

  FastLED.show(); // must be executed for neopixel becoming effective
  EVERY_N_MILLISECONDS(1000)
  {
    selected = (selected + 1) % 3;
  }
}

void do_breathing(CRGB color)
{
  color.fadeToBlackBy(fade);

  fill_solid(leds, NUM_LEDS, color);

  FastLED.show();

  EVERY_N_MILLISECONDS(10)
  {
    if (going_up)
    {
      fade++;
      if (fade == 240) // Only go up to 140 instead of 155 to prevent black
      {
        going_up = false;
      }
    }
    else
    {
      fade--;
      if (fade == 0)
      {
        going_up = true;
      }
    }
  }
}

void do_wave(CRGB color)
{
  EVERY_N_MILLISECONDS(50)
  {
    // Paint the colors
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
      if (i < wave_offset || i > wave_offset + wave_width)
      {
        leds[i] = CRGB::Black;
      }
      else
      {
        leds[i] = color;
      }
    }
    FastLED.show();

    // Check if we bumped into the edge
    if (wave_offset + wave_width >= NUM_LEDS)
    {
      wave_direction = false;
    }
    else if (wave_offset <= 0)
    {
      wave_direction = true;
    }

    // Move the wave
    if (wave_direction)
    {
      wave_offset++;
    }
    else
    {
      wave_offset--;
    }
  }
}

void FastLEDshowTask(void *pvParameters)
{
  for (;;)
  {
    xSemaphoreTake(effect_state_mutex, portMAX_DELAY);
    int effect_to_use = effect;
    xSemaphoreGive(effect_state_mutex);

    switch (effect_to_use)
    {
    case 0:
      do_rainbow();
      break;
    case 1:
      do_cycle();
      break;
    case 2:
      do_breathing(CRGB::Orange);
      break;
    case 3:
      do_wave(CRGB::BlueViolet);
      break;
    }

    delay(1);
  }
}

const char *find_effect_name(int effect_id)
{
  switch (effect_id)
  {
  case 0:
    return EFFECT_RAINBOW;
  case 1:
    return EFFECT_CYCLE;
  case 2:
    return EFFECT_BREATHING;
  case 3:
    return EFFECT_WAVE;
  default:
    return EFFECT_RAINBOW;
  }
}

void print_effect(int effect_id)
{
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(10, 25);
  M5.Lcd.println(find_effect_name(effect_id));
}

void setup()
{
  M5.begin();

  // Setup screen
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.setTextSize(4);
  print_effect(effect); // No lock needed because thread not started.
  FastLED.addLeds<WS2811, Neopixel_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(10);

  effect_state_mutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(FastLEDshowTask, "FastLEDshowTask", 2048, NULL, 2, NULL, 1);
}

void loop()
{
  if (digitalRead(M5_BUTTON_HOME) == LOW)
  {
    xSemaphoreTake(effect_state_mutex, portMAX_DELAY);
    effect = effect == (num_effects - 1) ? 0 : effect + 1;
    int effect_id = effect;
    xSemaphoreGive(effect_state_mutex);
    print_effect(effect_id);
    delay(500);
  }
  delay(50);
}
