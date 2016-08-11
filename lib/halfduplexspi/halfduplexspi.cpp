#include "halfduplexspi.h"

void HalfDuplexSPI::setup() {
  // Output mode.
  sbi(SPI_DDR, SPI_SCK);
}

uint8_t HalfDuplexSPI::byte(uint8_t dataout) {
  uint8_t datain, bits = 8;

  do {
    datain <<= 1;
    if (SPI_PIN & (1 << SPI_MOMI)) datain++;

    sbi (SPI_DDR, SPI_MOMI);        // output mode
    if (dataout & 0x80) sbi (SPI_PORT, SPI_MOMI);
    SPI_PIN = (1 << SPI_SCK);
    cbi (SPI_DDR, SPI_MOMI);        // input mode
    SPI_PIN = (1 << SPI_SCK);         // toggle SCK

    cbi (SPI_PORT, SPI_MOMI);
    dataout <<= 1;

  } while (--bits);

  return datain;
}

uint8_t HalfDuplexSPI::in() {
  uint8_t pinstate, datain, bits = 8;

  do {
    datain <<= 1;
    SPI_PIN = (1 << SPI_SCK);
    pinstate = SPI_PIN;
    SPI_PIN = (1 << SPI_SCK);         // toggle SCK
    if (pinstate & (1 << SPI_MOMI)) datain++;
  } while (--bits);

  return datain;
}

void HalfDuplexSPI::out(uint8_t dataout) {
  sbi (SPI_DDR, SPI_MOMI);        // output mode
  uint8_t bits = 8;

  do {
    if (dataout & 0x80) SPI_PIN = (1 << SPI_MOMI);
    SPI_PIN = (1 << SPI_SCK);
    SPI_PIN = (1 << SPI_SCK);         // toggle SCK
    cbi (SPI_PORT, SPI_MOMI);
    dataout <<= 1;
  } while (--bits);

  cbi (SPI_DDR, SPI_MOMI);        // input mode
}
