#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

// WiFi configuration
const char* ssid = "YourWiFi";
const char* password = "YourWiFiPassword";

// NightScout URL
const char* nightScoutUrl = "https://yournightscouturl/api/v1/entries.json?count=1";

// NightScout API Key
const char* apiKey = "YourApiKey";

// LED and Button Configuration
#define LED_PIN     8
#define BUTTON_PIN  9
#define LED_COUNT   25
#define BRIGHTNESS  50

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Global variables for the current color and brightness
uint8_t currentRed = 0;
uint8_t currentGreen = 0;
uint8_t currentBlue = 0;
uint8_t currentBrightness = 3; // Initial brightness 1%
uint8_t savedRed = 0;
uint8_t savedGreen = 0;
uint8_t savedBlue = 0;
bool buttonPressed = false;

unsigned long lastUpdate = 0; // Time variable for update

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  pinMode(BUTTON_PIN, INPUT_PULLUP); // Configure the button pin

  strip.begin();
  strip.show();
  strip.setBrightness(0);

  fadeFromBlack(5);
  rainbowColors(3);
  fadeToBlack(5);

  setBrightness(currentBrightness);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    patternWifiNotConnected();
    delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    acquireColorAndRunPattern();
  }
}

void loop() {
  unsigned long currentTime = millis();
  if (currentTime - lastUpdate >= 60000) { // Check if a minute has passed
    acquireColorAndRunPattern();
    lastUpdate = currentTime;
  }

  if (WiFi.status() != WL_CONNECTED) {
    patternWifiNotConnected();
  } else {
    if (digitalRead(BUTTON_PIN) == LOW && !buttonPressed) {
      buttonPressed = true;
      delay(50); // Debounce delay
      while (digitalRead(BUTTON_PIN) == LOW) {
        // Wait for the button to be released
      }
      runAnimationWithButtonPress();
    } else {
      buttonPressed = false;
    }
  }

  delay(100); // Small delay to avoid overloading the loop
}

void applyCurrentSettings() {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(currentRed, currentGreen, currentBlue));
  }
  strip.setBrightness(currentBrightness); // Set the brightness
  strip.show();
}

void setColor(uint8_t red, uint8_t green, uint8_t blue) {
  currentRed = red;
  currentGreen = green;
  currentBlue = blue;
  applyCurrentSettings();
}

void setBrightness(uint8_t brightness) {
  currentBrightness = brightness < 3 ? 3 : brightness; // Minimum brightness 1% (3 out of 255)
  applyCurrentSettings();
}

void saveCurrentColor() {
  savedRed = currentRed;
  savedGreen = currentGreen;
  savedBlue = currentBlue;
}

void restoreSavedColor() {
  setColor(savedRed, savedGreen, savedBlue);
}

void patternWifiNotConnected() {
  while (WiFi.status() != WL_CONNECTED) { // Continue until WiFi connection is established
    setColor(0, 0, 0); // off
    delay(2000);
    setColor(70, 70, 70); // Gray
    delay(100);
    setColor(0, 0, 0); // off
    delay(100);
    setColor(70, 70, 70); // Gray
    delay(100);
  }
}

// Functions for the startup pattern
void fadeFromBlack(uint8_t wait) {
  for (int j = 0; j <= BRIGHTNESS; j++) {
    strip.setBrightness(j);
    strip.show();
    delay(wait);
  }
}

void rainbowColors(int duration) {
  unsigned long startTime = millis();
  while (millis() - startTime < duration * 1000) {
    for (uint32_t firstPixelHue = 0; firstPixelHue < 65536; firstPixelHue += 256) {
      for (int i = 0; i < strip.numPixels(); i++) {
        uint32_t pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue, 255, 255)));
      }
      strip.show();
      delay(3); // Adjust delay if needed for faster/slower color change
    }
  }
}

void fadeToBlack(uint8_t wait) {
  for (int j = BRIGHTNESS; j >= 0; j--) {
    strip.setBrightness(j);
    strip.show();
    delay(wait);
  }
  strip.clear(); // Ensure all pixels are turned off
  strip.show();
}

void acquireColorAndRunPattern() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String urlWithApiKey = String(nightScoutUrl) + "&api_secret=" + apiKey;
    http.begin(urlWithApiKey);

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();

      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);
      if (error) { return; }

      if (doc.containsKey("status") && doc["status"] == 401) { return; }

      float glucoseValue = doc[0]["sgv"];
      const char* direction = doc[0]["direction"];

      // Set the base color based on glucose values
      if (glucoseValue < 60) {
        setColor(70, 0, 255); // Purple
      } else if (glucoseValue < 70) {
        setColor(0, 0, 255); // Blue
      } else if (glucoseValue < 90) {
        setColor(0, 70, 255); // Cyan
      } else if (glucoseValue < 110) {
        setColor(0, 255, 0); // Green
      } else if (glucoseValue < 130) {
        setColor(70, 255, 0); // Green-Yellow
      } else if (glucoseValue < 160) {
        setColor(255, 255, 0); // Yellow
      } else if (glucoseValue < 180) {
        setColor(255, 70, 0); // Orange
      } else if (glucoseValue < 220) {
        setColor(255, 0, 0); // Red
      } else {
        setColor(255, 0, 255); // Magenta
      }

      // Apply the lighting patterns based on glucose direction
      if (strcmp(direction, "DoubleUp") == 0) {
        patternDoubleUp();
      } else if (strcmp(direction, "SingleUp") == 0) {
        patternSingleUp();
      } else if (strcmp(direction, "FortyFiveUp") == 0) {
        patternFortyFiveUp();
      } else if (strcmp(direction, "FortyFiveDown") == 0) {
        patternFortyFiveDown();
      } else if (strcmp(direction, "SingleDown") == 0) {
        patternSingleDown();
      } else if (strcmp(direction, "DoubleDown") == 0) {
        patternDoubleDown();
      } else if (strcmp(direction, "Flat") == 0) {
        patternFlat();
      }

      String trend = direction;
      for (int i = 0; i < 2; i++) {
        runAnimation(glucoseValue, trend);
        delay(2000);
      }

    }
    http.end();
  }
}

void runAnimationWithButtonPress() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String urlWithApiKey = String(nightScoutUrl) + "&api_secret=" + apiKey;
    http.begin(urlWithApiKey);

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();

      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);
      if (error) { return; }

      if (doc.containsKey("status") && doc["status"] == 401) { return; }

      float glucoseValue = doc[0]["sgv"];
      const char* direction = doc[0]["direction"];
      String trend = direction;
      runAnimation(glucoseValue, trend);
    }
    http.end();
  }
}

// Functions to draw numbers and arrows
void setMatrixPixelColor(int x, int y, uint32_t color) {
  if (x >= 0 && x < 5 && y >= 0 && y < 5) {
    int rotatedX = 4 - y;
    int rotatedY = x;
    strip.setPixelColor(rotatedY * 5 + rotatedX, color); // Rotated position
  }
}

void clearMatrix(uint32_t color) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void drawNumber(int num, int offset, uint32_t color) {
  const uint8_t numbers[10][5] = {
    {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
    {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
    {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
    {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
    {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
    {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
    {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
    {0b111, 0b001, 0b010, 0b100, 0b100}, // 7
    {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
    {0b111, 0b101, 0b111, 0b001, 0b111}  // 9
  };

  for (int y = 0; y < 5; y++) {
    for (int x = 0; x < 3; x++) {
      if (numbers[num][y] & (1 << (2 - x))) {
        setMatrixPixelColor(offset + x, y, color);
      }
    }
  }
}

void drawSingleUpArrow(int offset, uint32_t color) {
  setMatrixPixelColor(offset + 2, 4, color);
  setMatrixPixelColor(offset + 2, 3, color);
  setMatrixPixelColor(offset + 2, 2, color);
  setMatrixPixelColor(offset + 2, 1, color);
  setMatrixPixelColor(offset + 2, 0, color);
  setMatrixPixelColor(offset + 1, 1, color);
  setMatrixPixelColor(offset + 3, 1, color);
}

void drawDoubleUpArrow(int offset, uint32_t color) {
  drawSingleUpArrow(offset, color);
  drawSingleUpArrow(offset + 4, color);
}

void drawFortyFiveUpArrow(int offset, uint32_t color) {
  setMatrixPixelColor(offset + 0, 4, color);
  setMatrixPixelColor(offset + 1, 3, color);
  setMatrixPixelColor(offset + 2, 2, color);
  setMatrixPixelColor(offset + 3, 1, color);
  setMatrixPixelColor(offset + 4, 0, color);
  setMatrixPixelColor(offset + 3, 0, color);
  setMatrixPixelColor(offset + 4, 1, color);
  setMatrixPixelColor(offset + 2, 0, color);
  setMatrixPixelColor(offset + 4, 2, color);
}

void drawSingleDownArrow(int offset, uint32_t color) {
  setMatrixPixelColor(offset + 2, 0, color);
  setMatrixPixelColor(offset + 2, 1, color);
  setMatrixPixelColor(offset + 2, 2, color);
  setMatrixPixelColor(offset + 2, 3, color);
  setMatrixPixelColor(offset + 2, 4, color);
  setMatrixPixelColor(offset + 1, 3, color);
  setMatrixPixelColor(offset + 3, 3, color);
}

void drawDoubleDownArrow(int offset, uint32_t color) {
  drawSingleDownArrow(offset, color);
  drawSingleDownArrow(offset + 4, color);
}

void drawFortyFiveDownArrow(int offset, uint32_t color) {
  setMatrixPixelColor(offset + 0, 0, color);
  setMatrixPixelColor(offset + 1, 1, color);
  setMatrixPixelColor(offset + 2, 2, color);
  setMatrixPixelColor(offset + 3, 3, color);
  setMatrixPixelColor(offset + 4, 4, color);
  setMatrixPixelColor(offset + 3, 4, color);
  setMatrixPixelColor(offset + 4, 3, color);
  setMatrixPixelColor(offset + 2, 4, color);
  setMatrixPixelColor(offset + 4, 2, color);
}

void drawFlatArrow(int offset, uint32_t color) {
  setMatrixPixelColor(offset + 0, 2, color);
  setMatrixPixelColor(offset + 1, 2, color);
  setMatrixPixelColor(offset + 2, 2, color);
  setMatrixPixelColor(offset + 3, 2, color);
  setMatrixPixelColor(offset + 4, 2, color);
  setMatrixPixelColor(offset + 3, 1, color);
  setMatrixPixelColor(offset + 3, 3, color);
}

void runAnimation(int glucoseValue, String trend) {
    uint32_t bgColor = strip.Color(currentRed, currentGreen, currentBlue); // Use the current background color
    uint32_t contrastColor;

    if (currentRed == 70 && currentGreen == 0 && currentBlue == 255) {
        contrastColor = strip.Color(70, 70, 70); // Gray for Purple
    } else if (currentRed == 0 && currentGreen == 0 && currentBlue == 255) {
        contrastColor = strip.Color(70, 70, 70); // Gray for Blue
    } else if (currentRed == 0 && currentGreen == 70 && currentBlue == 255) {
        contrastColor = strip.Color(0, 0, 0); // Black for Cyan
    } else if (currentRed == 0 && currentGreen == 255 && currentBlue == 0) {
        contrastColor = strip.Color(0, 0, 0); // Black for Green
    } else if (currentRed == 70 && currentGreen == 255 && currentBlue == 0) {
        contrastColor = strip.Color(0, 0, 0); // Black for Yellow-Green
    } else if (currentRed == 255 && currentGreen == 255 && currentBlue == 0) {
        contrastColor = strip.Color(0, 0, 0); // Black for Yellow
    } else if (currentRed == 255 && currentGreen == 70 && currentBlue == 0) {
        contrastColor = strip.Color(0, 0, 0); // Black for Orange
    } else if (currentRed == 255 && currentGreen == 0 && currentBlue == 0) {
        contrastColor = strip.Color(70, 70, 70); // Gray for Red
    } else if (currentRed == 255 && currentGreen == 0 && currentBlue == 255) {
        contrastColor = strip.Color(0, 0, 0); // Black for Magenta
    }

    // Clear the screen for 100 ms before the animation
    clearMatrix(bgColor);
    delay(100);

    // Convert the glucose value to a string and draw the numbers
    String glucoseStr = String(glucoseValue);
    int offset = 5;
    for (int i = 0; i < glucoseStr.length(); i++) {
        int num = glucoseStr.charAt(i) - '0';
        drawNumber(num, offset, contrastColor);
        offset += 4; // Space between numbers
    }

    // Draw the trend after the numbers
    if (trend == "DoubleDown") {
        drawDoubleDownArrow(offset, contrastColor);
    } else if (trend == "Flat") {
        drawFlatArrow(offset, contrastColor);
    } else if (trend == "SingleUp") {
        drawSingleUpArrow(offset, contrastColor);
    } else if (trend == "DoubleUp") {
        drawDoubleUpArrow(offset, contrastColor);
    } else if (trend == "FortyFiveUp") {
        drawFortyFiveUpArrow(offset, contrastColor);
    } else if (trend == "SingleDown") {
        drawSingleDownArrow(offset, contrastColor);
    } else if (trend == "FortyFiveDown") {
        drawFortyFiveDownArrow(offset, contrastColor);
    } // Add other conditions for other trends if necessary

    // Show the animation
    for (int i = 5; i >= -offset - 8; i--) {
        // Draw only the pixels that change state
        for (int j = 0; j < strip.numPixels(); j++) {
            strip.setPixelColor(j, bgColor);
        }

        // Redraw the numbers
        int tempOffset = i;
        for (int j = 0; j < glucoseStr.length(); j++) {
            int num = glucoseStr.charAt(j) - '0';
            drawNumber(num, tempOffset, contrastColor);
            tempOffset += 4; // Space between numbers
        }

        // Redraw the trend
        if (trend == "DoubleDown") {
            drawDoubleDownArrow(tempOffset, contrastColor);
        } else if (trend == "Flat") {
            drawFlatArrow(tempOffset, contrastColor);
        } else if (trend == "SingleUp") {
            drawSingleUpArrow(tempOffset, contrastColor);
        } else if (trend == "DoubleUp") {
            drawDoubleUpArrow(tempOffset, contrastColor);
        } else if (trend == "FortyFiveUp") {
            drawFortyFiveUpArrow(tempOffset, contrastColor);
        } else if (trend == "SingleDown") {
            drawSingleDownArrow(tempOffset, contrastColor);
        } else if (trend == "FortyFiveDown") {
            drawFortyFiveDownArrow(tempOffset, contrastColor);
        } // Add other conditions for other trends if necessary

        strip.show();
        delay(150); // Scroll speed
    }

    // Clear the screen for 100 ms after the animation
    clearMatrix(bgColor);
    delay(100);
}

void patternFlat() {
  delay(5000); // 5-second delay
}

void patternDoubleUp() {
  for (int i = 0; i < 10; i++) {
    smoothBrightnessTransition(3, 10, 250); // From 3 to 10 out of 255 in 0.5 seconds
    smoothBrightnessTransition(10, 3, 250); // From 10 to 3 out of 255 in 0.5 seconds
  }
}

void patternSingleUp() {
  for (int i = 0; i < 5; i++) {
    smoothBrightnessTransition(3, 10, 2000); // From 3 to 10 out of 255 in 2 seconds
    smoothBrightnessTransition(10, 3, 2000); // From 10 to 3 out of 255 in 2 seconds
  }
}

void patternFortyFiveUp() {
  for (int i = 0; i < 3; i++) {
    smoothBrightnessTransition(3, 10, 4000); // From 3 to 10 out of 255 in 4 seconds
    smoothBrightnessTransition(10, 3, 4000); // From 10 to 3 out of 255 in 4 seconds
  }
}

void patternFortyFiveDown() {
  for (int i = 0; i < 3; i++) {
    saveCurrentColor();
    setColor(0, 0, 0); // off
    delay(4000);
    restoreSavedColor(); // Current color
    delay(4000);
  }
}

void patternSingleDown() {
  for (int i = 0; i < 5; i++) {
    saveCurrentColor();
    setColor(0, 0, 0); // off
    delay(2000);
    restoreSavedColor(); // Current color
    delay(2000);
  }
}

void patternDoubleDown() {
  for (int i = 0; i < 10; i++) {
    saveCurrentColor();
    setColor(0, 0, 0); // off
    delay(250);
    restoreSavedColor(); // Current color
    delay(250);
  }
}

void smoothBrightnessTransition(uint8_t from, uint8_t to, uint16_t duration) {
  int steps = 100;
  int delayTime = duration / (steps * 2); // Speed up the transition
  for (int i = 0; i <= steps; i++) {
    uint8_t intermediateBrightness = from + ((to - from) * i) / steps;
    setBrightness(intermediateBrightness);
    delay(delayTime);
  }
}
