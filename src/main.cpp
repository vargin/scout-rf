#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include "uart.h"
#include "utils.h"
#include "halfduplexspi.h"
#include "radio.h"
#include "nRF24L01.h"

uint8_t data[6] = {"_PING"};
uint8_t rxData[6] = {"-----"};
char numberString[10];
const uint8_t registers[26] = {CONFIG, EN_AA, EN_RXADDR, SETUP_AW, SETUP_RETR, RF_CH, RF_SETUP, STATUS, OBSERVE_TX, CD,
                               RX_ADDR_P0, RX_ADDR_P1, RX_ADDR_P2, RX_ADDR_P3, RX_ADDR_P4, RX_ADDR_P5, TX_ADDR,
                               RX_PW_P0, RX_PW_P1, RX_PW_P2, RX_PW_P3, RX_PW_P4, RX_PW_P5, FIFO_STATUS, DYNPD,
                               FEATURE};
#define SCOUT_TX = 1

#if defined(SCOUT_TX)
  const uint8_t txPipe[5] = {0x7C, 0x68, 0x52, 0x4d, 0x54};
  const uint8_t rxPipe[5] = {0x71, 0xCD, 0xAB, 0xCD, 0xAB};
#else
  const uint8_t txPipe[5] = {0x71, 0xCD, 0xAB, 0xCD, 0xAB};
  const uint8_t rxPipe[5] = {0x7C, 0x68, 0x52, 0x4d, 0x54};
#endif

const uint32_t timeoutPeriod = 3000;
uint16_t analogResult = 999;

void
initADC() {
  // Choose Vcc as reference voltage and right value adjustment.
  ADMUX &= ~(_BV(REFS1) | _BV(REFS0) | _BV(ADLAR));

  // Choose ADC2.
  ADMUX |= _BV(MUX1);

  // Choose free running mode.
  ADCSRB &= ~(_BV(ADTS0) | _BV(ADTS1) | _BV(ADTS2));
  ADCSRA |= _BV(ADPS0) | _BV(ADPS1) | _BV(ADPS2);

  // Turn off Digital input on PB4;
  // DIDR0 |= ADC2D;

  // Enable ADC and interruptions.
  ADCSRA |= _BV(ADEN) | _BV(ADIE);
}

void debug(const uint8_t *str, bool newLine = true) {
  while (*str) {
    TxByte(*str++);
  }

  if (newLine) {
    TxByte('\n');
  }
}

void debug(const char *str, bool newLine = true) {
  debug((const uint8_t*)str, newLine);
}

void debug(uint32_t number, bool newLine = true) {
  ltoa(number, numberString, 10);
  debug(numberString, newLine);
}

ISR(ADC_vect) {
  // Read low bit first as suggested in the datasheet, to make sure we read low and high bits of the same number.
  uint8_t analogLow = ADCL;
  analogResult = (ADCH << 8) | analogLow;
}

void startConversion() {
  ADCSRA |= _BV(ADSC);
}

void print2chars(uint16_t chars) {
  TxByte(chars >> 8);
  TxByte(chars & 0xff);
}

int main(void) {
  // Setup outputs. Set port to HIGH to signify UART default condition.
  DDRB |= _BV(DDB3);
  PORTB |= _BV(PB3);

  initADC();
  sei();

  startConversion();

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
  radio.openWritingPipe(txPipe);
  radio.openReadingPipe(rxPipe);

  #if defined(SCOUT_TX)
    radio.stopListening();
  #else
    radio.startListening();
  #endif

  radio.powerUp();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
  uint8_t counter = 0;

  #if defined(SCOUT_TX)
    bool isReceiving = false;
  #else
    bool isReceiving = true;
  #endif

  for (registerIndex = 0; registerIndex < 26; registerIndex++) {
    registerAddress = registers[registerIndex];

    print2chars(u8tohex(registerAddress));
    TxByte(' ');

    result = radio.read_register(registerAddress);

    print2chars(u8tohex(result));

    TxByte('\r');
    TxByte('\n');
  }

  while (true) {
    if (counter >= 10) {
      //isReceiving = !isReceiving;
      counter = 0;

      /*if (isReceiving) {
        radio.openWritingPipe(txPipe);
        radio.openReadingPipe(rxPipe);
        radio.startListening();
      } else {
        radio.openWritingPipe(txPipe);
        radio.openReadingPipe(rxPipe);
        radio.stopListening();
      }*/
    }

    if (isReceiving) {
      if (radio.available()) {
        radio.read(&rxData, 6);

        debug("Message received: ");
        debug(rxData);

        radio.openWritingPipe(txPipe);
        radio.openReadingPipe(rxPipe);
        radio.stopListening();

        if (!radio.writeBlocking(&data, 6, timeoutPeriod)) {
          debug("Message timed out!");
        } else {
          debug("Message successfully sent!");
        }

        radio.openWritingPipe(txPipe);
        radio.openReadingPipe(rxPipe);
        radio.startListening();
      } else {
        debug("No data is received!");
      }
    } else {
      if (analogResult > 300) {
        for (counter = 0; counter < 10; counter++) {
          radio.openWritingPipe(txPipe);
          radio.openReadingPipe(rxPipe);
          radio.stopListening();

          // Change the first byte of the payload for identification
          data[0] = counter + 48;

          // If retries are failing and the user defined timeout is exceeded, let's indicate a failure and set the fail
          // count to maximum and break out of the for loop.
          if (!radio.writeBlocking(&data, 6, timeoutPeriod)) {
            debug("Message timed out!");
          } else {
            debug("Message successfully sent!");
          }

          radio.openWritingPipe(txPipe);
          radio.openReadingPipe(rxPipe);
          radio.startListening();

          if (radio.available()) {
            radio.read(&rxData, 6);

            debug("Message received: ");
            debug(rxData);
          } else {
            debug("No data is received!");
          }

          _delay_ms(1000);
        }
      }

      debug(analogResult);
    }

    startConversion();

    _delay_ms(500);
  }
#pragma clang diagnostic pop
}