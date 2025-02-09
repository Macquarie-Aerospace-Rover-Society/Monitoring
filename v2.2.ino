/*
   == DISCLAIMER ==
   I wrangle electrons for a living. There may be a liberal smattering of warcrimes incorporated into this program.
   If somebody who is more software inclined would like to rewrite this, please do. I am in no way (totally) attached to this program and I won't (will) cry.
   Read and understand the schematic before you modify the program.

   == CHANGELOG ==
   + Begun ADC integration begun
   ~ ADC integration not finished yet
*/
#include <ModbusRTU.h>
#include <Preferences.h>
#include <SPI.h>

Preferences prefs;
ModbusRTU modbus;

//Pin definitions
#define nRX_TX 4
#define nRST_MCU 25
#define nRST_ADC 33
#define START 32
#define CS 5

//Memory mapping settings
#define COIL_START_ADDR  0x00  //Starting address for coil registers
#define ISTS_START_ADDR  0x00  //Starting address for input status registers
#define IREG_START_ADDR  0x00  //Starting address for input registers
#define HREG_START_ADDR  0x00  //Starting address for holding registers
#define COIL_NUM_REG 0         //Amount of holding registers
#define ISTS_NUM_REG 1         //Amount of input status registers
#define IREG_NUM_REG 12        //Amount of input registers 16 maximum, 8 bits truncated to 4 later on for ease of ADC sweep
#define HREG_NUM_REG 1         //Amount of holding registers

SPISettings SPI_SETTINGS(4000000, MSBFIRST, SPI_MODE1);        //SPI settings 4MHz, MSB first, Mode 1

int SLAVE_ADDRESS;             //Declare the slave address variable globally
bool ADC_READY = 1;            //Declare ADC ready in preperation for interrupts SET TO TRUE TO TEST RANDOM DATA


void setup() {
  /*
    == FUNCTION DESCRIPTION ==
    Initializes serial interfaces, performs persistant modbus address check and writes default if required
  */
  pinMode(nRST_MCU, INPUT);                                                             //Emulates hi-z state
  Serial.begin(115200, SERIAL_8N1);
  Serial2.begin(115200, SERIAL_8N1);
  ADCSetup();
  modbus.begin(&Serial2, nRX_TX);
  reserveRegisters(modbus);                                                         //Initializes registers
  prefs.begin("SLAVE_ADDRESS", false);                                              //Initialize Preferences
  SLAVE_ADDRESS = prefs.getInt("SLAVE_ADDRESS", -1);                                //Attempt to read the stored Modbus value, return -1 if none found
  if (SLAVE_ADDRESS == -1) {                                                        //Check if Modbus slave ID exists in flash
    SLAVE_ADDRESS = 1;                                                              //No Modbus address found. Setting to a default of 1
    prefs.putInt("SLAVE_ADDRESS", SLAVE_ADDRESS);                                   //Save it to flash
    Serial.println("Slave address not set; Defaulting to 1");
  } else {                                                                          //Modbus address exists, leave it as is
    modbus.Hreg(HREG_START_ADDR, SLAVE_ADDRESS);                                    //Mirror holding register with saved flash value
    Serial.println("Slave address found and is " + String(SLAVE_ADDRESS));
  }
  prefs.end();                                                                      //End the Preferences library
  modbus.slave(SLAVE_ADDRESS);
}


void ADCSetup() {
  /*
    == FUNCTION DESCRIPTION ==
    Initializes the ADC by setting the appropriate registers
  */
  SPI.begin();
  pinMode(CS, OUTPUT);
  pinMode(START, OUTPUT);
  digitalWrite(nRST_ADC , HIGH);
  SPI.beginTransaction(SPI_SETTINGS);      //---------------------+-----------------------------------------------------------------------------------------------------+------------+
  digitalWrite(START, LOW);                //Datasheet Name       |  Brief breakdown                                                                                    |  Reference |
  digitalWrite(CS, LOW);                   //---------------------+-----------------------------------------------------------------------------------------------------+------------+
  SPI.transfer(0x19);                      //COMMAND byte 1 of 3  |  00011001 SFOCAL Self offset calibration                                                            |  Table 24  |
  SPI.transfer(0x42);                      //COMMAND byte 2 of 3  |  010 Write 00010 Starting at 0x02                                                                   |  Table 24  |
  SPI.transfer(0x07);                      //COMMAND byte 3 of 3  |  00000111 Write 7 registers                                                                         |  Table 24  |
  SPI.transfer(0x0C);                      //INPMUX byte          |  0000 AIN0 PREF 1100 AINCOM NREF                                                                    |  Table 28  |
  SPI.transfer(0x00);                      //PGA byte             |  000 No conversion delay 00 PGA off 000 No PGA gain                                                 |  Table 29  |
  SPI.transfer(0x16);                      //DATARATE byte        |  0 No chop 0 Intnl clock 0 Cont. mode 1 LL Filter 0110 60SPS                                        |  Table 30  |
  SPI.transfer(0x1A);                      //REF byte             |  00 No refernce monitoring 0 PREF unbuffered 1 NREF buffered 10 Intnl reference 10 No powerdown     |  Table 31  |
  SPI.transfer(0x00);                      //IDACMAG byte         |  0 PGA off 0 No excitation 00 Reserved 1010 Source off                                              |  Table 32  |
  SPI.transfer(0xFF);                      //IDACMUX byte         |  1111 Disabled 1111 Disabled                                                                        |  Table 33  |
  SPI.transfer(0x00);                      //VBIAS byte           |  00000000 No bias                                                                                   |  Table 34  |
  SPI.transfer(0x14);                      //SYS byte             |  000 No sysmon 10 ACAL 8 1 SPI timeout enable 0 No CRC 0 No status byte                             |  Table 35  |
  SPI.transfer(0x08);                      //COMMAND byte         |  00001000 START Start byte                                                                          |  Table 24  |
}                                          //---------------------+-----------------------------------------------------------------------------------------------------+------------+


int32_t readADC() {
  /*
    == FUNCTION DESCRIPTION ==
    Sweeps the input multiplexer and reads ADC contents
  */
  SPI.transfer(0x12);                         //COMMAND byte 00010010 RDATA Read data
  int32_t value = SPI.transfer(0x00) << 16;   //Takes the first SPI byte and offsets it by 16 bits into value
  value |= SPI.transfer(0x00) << 8;           //Takes the second SPI byte and offsets it by 8 bits into value
  value |= SPI.transfer(0x00);                //Takes the third SPI byte with no offset and stores into value
  if (value & 0x800000) {                     //I think this is wrong because byte 3 of 3 is a CRC byte as disabled by the SYS register
    value |= 0xFF000000;
  }
  return value;
}


void reserveRegisters(ModbusRTU& modbus) {
  /*
    == FUNCTION DESCRIPTION ==
    Initializes all Modbus memory locations
  */
  for (int i = 0; i < COIL_NUM_REG; i++) {
    modbus.addCoil(COIL_START_ADDR + i); //Add coil status register and set to 0
    modbus.Coil(COIL_START_ADDR + i, 0);
  }
  for (int i = 0; i < ISTS_NUM_REG; i++) {
    modbus.addIsts(ISTS_START_ADDR + i); //Add input status register and set to 0
    modbus.Ists(ISTS_START_ADDR + i, 0);
  }
  for (int i = 0; i < IREG_NUM_REG; i++) {
    modbus.addIreg(IREG_START_ADDR + i); //Add input register and set to 0
    modbus.Ireg(IREG_START_ADDR + i, 0);
  }
  for (int i = 0; i < HREG_NUM_REG; i++) {
    modbus.addHreg(HREG_START_ADDR + i); //Add holding register and set to 0
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
  pinMode(nRST_MCU, OUTPUT);                    //Allows reset pin to be pulled low
  digitalWrite(nRST_MCU, LOW);                  //Reset the microcontroller to apply updates
  return ("Will to live");                      //This should never return
}


void updateIregFromADC () {
  /*
    == FUNCTION DESCRIPTION ==
    Fetch data from ADC and populate holding registers
  */
  for (int8_t i = 0; i < IREG_NUM_REG; i++) {
    SPI.transfer(0x42);                          //Begin register write at input multiplexer register
    SPI.transfer(0x00);                          //Write just the input multiplexer register
    SPI.transfer(0x08);                          //COMMAND byte 00001000 START Start byte  
    SPI.transfer(((i & 0x0F) << 4) | 0x0C);      //Shift lower 4 bits of i to 7:4, set 3:0 to 1100
    Serial.println(((i & 0x0F) << 4) | 0x0C, BIN);
    modbus.Ireg(IREG_START_ADDR + i, readADC()); //Sets Modbus memory location at current sweep location from ADC read
  }
}



void loop() {
  if (modbus.Hreg(HREG_START_ADDR) != SLAVE_ADDRESS) {    //Checks if an address update has occured over a holding register
    addressChangeReset(modbus.Hreg(HREG_START_ADDR));     //Invokes addressChangeReset with the new address value
  }
  if (ADC_READY) {                                        //Still need to set up interrupt
    updateIregFromADC();                                  //Reads ADC data
  }
  modbus.task();
  yield();
}
