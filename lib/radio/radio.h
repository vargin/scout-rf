#ifndef SCOUT_RF_RADIO_H
#define SCOUT_RF_RADIO_H

enum OutputPower {
  MIN = 0,
  LOW,
  HIGH,
  MAX
};

enum DataRate {
  RATE_1MBPS = 0,
  RATE_2MBPS,
  RATE_250KBPS
};

class Radio {
public:
  bool setup(void);

  /**
   * Retrieve the current status of the chip
   *
   * @return Current value of status register
   */
  uint8_t get_status(void);

  /**
  * Begin operation of the chip
  *
  * Call this in setup(), before calling any other methods.
  * @code radio.begin() @endcode
  */
  /*bool begin(void);*/

  /**
   * Read a chunk of data in from a register
   *
   * @param reg Which register. Use constants from nRF24L01.h
   * @param buf Where to put the data
   * @param len How many bytes of data to transfer
   * @return Current value of status register
   */
  uint8_t read_register(uint8_t reg, uint8_t* buf, uint8_t len);

  /**
   * Read single byte from a register
   *
   * @param reg Which register. Use constants from nRF24L01.h
   * @return Current value of register @p reg
   */
  uint8_t read_register(uint8_t reg);

  /**
   * Write a chunk of data to a register
   *
   * @param reg Which register. Use constants from nRF24L01.h
   * @param buf Where to get the data
   * @param len How many bytes of data to transfer
   * @return Current value of status register
   */
  uint8_t write_register(uint8_t reg, const uint8_t* buf, uint8_t len);

  /**
   * Write a single byte to a register
   *
   * @param reg Which register. Use constants from nRF24L01.h
   * @param value The new value to write
   * @return Current value of status register
   */
  uint8_t write_register(uint8_t reg, uint8_t value);

  /**
   * Set the number and delay of retries upon failed submit
   *
   * @param delay How long to wait between each retry, in multiples of 250us,
   * max is 15.  0 means 250us, 15 means 4000us.
   * @param count How many retries before giving up, max 15
   */
  void setRetries(uint8_t delay, uint8_t count);

  /**
   * Sets RF output power level.
   *
   * The power levels correspond to the following output levels respectively:
   * -18dBm, -12dBm,-6dBM, and 0dBm.
   *
   * @param power Desired power output level.
   */
  void setOutputPower(OutputPower power);

  bool setDataRate(DataRate rate);

  /**
   * Set RF communication channel
   *
   * @param channel Which RF channel to communicate on, 0-125
   */
  void setChannel(uint8_t channel);

  /**
   * Enter low-power mode
   *
   * To return to normal power mode, call powerUp().
   *
   * @note After calling startListening(), a basic radio will consume about 13.5mA
   * at max PA level.
   * During active transmission, the radio will consume about 11.5mA, but this will
   * be reduced to 26uA (.026mA) between sending.
   * In full powerDown mode, the radio will consume approximately 900nA (.0009mA)
   *
   * @code
   * radio.powerDown();
   * avr_enter_sleep_mode(); // Custom function to sleep the device
   * radio.powerUp();
   * @endcode
   */
  void powerDown(void);

  /**
   * Leave low-power mode - required for normal radio operation after calling powerDown()
   *
   * To return to low power mode, call powerDown().
   * @note This will take up to 5ms for maximum compatibility
   */
  void powerUp(void);

  /**
   * Enable or disable auto-acknowlede packets
   *
   * This is enabled by default, so it's only needed if you want to turn
   * it off for some reason.
   *
   * @param enable Whether to enable (true) or disable (false) auto-acks
   */
  void setAutoAck(bool enable);

  /**
   * Enable or disable auto-acknowlede packets on a per pipeline basis.
   *
   * AA is enabled by default, so it's only needed if you want to turn
   * it off/on for some reason on a per pipeline basis.
   *
   * @param pipe Which pipeline to modify
   * @param enable Whether to enable (true) or disable (false) auto-acks
   */
  void setAutoAck(uint8_t pipe, bool enable);

  /**
   * Open a pipe for writing via byte array.
   *
   * Only one writing pipe can be open at once, but you can change the address
   * you'll write to. Call stopListening() first.
   *
   * Addresses are assigned via a byte array, default is 5 byte address length
   *
   * @code
   *   uint8_t addresses[][6] = {"1Node","2Node"};
   *   radio.openWritingPipe(addresses[0]);
   * @endcode
   * @code
   *  uint8_t address[] = { 0xCC,0xCE,0xCC,0xCE,0xCC };
   *  radio.openWritingPipe(address);
   *  address[0] = 0x33;
   *  radio.openReadingPipe(1,address);
   * @endcode
   * @see setAddressWidth
   *
   * @param address The address of the pipe to open. Coordinate these pipe
   * addresses amongst nodes on the network.
   */
  void openWritingPipe(const uint8_t *address);

  /**
   * Stop listening for incoming messages, and switch to transmit mode.
   *
   * Do this before calling write().
   * @code
   * radio.stopListening();
   * radio.write(&data,sizeof(data));
   * @endcode
   */
  void stopListening(void);

  /**
   * Write the transmit payload
   *
   * The size of data written is the fixed payload size, see getPayloadSize()
   *
   * @param buf Where to get the data
   * @param len Number of bytes to be sent
   * @return Current value of status register
   */
  uint8_t write_payload(const void* buf, uint8_t len, const uint8_t writeType);

  /**
   * This will not block until the 3 FIFO buffers are filled with data.
   * Once the FIFOs are full, writeFast will simply wait for success or
   * timeout, and return 1 or 0 respectively. From a user perspective, just
   * keep trying to send the same data. The library will keep auto retrying
   * the current payload using the built in functionality.
   * @warning It is important to never keep the nRF24L01 in TX mode and FIFO full for more than 4ms at a time. If the auto
   * retransmit is enabled, the nRF24L01 is never in TX mode long enough to disobey this rule. Allow the FIFO
   * to clear by issuing txStandBy() or ensure appropriate time between transmissions.
   *
   * @code
   * Example (Partial blocking):
   *
   *			radio.writeFast(&buf,32);  // Writes 1 payload to the buffers
   *			txStandBy();     		   // Returns 0 if failed. 1 if success. Blocks only until MAX_RT timeout or success. Data flushed on fail.
   *
   *			radio.writeFast(&buf,32);  // Writes 1 payload to the buffers
   *			txStandBy(1000);		   // Using extended timeouts, returns 1 if success. Retries failed payloads for 1 seconds before returning 0.
   * @endcode
   *
   * @see txStandBy()
   * @see write()
   * @see writeBlocking()
   *
   * @param buf Pointer to the data to be sent
   * @param len Number of bytes to be sent
   * @return True if the payload was delivered successfully false if not
   */
  bool writeFast(const void* buf, uint8_t len);

  /**
  * WriteFast for single NOACK writes. Disables acknowledgements/autoretries for a single write.
  *
  * @note enableDynamicAck() must be called to enable this feature
  * @see enableDynamicAck()
  * @see setAutoAck()
  *
  * @param buf Pointer to the data to be sent
  * @param len Number of bytes to be sent
  * @param multicast Request ACK (0) or NOACK (1)
  */
  bool writeFast(const void* buf, uint8_t len, const bool multicast);

  /**
   * Non-blocking write to the open writing pipe used for buffered writes
   *
   * @note Optimization: This function now leaves the CE pin high, so the radio
   * will remain in TX or STANDBY-II Mode until a txStandBy() command is issued. Can be used as an alternative to startWrite()
   * if writing multiple payloads at once.
   * @warning It is important to never keep the nRF24L01 in TX mode with FIFO full for more than 4ms at a time. If the auto
   * retransmit/autoAck is enabled, the nRF24L01 is never in TX mode long enough to disobey this rule. Allow the FIFO
   * to clear by issuing txStandBy() or ensure appropriate time between transmissions.
   *
   * @see write()
   * @see writeFast()
   * @see startWrite()
   * @see writeBlocking()
   *
   * For single noAck writes see:
   * @see enableDynamicAck()
   * @see setAutoAck()
   *
   * @param buf Pointer to the data to be sent
   * @param len Number of bytes to be sent
   * @param multicast Request ACK (0) or NOACK (1)
   * @return True if the payload was delivered successfully false if not
   */
  void startFastWrite( const void* buf, uint8_t len, const bool multicast, bool startTx = 1 );

private:
  uint32_t txRxDelay; /**< Var for adjusting delays depending on datarate */

  void csnLow(void);
  void csnHigh(void);

  /**
  * Empty the receive buffer
  *
  * @return Current value of status register
  */
  uint8_t flush_rx(void);

  /**
   * Empty the transmit buffer. This is generally not required in standard operation.
   * May be required in specific cases after stopListening() , if operating at 250KBPS data rate.
   *
   * @return Current value of status register
   */
  uint8_t flush_tx(void);
};


#endif //SCOUT_RF_RADIO_H
