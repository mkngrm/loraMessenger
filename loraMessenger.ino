/* TO DO:
 *  Add battery percentage
 *  Setup sleep mode for extended use
 *  Refine tx power settings
 *  Fix input character deletion
 *  Fix extra characters in receive buffer
 *  Display signal strength of received messages
 *  GPS
 *  Speaker to alert on message receipt
 *  Resend until message acked (possible?)
 */

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
const int MESSAGE_HEIGHT = 10;
const int MAX_MESSAGES_DISPLAYED = 20;
Adafruit_ILI9341 tft(TFT_CS, TFT_DC);

#define ILI9341_BLACK   0x0000
#define ILI9341_GRAY    0x8410
#define ILI9341_WHITE   0xFFFF
#define ILI9341_RED     0xF800
//#define ILI9341_ORANGE  0xFA60
#define ILI9341_YELLOW  0xFFE0  
#define ILI9341_LIME    0x07FF
#define ILI9341_GREEN   0x07E0
#define ILI9341_CYAN    0x07FF
#define ILI9341_AQUA    0x04FF
#define ILI9341_BLUE    0x001F
#define ILI9341_MAGENTA 0xF81F
//#define ILI9341_PINK    0xF8FF

// For battery measurement
#define VBATPIN A7

#define NEOPIXEL_PIN 11
Adafruit_NeoPixel neopixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
bool neopixel_flash = false;

const uint16_t STATUS_BAR_COLOR = ILI9341_WHITE;
const uint16_t STATUS_BAR_TEXT_COLOR = ILI9341_BLACK;

const uint16_t MESSAGE_WINDOW_COLOR = ILI9341_BLACK;
const uint16_t MESSAGE_TEXT_COLOR = ILI9341_WHITE;
const uint16_t MESSAGE_ALT_TEXT_COLOR = ILI9341_GRAY;
const uint16_t SENT_MESSAGE_COLOR = ILI9341_GREEN;
const uint16_t RECEIVED_MESSAGE_COLOR = ILI9341_RED;
const uint16_t USERNAME_COLOR = ILI9341_BLUE;

const uint16_t INPUT_BAR_COLOR = ILI9341_WHITE;
const uint16_t INPUT_BAR_TEXT_COLOR = ILI9341_BLACK;

// Text input buffer
#define INPUT_BUFFER_SIZE 256
char inputBuffer[INPUT_BUFFER_SIZE] = "";
int inputBufferIndex = 0;

// Message array
String messageArray[255];
int messageArrayIndex = 0;
//String messageArray[255] = { "Message 1","Message 2", "Message 3" };
//int messageArrayIndex = 2;

BBQ10Keyboard keyboard;

//#define DEVICE_NAME "myMessenger"
char DEVICE_NAME[INPUT_BUFFER_SIZE]; 

int batteryPercentage;

void setup() //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.begin(4800);
  Serial.println("Beginning setup...");
  
  // Initialize LoRa
  if (!rf95.init()) {
    Serial.println("LoRa init failed!");
  }
  if (!rf95.setFrequency(915.0)) {
    Serial.println("setFrequency failed!");
  }
  if (!rf95.setModemConfig(RH_RF95::Bw31_25Cr48Sf512)) {
    Serial.println("setModemConfig failed!");
  }
  //rf95.setTxPower(23, false); // 23dBm, +20dB boost

  // Initialize Screen
  tft.begin();
  tft.setRotation(1);

  // Initialize Neopixel
  neopixel.begin();
  //neopixel.show(); // Turn off neopixel
  
  keyboard.begin();
  
  updateScreen();
  
  setUsername();
  
  Serial.println("Setup complete, beginning program.");
  flashNeopixel(255, 255, 255, 10);
}

void loop() ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{  
  // Wait for user input from the attached keyboard FeatherWing
  while (true) {
    // Check for incoming messages
    //rf95.setModeRx();
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
        
        //updateScreen();
        drawMessageArea();

        for (int i = 0; i <= len; i++) {
          buf[i] = 0;
        }
      }
    }

    if (keyboard.keyCount()) {
      const BBQ10Keyboard::KeyEvent key = keyboard.keyEvent();
      if (key.state == BBQ10Keyboard::StateRelease) {
        if (key.key >= 32 && key.key <= 126 && inputBufferIndex < 255) { // If printable character is entered, append it to text
          inputBuffer[inputBufferIndex++] = key.key;
          drawInputBar(inputBuffer);       
        } 
        else if (key.key == '\x08' && inputBufferIndex >= 0) { // If backspace key is pressed, delete previous character
          inputBuffer[inputBufferIndex--] = 0;
          drawInputBar(inputBuffer);
        }
        else if (key.key == '\n' && inputBufferIndex > 0) { // If enter key is pressed, send the text as a LoRa message
          inputBuffer[inputBufferIndex] = '\0'; // Null-terminate the text
          sendLoRaMessage((char*) inputBuffer);
          break; 
        }
      }
    }
  }
}

void setUsername() ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling setUsername()");
  
  tft.setTextColor(MESSAGE_TEXT_COLOR);
  tft.setTextSize(1); 
  tft.setCursor(0, TFT_HEIGHT - INPUT_BAR_HEIGHT - 10);  
  tft.println("Please enter your username: ");

  DEVICE_NAME[0] = 0;
  while (DEVICE_NAME[0] == 0) {
    if (keyboard.keyCount()) {
      const BBQ10Keyboard::KeyEvent key = keyboard.keyEvent();
      if (key.state == BBQ10Keyboard::StateRelease) {
        if (key.key >= 32 && key.key <= 126 && inputBufferIndex < 255) { // If printable character is entered, append it to text
          inputBuffer[inputBufferIndex++] = key.key;
          drawInputBar(inputBuffer);       
        } 
        else if (key.key == '\x08' && inputBufferIndex >= 0) { // If backspace key is pressed, delete previous character
          inputBuffer[inputBufferIndex--] = 0;
          drawInputBar(inputBuffer);
        }
        else if (key.key == '\n' && inputBufferIndex > 0) { // If enter key is pressed, save the device input
          inputBuffer[inputBufferIndex] = '\0'; // Null-terminate the text
          for (int i = 0; i <= inputBufferIndex; i++) {
            DEVICE_NAME[i] = inputBuffer[i];          
          }
        }
      }
    }
  }

  clearInputBuffer();
  updateScreen();
  
  Serial.println("Completing setUsername()");
}

void drawStatusBar() //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling drawStatusBar()");

  // Draw status bar at the top
  tft.fillRect(0, 0, TFT_WIDTH, STATUS_BAR_HEIGHT, STATUS_BAR_COLOR);
  tft.setTextColor(STATUS_BAR_TEXT_COLOR);
  tft.setTextSize(1);
  tft.setCursor(2, 2);
  //tft.print("Device: ");
  tft.print(DEVICE_NAME);
  
  tft.setCursor(TFT_WIDTH - 25, 2);
  // TO FIX: Calculate battery percentage
  /*float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  Serial.print("VBat: " ); Serial.println(measuredvbat);
  */
  tft.print("100%");

  Serial.println("Completing drawStatusBar()");
}

void drawMessageArea() ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling drawMessageArea()");
  
  // Draw message area in the middle
  tft.fillRect(0, STATUS_BAR_HEIGHT, TFT_WIDTH, (TFT_HEIGHT - (STATUS_BAR_HEIGHT + INPUT_BAR_HEIGHT)), MESSAGE_WINDOW_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(MESSAGE_TEXT_COLOR);
  // For loop to display latest message at the bottom, with preceding above
  int messagesDisplayed = 0;
  for(int i = messageArrayIndex; i >= 0 && messagesDisplayed <= MAX_MESSAGES_DISPLAYED; i--) {
    if(i % 2 == 0) {
        tft.setTextColor(MESSAGE_TEXT_COLOR);
    }    
    else {
        tft.setTextColor(MESSAGE_ALT_TEXT_COLOR);
    }
    tft.setCursor(0, TFT_HEIGHT - INPUT_BAR_HEIGHT - (MESSAGE_HEIGHT * messagesDisplayed));
    tft.println((String) messageArray[i]);  
    messagesDisplayed++;
  }

  Serial.println("Completing drawMessageArea()");
}

void drawInputBar(const char* text) ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling drawInputBar()");
  
  // Draw input bar at the bottom
  //tft.fillRoundRect(0, (TFT_HEIGHT - INPUT_BAR_HEIGHT), TFT_WIDTH, INPUT_BAR_HEIGHT, 10, INPUT_BAR_COLOR);
  tft.fillRect(0, (TFT_HEIGHT - INPUT_BAR_HEIGHT), TFT_WIDTH, INPUT_BAR_HEIGHT, INPUT_BAR_COLOR);
  tft.setTextColor(INPUT_BAR_TEXT_COLOR);
  tft.setTextSize(2);
  tft.setCursor(5, (TFT_HEIGHT - INPUT_BAR_HEIGHT + 1));
  tft.print(text);

  Serial.println("Completing drawInputBar()");
}

void sendLoRaMessage(const char* text) ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling sendLoRaMessage()");
  flashNeopixel(0, 255, 0, 10);
  
  char message[256];
  
  snprintf(message, sizeof(message), "[%s] %s", DEVICE_NAME, text); // Add device name to the beginning of the message
  Serial.print("  Message creation: ");
  Serial.println(message);

  //rf95.setModeTx();  
  if(rf95.send((uint8_t*)message, strlen(message))) {
    Serial.print("Adding message to the array in spot #");
    Serial.print(messageArrayIndex);
    Serial.print(": ");
    Serial.println(message);
   
    messageArray[messageArrayIndex++] = message;
    //messageArrayIndex++; 
    
    rf95.waitPacketSent();    
    Serial.println("Message sent!");
  }
  else {
    Serial.println("ERROR: could not send message!");
  }
  drawMessageArea();

  // Wait for LoRa message to be sent
  //rf95.waitPacketSent();
  //rf95.setModeIdle();
  
  // Flash neopixel green
  flashNeopixel(0, 255, 0, 1);
  
  clearInputBuffer();

  Serial.println("Completing sendLoRaMessage()");
  flashNeopixel(0, 255, 0, 10);
}

void updateScreen() ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling updateScreen()");
  
  drawStatusBar();

  drawMessageArea();

  drawInputBar(inputBuffer);

  Serial.println("Completing updateScreen()");
}

void flashNeopixel(uint8_t r, uint8_t g, uint8_t b, uint8_t bright) ///////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling flashNeoPixel()");
  
  neopixel.setPixelColor(0, r, g, b, bright);
  neopixel.show();
  delay(50);
  neopixel.setPixelColor(0, 0, 0, 0);
  neopixel.show();
  delay(50);

  Serial.println("Calling flashNeoPixel()");
}

void clearInputBuffer() ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  for(int i = 0; i <= inputBufferIndex; i++) {
    inputBuffer[i] = 0;    
  }
  inputBufferIndex = 0;
  drawInputBar(inputBuffer);
}
