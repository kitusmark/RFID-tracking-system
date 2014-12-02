//RFID Tracking System
//RFID Sensor + Arduino UNO + LCD i2c Shield + RTC Module
//System to keep track of employees at any small, cool and Open Source company

//Project for RepRapBCN Headquarters - Oct 2014
//Marc Cobler
//Jordi Calduch

//The NFC Shield is designed to be used using I2C by default. 
//It uses only 2 pins, Analog 4 and 5 fixed in hardware and Digital 2 as interrupt

//Libraries and constant definitions
//LCD shield
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
//NFC Shield
#include <Adafruit_NFCShield_I2C.h>

// Defines
#define DS1307_ADDRESS 0x68 //Address for the RTC Module I2C protocol
#define IRQ   2 //Interrupt pin used to detect the Mifare Card
#define RESET 3  // Not connected by default on the NFC Shield
#define WHITE 0x7 //Background color
#define OFF 0
#define ON 1

//Let's declare a LCD object
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
//Let's declare a NFC object too!
Adafruit_NFCShield_I2C nfc(IRQ, RESET);

//Global Scope Variables
boolean backlightState = ON;
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

//--------------------------------SETUP---------------------------------------
void setup() {
  //First we're going to setup the timer interrupt for the Time Reading
  cli(); //Stop all interrupts
  //set timer1 interrupt at 1Hz
  TCCR1A = 0;      // set entire TCCR1A register to 0
  TCCR1B = 0;      // same for TCCR1B
  TCNT1  = 0;      //initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 15624;   // = (16*10^6) / (1*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei(); //Allow all interrupts
  // put your setup code here, to run once:
  Serial.begin(9600);
  Wire.begin();
  //LCD's Setup and init
  lcd.begin(16,2); // Set the number of columns and rows
  lcd.setBacklight(WHITE);
  lcd.setCursor(1,0);
  lcd.print("RepRapBCN NFC");
  lcd.setCursor(0,1);
  lcd.print("Tracking System");
  
  nfc.begin();
  
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  
  Serial.println("Waiting for an ISO14443A Card ...");
  
  delay(3000);
  lcd.clear();
 
}
//------------------------------MAIN LOOP------------------------------------
void loop() {
  // put your main code here, to run repeatedly: 
  uint8_t buttons = lcd.readButtons(); // Need the wire.h library
  handleButtons(buttons);

  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  handleNFCReading(success);
  
}

//----------------------------FUNCTIONS-----------------------------------

void handleButtons (uint8_t buttons) {
     // This part takes care of button pressings
    if (buttons) {
    if (buttons & BUTTON_UP) {
      Serial.println("UP button pressed");
    }
    if (buttons & BUTTON_DOWN) {
      Serial.println("DOWN button pressed");
    }
    if (buttons & BUTTON_LEFT) {
      Serial.println("LEFT button pressed");
    }
    if (buttons & BUTTON_RIGHT) {
      Serial.println("RIGHT button pressed");
    }
    if (buttons & BUTTON_SELECT) {
      Serial.println("SELECT button pressed");
      //Functionality test to save power... look for debouncing code!
      if (backlightState == ON) {
        lcd.setBacklight(OFF); 
        backlightState = OFF;
      } else {
        lcd.setBacklight(ON);
        backlightState = ON;
      }
    }
  } 
} // End of handleButtons()

void handleNFCReading (uint8_t success) {
  if (success) {
    // Display some basic information about the card
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");
    
    if (uidLength == 4)
    {
      // We probably have a Mifare Classic card ... 
      Serial.println("Seems to be a Mifare Classic card (4 byte UID)");
    
      // Now we need to try to authenticate it for read/write access
      // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
      Serial.println("Trying to authenticate block 4 with default KEYA value");
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    
      // Start with block 4 (the first block of sector 1) since sector 0
      // contains the manufacturer data and it's probably better just
      // to leave it alone unless you know what you're doing
      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
    
      if (success)
      {
        Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
        uint8_t data[16];
    
        // If you want to write something to block 4 to test with, uncomment
        // the following line and this text should be read back in a minute
        //memcpy(data, (const uint8_t[]){ 'a', 'd', 'a', 'f', 'r', 'u', 'i', 't', '.', 'c', 'o', 'm', 0, 0, 0, 0 }, sizeof data);
        //success = nfc.mifareclassic_WriteDataBlock (4, data);

        // Try to read the contents of block 4
        success = nfc.mifareclassic_ReadDataBlock(4, data);
    
        if (success)
        {
          // Data seems to have been read ... spit it out
          Serial.println("Reading Block 4:");
          nfc.PrintHexChar(data, 16);
          Serial.println("");
      
          // Wait a bit before reading the card again
          delay(1000);
        }
        else
        {
          Serial.println("Ooops ... unable to read the requested block.  Try another key?");
        }
      }
      else
      {
        Serial.println("Ooops ... authentication failed: Try another key?");
      }
    }
    
    if (uidLength == 7)
    {
      // We probably have a Mifare Ultralight card ...
      Serial.println("Seems to be a Mifare Ultralight tag (7 byte UID)");
    
      // Try to read the first general-purpose user page (#4)
      Serial.println("Reading page 4");
      uint8_t data[32];
      success = nfc.mifareultralight_ReadPage (4, data);
      if (success)
      {
        // Data seems to have been read ... spit it out
        nfc.PrintHexChar(data, 4);
        Serial.println("");
    
        // Wait a bit before reading the card again
        delay(1000);
      }
      else
      {
        Serial.println("Ooops ... unable to read the requested page!?");
      }
    }
  }
}

byte bcdToDec(byte val)  {
// Convert binary coded decimal to normal decimal numbers
  return ( (val/16*10) + (val%16) );
}

void printDate() {
  // Reset the register pointer
  Wire.beginTransmission(DS1307_ADDRESS);

  byte zero = 0x00;
  Wire.write(zero);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 7);

  int second = bcdToDec(Wire.read());
  int minute = bcdToDec(Wire.read());
  int hour = bcdToDec(Wire.read() & 0b111111); //24 hour time
  int weekDay = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
  int monthDay = bcdToDec(Wire.read());
  int month = bcdToDec(Wire.read());
  int year = bcdToDec(Wire.read());

  //print the date in this format   3/1/11 23:59:59
  Serial.print(month);
  Serial.print("/");
  Serial.print(monthDay);
  Serial.print("/");
  Serial.print(year);
  Serial.print(" ");
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minute);
  Serial.print(":");
  Serial.println(second);
}

//------------------------------INTERRUPT SERVICE ROUTINE------------
ISR(TIMER1_COMPA_vect) {
 printDate(); 
}
