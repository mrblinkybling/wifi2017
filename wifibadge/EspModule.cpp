/*
  EspModule.c - Library for handling the ESP-12E module.
  Created by The Hat, July 25, 2017.
  Released under the MIT License.
*/

//NOTE: This module is currently hard-coded to use Serial1; this should be changed some day.

#include "Arduino.h"
#include "EspModule.h"

// https://room-15.github.io/blog/2015/03/26/esp8266-at-command-reference/

#define SERIAL_BAUD_RATE 115200

#define RESPONSE_NONE   0
#define RESPONSE_ANY    1
#define RESPONSE_LIST   2

const PROGMEM char EndOfData[] = "\r\nOK\r\n";

EspModule::EspModule(void)
{
  _okState = 0;
  _parseState = 0;
  _curResponse = RESPONSE_NONE;
}

void EspModule::_resetResponse(uint8_t typ)
{
  _okState = 0;
  _parseState = 0;
  _curResponse = typ;
}

void EspModule::begin(void)
{
  Serial1.begin(SERIAL_BAUD_RATE);
  /*
  Serial1.print(F("AT+RST\r\n"));
  _resetResponse(RESPONSE_ANY);
  flushData();
  */
  Serial1.print(F("\r\n\r\nAT+CWMODE=1\r\n"));
  _resetResponse(RESPONSE_ANY);
  flushData();
}

void EspModule::_resetNetworkListElement(void)
{
  _parseCmd.listNetworks.security = 0;
  _parseCmd.listNetworks.ssidBuffer[0] = 0;
  _parseCmd.listNetworks.rssi = 0;
  memset(_parseCmd.listNetworks.mac, 0, 6);
  _parseCmd.listNetworks.channel = 0;
}

void EspModule::startListNetworks(void *obj, void *(*callback)(void *, uint8_t, const char *, int8_t, const uint8_t *, uint8_t), char *ssidBuffer, uint8_t ssidLen)
{
  _parseCmd.listNetworks.callback = callback;
  _parseCmd.listNetworks.obj = obj;
  _parseCmd.listNetworks.ssidBuffer = ssidBuffer;
  _parseCmd.listNetworks.ssidLen = ssidLen;
  _resetNetworkListElement();
  Serial1.print(F("AT+CWLAP\r\n"));
  _resetResponse(RESPONSE_LIST);
  // response: +CWLAP:(<security>,<ssid>,<rssi>,<mac>,<channel>,<???>,<???>)\r\n
  //           <security>: 0 = open, 1 = WEP, 2 = WPA_PSK, 3 = WPA2_PSK, 4 = WPA_WPA2_PSK
  //           <ssid> and <mac> are in double-quotes
}

uint8_t EspModule::_parseHex(char ch)
{
  if ((ch >= '0') && (ch <= '9')) {
    return ch - '0';
  } else if ((ch >= 'a') && (ch <= 'f')) {
    return ch + 10 - 'a';
  } else if ((ch >= 'A') && (ch <= 'F')) {
    return ch + 10 - 'a';
  } else {
    return 0xff;
  }
}

void EspModule::_startNumber(void)
{
  _parseNeg = false;
  _parseNum = 0;
}

boolean EspModule::_parseDigit(char ch)
{
  if ((!_parseNum) && (!_parseNeg) && ((ch == '-') || (ch == '+')))
  {
    _parseNeg = (ch == '-');
    return true;
  } else if ((ch >= '0') && (ch <= '9')) {
    _parseNum = (_parseNum * 10) + ch - '0';
    return true;
  } else {
    return false;
  }
}

int16_t EspModule::_endNumber(void)
{
  return _parseNeg ? -_parseNum : _parseNum;
}

boolean EspModule::handleData(void)
{
  char ch;
  boolean finishElement = false;
  uint8_t i;
  // Parse received bytes until \r\nOK\r\n
  while (Serial1.available()) {
    ch = Serial1.read();
    //Serial.write(ch);
    if (_curResponse == RESPONSE_LIST) {
      switch (_parseState) {
      case 1:   // parsing security and looking for comma
        if (_parseDigit(ch)) {
          // handled in _parseDigit
        } else if (ch == ',') {
          _parseCmd.listNetworks.security = _endNumber();
          _parseState = 2;
        } else if (ch == ')') {
          _parseCmd.listNetworks.security = _endNumber();
          finishElement = true;
        }
        break;
      case 2:   // looking for quote
        if (ch == '"') {
          _parsePtr = 0;
          _parseState = 3;
        } else if (ch == ',') {
          _startNumber();
          _parseState = 5;
        } else if (ch == ')') {
          finishElement = true;
        }
        break;
      case 3:   // reading SSID
        if (ch == '"') {
          _parseCmd.listNetworks.ssidBuffer[_parsePtr] = 0;
          _parseState = 4;
        } else if (_parsePtr < (_parseCmd.listNetworks.ssidLen - 1)) {
          _parseCmd.listNetworks.ssidBuffer[_parsePtr++] = ch;
        }
        break;
      case 4:   // looking for field separator
        if (ch == ',') {
          _startNumber();
          _parseState = 5;
        } else if (ch == ')') {
          finishElement = true;
        }
        break;
      case 5:   // reading RSSI
        if (_parseDigit(ch)) {
          // handled in _parseDigit
        } else if (ch == ',') {
          _parseCmd.listNetworks.rssi = _endNumber();
          _parseState = 6;
        } else if (ch == ')') {
          _parseCmd.listNetworks.rssi = _endNumber();
          finishElement = true;
        }
        break;
      case 6:   // looking for quote
        if (ch == '"') {
          _parsePtr = 0;
          _parseNeg = false;
          _parseState = 7;
        } else if (ch == ',') {
          _startNumber();
          _parseState = 9;
        } else if (ch == ')') {
          finishElement = true;
        }
        break;
      case 7:   // parsing MAC address
        if (ch == '"') {
          _parseState = 8;
        } else {
          i = _parseHex(ch);
          if ((i != 0xff) && (_parsePtr < 6)) {
            if (_parseNeg) {
              _parseCmd.listNetworks.mac[_parsePtr++] |= i;
              _parseNeg = false;
            }
            else
            {
              _parseCmd.listNetworks.mac[_parsePtr] = i << 4;
              _parseNeg = true;
            }
          }
        }
        break;
      case 8:
        if (ch == ',') {
          _startNumber();
          _parseState = 9;
        } else if (ch == ')') {
          finishElement = true;
        }
        break;
      case 9:
        if (_parseDigit(ch)) {
          // handled in _parseDigit
        } else if (ch == ',') {
          _parseCmd.listNetworks.channel = _endNumber();
          _parseState = 10;
        } else if (ch == ')') {
          _parseCmd.listNetworks.channel = _endNumber();
          finishElement = true;
        }
        break;
      case 10:   // looking for close parenthesis
        if (ch == ')') {
          finishElement = true;
        }
        break;
      default:   // looking for open parenthesis
        if (ch == '(') {
          _parseState = 1;
          _startNumber();
        } else {
          _parseState = 0;
        }
        break;
      }
      if (finishElement) {
        if (_parseCmd.listNetworks.callback) {
          _parseCmd.listNetworks.obj = _parseCmd.listNetworks.callback(_parseCmd.listNetworks.obj, _parseCmd.listNetworks.security, _parseCmd.listNetworks.ssidBuffer, _parseCmd.listNetworks.rssi, _parseCmd.listNetworks.mac, _parseCmd.listNetworks.channel);
        }
        _resetNetworkListElement();
        _parseState = 0;
        finishElement = false;
      }
    }
    // Any other commands that should be parsed may be added here
    if (_okState) {
      _okState = (ch == pgm_read_byte(&(EndOfData[_okState]))) ? _okState + 1 : 0;
      if (!pgm_read_byte(&(EndOfData[_okState]))) {
        _curResponse = RESPONSE_NONE;
      }
    }
    if ((!_okState) && (ch == pgm_read_byte(EndOfData))) {
      _okState = 1;
    }
  }
  return !!_curResponse;   // return true if still processing an operation
}

void EspModule::flushData()
{
  while (handleData()) ;
}

