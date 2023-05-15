/* TO DO:
 *  Add battery percentage
 *  Setup sleep mode for extended use
 *  Fix input character deletion
 *  Fix extra characters in receive buffer
 *  Display signal strength of received messages
 *  Truncate messages that don't fit on single line
 *  GPS
 *  Speaker to alert on message receipt
 *  Send ack
 *    Resend until message acked
 *  Change screen/keyboard brightness based on ambient light -or- use keyboard buttons to increment/decrement
 *  Send broadcast when user comes online
 *  Shutoff when battery voltage too low
 *  Use keyboard button to ping users in range
 *  
 *  WHEN COMPLETE, BUMP UP TX POWER LEVEL
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

// For battery monitoring
#define VBATPIN A7

#define ILI9341_BLACK 0x0000       ///<   0,   0,   0
#define ILI9341_NAVY 0x000F        ///<   0,   0, 123
#define ILI9341_DARKGREEN 0x03E0   ///<   0, 125,   0
#define ILI9341_DARKCYAN 0x03EF    ///<   0, 125, 123
#define ILI9341_MAROON 0x7800      ///< 123,   0,   0
#define ILI9341_PURPLE 0x780F      ///< 123,   0, 123
#define ILI9341_OLIVE 0x7BE0       ///< 123, 125,   0
#define ILI9341_LIGHTGREY 0xC618   ///< 198, 195, 198
#define ILI9341_DARKGREY 0x7BEF    ///< 123, 125, 123
#define ILI9341_BLUE 0x001F        ///<   0,   0, 255
#define ILI9341_GREEN 0x07E0       ///<   0, 255,   0
#define ILI9341_CYAN 0x07FF        ///<   0, 255, 255
#define ILI9341_RED 0xF800         ///< 255,   0,   0
#define ILI9341_MAGENTA 0xF81F     ///< 255,   0, 255
#define ILI9341_YELLOW 0xFFE0      ///< 255, 255,   0
#define ILI9341_WHITE 0xFFFF       ///< 255, 255, 255
#define ILI9341_ORANGE 0xFD20      ///< 255, 165,   0
#define ILI9341_GREENYELLOW 0xAFE5 ///< 173, 255,  41
#define ILI9341_PINK 0xFC18        ///< 255, 130, 198

#define NEOPIXEL_PIN 11
Adafruit_NeoPixel neopixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
bool neopixel_flash = false;
int NEOPIXEL_BRIGHTNESS = 16; 

const uint16_t STATUS_BAR_COLOR = ILI9341_WHITE;
const uint16_t STATUS_BAR_TEXT_COLOR = ILI9341_BLACK;

const uint16_t MESSAGE_WINDOW_COLOR = ILI9341_BLACK;
const uint16_t MESSAGE_TEXT_COLOR = ILI9341_WHITE;
const uint16_t MESSAGE_ALT_TEXT_COLOR = ILI9341_DARKGREY;
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

float KEYBOARD_BACKLIGHT = 1.0;
BBQ10Keyboard keyboard;

//#define DEVICE_NAME "myMessenger"
char DEVICE_NAME[INPUT_BUFFER_SIZE]; 

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
  neopixel.setBrightness(NEOPIXEL_BRIGHTNESS);
  //neopixel.show(); // Turn off neopixel
  
  keyboard.begin();
  keyboard.setBacklight(KEYBOARD_BACKLIGHT); // Turn on keyboard backlight
  
  updateScreen(); 
  setUsername();
  
  Serial.println("Setup complete, beginning program.");
  flashNeopixel(255, 255, 255, 1);
}

void loop() ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{  
  // Wait for user input from the attached keyboard FeatherWing
  while (true) {
    // Check for incoming messages
    if (rf95.available()) {
      Serial.println("  Message available.");
      // Receive message
      uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t len = sizeof(buf);
      if (rf95.recv(buf, &len)) {
        Serial.println("  Message received!");
        // Flash neopixel red
        flashNeopixel(255, 0, 0, 1);
        char* receivedMessage = ((char*) buf);
        char formattedMessage[256];
        
        // Fix RSSI & SNR display
        int rssi = (rf95.lastRssi(), DEC);
        Serial.print(" Last RSSI: "); Serial.println(rf95.lastRssi());
        Serial.print(" Last SNR: "); Serial.println(rf95.lastSNR());
        
        snprintf(formattedMessage, sizeof(formattedMessage), "%s (%s)", receivedMessage, (String) rf95.lastRssi()); 

        messageArray[messageArrayIndex] = formattedMessage;
        messageArrayIndex++;

        drawStatusBar(); // Update battery percentage
        drawMessageArea();
      }
      // Clear receiveBuffer
      for (int i = 0; i <= len; i++) {
        buf[i] = 0;
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
        else if (key.key == 6) { // Keyboard button 1 - Ping local users to see who's available
          
        }
        else if (key.key == 17) { // Keyboard button 2 - 
          
        }
        else if (key.key == 1) { // Joystick Up - 
          
        }
        else if (key.key == 2) { // Joystick Down - 
          
        }
        else if (key.key == 3) { // Joystick Left - 
          
        }
        else if (key.key == 4) { // Joystick Right - 
          
        }
        else if (key.key == 5) { // Joystick Click - 
          
        }
        else if (key.key == 7) { // Keyboard button 3 - Decrement keyboard brightness by 10%
          if (KEYBOARD_BACKLIGHT > 0) {
            KEYBOARD_BACKLIGHT -= .25;
            keyboard.setBacklight(KEYBOARD_BACKLIGHT);
            if (KEYBOARD_BACKLIGHT == 0) {
              tft.writeCommand(0x10); // Sleep
              delay(5); // Delay for shutdown time before another command can be sent
            }
          }
        }
        else if (key.key == 18) { // Keyboard button 4 - Increment keyboard brightness by 10%
          if (KEYBOARD_BACKLIGHT < 1.0) {
            if (KEYBOARD_BACKLIGHT == 0) {
              tft.writeCommand(0x11); // Wake display
              delay(120); // Delay for pwer supplies to stabilise
            }            
            KEYBOARD_BACKLIGHT += .25;
            keyboard.setBacklight(KEYBOARD_BACKLIGHT);
            
          }          
        }
        else { // Unmapped key pressed, output value to determine usability
          Serial.printf("key: '%c' (dec %d, hex %02x)\r\n", key.key, key.key, key.key);
        }
      }
      else if (key.state == BBQ10Keyboard::StateLongPress) { // Additional functionality with a long press
        
      }      
      else { // Additional functionality with a short press
        
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
  
  Serial.println(" Completing setUsername()");
}

void drawStatusBar() //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling drawStatusBar()");

  // Draw status bar at the top and print device name
  tft.fillRect(0, 0, TFT_WIDTH, STATUS_BAR_HEIGHT, STATUS_BAR_COLOR);
  tft.setTextColor(STATUS_BAR_TEXT_COLOR);
  tft.setTextSize(1);
  tft.setCursor(2, 2);
  tft.print(DEVICE_NAME);
  
  // TO FIX: Calculate battery percentage            
  tft.setCursor(TFT_WIDTH - 25, 2);
  int batteryPercent = readVoltage();
  /*if (batteryPercent <= 40) {
    tft.setTextColor(ILI9341_YELLOW);
  }
  else if (batteryPercent <= 10) {
    tft.setTextColor(ILI9341_RED);
  }
  else {
    tft.setTextColor(ILI9341_GREEN);
  }*/

  tft.printf("%3d", batteryPercent); tft.print("%");
  
  Serial.println(" Completing drawStatusBar()");
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

  Serial.println(" Completing drawMessageArea()");
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

  Serial.println(" Completing drawInputBar()");
}

void sendLoRaMessage(const char* text) ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling sendLoRaMessage()");  
  char message[256];
  
  snprintf(message, sizeof(message), "[%s] %s", DEVICE_NAME, text); // Add device name to the beginning of the message
  Serial.print("  Message creation: ");
  Serial.println(message);

  //rf95.setModeTx();  
  if(rf95.send((uint8_t*)message, strlen(message))) {
    rf95.waitPacketSent();
    // Flash neopixel green
    flashNeopixel(0, 255, 0, 1);
    Serial.println("  Message sent!");

    messageArray[messageArrayIndex++] = message;
  }
  else {
    Serial.println("ERROR: could not send message!");
  }
  
  drawStatusBar();
  drawMessageArea();
  clearInputBuffer();
  
  Serial.println(" Completing sendLoRaMessage()");
}

void sendAck() ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  uint8_t ack[] = "*";
  rf95.send(ack, sizeof(ack));
  //rf95.waitPacketSent();
}

void updateScreen() ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling updateScreen()");
  
  drawStatusBar();
  drawMessageArea();
  drawInputBar(inputBuffer);

  Serial.println(" Completing updateScreen()");
}

void flashNeopixel(uint8_t r, uint8_t g, uint8_t b, uint8_t count) ///////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling flashNeoPixel()");

  for (int i = 0; i < count; i++) {
    neopixel.setPixelColor(0, r, g, b);
    neopixel.show();
    delay(50);
    neopixel.setPixelColor(0, 0, 0, 0);
    neopixel.show();
    delay(50);
  }
  Serial.println(" Completing flashNeoPixel()");
}

void clearInputBuffer() ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling clearInputBUffer()");

  for(int i = 0; i <= inputBufferIndex; i++) {
    inputBuffer[i] = 0;    
  }
  inputBufferIndex = 0;
  drawInputBar(inputBuffer);

  Serial.println(" inputBuffer Cleared");  
}

int readVoltage() /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  Serial.println("Calling readVoltage()"); 
   
  float voltageLevel = (analogRead(VBATPIN) * 2 * 3.3 / 1024) - 2; 
  int batteryPercent = (int) ((voltageLevel / 4.2) * 100);

  tft.begin();
  tft.setRotation(1);
  
  return batteryPercent;
  Serial.println(" Completing readVoltage()");  
}

void setBacklight() ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  // Read current ambient light

  // Calculate 
}
