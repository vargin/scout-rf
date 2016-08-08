#include "uart.h"
#include <avr/io.h>
#include <util/delay.h>
#include "utils.h"

//#define SPI_PORT PORTB
//#define SPI_SCK 2
//#define SPI_MOMI 0

#include "nRF24L01.h"
#include "radio.h"

uint8_t data[32] = {"_This is a message via NRF24L+!"};
const uint8_t registers[26] = {CONFIG, EN_AA, EN_RXADDR, SETUP_AW, SETUP_RETR, RF_CH, RF_SETUP, STATUS, OBSERVE_TX, CD,
                               RX_ADDR_P0, RX_ADDR_P1, RX_ADDR_P2, RX_ADDR_P3, RX_ADDR_P4, RX_ADDR_P5, TX_ADDR,
                               RX_PW_P0, RX_PW_P1, RX_PW_P2, RX_PW_P3, RX_PW_P4, RX_PW_P5, FIFO_STATUS, DYNPD,
                               FEATURE};
const uint8_t pipe[5] = {0x7C, 0x68, 0x52, 0x4d, 0x54};
const uint32_t timeoutPeriod = 3000;
volatile uint32_t counter;

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

  Radio radio;

  uint8_t result, registerAddress, registerIndex;

  if (radio.setup()) {
    debug("nRF24L01+ is set up and ready!");
  } else {
    debug("nRF24L01+ DOES NOT respond!");
  }

  radio.setChannel(1);
  radio.setOutputPower(OutputPower::HIGH);

  if (radio.setDataRate(DataRate::RATE_250KBPS)) {
    debug("True + Module!");
  } else {
    debug("Panic!!!! It's not true + module");
  }

  radio.setAutoAck(1);
  radio.setRetries(2, 15);
  radio.openWritingPipe(pipe);
  radio.stopListening();
  radio.powerUp();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
  while (true) {
    for (registerIndex = 0; registerIndex < 26; registerIndex++) {
      registerAddress = registers[registerIndex];

      print2chars(u8tohex(registerAddress));
      TxByte(' ');

      result = radio.read_register(registerAddress);

      print2chars(u8tohex(result));

      TxByte('\r');
      TxByte('\n');
    }

    _delay_ms(2000);

    // Change this to a higher or lower number. This is the number of payloads that will be sent.
    uint32_t cycles = 1000;

    // Indicate to the other radio that we are starting, and provide the number of payloads that will be sent
    unsigned long transferCMD[] = {'H', 'S', cycles};

    radio.writeFast(&transferCMD, 12);

    // If transfer initiation was successful, do the following
    if (radio.txStandBy(timeoutPeriod)) {
      bool timedOut = false;

      for (uint32_t i = 0; i < cycles; i++) {
        // Change the first byte of the payload for identification
        data[0] = i;

        // If retries are failing and the user defined timeout is exceeded, let's indicate a failure and set the fail
        // count to maximum and break out of the for loop.
        if (!radio.writeBlocking(&data, 32, timeoutPeriod)) {
          timedOut = 1;
          counter = cycles;
          break;
        }
      }

      // This should be called to wait for completion and put the radio in standby mode after transmission, returns 0 if
      // data still in FIFO (timed out), 1 if success.
      if (timedOut) {
        // Partially blocking standby, blocks until success or max retries. FIFO flushed if auto timeout reached.
        radio.txStandBy();
      } else {
        // Standby, block until FIFO empty (sent) or user specified timeout reached. FIFO flushed if user timeout is
        // reached.
        radio.txStandBy(timeoutPeriod);
      }

    } else {
      //If unsuccessful initiating transfer, exit and retry later
      counter = cycles + 1;
    }

    counter = 0;
  }
#pragma clang diagnostic pop
}