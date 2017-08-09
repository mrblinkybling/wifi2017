/*
  EspModule.h - Library for handling the ESP-12E module.
  Created by The Hat, July 25, 2017.
  Released under the MIT License.
*/

#ifndef EspModule_h
#define EspModule_h

#include "Arduino.h"

#define ESP_SECURITY_OPEN           0
#define ESP_SECURITY_WEP            1
#define ESP_SECURITY_WPA_PSK        2
#define ESP_SECURITY_WPA2_PSK       3
#define ESP_SECURITY_WPA_WPA2_PSK   4

typedef union {
  struct {
    void *(*callback)(void *, uint8_t, const char *, int8_t, const uint8_t *, uint8_t);
    void *obj;
    uint8_t mac[6];
    char *ssidBuffer;
    uint8_t ssidLen;
    uint8_t security;
    int8_t rssi;
    uint8_t channel;
  } listNetworks;
} _EspModuleResponseParsingState;

class EspModule
{
  public:
    EspModule(void);
    void begin(void);
    void startListNetworks(void *obj, void *(*callback)(void *, uint8_t, const char *, int8_t, const uint8_t *, uint8_t), char *ssidBuffer, uint8_t ssidLen);
    boolean handleData(void);
    void flushData(void);
  private:
    uint8_t _okState;   // looking for \r\nOK\r\n to indicate end of response data
    uint8_t _parseState;   // parsing return data
    void _resetResponse(uint8_t typ);   // start parsing a new response
    uint8_t _parseHex(char ch);   // parse a single hex digit; return 0xff if not a valid hex digit
    boolean _parseNeg;   // true if number being parsed is negative
    uint16_t _parseNum;   // number being parsed
    void _startNumber(void);   // start parsing a new number
    boolean _parseDigit(char ch);   // parse a digit; return true if a valid numeric character
    int16_t _endNumber(void);   // return the parsed number
    uint8_t _curResponse;   // current response type being parsed
    uint8_t _parsePtr;   // pointer into current field being parsed
    _EspModuleResponseParsingState _parseCmd;
    void _resetNetworkListElement(void);
};

#endif

