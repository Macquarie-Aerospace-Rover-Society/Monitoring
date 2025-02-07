
// I don't want to be associated with this bullshit but Adrian made me commit this warcrime.

#include <SPI.h>

#define START 16
#define RESET 33
#define CS 5
long signed int ADCCOUNT;

SPISettings settings1(4000000, MSBFIRST, SPI_MODE1);

void setup() {
  // ADC Setup
  SPI.begin();
  pinMode(CS, OUTPUT);
  pinMode(RESET, OUTPUT);
  pinMode(START, OUTPUT);
  digitalWrite(RESET , HIGH);
  delay(5);
  Serial.begin(115200);
  SPI.beginTransaction(settings1);
  digitalWrite(START, LOW);
  SPI.transfer(0x19); // For the love of god never omit OCAL ever again
  delay(400);
}


void loop() {
  SPI.begin();
  digitalWrite(CS, LOW);
  delay(10);
  for (int i0 = 0; i0 < 256; i0++) //this was for something else but now serves as a delay for no reason other than I can't be bothered to remove it
  {
    if (i0 == 0) {
      SPI.transfer(0x42);                                    // The ADC will do nothing if you write the wrong configuration registers! Wow!
      SPI.transfer(0x07);                                    // The ADC will do nothing if you write to the wrong amount of registers too!
      SPI.transfer(0x0C); //0000 AIN0 PREF 1100 AINCOM NREF      The ADC will give you shit if it's told to use the wrong pins wow!
      SPI.transfer(0x00); //000 No conversion delay 00 PGA off 000 No PGA gain
      SPI.transfer(0x16); //0 No chop 0 Intnl clock 0 Cont. mode 1 LL Filter 0001 5SPS
      SPI.transfer(0x1A);                                     // The ADC will give you shit without a reference who would have thought?
      SPI.transfer(0x00);                                    // Disables PGA monitor burnout sources. The only thing that didn't give me shit
      SPI.transfer(0xFF);                                    // Disables excitation current because a potentiometer is not a RTD
      SPI.transfer(0x00);                                    // The ADC will also give you shit if you bias the inputs when you don't intend to!
      SPI.transfer(0x14);                                    // The ADC loses its ability to talk if you time it out and tell it to not try recover?
      SPI.transfer(0x08);                                    // The ADC never starts if you don't tell it to start who would have thought?
      delay(10);
    }
    else
    {
      SPI.transfer(0x12); // Read command
      ADCCOUNT = SPI.transfer(0);
      ADCCOUNT = (ADCCOUNT << 8) | SPI.transfer(0);
      ADCCOUNT = (ADCCOUNT << 8) | SPI.transfer(0);

      // Sign extend from 24-bit to 32-bit
      if (ADCCOUNT & 0x800000) { // If sign bit (bit 23) is set
        ADCCOUNT |= 0xFF000000;  // There is still something funny with the data out but the chip works wooooo
      }
    }

  }

  Serial.print("AIN0:");
  Serial.println(ADCCOUNT);
  digitalWrite(CS, HIGH);
  delay(10);
  SPI.endTransaction();

}

// The amount of dick shitting involved with this was phenominal. I have never been defeated by my own stupidity more than here. This abomination will be added to the modbus thing when I can be arsed. Hell, I might even write a library but lets not get too far ahead of ourselves here
