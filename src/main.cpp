#include "uart.h"
#include <avr/io.h>
#include <util/delay.h>
#include "utils.h"

#define SPI_PORT PORTB
#define SPI_SCK 2
#define SPI_MOMI 0

#include "halfduplexspi.h"

const char data[14] = {"New iteration"};

void debug(const char *str, bool newLine = true) {
  while (*str) {
    TxByte(*str++);
  }

  if (newLine) {
    TxByte('\n');
  }
}

void print2chars(uint16_t chars) {
  TxByte(chars >> 8);
  TxByte(chars & 0xff);
}

int main(void) {
  // Setup outputs. Set port to HIGH to signify UART default condition.
  DDRB |= _BV(DDB3);
  PORTB |= _BV(PB3);

  uint8_t result, address;
  spi_setup();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
  while (true) {
    debug(data);

    address = 0x0f;

    do {
      // discharge SCK->CSN RC
      cbi(SPI_PORT, SPI_SCK);
      print2chars(u8tohex(address));
      debug(" ");

      spi_byte(address);
      result = spi_byte(address);

      // charge SCK->CSN RC
      sbi(SPI_PORT, SPI_SCK);

      print2chars(u8tohex(result));

      debug("\r\n");
    } while (address--);

    _delay_ms(1000);
  }
#pragma clang diagnostic pop
}