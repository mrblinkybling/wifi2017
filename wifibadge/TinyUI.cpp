/*
  TinyUI.cpp - Library for handling the ATTINY UI chip, supporting LEDs, capsense buttons, and cryptographic/storage functions.
  Created by The Hat, July 7, 2017.
  Released under the MIT License.
*/

#include <SPI.h>
#include "Arduino.h"
#include "TinyUI.h"

#define SPI_PAYLOAD_LENGTH    15        // number of bytes that follow an opcode
#define SPI_MAX_RX_TIMEOUT    96        // maximum number of bytes to wait for requested data

#define SPI_OP_DIM            0x6c      // set dimming data
#define SPI_OP_PULSE          0x69      // set pulse data
#define SPI_OP_TRANSITION     0x66      // set transition data for remainder of SPI transaction
#define SPI_OP_PULSE_CMP      0x63      // set pulse lengths
#define SPI_OP_BLING_MODE     0x3c      // set automatic bling mode
#define SPI_OP_NAVHASH_IVS    0x39      // get nav-hash data
#define SPI_OP_NVM_REQUEST    0x36      // perform NVM operation
#define SPI_OP_RESERVED_33    0x33      // reserved
#define SPI_OP_TOUCH          0x80      // touch data
#define SPI_OP_TOUCH_MASK     0xe0      // mask for SPI_OP_TOUCH
#define SPI_OP_ADC_DATA       0xcc      // ADC data
#define SPI_OP_RESERVED_C9    0xc9      // reserved
#define SPI_OP_NAVHASH_OUT    0xc6      // nav-hash result
#define SPI_OP_NVM_RESULT     0xc3      // NVM operation result

#define NVM_OP_MASK           0xf0      // NVM operation bitmask
#define NVM_OP_FLASH_READ     0x00      // read from FLASH memory
#define NVM_OP_FLASH_WRITE    0x10      // write to FLASH memory
#define NVM_OP_EEPROM_READ    0x20      // read from EEPROM
#define NVM_OP_EEPROM_WRITE   0x30      // write to EEPROM
#define NVM_OP_ENCRYPT        0x40      // encrypt data with internal key
#define NVM_OP_DECRYPT        0x50      // decrypt data with internal key
#define NVM_OP_HASH           0x60      // hash internal IV with given data
#define NVM_OP_RESERVED_7     0x70      // reserved
#define NVM_OP_RESERVED_8     0x80      // reserved
#define NVM_OP_RESERVED_9     0x90      // reserved
#define NVM_OP_RESERVED_A     0xa0      // reserved
#define NVM_OP_RESERVED_B     0xb0      // reserved
#define NVM_OP_RESERVED_C     0xc0      // reserved
#define NVM_OP_RESERVED_D     0xd0      // reserved
#define NVM_OP_GET_SIZES      0xe0      // get memory sizes
#define NVM_OP_ERROR          0xf0      // error performing NVM operation
#define NVM_LEN_MASK          0x0f      // NVM length bitmask

#define NVM_BUFFER_SIZE       12        // maximum number of bytes in NVM operation buffer

#define BLING_MODE_NONE       0x00      // no bling mode
#define BLING_MODE_SPIN       0x01      // spinner with 1, 2, 3, or 4 lights
#define BLING_MODE_HEARTBEAT  0x02      // fade in and out at an interval
#define BLING_MODE_SPARKLE    0x04      // random LEDs light up and fade out
#define BLING_MODE_SWEEP      0x08      // sweep from initial position to opposite side
#define BLING_MODE_RESERVED   0x10      // reserved bling mode
#define BLING_MODE_CLOCK      0x20      // clock face animation
#define BLING_MODE_BUTTONS    0x80      // flash LEDs when capsense buttons are activated

#define DEBOUNCE_MILLIS       200       // minimum number of milliseconds between recognized button presses
#define BUTTON_HASH_IV        0         // IV to use for getButtonHash
#define CAPSENSE_THRESHOLD_SH 6         // number of bits to shift when averaging in new capacitive touch threshold data

SPISettings TINYUI_SPISettings(250000, MSBFIRST, SPI_MODE0);   // should be able to communicate at 1MHz? Arduino SPI library doesn't seem to obey this exactly, so tell it much slower

TinyUI::TinyUI(int csPin, int echPin)
{
  uint8_t i;

  _csPin = csPin;
  _echPin = echPin;
  digitalWrite(_csPin, HIGH);
  pinMode(_csPin, OUTPUT);
  digitalWrite(_echPin, HIGH);
  pinMode(_echPin, OUTPUT);
}

boolean TinyUI::isRefilterCapSense(void)
{
  return _refilterCapSense;
}

void TinyUI::begin(void)
{
  uint8_t i;
  
  SPI.begin();
  
  disableExtraChannels();
  for (i = 0; i < TINYUI_BUTTON_COUNT; i++) {
    _btn[i] = 0;
    _press[i] = 0;
    _capAvg[i] = 0x3fff;
    _capThrPress[i] = 0;
    _capThrRelease[i] = 0;
  }
  _btnMask = 0;
  _navHistory = 0;
  for (i = 0; i < TINYUI_POWER_COUNT; i++) {
    _pwr[i] = 0;
  }
  for (i = 0; i < TINYUI_LED_COUNT; i++) {
    _dim[i] = 0;
    _pulse[i] = 0;
    _trans[i] = TINYUI_TRANS_IMMEDIATE;
    _len[i] = TINYUI_PULSE_LENGTH;
  }
  for (i = 0; i < TINYUI_PAYLOAD_LENGTH; i++) {
    _bling[i] = 0;
  }
  _isExtraChannels = false;
  _dimDirty = true;
  _pulseDirty = true;
  _lenDirty = true;
  _blingDirty = true;
  update(TINYUI_GET_BUTTONS | TINYUI_GET_POWER);
  _refilterCapSense = true;
  //FIXME: for next code version, check ATTINY88 firmware version and set _refilterCapSense accordingly
  //       (this is not needed until hardware is released with a newer version of the ATTINY88 firmware)
  if (_refilterCapSense)
  {
    for (i = 0; i < TINYUI_BUTTON_COUNT; i++)
    {
      _press[i] = _btn[i] = 0;
    }
  }
  else
  {
    for (i = 0; i < TINYUI_BUTTON_COUNT; i++)
    {
      _press[i] = _btn[i];
    }
  }
  _pressMask = 0;
  _pressAck = 0;
}

void TinyUI::enableExtraChannels(void)
{
  digitalWrite(_echPin, LOW);
  TXLED1;
  RXLED1;
  _isExtraChannels = true;
}

void TinyUI::disableExtraChannels(void)
{
  digitalWrite(_echPin, HIGH);
  _isExtraChannels = false;
}

void TinyUI::setPixel(uint8_t n, uint8_t v)
{
  if (n < TINYUI_LED_COUNT)
  {
    _dim[n] = v;
    _trans[n] = TINYUI_TRANS_IMMEDIATE;
    _dimDirty = true;
  }
}

void TinyUI::setPixelTransition(uint8_t n, uint8_t v, uint8_t frames)
{
  if (n < TINYUI_LED_COUNT)
  {
    _dim[n] = v;
    _trans[n] = frames;
    _dimDirty = true;
  }
}

void TinyUI::setPulse(uint8_t n, uint8_t v)
{
  if (n < TINYUI_LED_COUNT)
  {
    _pulse[n] = v;
    _trans[n] = TINYUI_TRANS_IMMEDIATE;
    _pulseDirty = true;
  }
}

void TinyUI::setPulseTransition(uint8_t n, uint8_t v, uint8_t frames)
{
  if (n < TINYUI_LED_COUNT)
  {
    _pulse[n] = v;
    _trans[n] = frames;
    _pulseDirty = true;
  }
}

void TinyUI::setPixelPulseTransition(uint8_t n, uint8_t dim, uint8_t pulse, uint8_t frames)
{
  if (n < TINYUI_LED_COUNT)
  {
    _dim[n] = dim;
    _pulse[n] = pulse;
    _trans[n] = frames;
    _dimDirty = true;
    _pulseDirty = true;
  }
}

void TinyUI::setPulseLength(uint8_t n, uint8_t v)
{
  if (n < TINYUI_LED_COUNT)
  {
    _len[n] = v;
    _lenDirty = true;
  }
}

void TinyUI::buttonFeedbackOn(void)
{
  _bling[0] |= BLING_MODE_BUTTONS;
  _blingDirty = true;
}

void TinyUI::buttonFeedbackOff(void)
{
  _bling[0] &= ~BLING_MODE_BUTTONS;
  _blingDirty = true;
}

boolean TinyUI::buttonFeedbackEnabled(void)
{
  return !!(_bling[0] & BLING_MODE_BUTTONS);
}

void TinyUI::blingOff(void)
{
  _bling[0] &= BLING_MODE_BUTTONS;   // buttons are enabled and disabled with buttonFeedbackOn and buttonFeedbackOff
  _blingDirty = true;
}

void TinyUI::blingSpin(uint8_t speed, uint8_t n)
{
  _bling[0] |= BLING_MODE_SPIN;
  _bling[1] = speed;
  _bling[2] = n;
  _blingDirty = true;
}

void TinyUI::blingHeartbeat(uint8_t speed, uint8_t period)
{
  _bling[0] |= BLING_MODE_HEARTBEAT;
  _bling[3] = speed;
  _bling[4] = period;
  _blingDirty = true;
}

void TinyUI::blingSparkle(uint8_t speed, uint8_t freq)
{
  _bling[0] |= BLING_MODE_SPARKLE;
  _bling[5] = speed;
  _bling[6] = freq;
  _blingDirty = true;
}

void TinyUI::blingSweep(uint8_t speed, uint8_t period)
{
  _bling[0] |= BLING_MODE_SWEEP;
  _bling[7] = speed;
  _bling[8] = period;
  _blingDirty = true;
}

void TinyUI::blingClock(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
  _bling[0] |= BLING_MODE_CLOCK;
  _bling[12] = seconds;
  _bling[13] = minutes;
  _bling[14] = seconds;
  _blingDirty = true;
}

void TinyUI::_rxBegin(void)
{
  _rxOpcode = 0;
}

void TinyUI::_rxByte(uint8_t b)
{
  uint8_t i;

  if (_rxOpcode)
  {
    _rxBuf[_rxPtr++] = b;
    if (_rxPtr >= SPI_PAYLOAD_LENGTH)
    {
      if (_rxOpcode == SPI_OP_TOUCH)
      {
        _rxFlags &= ~TINYUI_GET_BUTTONS;
        if (_refilterCapSense)
        {
          for (i = 0; i < TINYUI_BUTTON_COUNT; i++)
          {
            _capAvg[i] = ((uint16_t *)(_rxBuf + TINYUI_BUTTON_COUNT))[i];
          }
        }
        else
        {
          for (i = 0; i < TINYUI_BUTTON_COUNT; i++)
          {
            _btn[i] = _rxBuf[i];
          }
        }
      }
      else if (_rxOpcode == SPI_OP_ADC_DATA)
      {
        _rxFlags &= ~TINYUI_GET_POWER;
        for (i = 0; i < TINYUI_POWER_COUNT; i++)
        {
          _pwr[i] = ((uint16_t *)_rxBuf)[i];
        }
      }
      else if (_rxOpcode == SPI_OP_NAVHASH_OUT)
      {
        _rxFlags &= ~TINYUI_GET_NAVHASH;
        for (i = 0; i < TINYUI_PAYLOAD_LENGTH; i++)
        {
          _nvmBuf[i] = _rxBuf[i];
        }
      }
      else if (_rxOpcode == SPI_OP_NVM_RESULT)
      {
        _rxFlags &= ~TINYUI_GET_NVM_RESULT;
        for (i = 0; i < TINYUI_PAYLOAD_LENGTH; i++)
        {
          _nvmBuf[i] = _rxBuf[i];
        }
      }
      _rxOpcode = 0;
    }
  }
  else
  {
    if ((b & SPI_OP_TOUCH_MASK) == SPI_OP_TOUCH)
    {
      _rxOpcode = SPI_OP_TOUCH;
      if (!_refilterCapSense)
      {
        _pressMask &= ~_pressAck;
        _pressMask |= b & ~SPI_OP_TOUCH_MASK;
      }
    }
    else if ((b == SPI_OP_ADC_DATA) || (b == SPI_OP_RESERVED_C9) || (b == SPI_OP_NAVHASH_OUT) || (b == SPI_OP_NVM_RESULT))
    {
      _rxOpcode = b;
    }
    _rxPtr = 0;
  }
}

void TinyUI::_txPacket(uint8_t op, uint8_t len, const void *ptr)
{
  uint8_t i;
  const uint8_t *p = (const uint8_t *) ptr;

  _rxByte(SPI.transfer(op));
  for (i = len; i; i--)
  {
    _rxByte(SPI.transfer(*p++));
  }
  if (len < SPI_PAYLOAD_LENGTH)
  {
    for (i = SPI_PAYLOAD_LENGTH - len; i; i--)
    {
      _rxByte(SPI.transfer(0x00));
    }
  }
  _rxByte(SPI.transfer(0x00));   // bug in the ATTINY firmware is making it require an extra byte between packets to parse correctly
}

void TinyUI::update(uint8_t flags)
{
  uint8_t i;

  // Fix RX and TX LEDs if ATTINY is supposed to be controlling them (Arduino stuff tends to take them back periodically)
  if (_isExtraChannels)
  {
    TXLED1;
    RXLED1;
  }

  // Set up the SPI transaction
  _rxBegin();
  _rxFlags = flags;
  digitalWrite(_csPin, LOW);
  SPI.beginTransaction(TINYUI_SPISettings);

  // Start with bling mode data
  if (_blingDirty)
  {
    _txPacket(SPI_OP_BLING_MODE, TINYUI_PAYLOAD_LENGTH, _bling);
    _blingDirty = false;
  }

  // If the pulse lengths are dirty, resend them (transmit before transitions packet, since transitions do not apply to them)
  if (_lenDirty)
  {
    _txPacket(SPI_OP_PULSE_CMP, TINYUI_LED_COUNT, _len);
    _lenDirty = false;
  }

  // If all pixels will be updated, we can skip the transitions packet; else, we need to send it
  for (i = 0; (i < TINYUI_LED_COUNT) && (_trans[i] == TINYUI_TRANS_IMMEDIATE); i++) ;
  if (i < TINYUI_LED_COUNT)
  {
    _txPacket(SPI_OP_TRANSITION, TINYUI_LED_COUNT, _trans);
    // If a transition has been specified for a pixel, set the transition mask to ignore it in future updates (explicitly setting the pixel value will reset this)
    for (i = 0; i < TINYUI_LED_COUNT; i++)
    {
      if (_trans[i] != TINYUI_TRANS_IMMEDIATE)
      {
        _trans[i] = TINYUI_TRANS_IGNORE;
      }
    }
  }

  // Send updated dimming values
  if (_dimDirty)
  {
    _txPacket(SPI_OP_DIM, TINYUI_LED_COUNT, _dim);
    _dimDirty = false;
  }

  // Send updated pulsing values
  if (_pulseDirty)
  {
    _txPacket(SPI_OP_PULSE, TINYUI_LED_COUNT, _pulse);
    _pulseDirty = false;
  }

  // Execute a NavHash or NVM operation if one has been requested
  if (flags & TINYUI_GET_NAVHASH)
  {
    _txPacket(SPI_OP_NAVHASH_IVS, TINYUI_PAYLOAD_LENGTH, _nvmBuf);
  }
  else if (flags & TINYUI_GET_NVM_RESULT)
  {
    _txPacket(SPI_OP_NVM_REQUEST, TINYUI_PAYLOAD_LENGTH, _nvmBuf);
  }

  // Keep receiving packets until all requested data has been received
  for (i = SPI_MAX_RX_TIMEOUT; i && _rxFlags; i--)
  {
    _rxByte(SPI.transfer(0x00));
  }

  // Transaction cleanup
  SPI.endTransaction();
  digitalWrite(_csPin, HIGH);
}

boolean TinyUI::isPressed(uint8_t btn)
{
  _pressAck |= btn;
  return (_pressMask & btn) ? true : false;
}

uint8_t TinyUI::getButton(void)
{
  uint8_t r, h;
  uint16_t capTgt;

  // Internal capacitive sense filtering
  if (_refilterCapSense)
  {
    for (h = 0, r = 0x10; h < TINYUI_BUTTON_COUNT; h++, r >>= 1)
    {
      if (_capAvg[h] < _capThrPress[h])
      {
        // Lower than the press threshold means the button has been pressed; see if that means it has changed state
        if ((_btnMask & r) == 0)
        {
          _btnMask |= r;
          _btn[h]++;
        }
      }
      else if (_capAvg[h] > _capThrRelease[h])
      {
        // Higher than the release threshold means the button has been released
        _btnMask &= ~r;
        // If the button is safely in the released region, tweak the thresholds (this adapts the thresholds to changes in baseline capacitance due to environmental factors)
        capTgt = _capAvg[h] >> 1;   // observations show a press will quickly generate some values below 3/4 the baseline reading, so gravitate toward that
        capTgt = (capTgt + _capAvg[h]) >> 1;   // (we get 3/4 by taking 1/2 and averaging it with the full value)
        _capThrPress[h] += (capTgt >> CAPSENSE_THRESHOLD_SH) - (_capThrPress[h] >> CAPSENSE_THRESHOLD_SH);
        capTgt = (capTgt + _capAvg[h]) >> 1;   // observations show that a return to 7/8 baseline reading fairly accurately indicates a button release
        _capThrRelease[h] += (capTgt >> CAPSENSE_THRESHOLD_SH) - (_capThrRelease[h] >> CAPSENSE_THRESHOLD_SH);
      }
    }
    // Handle acknowledged presses
    _pressMask &= ~_pressAck;
    _pressMask |= _btnMask;
  }

  // Find a button that has been pressed more times than it has been returned by this function
  for (h = 0, r = 0x10; r && (_btn[h] == _press[h]); h++, r >>= 1) ;
  if (r)
  {
    _press[h]++;
  }

  // If reporting a button press, acknowledge it for the currently-pressed-buttons field and enter it into the history
  if (r)
  {
    _pressAck |= r;
    _navHistory = (_navHistory << 3) | (h + 1);
  }

  return r;
}

uint16_t TinyUI::getPower(uint8_t powerType)
{
  uint16_t p = (powerType < TINYUI_POWER_COUNT) ? _pwr[powerType] : 0;
  return (p << 2) + p + (p >> 1);   // multiply ADC value by 5.5 to get millivolts
}

uint8_t TinyUI::getPixel(uint8_t n)
{
  return (n < TINYUI_LED_COUNT) ? _dim[n] : 0;
}

uint8_t TinyUI::getPulse(uint8_t n)
{
  return (n < TINYUI_LED_COUNT) ? _pulse[n] : 0;
}

uint8_t TinyUI::getPulseLength(uint8_t n)
{
  return (n < TINYUI_LED_COUNT) ? _len[n] : 0;
}

void TinyUI::getNavHashes(uint8_t len0, uint32_t *data0, uint8_t len1, uint32_t *data1, uint8_t len2, uint32_t *data2)
{
  _nvmBuf[0] = len0;
  _nvmBuf[1] = len1;
  _nvmBuf[2] = len2;
  *((uint32_t *) (_nvmBuf + 3)) = data0 ? (*data0) : 0;
  *((uint32_t *) (_nvmBuf + 7)) = data1 ? (*data1) : 0;
  *((uint32_t *) (_nvmBuf + 11)) = data2 ? (*data2) : 0;
  update(TINYUI_GET_NAVHASH);
  if (data0) {
    *data0 = *((uint32_t *) (_nvmBuf + 3));
  }
  if (data1) {
    *data1 = *((uint32_t *) (_nvmBuf + 7));
  }
  if (data2) {
    *data2 = *((uint32_t *) (_nvmBuf + 11));
  }
}

void TinyUI::getNavHashes(uint8_t len0, uint32_t *data0, uint8_t len1, uint32_t *data1)
{
  getNavHashes(len0, data0, len1, data1, 0, NULL);
}

void TinyUI::getNavHash(uint8_t len, uint32_t *data)
{
  getNavHashes(len, data, 0, NULL, 0, NULL);
}

void TinyUI::_nvmOp(uint8_t opcode, uint8_t len, uint8_t outlen, uint16_t addr, void *rdata, const void *wdata)
{
  uint8_t i;
  uint8_t _len;
  _len = (len > NVM_BUFFER_SIZE) ? NVM_BUFFER_SIZE : len;
  _nvmBuf[0] = opcode | _len;
  *((uint16_t *) (_nvmBuf + 1)) = addr;
  if (wdata)
  {
    for (i = 0; i < _len; i++)
    {
      _nvmBuf[i + 3] = ((uint8_t *)wdata)[i];
    }
    for (i = _len; i < NVM_BUFFER_SIZE; i++)
    {
      _nvmBuf[i + 3] = 0;
    }
  }
  update(TINYUI_GET_NVM_RESULT);
  if (rdata)
  {
    _len = (outlen > NVM_BUFFER_SIZE) ? NVM_BUFFER_SIZE : outlen;
    for (i = 0; i < _len; i++)
    {
      ((uint8_t *)rdata)[i] = _nvmBuf[i + 3];
    }
  }
}

void TinyUI::readFLASH(uint16_t addr, uint8_t len, void *data)
{
  _nvmOp(NVM_OP_FLASH_READ, len, len, addr, data, NULL);
}

void TinyUI::readEEPROM(uint16_t addr, uint8_t len, void *data)
{
  _nvmOp(NVM_OP_EEPROM_READ, len, len, addr, data, NULL);
}

void TinyUI::writeEEPROM(uint16_t addr, uint8_t len, const void *data)
{
  _nvmOp(NVM_OP_EEPROM_WRITE, len, len, addr, NULL, data);
}

void TinyUI::encrypt(uint8_t key, uint8_t len, void *data)
{
  _nvmOp(NVM_OP_ENCRYPT, len, len, key, data, data);
}

void TinyUI::decrypt(uint8_t key, uint8_t len, void *data)
{
  _nvmOp(NVM_OP_DECRYPT, len, len, key, data, data);
}

void TinyUI::hash(uint8_t iv, uint8_t len, const void *data, uint32_t *out)
{
  _nvmOp(NVM_OP_HASH, len, 4, iv, out, data);
}

void TinyUI::getButtonHash(uint8_t len, uint32_t *data)
{
  struct {
    uint32_t x;
    uint32_t y;
  } buf;
  uint32_t m;
  uint8_t i;
  for (i = 0, m = 0; i < len; i++, m = m << 3 | 7) ;
  buf.x = *data;
  buf.y = _navHistory & m;
  hash(BUTTON_HASH_IV, 8, &buf, data);
}

void TinyUI::seedRandom(void)
{
  uint8_t i;
  uint32_t s;
  s = 0;
  for (i = 0; i < TINYUI_BUTTON_COUNT; i++) {
    s = (s << 8) ^ (s >> 24) ^ _capAvg[i];
  }
  for (i = 0; i < TINYUI_POWER_COUNT; i++) {
    s = (s << 8) ^ (s >> 24) ^ _pwr[i];
  }
  randomSeed(s);
}

uint32_t TinyUI::_getNavHistory(uint8_t n) {
  uint32_t m;
  uint8_t i;
  for (i = 0, m = 0; i < n; i++, m = (m << 3) | 0x07) ;
  return _navHistory & m;
}

