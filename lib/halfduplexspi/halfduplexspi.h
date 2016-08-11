#include <avr/io.h>

/* AVR half-duplex software SPI master
 * @author: Ralph Doncaster 2015 public domain software
 * connect 4.7K Ohm resistor between slave MO and MI pins and
 * connect slave MOSI to AVR MO/MI
 *  AVR              SLAVE
 *  SCK ------------ SCK
 *  MOMI --+-------- MOSI
 *         +-\/\/\-- MISO
 *            4.7K
 *
 * use spi_byte for tdd bi-directional spi transfer, or
 * spi_in and spi_out for faster uni-directional transfer.
 *
 * define SPI_PORT and SPI_SCK before including this file
 */

#define SPI_PORT PORTB
#define SPI_SCK 2
#define SPI_MOMI 0

#define cbi(x, y)    x &= ~(1 << y)
#define sbi(x, y)    x |= (1 << y)

#ifndef SPI_MOMI
#define SPI_MOMI (SPI_SCK - 1)
#endif

#define SPI_DDR (*((&SPI_PORT) -1))
#define SPI_PIN (*((&SPI_PORT) -2))

class HalfDuplexSPI {
public:
  static void setup(void);
  static uint8_t byte(uint8_t);
  static uint8_t in(void);
  static void out(uint8_t);
};
