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


#include "SigFox.h"
#include "SPI.h"

#ifdef SIGFOX_SPI
#include "ArduinoLowPower.h"
#endif

const char str0[]  = "OK";
const char str1[]  = "Manufacturer error";
const char str2[]  = "ID or key error";
const char str3[]  = "State machine error";
const char str4[]  = "Frame size error";
const char str5[]  = "Manufacturer send error";
const char str6[]  = "Get voltage/temperature error";
const char str7[]  = "Close issues encountered";
const char str8[]  = "API error indication";
const char str9[]  = "Error getting PN9";
const char str10[] = "Error getting frequency";
const char str11[] = "Error building frame";
const char str12[] = "Error in delay routine";
const char str13[] = "Callback causes error";
const char str14[] = "Timing error";
const char str15[] = "Frequency error";

const char * sigstr[16]  =        // SIGFOX message status
{str0, str1, str2, str3, str4, str5, str6, str7, str8, str9, str10, str11, str12, str13, str14, str15};

#define SPICONFIG   SPISettings(100000UL, MSBFIRST, SPI_MODE0)

void SIGFOXClass::debug() {
  // Enables debug via LED and Serial prints
  // Also disables greedy sleep strategy
  debugging = true;
  pinMode(led_pin, OUTPUT);
}

void SIGFOXClass::noDebug() {
  debugging = false;
}

int SIGFOXClass::begin()
{
#ifdef SIGFOX_SPI
  spi_port = &SIGFOX_SPI;
  reset_pin = SIGFOX_RES_PIN;
  poweron_pin = SIGFOX_PWRON_PIN;
  interrupt_pin = SIGFOX_EVENT_PIN;
  chip_select_pin = SIGFOX_SS_PIN;
  led_pin = LED_BUILTIN;
#else
  // begin() can only be used on boards with embedded Sigfox module
  if (_configured == false) {
    return false;
  }
#endif

  pinMode(interrupt_pin, INPUT_PULLUP);
  pinMode(poweron_pin, OUTPUT);
  digitalWrite(poweron_pin, HIGH);
  pinMode(chip_select_pin, OUTPUT);
  digitalWrite(chip_select_pin, HIGH);
  pinMode(reset_pin, OUTPUT);
  // Power cycle the chip
  digitalWrite(reset_pin, HIGH);
  delay(10);
  digitalWrite(reset_pin, LOW);
  delay(10);
  digitalWrite(reset_pin, HIGH);
  spi_port->begin();
  spi_port->setDataMode(SPI_MODE0);
  spi_port->setBitOrder(MSBFIRST);

  delay(100);

  String version = SigVersion();
  if (version == "0.0")
    return false;
  return true;
}

int SIGFOXClass::begin(SPIClass& spi, int reset, int poweron, int interrupt, int chip_select, int led)
{
  spi_port = &spi;
  reset_pin = reset;
  poweron_pin = poweron;
  interrupt_pin = interrupt;
  chip_select_pin = chip_select;
  led_pin = led;
  _configured = true;
  return begin();
}

int SIGFOXClass::send(unsigned char mess[], int len, bool rx)
{
  if (len == 0) return 98;

  if (rx == false && len == 1 && mess[0] < 2) {
    //we can use send_bit command
    return sendBit(mess[0]);
  }

  status();

  digitalWrite(chip_select_pin, LOW);
  delay(1);
  spi_port->beginTransaction(SPICONFIG);
  if (len > 12) len = 12;
  int i = 0;

  spi_port->transfer(0x07);
  spi_port->transfer(len);
  spi_port->transfer(mess, len);
  spi_port->endTransaction();
  delay(1);
  digitalWrite(chip_select_pin, HIGH);

  delay(5);
  calibrateCrystal();
  delay(5);

  digitalWrite(chip_select_pin, LOW);
  delay(1);
  spi_port->beginTransaction(SPICONFIG);
  if (rx) {
    spi_port->transfer(0x0E);
  } else {
    spi_port->transfer(0x0D);
  }
  spi_port->endTransaction();
  delay(1);
  digitalWrite(chip_select_pin, HIGH);
  int ret = 99;

  int timeout = 10000;  //10 seconds
  if (rx) {
    timeout = 60000;    //60 seconds
  }

  if (!debugging) {
#ifdef SIGFOX_SPI
    LowPower.attachInterruptWakeup(interrupt_pin, NULL, FALLING);
    LowPower.sleep(timeout);
#endif
    if (digitalRead(interrupt_pin) == 0) {
      status();
      ret = statusCode(SIGFOX);
    }
    goto exit;
  }

  for (i = 0; i < timeout/10; i++)
  {
    if (digitalRead(interrupt_pin) == 0) {
      status();
      ret = statusCode(SIGFOX);
      break;
    }
    else {
      digitalWrite(led_pin, HIGH);
      delay(50);
      digitalWrite(led_pin, LOW);
      delay(50);
    }
  }

exit:
  if (ret == 99) sig = 13;

  if (sig == 0 && rx) {
    digitalWrite(chip_select_pin, LOW);
    delay(1);
    spi_port->beginTransaction(SPICONFIG);
    spi_port->transfer(0x10);
    spi_port->transfer(MAX_RX_BUF_LEN);
    spi_port->transfer(rx_buffer, MAX_RX_BUF_LEN);
    spi_port->endTransaction();
    delay(1);
    digitalWrite(chip_select_pin, HIGH);

    rx_buf_len = MAX_RX_BUF_LEN;
  }

  return sig;
}

int SIGFOXClass::sendBit(bool value){
  int ret;
  int i = 0;
  status();

  digitalWrite(chip_select_pin, LOW);
  delay(1);
  spi_port->beginTransaction(SPICONFIG);

  spi_port->transfer(0x0B);
  spi_port->transfer(value ? 1 : 0);
  spi_port->endTransaction();
  delay(1);
  digitalWrite(chip_select_pin, HIGH);

  int timeout = 7000;  //7 seconds

  if (!debugging) {
#ifdef SIGFOX_SPI
    LowPower.attachInterruptWakeup(interrupt_pin, NULL, FALLING);
    LowPower.sleep(timeout);
#endif
    if (digitalRead(interrupt_pin) == 0) {
      status();
      return  statusCode(SIGFOX);
    }
  }

  for (i = 0; i < timeout/10; i++)
  {
    if (digitalRead(interrupt_pin) == 0) {
      status();
      return statusCode(SIGFOX);
      break;
    }
    else {
      digitalWrite(led_pin, HIGH);
      delay(50);
      digitalWrite(led_pin, LOW);
      delay(50);
    }
  }
  return 99; //default
}

int SIGFOXClass::beginPacket() {
  bool ret = (tx_buffer_index == -1);
  tx_buffer_index = 0;
  return (ret ? 1 : 0);
}

int SIGFOXClass::endPacket(bool rx) {
  int ret = send(tx_buffer, tx_buffer_index, rx);
  // invalidate the buffer
  tx_buffer_index = -1;
  return ret;
}

size_t SIGFOXClass::write(uint8_t val) {
  if (tx_buffer_index >= 0 && tx_buffer_index < MAX_TX_BUF_LEN) {
      tx_buffer[tx_buffer_index++] = val;
      return 1;
  }
  return 0;
};

size_t SIGFOXClass::write(const uint8_t *buffer, size_t size) {
  if (tx_buffer_index >= 0) {
    if (tx_buffer_index + size < MAX_TX_BUF_LEN) {
      memcpy(&tx_buffer[tx_buffer_index], buffer, size);
      tx_buffer_index += size;
      return size;
    } else {
      int len = MAX_TX_BUF_LEN - tx_buffer_index -1;
      memcpy(&tx_buffer[tx_buffer_index], buffer, len);
      tx_buffer_index += len;
      return len;
    }
  }
  return 0;
}

int SIGFOXClass::available() {
  return rx_buf_len;
}

int SIGFOXClass::read() {
  int ret = rx_buffer[0];
  for (int i = 0; i < rx_buf_len - 1; i++) {
    rx_buffer[i] = rx_buffer[i+1];
  }
  rx_buf_len --;
  return ret;
}

int SIGFOXClass::peek() {
  return rx_buffer[0];
}

int SIGFOXClass::parsePacket() {
  if (rx_buf_len == MAX_RX_BUF_LEN) {
    return MAX_RX_BUF_LEN;
  }
  return 0;
}

int SIGFOXClass::calibrateCrystal() {
  digitalWrite(chip_select_pin, LOW);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x14);
  spi_port->endTransaction();
  delay(1);
  digitalWrite(chip_select_pin, HIGH);
  delay(1);
  int ret = 99;
  for (int i = 0; i < 6000; i++)
  {
    if (digitalRead(interrupt_pin) == 0) {
      status();
      ret = statusCode(SIGFOX);
      break;
    }
    else {
      digitalWrite(led_pin, HIGH);
      delay(50);
      digitalWrite(led_pin, LOW);
      delay(50);
    }
  }
  if (ret == 99) sig = 13;
  return sig;
}

int SIGFOXClass::statusCode(Protocol type)
{
  switch (type)
  {
    case SSM : return ssm; break;
    case ATMEL : return atm; break;
    case SIGFOX : return sig; break;
  }
  return -1;
}

char* SIGFOXClass::status(Protocol type)
{
  status();
  switch (type)
  {
    case SSM :
      break;
    case ATMEL :
      return getStatusAtm();
      break;
    case SIGFOX :
      return getStatusSig();
      break;
  }
  buffer[0] = '\0';
  return (char*)buffer;
}

char* SIGFOXClass::getStatusAtm()
{
  buffer[0] = '\0';
  byte err = (atm & B0011110) >> 1;
  char pa[10]; pa[0] = '\0';
  if (bitRead(atm, 0) == 1) strcpy(pa, "PA ON"); else strcpy(pa, "PA OFF");
  if (err > 0)
  {
    snprintf((char*)buffer, BLEN, "Error code: %i", err);
    return (char*)buffer;
  }
  if (bitRead(atm, 5) == 1)
  {
    snprintf((char*)buffer, BLEN, "Frame sent");
    return (char*)buffer;
  }
  if (bitRead(atm, 6) == 1)
  {
    snprintf((char*)buffer, BLEN, "%s . System ready", pa);
    return (char*)buffer;
  }
  snprintf((char*)buffer, BLEN, "%s", pa);
  return (char*)buffer;
}

char* SIGFOXClass::getStatusSig()
{
  if (sig > 0xF)
  {
    snprintf((char*)buffer, BLEN, "Controller comm. error: %d", sig);
    return (char*)buffer;
  }
  strncpy((char*)buffer, sigstr[sig], BLEN);

  return (char*)buffer;
}

void SIGFOXClass::status()
{
  digitalWrite(chip_select_pin, LOW);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x0A);
  spi_port->transfer(0);
  ssm = spi_port->transfer(0);
  atm = spi_port->transfer(0);
  sig = spi_port->transfer(0);
  sig2 = spi_port->transfer(0);
  spi_port->endTransaction();
  digitalWrite(chip_select_pin, HIGH);
  delay(1);
}

float SIGFOXClass::internalTemperature()
{
  digitalWrite(chip_select_pin, LOW);
  delay(1);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x14);
  spi_port->endTransaction();
  delay(1);
  digitalWrite(chip_select_pin, HIGH);
  delay(1);

  for (int i = 0; i < 10; i++)
  {
    if (digitalRead(interrupt_pin) == 0) {
      status();
      break;
    }
    else {
      delay(10);
    }
  }

  digitalWrite(chip_select_pin, LOW);
  delay(1);
  spi_port->beginTransaction(SPICONFIG);
  uint8_t buf[8];
  buf[0] = 0x13;
  spi_port->transfer(buf, 8);
  temperatureL = buf[6];
  temperatureH = buf[7];
  delay(1);
  spi_port->endTransaction();
  digitalWrite(chip_select_pin, HIGH);
  delay(1);

  return ((float)((int16_t)((uint16_t)temperatureH << 8 | temperatureL)) - 50.0f) / 10;
}

char* SIGFOXClass::readConfig(int* len)
{

  digitalWrite(chip_select_pin, LOW);
  delay(1);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x1F);
  spi_port->endTransaction();
  digitalWrite(chip_select_pin, HIGH);

  delay(5);

  digitalWrite(chip_select_pin, LOW);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x20);
  spi_port->transfer(0);
  tx_freq = SPI.transfer(0) | tx_freq << 8;
  tx_freq = SPI.transfer(0) | tx_freq << 8;
  tx_freq = SPI.transfer(0) | tx_freq << 8;
  tx_freq = SPI.transfer(0) | tx_freq << 8;
  rx_freq = SPI.transfer(0) | rx_freq << 8;
  rx_freq = SPI.transfer(0) | rx_freq << 8;
  rx_freq = SPI.transfer(0) | rx_freq << 8;
  rx_freq = SPI.transfer(0) | rx_freq << 8;
  repeat = spi_port->transfer(0);
  configuration = spi_port->transfer(0);
  spi_port->endTransaction();
  delay(1);
  digitalWrite(chip_select_pin, HIGH);
  delay(1);

  buffer[0] = tx_freq;
  buffer[4] = rx_freq;
  buffer[8] = repeat;
  buffer[9] = configuration;
  buffer[10] = 0;
  *len = 10;
  return (char*)buffer;
}

String SIGFOXClass::AtmVersion()
{
  digitalWrite(chip_select_pin, LOW);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x06);
  spi_port->transfer(0);
  byte mv = spi_port->transfer(0);
  byte lv = spi_port->transfer(0);
  spi_port->endTransaction();
  digitalWrite(chip_select_pin, HIGH);
  delay(1);
  snprintf(buffer, BLEN, "%d.%d", mv, lv);
  return String(buffer);
}

String SIGFOXClass::SigVersion()
{
  digitalWrite(chip_select_pin, LOW);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x06);
  spi_port->transfer(0);
  byte mv = spi_port->transfer(0);
  byte lv = spi_port->transfer(0);
  spi_port->endTransaction();
  digitalWrite(chip_select_pin, HIGH);
  delay(1);
  snprintf(buffer, BLEN, "%d.%d", mv, lv);
  return String(buffer);
}

String SIGFOXClass::ID()
{
  digitalWrite(chip_select_pin, LOW);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x12);
  spi_port->transfer(0);
  byte ID3 = spi_port->transfer(0);
  byte ID2 = spi_port->transfer(0);
  byte ID1 = spi_port->transfer(0);
  byte ID0 = spi_port->transfer(0);
  spi_port->endTransaction();
  digitalWrite(chip_select_pin, HIGH);
  delay(1);
  snprintf(buffer, BLEN, "%02X%02X%02X%02X", ID0, ID1, ID2, ID3);
  return String(buffer);
}

String SIGFOXClass::PAC()
{
  uint8_t pac[16];
  digitalWrite(chip_select_pin, LOW);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x0F);
  spi_port->transfer(0);
  for (int i = 0; i < 16; i++) {
    pac[i] = spi_port->transfer(0);
  }
  spi_port->endTransaction();
  digitalWrite(chip_select_pin, HIGH);
  delay(1);
  for (int i = 0; i < 8; i++) {
    snprintf(buffer + (i * 2), BLEN - (i*2), "%02X", pac[i]);
  }
  return String(buffer);
}

void SIGFOXClass::reset()
{
  digitalWrite(chip_select_pin, LOW);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x01);
  spi_port->endTransaction();
  digitalWrite(chip_select_pin, HIGH);
  delay(1);
}

void SIGFOXClass::testMode(bool on)
{
  digitalWrite(chip_select_pin, LOW);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x17);
  if (on) {
    spi_port->transfer(0x11);
  } else {
    spi_port->transfer(0x00);
  }
  spi_port->endTransaction();
  digitalWrite(chip_select_pin, HIGH);
  delay(1);
}

void SIGFOXClass::setMode(Country EUMode, TxRxMode tx_rx)
{
  digitalWrite(chip_select_pin, LOW);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x11);
  spi_port->transfer(0);
  spi_port->transfer(1);
  spi_port->transfer(0x2);
  uint8_t mode = (0x3 << 4) | (1 << 3) | (EUMode << 2) | (tx_rx << 1) | 1;
  spi_port->transfer(mode);
  spi_port->endTransaction();
  digitalWrite(chip_select_pin, HIGH);
  delay(1);

  int ret = 99;
  for (int i = 0; i < 300; i++)
  {
    if (digitalRead(interrupt_pin) == 0) {
      status();
      ret = statusCode(SIGFOX);
      break;
    }
    delay(10);
  }
  if (ret == 99) {
    Serial.println("Failed to set mode");
  }

  digitalWrite(chip_select_pin, LOW);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x05);
  spi_port->endTransaction();
  digitalWrite(chip_select_pin, HIGH);
  delay(100);
}

void SIGFOXClass::end()
{
  pinMode(poweron_pin, LOW);
  delay(1);
  digitalWrite(chip_select_pin, LOW);
  delay(1);
  spi_port->beginTransaction(SPICONFIG);
  spi_port->transfer(0x05);
  spi_port->endTransaction();
  delay(1);
  digitalWrite(chip_select_pin, HIGH);
  delay(1);
}

SIGFOXClass SigFox; //singleton
