/*
   == DISCLAIMER ==
   I wrangle electrons for a living. There may be a liberal smattering of warcrimes incorporated into this program.
   If somebody who is more software inclined would like to rewrite this, please do. I am in no way (totally) attached to this program and I won't (will) cry.
   Read and understand the schematic before you modify the program.

   == CHANGELOG ==
   + Rewrote using different Modbus library which actually supports register codes
   + Modular af boiii
*/
#include <ModbusRTU.h>
#include <Preferences.h>

Preferences prefs;
ModbusRTU modbus;

//Pin definitions
#define nRX_TX  4
#define nRST 25

//Memory mapping settings
#define COIL_START_ADDR  0x00  //Starting address for coil registers
#define ISTS_START_ADDR  0x00  //Starting address for input status registers
#define IREG_START_ADDR  0x00  //Starting address for input registers
#define HREG_START_ADDR  0x00  //Starting address for holding registers
#define COIL_NUM_REG 0         //Amount of holding registers
#define ISTS_NUM_REG 1         //Amount of input status registers
#define IREG_NUM_REG 12        //Amount of input registers
#define HREG_NUM_REG 1         //Amount of holding registers

int SLAVE_ADDRESS;             //Declare the slave address variable globally
bool ADC_READY = 1;            //Declare ADC ready in preperation for interrupts SET TO TRUE TO TEST RANDOM DATA


void setup() {
  /*
    == FUNCTION DESCRIPTION ==
    Initializes serial interfaces, performs persistant modbus address check and writes default if required
  */
  pinMode(nRST, INPUT);                                                             //Emulates hi-z state
  Serial.begin(115200, SERIAL_8N1);
  Serial2.begin(9600, SERIAL_8N1);
  modbus.begin(&Serial2, nRX_TX);
  reserveRegisters(modbus);                                                         //Initializes registers
  prefs.begin("SLAVE_ADDRESS", false);                                              //Initialize Preferences
  SLAVE_ADDRESS = prefs.getInt("SLAVE_ADDRESS", -1);                                //Attempt to read the stored Modbus value, return -1 if none found
  if (SLAVE_ADDRESS == -1) {                                                        //Check if Modbus slave ID exists in flash
    SLAVE_ADDRESS = 1;                                                              //No Modbus address found. Setting to a default of 1                                                                  //Set holding register value to mirror address
    prefs.putInt("SLAVE_ADDRESS", SLAVE_ADDRESS);                                   // Save it to flash
    Serial.println("Slave address not set; Defaulting to 1");
  } else {                                                                          //Modbus address exists, leave it as is
    modbus.Hreg(HREG_START_ADDR, SLAVE_ADDRESS);                                    //Mirror holding register with saved flash value
    Serial.println("Slave address found and is " + String(SLAVE_ADDRESS));
  }
  prefs.end();                                                                      //End the Preferences library
  modbus.slave(SLAVE_ADDRESS);
}


void reserveRegisters(ModbusRTU& modbus) {
  /*
    == FUNCTION DESCRIPTION ==
    Initializes all Modbus memory locations
  */
  for (int i = 0; i < COIL_NUM_REG; i++) {
    modbus.addCoil(COIL_START_ADDR + i); // Add coil status register and set to 0
    modbus.Coil(COIL_START_ADDR + i, 0);
  }
  for (int i = 0; i < ISTS_NUM_REG; i++) {
    modbus.addIsts(ISTS_START_ADDR + i); // Add input status register and set to 0
    modbus.Ists(ISTS_START_ADDR + i, 0);
  }
  for (int i = 0; i < IREG_NUM_REG; i++) {
    modbus.addIreg(IREG_START_ADDR + i); // Add input register and set to 0
    modbus.Ireg(IREG_START_ADDR + i, 0);
  }
  for (int i = 0; i < HREG_NUM_REG; i++) {
    modbus.addHreg(HREG_START_ADDR + i); // Add holding register and set to 0
    modbus.Hreg(HREG_START_ADDR + i, 0);
  }
}


String addressChangeReset(int address) {
  /*
    == FUNCTION DESCRIPTION ==
    Saves new slave address to flash then resets the microcontroller, ideally in that order
  */
  if (address > 247 || address < 1) {           //Perform input sanitization to stop insane addresses
    address = SLAVE_ADDRESS;
  }
  prefs.begin("SLAVE_ADDRESS", false);          //Begin preferences write to namespace SLAVE_ADDRESS
  prefs.putInt("SLAVE_ADDRESS", address);       //Writes integer to namespace SLAVE_ADDRESS
  prefs.end();                                  //Ends the preferences library
  pinMode(nRST, OUTPUT);                        //Allows reset pin to be pulled low
  digitalWrite(nRST, LOW);                      //Reset the microcontroller to apply updates
  return ("Will to live");                      //This should never return
}


void updateIregFromADC () {
  /*
    == FUNCTION DESCRIPTION ==
    Fetch data from ADC and populate holding registers
  */
  for (int i = 0; i < IREG_NUM_REG; i++) {
    modbus.Ireg(IREG_START_ADDR + i, random(29000, 29200)); //Set random value, ADC integration goes here
  }
}


void loop() {
  if (modbus.Hreg(HREG_START_ADDR) != SLAVE_ADDRESS) {    //Checks if an address update has occured over a holding register
    addressChangeReset(modbus.Hreg(HREG_START_ADDR));     //Invokes addressChangeReset with the new address value
  }
  if (ADC_READY) {                                        //Checks to see if ADC_READY has been set by interrupt
    updateIregFromADC();                                  //Reads ADC data
  }
  modbus.task();
  yield();
}
