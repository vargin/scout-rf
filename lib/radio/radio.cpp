#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "nRF24L01.h"
#include "radio.h"

#define SPI_PORT PORTB
#define SPI_SCK 2
#define SPI_MOMI 0

#include "../halfduplexspi/halfduplexspi.h"

static const uint8_t child_pipe_enable[]
    PROGMEM = {ERX_P0, ERX_P1, ERX_P2, ERX_P3, ERX_P4, ERX_P5};

static const uint8_t PAYLOAD_SIZE = 32;
static const uint8_t ADDRESS_WIDTH = 5;

bool Radio::setup(void) {
  spi_setup();

  csnHigh();

  // Must allow the radio time to settle else configuration bits will not necessarily stick.
  // This is actually only required following power up but some settling time also appears to
  // be required after resets too. For full coverage, we'll always assume the worst.
  // Enabling 16b CRC is by far the most obvious case if the wrong timing is used - or skipped.
  // Technically we require 4.5ms + 14us as a worst case. We'll just call it 5ms for good measure.
  // WARNING: Delay is based on P-variant whereby non-P *may* require different timing.
  _delay_ms(5);

  // Reset CONFIG and enable 16-bit CRC.
  write_register(CONFIG, 0 | _BV(EN_CRC) | _BV(CRCO));

  // Set 1500uS (minimum for 32B payload in ESB@250KBPS) timeouts, to make testing a little easier
  // WARNING: If this is ever lowered, either 250KBS mode with AA is broken or maximum packet
  // sizes must never be used. See documentation for a more complete explanation.
  setRetries(5, 15);

  uint8_t setup = read_register(RF_SETUP);

  // Then set the data rate to the slowest (and most reliable) speed supported by all
  // hardware.
  setDataRate(DataRate::RATE_1MBPS);

  write_register(FEATURE, 0);
  write_register(DYNPD, 0);

  // Reset current status
  // Notice reset and flush is the last thing we do
  write_register(STATUS, _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT));

  setChannel(76);

  // Flush buffers
  flush_rx();
  flush_tx();

  //Power up by default when setup() is called.
  powerUp();


  // Enable PTX, do not write CE high so radio will remain in standby I mode ( 130us max to transition to RX or TX
  // instead of 1500us from powerUp ) PTX should use only 22uA of power.
  write_register(CONFIG, (read_register(CONFIG)) & ~_BV(PRIM_RX));

  // If setup is 0 or ff then there was no response from module.
  return (setup != 0 && setup != 0xff);
}

uint8_t Radio::get_status(void) {
  csnLow();

  uint8_t status = spi_byte(NOP);

  csnHigh();

  return status;
}

uint8_t Radio::read_register(uint8_t reg, uint8_t *buf, uint8_t len) {
  csnLow();

  uint8_t status = spi_byte(R_REGISTER | (REGISTER_MASK & reg));

  while (len--) {
    *buf++ = spi_byte(0xff);
  }

  csnHigh();

  return status;
}

uint8_t Radio::read_register(uint8_t reg) {
  csnLow();

  spi_byte(R_REGISTER | (REGISTER_MASK & reg));
  uint8_t result = spi_byte(0xff);

  csnHigh();

  return result;
}

uint8_t Radio::write_register(uint8_t reg, const uint8_t *buf, uint8_t len) {
  csnLow();

  uint8_t status = spi_byte(W_REGISTER | (REGISTER_MASK & reg));
  while (len--) {
    spi_byte(*buf++);
  }

  csnHigh();

  return status;
}

uint8_t Radio::write_register(uint8_t reg, uint8_t value) {
  csnLow();

  uint8_t status = spi_byte(W_REGISTER | (REGISTER_MASK & reg));
  spi_byte(value);

  csnHigh();

  return status;
}

void Radio::setRetries(uint8_t delay, uint8_t count) {
  write_register(SETUP_RETR, (delay & 0xf) << ARD | (count & 0xf) << ARC);
}

void Radio::setOutputPower(OutputPower power) {
  uint8_t setup = read_register(RF_SETUP) & 0b11111000;
  uint8_t level = (power << 1) + 1;

  write_register(RF_SETUP, setup |= level);
}

bool Radio::setDataRate(DataRate rate) {
  uint8_t setup = read_register(RF_SETUP);

  // HIGH and LOW '00' is 1Mbs - our default
  setup &= ~(_BV(RF_DR_LOW) | _BV(RF_DR_HIGH));

  txRxDelay = 85;

  if (rate == DataRate::RATE_250KBPS) {
    // Must set the RF_DR_LOW to 1; RF_DR_HIGH (used to be RF_DR) is already 0. Making it '10'.
    setup |= _BV(RF_DR_LOW);
    txRxDelay = 155;
  } else if (rate == DataRate::RATE_2MBPS) {
    // Set 2Mbs, RF_DR (RF_DR_HIGH) is set 1. Making it '01'.
    setup |= _BV(RF_DR_HIGH);
    txRxDelay = 65;
  }

  write_register(RF_SETUP, setup);

  // Verify our result.
  return read_register(RF_SETUP) == setup;
}

void Radio::setChannel(uint8_t channel) {
  const uint8_t max_channel = 125;
  write_register(RF_CH, channel > max_channel ? max_channel : channel);
}

void Radio::powerDown(void) {
  // Guarantee CE is low on powerDown
  //ce(LOW);
  write_register(CONFIG, read_register(CONFIG) & ~_BV(PWR_UP));
}

//Power up now. Radio will not power down unless instructed by MCU for config changes etc.
void Radio::powerUp(void) {
  uint8_t cfg = read_register(CONFIG);

  // if not powered up then power up and wait for the radio to initialize
  if (!(cfg & _BV(PWR_UP))) {
    write_register(CONFIG, cfg | _BV(PWR_UP));

    // For nRF24L01+ to go from power down mode to TX or RX mode it must first pass through stand-by mode.
    // There must be a delay of Tpd2stby (see Table 16.) after the nRF24L01+ leaves power down mode before
    // the CEis set high. - Tpd2stby can be up to 5ms per the 1.0 datasheet.
    _delay_ms(5);
  }
}

void Radio::setAutoAck(bool enable) {
  if (enable) {
    write_register(EN_AA, 0b111111);
  } else {
    write_register(EN_AA, 0);
  }
}

/****************************************************************************/

void Radio::setAutoAck(uint8_t pipe, bool enable) {
  if (pipe <= 6) {
    uint8_t en_aa = read_register(EN_AA);
    if (enable) {
      en_aa |= _BV(pipe);
    } else {
      en_aa &= ~_BV(pipe);
    }
    write_register(EN_AA, en_aa);
  }
}

void Radio::openWritingPipe(const uint8_t *address) {
  // Note that AVR 8-bit uC's store this LSB first, and the NRF24L01(+)
  // expects it LSB first too, so we're good.
  write_register(RX_ADDR_P0, address, ADDRESS_WIDTH);
  write_register(TX_ADDR, address, ADDRESS_WIDTH);
  write_register(RX_PW_P0, PAYLOAD_SIZE);
}

void Radio::stopListening(void) {
  if (read_register(FEATURE) & _BV(EN_ACK_PAY)) {
    _delay_us(155);
    flush_tx();
  }

  write_register(CONFIG, (read_register(CONFIG)) & ~_BV(PRIM_RX));

  // for 3 pins solution TX mode is only left with additional powerDown/powerUp cycle.
  powerDown();
  powerUp();

  // Enable RX on pipe0.
  write_register(EN_RXADDR, read_register(EN_RXADDR) | _BV(pgm_read_byte(&child_pipe_enable[0])));
}

uint8_t Radio::write_payload(const void *buf, uint8_t data_len, const uint8_t writeType) {
  const uint8_t *current = reinterpret_cast<const uint8_t *>(buf);

  data_len = data_len < PAYLOAD_SIZE ? data_len : PAYLOAD_SIZE;
  uint8_t blank_len = PAYLOAD_SIZE - data_len;

  csnLow();

  uint8_t status = spi_byte(writeType);
  while (data_len--) {
    spi_byte(*current++);
  }

  while (blank_len--) {
    spi_byte(0);
  }

  csnHigh();

  return status;
}

bool Radio::writeFast(const void *buf, uint8_t len, const bool multicast) {
  //Block until the FIFO is NOT full.
  //Keep track of the MAX retries and set auto-retry if seeing failures
  //Return 0 so the user can control the retrys and set a timer or failure counter if required
  //The radio will auto-clear everything in the FIFO as long as CE remains high

  // Blocking only if FIFO is full. This will loop and block until TX is successful or fail
  while ((get_status() & (_BV(TX_FULL)))) {
    if (get_status() & _BV(MAX_RT)) {
      // Clear max retry flag.
      write_register(STATUS, _BV(MAX_RT));

      // The previous payload has been retransmitted. From the user perspective, if you get a 0, just keep trying to
      // send the same payload.
      return 0;
    }
  }

  //Start Writing
  write_payload(buf, len, multicast ? W_TX_PAYLOAD_NO_ACK : W_TX_PAYLOAD);

  return 1;
}

bool Radio::writeFast(const void *buf, uint8_t len) {
  return writeFast(buf, len, 0);
}

void Radio::csnLow(void) {
  // Discharge SCK->CSN RC.
  cbi(SPI_PORT, SPI_SCK);
  _delay_us(50);
}

void Radio::csnHigh(void) {
  // Charge SCK->CSN RC.
  sbi(SPI_PORT, SPI_SCK);
  _delay_us(50);
}

uint8_t Radio::flush_rx(void) {
  csnLow();

  uint8_t status = spi_byte(FLUSH_RX);

  csnHigh();

  return status;
}

uint8_t Radio::flush_tx(void) {
  csnLow();

  uint8_t status = spi_byte(FLUSH_TX);

  csnHigh();

  return status;
}
