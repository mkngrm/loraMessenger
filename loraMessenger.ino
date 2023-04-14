#include <Adafruit_GFX.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_ILI9341.h>
#include <TSC2004.h>
#include <Adafruit_NeoPixel.h>
#include <BBQ10Keyboard.h>
#include <SD.h>
#include <RH_RF95.h>

// Create an instance of the RFM95 LoRa module
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// Create an instance of the ILI9341 TFT display
#define TFT_CS 9
#define TFT_DC 10
const int TFT_WIDTH = 320;
const int TFT_HEIGHT = 240;
const int STATUS_BAR_HEIGHT = 12;
const int INPUT_BAR_HEIGHT = 20;
const int MESSAGE_HEIGHT = 20;
Adafruit_ILI9341 tft(TFT_CS, TFT_DC);

#define NEOPIXEL_PIN 11
Adafruit_NeoPixel neopixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
bool neopixel_flash = false;

const uint16_t STATUS_BAR_COLOR = ILI9341_WHITE;
const uint16_t STATUS_BAR_TEXT_COLOR = ILI9341_BLACK;

const uint16_t MESSAGE_WINDOW_COLOR = ILI9341_BLACK;
const uint16_t MESSAGE_TEXT_COLOR = ILI9341_WHITE;
const uint16_t SENT_MESSAGE_COLOR = ILI9341_GREEN;
const uint16_t RECEIVED_MESSAGE_COLOR = ILI9341_RED;
const uint16_t USERNAME_COLOR = ILI9341_BLUE;

const uint16_t INPUT_BAR_COLOR = ILI9341_WHITE;
const uint16_t INPUT_BAR_TEXT_COLOR = ILI9341_BLACK;

// Text input buffer
#define INPUT_BUFFER_SIZE 128
char inputBuffer[INPUT_BUFFER_SIZE] = "";
int inputBufferIndex = 0;

// Message array
//char* messageArray[] = {};
//int messageArrayIndex = 0;
char* messageArray[] = {"First test message","Second test message"};
int messageArrayIndex = 2;

BBQ10Keyboard keyboard;

#define DEVICE_NAME "myMessenger"
int batteryPercentage;

void setup() {
  //Serial.begin(9600);
  Serial.begin(115200);
  
  // Initialize LoRa
  if (!rf95.init())
  {
    Serial.println("LoRa init failed!");
    messageArray[messageArrayIndex] = "WARNING: Unable to initialize radio!";
    messageArrayIndex++;
    while (1);
  }
  if (!rf95.setFrequency(915.0))
  {
    Serial.println("setFrequency failed!");
    messageArray[messageArrayIndex] = "WARNING: Unable to set frequency on the radio!";
    messageArrayIndex++;
    while (1);
  }
  rf95.setTxPower(23, false); // 23dBm, +20dB boost

  // Initialize Screen
  tft.begin();
  tft.setRotation(1); // Set the rotation of the TFT display
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(MESSAGE_TEXT_COLOR);
  tft.setTextSize(2);
  tft.setCursor(0, 0);

  // Initialize Neopixel
  neopixel.begin();
  neopixel.show(); // Turn off neopixel
  
  keyboard.begin();

  //updateStatusBar();
  //drawStatusBar();
  //drawMessageArea();
  //drawInputBar(inputBuffer);
  updateScreen();

}

void loop() {
  // Wait for user input from the attached keyboard FeatherWing
  while (true) {
    if (keyboard.keyCount()) {
      const BBQ10Keyboard::KeyEvent key = keyboard.keyEvent();
      if (key.state == BBQ10Keyboard::StateRelease) {
        if (key.key == '\n') { // If enter key is pressed, send the text as a LoRa message
          inputBuffer[inputBufferIndex] = '\0'; // Null-terminate the text
          //tft.println();
          sendLoRaMessage(inputBuffer);
          break;
        } else if (key.key == '\x08' && inputBufferIndex >= 0) { // If backspace key is pressed, delete previous character
          inputBuffer[inputBufferIndex] = ' ';
          inputBufferIndex--;
          updateScreen();
        } else if (key.key >= 32 && key.key <= 126 && inputBufferIndex < 255) { // If printable character is entered, append it to text
          inputBuffer[inputBufferIndex++] = key.key;
          updateScreen();
        }
      }
    }
  
    // Check for incoming messages
    if (rf95.available()) {
      // Receive message
      uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t len = sizeof(buf);
      if (rf95.recv(buf, &len)) {
        char* receivedMessage = ((char*) buf);
        messageArray[messageArrayIndex] = receivedMessage;
        messageArrayIndex++;
        
        // Flash neopixel red
        flashNeopixel(255, 0, 0, 127);
        
        updateScreen();
      }
    }
  }
}

void drawStatusBar()
{
  // Draw status bar at the top
  tft.fillRect(0, 0, TFT_WIDTH, STATUS_BAR_HEIGHT, STATUS_BAR_COLOR);
  tft.setTextColor(STATUS_BAR_TEXT_COLOR);
  tft.setTextSize(1);
  tft.setCursor(2, 2);
  //tft.print("Device: ");
  tft.print(DEVICE_NAME);
  tft.setCursor(TFT_WIDTH - 25, 2);
  tft.print("100%");
}

void drawMessageArea()
{
  // Draw message area in the middle
  tft.fillRect(0, STATUS_BAR_HEIGHT, TFT_WIDTH, (TFT_HEIGHT - (STATUS_BAR_HEIGHT + INPUT_BAR_HEIGHT)), MESSAGE_WINDOW_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(MESSAGE_TEXT_COLOR);
  // For loop to display latest message at the bottom, with preceding above
  int messagesDisplayed = 0;
  int lineHeight = 10;
  for(int i = messageArrayIndex; i >= 0; i--) {
    tft.setCursor(5, TFT_HEIGHT - INPUT_BAR_HEIGHT - (lineHeight * messagesDisplayed));
    tft.println(messageArray[i]);
    messagesDisplayed++;
  }
}

void drawInputBar(const char* text)
{
  char message[256];
  
  // Draw input bar at the bottom
  tft.fillRoundRect(0, (TFT_HEIGHT - INPUT_BAR_HEIGHT), TFT_WIDTH, INPUT_BAR_HEIGHT, 10, INPUT_BAR_COLOR);
  tft.setTextColor(INPUT_BAR_TEXT_COLOR);
  tft.setCursor(5, (TFT_HEIGHT - INPUT_BAR_HEIGHT));
  tft.print(message);
}

void sendLoRaMessage(const char* text) {
  char message[256];
  
  snprintf(message, sizeof(message), "[%s] %s", DEVICE_NAME, text); // Add device name to the beginning of the message
  //tft.println(message);
  // Add outgoing message to message array
  messageArray[messageArrayIndex] = message;
  messageArrayIndex++;
  //updateScreen();
  
  rf95.send((uint8_t*)message, strlen(message));
  // Wait for LoRa message to be sent
  while (!rf95.waitPacketSent()) {
    delay(100);
  }       
  //delay(2000); // Delay for readability
  rf95.waitPacketSent();
  
  // Flash neopixel green
  flashNeopixel(0, 255, 0, 1);
  
  // Clear input buffer
  //inputBuffer[0] = '\0';
  for(int i = 0; i <= inputBufferIndex; i++) {
    inputBuffer[i] = 0;    
  }
  inputBufferIndex = 0;
}

void updateScreen() {
  drawStatusBar();

  drawMessageArea();

  drawInputBar(inputBuffer);
}

void flashNeopixel(uint8_t r, uint8_t g, uint8_t b, uint8_t bright) {
  neopixel.setPixelColor(0, r, g, b, 127);
  neopixel.show();
  delay(500);
  neopixel.setPixelColor(0, 0, 0, 0);
  neopixel.show();
  delay(500);
}
