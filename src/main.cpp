#include <SPI85.h>
#include <Mirf.h>
#include <MirfHardwareSpi85Driver.h>
#include "TinyDebugSerial.h"

void setup()
{
  Serial.begin(115200);

  Mirf.spi = &MirfHardwareSpi85;
  Mirf.init();
}

void loop()
{
  uint8_t nrfStatus;
  delay(3000);
  Serial.print("\nMirf status: ");
  nrfStatus = Mirf.getStatus();
  // do back-to-back getStatus to test if CSN goes high
  nrfStatus = Mirf.getStatus();
  Serial.print(nrfStatus, HEX);
}
