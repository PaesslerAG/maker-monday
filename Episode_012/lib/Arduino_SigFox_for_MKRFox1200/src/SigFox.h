/*****************************************************************************/
/*
  Arduino library for Atmel SIGFOX module ATA8520.
  This library uses Arduino SPI library.
*/
/*****************************************************************************/

/*
 Copyright (c) 2016 Arduino LLC

 This software is open source software and is not owned by Atmel;
 you can redistribute it and/or modify it under the terms of the GNU 
 Lesser General Public License as published by the Free Software Foundation; 
 either version 2.1 of the License, or (at your option) any later version.

 You acknowledge that the ArduinoUNO software is distributed to you free of 
 charge on an "as is" basis and that it has not been developed to meet your 
 specific requirements. It is supplied in the hope that it will be useful but 
 WITHOUT ANY WARRANTY that its use will be uninterrupted or error-free.
 All other conditions, warranties or other terms which might have effect 
 between the parties or be implied or incorporated into this licence or any 
 collateral contract whether by statute or otherwise are hereby excluded, 
 including the implied conditions, warranties or other terms as to 
 satisfactory quality, fitness for purpose, non-infringement or the use of 
 reasonable skill and care.
 See the GNU Lesser General Public License for more details.
*/

#ifndef SIGFOX_h
#define SIGFOX_h

#include <Arduino.h>
#include <SPI.h>

#define BLEN  64            // Communication buffer length
#define MAX_RX_BUF_LEN  8
#define MAX_TX_BUF_LEN  13

typedef enum country {
  US = 0 ,
  EU
} Country;

typedef enum communication {
  TXRX = 0 ,
  TX
} TxRxMode;

typedef enum protocol {
  SSM = 0 ,
  ATMEL,
  SIGFOX
} Protocol;

class SIGFOXClass : public Stream
{
  public:

  /*
  * Enables debug LED and prints
  */
  void debug();
  /*
  * Disables debug LED and prints
  */
  void noDebug();
  /*
  * Initialize module (ready to transmit)
  */
  int begin();
    /*
  * Initialize module specifying SPI port and pins (ready to transmit)
  */
  int begin(SPIClass& spi, int reset, int poweron, int interrupt, int chip_select, int led);

  // Stream compatibility (like UDP)
  int beginPacket();
  int endPacket(bool rx = false);
  size_t write(uint8_t);
  size_t write(const uint8_t *buffer, size_t size);

  template <typename T> inline size_t write(T val) {return write((uint8_t*)&val, sizeof(T));};
  using Print::write;

  //makes no sense in Sigfox world
  void flush() {};

  /*
  * Commodity functions for Arduino-style message parse
  */
  int peek();
  int available();
  int read();

  int parsePacket();

  /*
  * Read status (fill ssm,atm,sig status variables)
  */
  void status();
  /*
  * Return status code.
  * Type: 0 -> ssm status ; 1 -> atm status ; 2 -> sigfox status    
  */
  int statusCode(Protocol type);
  /*
  * Return status code.
  * Type: 0 -> ssm status ; 1 -> atm status ; 2 -> sigfox status
  */
  char* status(Protocol type);
  /*
  * Return ATM version (major ver,minor ver)(two bytes)
  */
  String AtmVersion();
  /*
  * Return SIGFOX version (major ver, minor ver) (two bytes)
  */
  String SigVersion();
  /*
  * Return ID (4 bytes)
  */
  String ID();
  /*
  * Return PAC (16 bytes)
  */
  String PAC();
  /*
  * Reset module
  */
  void reset();

  float internalTemperature();

  /*
  *  Disable module
  */
  void end();

  private:

  /*
  * Send an array of bytes (max 12 bytes long) as message to SIGFOX network
  * Return SIGFOX status code
  */
  int send(unsigned char mess[], int len = 12, bool rx = false);

  /*
  * Send a single bit (0 | 1) over the Sigfox network
  * Returns the status code from the Atmel Sigfox chipset
  **/
  int sendBit(bool value);

  /*
  * Return atm status message
  */
  char* getStatusAtm();
  /*
  * Return SIGFOX status message
  */
  char* getStatusSig();

  int calibrateCrystal();

  /*
  * Test mode
  */
  void testMode (bool);

  char* readConfig(int* len);

  void setMode(Country EUMode, TxRxMode tx_rx);

  byte ssm;
  byte atm;
  byte sig;
  byte sig2;
  byte vhidle;
  byte vlidle;
  byte vhactive;
  byte vlactive;
  byte temperatureL;
  byte temperatureH;
  uint32_t tx_freq, rx_freq;
  uint8_t configuration;
  uint8_t repeat;
  bool _configured = false;
  char buffer[BLEN];
  unsigned char rx_buffer[MAX_RX_BUF_LEN];
  unsigned char tx_buffer[MAX_TX_BUF_LEN];
  int tx_buffer_index = -1;
  SPIClass *spi_port;
  int reset_pin;
  int poweron_pin;
  int interrupt_pin;
  int chip_select_pin;
  int led_pin;
  int rx_buf_len = 0;
  bool debugging = false;
};

extern SIGFOXClass SigFox;

#endif
