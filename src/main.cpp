#include <M5StickCPlus.h>
#include "FastLED.h"

#define Neopixel_PIN 32 // This is the pin when you plug in the neopixel strip
#define NUM_LEDS 60     // My strip is 60 LEDs long

// LEDs
CRGB leds[NUM_LEDS];

// TODO: The setup for effects here are super messy.
// Maybe handle the "global" task state differently
// and use structs for each effect? Or classes?
// (this is c++ technically)

// Rainbow effect
uint8_t gHue = 0;

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

double getBatteryLevel()
{
  // 4.188V seems to be max?
  uint16_t vbatData = M5.Axp.GetVbatData();
  double vbat = vbatData * 1.1 / 1000;
  return 100.0 * ((vbat - 3.0) / (4.188 - 3.0)); // Other source had 4.07
}

TFT_eSprite *disp_buffer;
char bat_msg_buf[50];
void init_print_bat()
{
  disp_buffer = new TFT_eSprite(&M5.Lcd);
  disp_buffer->setSwapBytes(false);
  disp_buffer->createSprite(240, 50);
  disp_buffer->fillRect(0, 0, 240, 50, BLACK);
}
void print_bat()
{
  float discharged = M5.Axp.GetCoulombData();
  float battery_capacity = 120.0; // 120 mah
  snprintf(bat_msg_buf, sizeof(bat_msg_buf), "battery: %.1f %%", battery_capacity + discharged);

  disp_buffer->setTextSize(2);
  disp_buffer->drawString(bat_msg_buf, 0, 0, 2);
  disp_buffer->pushSprite(10, 60);
}

void print_effect(int effect_id)
{
  M5.Lcd.fillScreen(BLACK);
  print_bat();
  M5.Lcd.setCursor(10, 25);
  M5.Lcd.println(find_effect_name(effect_id));
}

void setup()
{
  M5.begin();
  M5.Axp.EnableCoulombcounter(); // Battery status

  // Setup screen
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.setTextSize(4);
  init_print_bat();
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
  print_bat();
  delay(50);
}
