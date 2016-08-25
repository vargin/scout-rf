#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include "uart.h"
#include "halfduplexspi.h"
#include "radio.h"

volatile bool interrupt = false;

ISR(PCINT0_vect) {
  interrupt = true;
}

// {"PING"} = {80, 73, 78, 71, 0}.
const uint8_t data[5] = {80, 73, 78, 71, 0};
// {"PONG"} = {80, 79, 78, 71, 0}.
uint8_t rxData[5] = {0, 0, 0, 0, 0};

const uint8_t txPipe[5] = {0x7C, 0x68, 0x52, 0x4d, 0x54};
const uint8_t rxPipe[5] = {0x71, 0xCD, 0xAB, 0xCD, 0xAB};

const uint32_t timeoutPeriod = 3000;

void debug(const uint8_t *str, bool newLine = true) {
  while (*str) {
    TxByte(*str++);
  }

  if (newLine) {
    TxByte('\n');
  }
}

void debug(const char *str, bool newLine = true) {
  debug((const uint8_t *) str, newLine);
}

int main(void) {
  // Setup outputs. Set port to HIGH to signify UART default condition.
  DDRB |= _BV(DDB4);
  PORTB |= _BV(PB4);

  // Setup external interrupt pin.
  DDRB &= ~_BV(DDB3);
  PCMSK |= _BV(PCINT3);
  GIMSK |= _BV(PCIE);

  sei();

  Radio radio;

  if (radio.setup()) {
    debug("nRF24L01+ is set up and ready!");
  } else {
    debug("nRF24L01+ DOES NOT respond!");
  }

  radio.setChannel(1);
  radio.setOutputPower(OutputPower::HIGH);

  if (radio.setDataRate(DataRate::RATE_250KBPS)) {
    debug("nRF24L01+ is verified!");
  } else {
    debug("This is not nRF24L01+ module!");
  }

  radio.setAutoAck(1);
  radio.setRetries(2, 15);
  radio.openWritingPipe(txPipe);
  radio.openReadingPipe(rxPipe);
  radio.stopListening();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
  while (true) {
    if (interrupt) {
      debug("INTERRUPT");
      interrupt = false;

      radio.powerUp();

      bool isPongReceived = false;

      for (uint8_t counter = 0; counter < 10; counter++) {
        radio.openWritingPipe(txPipe);
        radio.openReadingPipe(rxPipe);
        radio.stopListening();

        // If retries are failing and the user defined timeout is exceeded, let's indicate a failure and set the fail
        // count to maximum and break out of the for loop.
        if (!radio.writeBlocking(&data, 5, timeoutPeriod)) {
          debug("Message has not been sent");
        } else {
          debug("Message has been sent!");
        }

        radio.openWritingPipe(txPipe);
        radio.openReadingPipe(rxPipe);
        radio.startListening();

        if (radio.available()) {
          radio.read(&rxData, 5);

          debug("Message has been received: ");
          debug(rxData);

          // {"PONG"} = {80, 79, 78, 71, 0}.
          if (rxData[0] == 80 && rxData[1] == 79 && rxData[2] == 78 && rxData[3] == 71 && rxData[4] == 0) {
            isPongReceived = true;
          }

          rxData[0] = rxData[1] = rxData[2] = rxData[3] = rxData[4] = 0;

          if (isPongReceived) {
            break;
          }
        } else {
          debug("No data is available!");
        }

        _delay_ms(1000);
      }

      radio.stopListening();
      radio.powerDown();

      // Let's wait 3000 ms before going sleep again. Just wanted to have enough time to measure current in
      // non-sleep mode :).
      _delay_ms(3000);
    } else {
      debug("NO INTERRUPT");
    }

    debug("Sleeping...");

    PORTB &= ~_BV(DDB4);

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    sleep_enable();
    sei();
    sleep_cpu();
    sleep_disable();
    sei();

    PORTB |= _BV(DDB4);

    debug("Waking!");
  }
#pragma clang diagnostic pop
}