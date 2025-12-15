// -------------------------------------------------------------------
// Libraries
// -------------------------------------------------------------------
#include <Adafruit_GFX.h>        // Core graphics library for OLED drawing
#include <Adafruit_SSD1306.h>    // Library for SSD1306 OLED display
#include <Wire.h>                // I2C communication library
#include "MAX30105.h"            // MAX30102/MAX30105 sensor library
#include "heartRate.h"           // Heart rate detection algorithm

// -------------------------------------------------------------------
// Objects & Definitions
// -------------------------------------------------------------------
MAX30105 particleSensor;         // Create MAX30102 sensor object

const byte RATE_SIZE = 4;        // Number of BPM samples used for averaging
byte rates[RATE_SIZE];           // Array to store BPM readings
byte rateSpot = 0;               // Index for BPM array
long lastBeat = 0;               // Stores last detected heartbeat time
float beatsPerMinute;            // Current BPM value
int beatAvg;                     // Averaged BPM value

#define BeatBuzzer 3             // Pin for heartbeat buzzer
#define LMaHeartBuzzer 2         // Pin for alarm buzzer (high BPM or temp)
#define LM35 A0                  // Analog pin connected to LM35 temperature sensor

#define SCREEN_WIDTH 128         // OLED screen width in pixels
#define SCREEN_HEIGHT 64         // OLED screen height in pixels
#define OLED_RESET    -1         // OLED reset pin (-1 means no reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // OLED object

// -------------------------------------------------------------------
// Bitmaps
// -------------------------------------------------------------------
const unsigned char epd_bitmap_black_heartbeat_icon_vector_illustration_260nw_2428187237_ezgif [] PROGMEM = {
    // Bitmap data for large heartbeat image stored in flash memory
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // (bitmap data continues…)
};

const int epd_bitmap_allArray_LEN = 1; // Number of bitmaps in array
const unsigned char* epd_bitmap_allArray[1] = {
    epd_bitmap_black_heartbeat_icon_vector_illustration_260nw_2428187237_ezgif // Pointer to bitmap
};

static const unsigned char PROGMEM image_heart_bits[] = {
    // Small heart icon bitmap
    0x38,0x38,0x6c,0x6c,0x46,0xc4,0xc3,0x86,
    0x81,0x02,0x80,0x02,0x80,0x02,0xc0,0x06,
    0x40,0x04,0x60,0x0c,0x20,0x08,0x30,0x18,
    0x18,0x30,0x0c,0x60,0x06,0xc0,0x03,0x80
};

static const unsigned char PROGMEM image_Layer_8_bits[] = { 0x80 }; // Degree symbol bitmap

static const unsigned char PROGMEM image_weather_temperature_bits[] = {
    // Temperature icon bitmap
    0x1c,0x00,0x22,0x02,0x2b,0x05,0x2a,0x02,
    0x2b,0x38,0x2a,0x60,0x2b,0x40,0x2a,0x40,
    0x2a,0x60,0x49,0x38,0x9c,0x80,0xae,0x80,
    0xbe,0x80,0x9c,0x80,0x41,0x00,0x3e,0x00
};

// -------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------
void setup() {

  Wire.setClock(400000);                         // Set I2C speed to 400kHz
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);     // Initialize OLED at I2C address 0x3C
  display.setTextColor(WHITE);                   // Set text color to white

  // MAX30102 Setup
  particleSensor.begin(Wire, I2C_SPEED_FAST);    // Initialize MAX30102 with fast I2C
  particleSensor.setup();                        // Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0F);    // Set red LED brightness

  // Welcome Screen
  display.clearDisplay();                        // Clear OLED buffer
  display.setTextColor(1);                       // Set text color
  display.setTextWrap(false);                    // Disable text wrapping
  display.setCursor(35, 19);                     // Set cursor position
  display.print("Welcome To");                  // Print welcome text
  display.setCursor(5, 29);                      // Move cursor
  display.print("BioSync Monitor V1.0");        // Print project name
  display.drawBitmap(15, 38, epd_bitmap_black_heartbeat_icon_vector_illustration_260nw_2428187237_ezgif, 100, 30, 1); // Draw heartbeat bitmap
  display.display();                             // Update OLED
  delay(7000);                                   // Wait 7 seconds

  // Author Screen
  display.clearDisplay();                        // Clear OLED
  display.setCursor(59, 20);                     // Set cursor position
  display.print("By");                          // Print text
  display.setCursor(8, 33);                      // Move cursor
  display.print("Eng. Yousef Elasfer");         // Print author name
  display.display();                             // Update OLED
  delay(4000);                                   // Wait 4 seconds

  pinMode(BeatBuzzer, OUTPUT);                   // Set heartbeat buzzer pin as output
  pinMode(LMaHeartBuzzer, OUTPUT);               // Set alarm buzzer pin as output
  pinMode(LM35, INPUT);                          // Set LM35 pin as input
}

// -------------------------------------------------------------------
// Main Loop
// -------------------------------------------------------------------
void loop() {

  int Vout = analogRead(LM35);                   // Read LM35 analog value
  int temp = Vout * 500 / 1023;                  // Convert ADC value to temperature (°C)

  long irValue = particleSensor.getIR();         // Read IR value from MAX30102

  if (irValue > 50000) {                         // Check if finger is detected

    if (checkForBeat(irValue) == true) {         // Check if heartbeat is detected

      long delta = millis() - lastBeat;          // Time difference between beats
      lastBeat = millis();                       // Update last beat time
      beatsPerMinute = 60 / (delta / 1000.0);    // Calculate BPM

      if (beatsPerMinute < 255 && beatsPerMinute > 20) { // Validate BPM range

        rates[rateSpot++] = (byte)beatsPerMinute; // Store BPM value
        rateSpot %= RATE_SIZE;                   // Wrap array index

        beatAvg = 0;                             // Reset average
        for (byte x = 0; x < RATE_SIZE; x++)     // Sum BPM values
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;                    // Calculate average BPM

        // Display BPM & Temperature
        display.clearDisplay();                  // Clear OLED
        display.setTextColor(1);                 // Set text color
        display.setTextSize(2);                  // Set large text size
        display.setTextWrap(false);              // Disable wrapping

        display.setCursor(33, 12);               // Set cursor for BPM label
        display.print("BPM:");                  // Print BPM label
        display.setCursor(88, 12);               // Set cursor for BPM value
        display.print(beatAvg);                  // Print averaged BPM
        display.drawBitmap(10, 11, image_heart_bits, 15, 16, 1); // Draw heart icon

        display.drawBitmap(10, 36, image_weather_temperature_bits, 16, 16, 1); // Draw temp icon
        display.setCursor(33, 37);               // Set cursor for temperature label
        display.print("BBT:");                  // Print body temperature label
        display.setCursor(88, 37);               // Set cursor for temperature value
        display.print(temp);                     // Print temperature
        display.setCursor(116, 37);              // Set cursor for degree symbol
        display.print("C");                    // Print Celsius unit
        display.drawBitmap(115, 35, image_Layer_8_bits, 1, 1, 1); // Draw degree symbol

        display.display();                       // Update OLED

        tone(BeatBuzzer, 1000);                  // Generate heartbeat sound
        delay(100);                              // Sound duration
        noTone(BeatBuzzer);                      // Stop sound
      }
    }

  } else {

    display.clearDisplay();                      // Clear OLED
    display.setTextColor(1);                     // Set text color
    display.setTextSize(1);                      // Set small text size
    display.setTextWrap(false);                  // Disable wrapping

    display.setCursor(0, 14);                    // Set cursor
    display.print("Place your finger");        // Instruction text
    display.setCursor(0, 23);                    // Move cursor
    display.print("in sensor and wait..");     // Instruction text
    display.display();                           // Update OLED
  }

  if (temp > 37 || beatAvg > 100) {              // Check abnormal temp or BPM
    digitalWrite(LMaHeartBuzzer, HIGH);           // Turn ON alarm buzzer
  } else {
    digitalWrite(LMaHeartBuzzer, LOW);            // Turn OFF alarm buzzer
  }
}
