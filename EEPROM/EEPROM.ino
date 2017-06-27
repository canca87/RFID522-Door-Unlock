/*
  Arduino RFID Access Control

  Security !

  To keep it simple we are going to use Tag's Unique IDs
  as only method of Authenticity. It's simple and not hacker proof.
  If you need security, don't use it unless you modify the code

  The program uses a Teensy 2.0 controller simulating the 'serial+kb+mouse+joystick'
  The teensy sends commands to the PC when a valid card arrives.
  When the card is removed from the scanner, a different set of commands are sent to the PC.

  Copyright (C) 2015 Omer Siar Baysal

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

// -------INCLUDES---------------------
#include <EEPROM.h>     // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>        // RC522 Module uses SPI protocol
#include "MFRC522.h"	  // Library for Mifare RC522 Devices
#include "MyTypes.h"    // Structs for storing passwords and card links

/*
	For visualizing whats going on hardware
	we need some leds and
	to control door lock a relay and a wipe button
	(or some other hardware)
	Used common cathode led,digitalWriting HIGH turns ON led
	Mind that if you are going to use common cathode led or
	just seperate leds, reverse the below defines:
 */
//------ DEFINES ----------------
#define LED_ON HIGH
#define LED_OFF LOW

// The UART inteface is on pins 7 and 8. Leaving them free for future use (bluetooth)
#define redLed 10		// Set Led Pins
#define greenLed 13
#define blueLed 22

/*
  We need to define MFRC522's pins and create instance
  Pin layout should be as follows (on Teensy 2.0):
  MOSI: Pin 2
  MISO: Pin 3
  SCK : Pin 1
  SS : Pin 0
  RST : Pin 4 (Configurable)
  IRQ : Pin 5 - not needed (yet?)
  look MFRC522 Library for
  other Arduinos' pin configuration 
 */

#define SS_PIN 0
#define RST_PIN 4

//--------VARIABLES ------------
boolean match = false;          // initialize card match to false
boolean programMode = false;	// initialize programming mode to false

int successRead;		// Variable integer to keep if we have Successful Read from Reader
int cardPresent;    // Variable to keep if the card is still present on the reader

byte readCard[4];		// Stores scanned ID read from RFID Module
byte masterCard[4];		// Stores master card's ID read from EEPROM

//------ OBJECTS------------------
MFRC522 mfrc522(SS_PIN, RST_PIN);	// Create MFRC522 instance.
record myRecord; //stores the UID and password associated with the card

///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
  //Arduino Pin Configuration
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);

  digitalWrite(redLed, LED_OFF);	// Make sure led is off
  digitalWrite(greenLed, LED_OFF);	// Make sure led is off
  digitalWrite(blueLed, LED_OFF);	// Make sure led is off

  //Protocol Configuration
  Serial.begin(9600);	 // Initialize serial communications with PC (usb speed, baud is irrelevant)
  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();    // Initialize MFRC522 Hardware
  
  //If you set Antenna Gain to Max it will increase reading distance
  //mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  
  Serial.println(F("Access Control v4.0"));   // For debugging purposes
  ShowReaderDetails();	// Show details of PCD - MFRC522 Card Reader details

  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(1) != 143) {  		
    Serial.println(F("No Master Card Defined"));
    Serial.println(F("Scan A PICC to Define as Master Card"));
    do {
      successRead = getID();            // sets successRead to 1 when we get read from reader otherwise 0
      digitalWrite(blueLed, LED_ON);    // Visualize Master Card need to be defined
      delay(200);
      digitalWrite(blueLed, LED_OFF);
      delay(200);
    }
    while (!successRead);                  // Program will not go further while you not get a successful read
    int j = 0;
    for ( ; j < 4; j++ ) {        // Loop 4 times
      EEPROM.write( 2 + j, readCard[j] );  // Write scanned PICC's UID to EEPROM, start from address 3
    }
    for ( ; j < 32; j++) {
     if (EEPROM.read(2 + j) == 0) {              //If EEPROM address 0
        // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
      }
      else {
        EEPROM.write(2 + j, 0);       // if not write 0 to clear, it takes 3.3mS
      }
    }
    EEPROM.write(1, 143);                  // Write to EEPROM we defined Master Card.
    Serial.println(F("Master Card Defined"));
  }
  Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));
  for ( int i = 0; i < 4; i++ ) {          // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2 + i);    // Write it to masterCard
    //Serial.print(masterCard[i], HEX);
    Serial.print("**");
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything Ready"));
  Serial.println(F("Waiting PICCs to be scanned"));
  cycleLeds();    // Everything ready lets give user some feedback by cycling leds
  /* //Use this for debug: dumps the contents of the EEPROM to the serial port (only first 100 bytes)
  for (int i = 0; i<100; i++) {
    Serial.print(EEPROM.read(i),HEX);
    Serial.print(" ");
  }
  Serial.println("");
  */
}


///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop () {
  do {
    successRead = getID(); 	// sets successRead to 1 when we get read from reader otherwise 0
        if (programMode) {
      cycleLeds();              // Program Mode cycles through RGB waiting to read a new card
    }
    else {
      normalModeOn(); 		// Normal mode, blue Power LED is on, all others are off
    }
  }while (!successRead); 	//the program will not go further while you not get a successful read
  
  if (programMode) {
    if ( isMaster(readCard) ) { //If master card scanned again exit program mode
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Program Mode"));
      Serial.println(F("-----------------------------"));
      programMode = false;
      return;
    }
    else {
      if ( findID(readCard) ) { // If scanned card is known delete it
        Serial.println(F("I know this PICC, removing..."));
        deleteID(readCard);
        Serial.println("-----------------------------");
      }
      else {                    // If scanned card is not known add it
        Serial.println(F("I do not know this PICC, adding..."));
        Serial.println(F("Please enter your login password:"));
        Serial.setTimeout(10000); //10 second time out.
        String pwStr = Serial.readStringUntil('\n');
        const char * c = pwStr.c_str();
        strncpy(myRecord.pw, c, strlen(c));
        myRecord.pwLength = strlen(c);
        writeID(readCard,&myRecord);
        Serial.println(F("-----------------------------"));
      }
    }
  }
  else {
    if ( isMaster(readCard) ) {  	// If scanned card's ID matches Master Card's ID enter program mode
      programMode = true;
      Serial.println(F("Hello Master - Entered Program Mode"));
      int count = EEPROM.read(0); 	// Read the first Byte of EEPROM that
      Serial.print(F("I have "));    	// stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");
      Serial.println(F("Scan a PICC to ADD or REMOVE"));
      Serial.println(F("-----------------------------"));
    }
    else {
      if ( findID(readCard) ) {	// If not, see if the card is in the EEPROM
        Serial.println(F("Welcome, You shall pass"));
        granted();
        cardPresent = true;
      }
      else {			// If not, show that the ID was not valid
        Serial.println(F("You shall not pass"));
        cardPresent = false;
      }
    }
  }
  
  if (!programMode && cardPresent){ //we we get here and we are not in programming mode, then a valid card is present. Wait here
    do{
      cardPresent = checkID();
    }while(cardPresent);
  }
}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted (void) {
  digitalWrite(blueLed, LED_OFF); 	// Turn off blue LED
  digitalWrite(redLed, LED_OFF); 	// Turn off red LED
  digitalWrite(greenLed, LED_ON); 	// Turn on green LED
  CardArriveActions();
  delay(1000);  
}

///////////////////////////////////// Access Actions ///////////////////////////////////////
void CardArriveActions(void){
  Serial.println("Access Granted");
  
  // press and hold CTRL
  Keyboard.set_modifier(MODIFIERKEY_CTRL);
  Keyboard.send_now();
  
  // press ALT while still holding CTRL
  Keyboard.set_modifier(MODIFIERKEY_CTRL | MODIFIERKEY_ALT);
  Keyboard.send_now();
  
  // press DELETE, while CLTR and ALT still held
  Keyboard.set_key1(KEY_DELETE);
  Keyboard.send_now();
  
  // release all the keys at the same instant
  Keyboard.set_modifier(0);
  Keyboard.set_key1(0);
  Keyboard.send_now();
  //short delay for the gui to load the login window
  delay(500);
  //enter the password (this needs to be stored along with the card details!)
  Keyboard.println(myRecord.pw);
  
}

///////////////////////////////////// Access Finished Actions ///////////////////////////////
void CardRemoveActions(void){
  Serial.println("Access Complete. Goodbye!");
  
  //keyboard WIN+R for run
    Keyboard.set_modifier(MODIFIERKEY_GUI);
    Keyboard.send_now();
    Keyboard.set_key1(KEY_L);
    Keyboard.send_now();
    Keyboard.set_modifier(0);
    Keyboard.set_key1(0);
    Keyboard.send_now();
    //short delay for the gui to load the run window
    delay(50);
    Serial.println("Goodbye!");
    
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied(void) {
  Serial.println("Access Denied");
  digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
  digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
  digitalWrite(redLed, LED_ON); 	// Turn on red LED
  delay(1000);
}

///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
int getID(void) {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for (int i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    //Serial.print(readCard[i], HEX);
    Serial.print("**");
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

///////////////////////////////////////// Check PICC's UID ///////////////////////////////////
int checkID(void) {
  byte bufferATQA[2]; //buffer to store the reply from the controller
  byte bufferSize = sizeof(bufferATQA); //size of the buffer
  MFRC522::StatusCode result = mfrc522.PICC_WakeupA(bufferATQA, &bufferSize); //wake up the reader

  if (mfrc522.PICC_ReadCardSerial()){ //read the serial of the card
    //check if the serial matches the current valid ID.
    //Serial.println(F("Scanned PICC still there:"));
    for (int i = 0; i < 4; i++) {  //
      if (readCard[i] != mfrc522.uid.uidByte[i]){
        //miss-matched ID. wrong card!
        mfrc522.PICC_HaltA(); // Stop reading
        if (cardPresent){
          CardRemoveActions();
        }
        return 0;
      }
    }
    cardPresent = true;
    mfrc522.PICC_HaltA(); // Stop reading
    return 1;
  }
            
  else {
    if (cardPresent){
      CardRemoveActions();
    }
    return 0; //card removed - no serial can be read
  }
}

void ShowReaderDetails(void) {
	// Get the MFRC522 software version
	byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
	Serial.print(F("MFRC522 Software Version: 0x"));
	Serial.print(v, HEX);
	if (v == 0x91)
		Serial.print(F(" = v1.0"));
	else if (v == 0x92)
		Serial.print(F(" = v2.0"));
	else
		Serial.print(F(" (unknown)"));
	Serial.println("");
	// When 0x00 or 0xFF is returned, communication probably failed
	if ((v == 0x00) || (v == 0xFF)) {
		Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
		while(true);  // do not go further
	}
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds(void) {
  digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
  digitalWrite(greenLed, LED_ON); 	// Make sure green LED is on
  digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
  delay(200);
  digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
  digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
  digitalWrite(blueLed, LED_ON); 	// Make sure blue LED is on
  delay(200);
  digitalWrite(redLed, LED_ON); 	// Make sure red LED is on
  digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
  digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
  delay(200);
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn (void) {
  digitalWrite(blueLed, LED_ON); 	// Blue LED ON and ready to read card
  digitalWrite(redLed, LED_OFF); 	// Make sure Red LED is off
  digitalWrite(greenLed, LED_OFF); 	// Make sure Green LED is off
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( int number ) {
  int start = (number * 32 ) + 2;     // Figure out starting position
    
  //Serial.println(start,DEC);
  
  int i = 0;
  for ( ; i < 4; i++ ) {    // Loop 4 times to get the 4 Bytes
    myRecord.storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
    //Serial.print(myRecord.storedCard[i], HEX);
    //Serial.print(" ");
  }
  //Serial.println(" ");
  myRecord.pwLength = EEPROM.read(start + i);
  //Serial.print(myRecord.pwLength, HEX);
  //Serial.println(" ");
  i++;
  for ( ; i < 32; i++){
    myRecord.pw[i-5] = EEPROM.read(start + i);
  }
  for (int ii = 0; ii<27 ; ii++){
    //Serial.print(myRecord.pw[ii], HEX);
    //Serial.print(" ");
  }
  //Serial.println("");

  
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] , record* p) {
  if ( !findID( a ) ) { 		// Before we write to the EEPROM, check to see if we have seen this card before!
    
    int num = EEPROM.read(0); 		// Get the numer of used spaces, position 0 stores the number of ID cards
    int start = ( num * 32 ) + 34; 	// Figure out where the next slot starts
    num++; 								// Increment the counter by one
    EEPROM.write( 0, num ); 		// Write the new count to the counter
    int j = 0;
    for (  ; j < 4; j++ ) { 	// Loop 4 times
      EEPROM.write( start + j, a[j] ); 	// Write the array values to EEPROM in the right position
    }
    
    EEPROM.write( start + j, (byte)p->pwLength ); // write the password size to the next EEPROM slot
    j++; //increment the memory location by one byte
    
    int kk = 0; //a counter to move the PW string in to memory
    for (  ; j < 32; j++ ) {   // Loop 27 times
      if (kk < p->pwLength){
        EEPROM.write( start + j, p->pw[kk] );  // Write the array values to EEPROM in the right position
        kk++;
      }
      else{
        if (EEPROM.read(start + j) == 0) {              //If EEPROM address 0
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        }
        else {
          EEPROM.write(start + j, 0);
        }
      }
    }
    successWrite();
  	Serial.println(F("Succesfully added ID record to EEPROM"));
    for (int ii = 0; ii<32 ; ii++){
      Serial.print(EEPROM.read(start + ii), HEX);
      Serial.print(" ");
    }
    
    Serial.println("");
    
  }
  else {
    failedWrite();
	Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) { 		// Before we delete from the EEPROM, check to see if we have this card!
    failedWrite(); 			// If not
	Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    int num = EEPROM.read(0); 	// Get the numer of used spaces, position 0 stores the number of ID cards
    int slot; 			// Figure out the slot number of the card
    int mstart;			// = ( num * 32 ) + 2; // Figure out where the next slot starts (start is a dedicated value)
    int looping; 		// The number of times the loop repeats
    int j;
    int count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a ); 	// Figure out the slot number of the card to delete
    mstart = (slot * 32) + 2; //
    looping = ((num - slot) * 32);
    num--; 			// Decrement the counter by one
    EEPROM.write( 0, num ); 	// Write the new count to the counter
    for ( j = 0; j < looping; j++ ) { 				// Loop the card shift times
      EEPROM.write( mstart + j, EEPROM.read(mstart + 32 + j)); 	// Shift the array values to 32 places earlier in the EEPROM
    }
    for ( int k = 0; k < 32; k++ ) { 				// Shifting loop
      EEPROM.write( mstart + j + k, 0);
    }
    successDelete();
	Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != NULL ) 			// Make sure there is something in the array first
    match = true; 			// Assume they match at first
  for ( int k = 0; k < 4; k++ ) { 	// Loop 4 times
    if ( a[k] != b[k] ) 		// IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if ( match ) { 			// Check to see if if match is still true
    return true; 			// Return true
  }
  else  {
    return false; 			// Return false
  }
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
int findIDSLOT( byte find[] ) {
  int count = EEPROM.read(0); 			// Read the first Byte of EEPROM that
  for ( int i = 1; i <= count; i++ ) { 		// Loop once for each EEPROM entry
    readID(i); 								// Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, myRecord.storedCard ) ) { 	// Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i; 				// The slot number of the card
      break; 					// Stop looking we found it
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  int count = EEPROM.read(0);			// Read the first Byte of EEPROM that
  for ( int i = 1; i <= count; i++ ) {  	// Loop once for each EEPROM entry
    readID(i); 					// Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, myRecord.storedCard ) ) {  	// Check to see if the storedCard read from EEPROM
      return true;
      break; 	// Stop looking we found it
    }
    else {  	// If not, return false
    }
  }
  return false;
}

///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 3 times to indicate a successful write to EEPROM
void successWrite() {
  digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
  digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
  digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, LED_ON); 	// Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
  delay(200);
  digitalWrite(greenLed, LED_ON); 	// Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
  delay(200);
  digitalWrite(greenLed, LED_ON); 	// Make sure green LED is on
  delay(200);
}

///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite() {
  digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
  digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
  digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
  delay(200);
  digitalWrite(redLed, LED_ON); 	// Make sure red LED is on
  delay(200);
  digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
  delay(200);
  digitalWrite(redLed, LED_ON); 	// Make sure red LED is on
  delay(200);
  digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
  delay(200);
  digitalWrite(redLed, LED_ON); 	// Make sure red LED is on
  delay(200);
}

///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the blue LED 3 times to indicate a success delete to EEPROM
void successDelete() {
  digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
  digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
  digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
  delay(200);
  digitalWrite(blueLed, LED_ON); 	// Make sure blue LED is on
  delay(200);
  digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
  delay(200);
  digitalWrite(blueLed, LED_ON); 	// Make sure blue LED is on
  delay(200);
  digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
  delay(200);
  digitalWrite(blueLed, LED_ON); 	// Make sure blue LED is on
  delay(200);
}

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}


