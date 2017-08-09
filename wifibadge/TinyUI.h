/*
  TinyUI.h - Library for handling the ATTINY UI chip, supporting LEDs, capsense buttons, and cryptographic/storage functions.
  Created by The Hat, July 7, 2017.
  Released under the MIT License.
*/

#ifndef TinyUI_h
#define TinyUI_h

#include "Arduino.h"

#define TINYUI_PAYLOAD_LENGTH           15                  // number of data bytes in a packet

// parameters to update(uint8_t)
#define TINYUI_GET_DEFAULT              0x00                // don't wait for any specific information
#define TINYUI_GET_BUTTONS              0x01                // wait for complete button press data packet
#define TINYUI_GET_POWER                0x02                // wait for complete power voltage data packet
#define TINYUI_GET_NAVHASH              0x04                // wait for navhash data packet (normally used internally with navhash functions)
#define TINYUI_GET_NVM_RESULT           0x08                // wait for NVM operation result (normally used internally with NVM functions)

// buttons for isPressed(uint8_t) and getButton()
#define TINYUI_BUTTON_NONE              0x00                // no button pressed
#define TINYUI_BUTTON_SELECT            0x10                // center button pressed
#define TINYUI_BUTTON_UP                0x08                // up button pressed
#define TINYUI_BUTTON_RIGHT             0x04                // right button pressed
#define TINYUI_BUTTON_DOWN              0x02                // down button pressed
#define TINYUI_BUTTON_LEFT              0x01                // left button pressed

// power supply types for getPower(uint8_t)
#define TINYUI_POWER_USB                0x00                // USB voltage
#define TINYUI_POWER_LIPO               0x01                // LiPo battery voltage
#define TINYUI_POWER_AA                 0x02                // AA battery voltage

// count of various parameters
#define TINYUI_LED_COUNT                14                  // number of LEDs that can be controlled
#define TINYUI_BUTTON_COUNT             5                   // number of buttons that can be pressed
#define TINYUI_POWER_COUNT              5                   // number of supply voltages that can be queried (only 3 are implemented)

// miscellaneous constants
#define TINYUI_PULSE_LENGTH             10                  // this is the default pulse length found in the ATTINY's firmware
#define TINYUI_TRANS_IMMEDIATE          0x00                // this value for a transition means the new values take effect immediately
#define TINYUI_TRANS_IGNORE             0xff                // this value for a transition means the ATTINY will ignore the new value
                                                            //   (TINYUI_TRANS_IGNORE is less useful since we buffer all the LED settings,
                                                            //    but still used to prevent interfering with previously started transitions)

// ATTINY does animations at 100 frames/sec.
// default pulse length is 16 frames; pulse periods less than this will be on solid
// the update(uint8_t), NavHash, and NVM-related methods perform the actual communication with the ATTINY; all other commands just prepare and buffer data to be sent on update, and button/power data does not change until update is called
class TinyUI
{
  public:
    TinyUI(int csPin, int echPin);                          // chip select pin and extra channel pin
    boolean isRefilterCapSense(void);                       // returns true if this module is applying its own capacitive touch data filtering
    void begin(void);                                       // initializes state and associated hardware
    void enableExtraChannels(void);                         // allow the ATTINY to control the RX and TX LEDs
    void disableExtraChannels(void);                        // set control of the RX and TX LEDs back to the Arduino
    void setPixel(uint8_t n, uint8_t v);                    // dim pixel n to value v
    void setPixelTransition(uint8_t n, uint8_t v, uint8_t frames);   // dim pixel n to value v over frames LED animation frames
    void setPulse(uint8_t n, uint8_t v);                    // pulse pixel n with a period of v
    void setPulseTransition(uint8_t n, uint8_t v, uint8_t frames);   // pulse pixel n with a period of v over frames LED animation frames
    void setPixelPulseTransition(uint8_t n, uint8_t dim, uint8_t pulse, uint8_t frames);   // dim pixel n to value dim and pulse it with period pulse over frames LED animation frames
    void setPulseLength(uint8_t n, uint8_t v);              // set pulse length for pixel n to v frames (pulse periods less than this will be on solid)
    void buttonFeedbackOn(void);                            // enables blinking of LEDs when capsense buttons are pressed
    void buttonFeedbackOff(void);                           // disables blinking of LEDs when capsense buttons are pressed
    boolean buttonFeedbackEnabled(void);                    // returns true if LEDs are set to blink when capsense buttons are pressed
    void blingOff(void);                                    // turn off all bling modes
    void blingSpin(uint8_t speed, uint8_t n);               // spinner bling mode, n can be 1, 2, 3, or 4
    void blingHeartbeat(uint8_t speed, uint8_t period);     // heartbeat bling mode, speed is fade in / out speed, period is number of animation frames between beats
    void blingSparkle(uint8_t speed, uint8_t freq);         // sparkle bling mode, speed is fade out speed, freq is probability out of 255 each animation frame that a new LED will light up
    void blingSweep(uint8_t speed, uint8_t period);         // sweep bling mode, period is number of animation frames between animations
    void blingClock(uint8_t hours, uint8_t minutes, uint8_t seconds);   // clock bling mode, with hours (0-23), minutes (0-59), and seconds (0-59)
    void update(uint8_t flags);                             // commit changes to the ATTINY and get updated button and power supply data
    boolean isPressed(uint8_t btn);                         // returns true if the given button is currently pressed
    uint8_t getButton(void);                                // get the next button down event (returns TINYUI_BUTTON_NONE if there are no more button down events)
    uint16_t getPower(uint8_t powerType);                   // get the requested supply voltage in millivolts
    uint8_t getPixel(uint8_t n);                            // get the current dimming value for pixel n (this is the last set value; it may not have been commited to the ATTINY yet)
    uint8_t getPulse(uint8_t n);                            // get the current pulsing value for pixel n (this is the last set value; it may not have been commited to the ATTINY yet)
    uint8_t getPulseLength(uint8_t n);                      // get the current pulse length for pixel n (this is the last set value; it may not have been commited to the ATTINY yet)
    void getNavHashes(uint8_t len0, uint32_t *data0, uint8_t len1, uint32_t *data1, uint8_t len2, uint32_t *data2);   // get navigation hash values for the given initial vectors
    void getNavHashes(uint8_t len0, uint32_t *data0, uint8_t len1, uint32_t *data1);   // get navigation hash values for the given initial vectors
    void getNavHash(uint8_t len, uint32_t *data);           // get navigation hash value for the given initial vector
    void readFLASH(uint16_t addr, uint8_t len, void *data);   // read from FLASH memory
    void readEEPROM(uint16_t addr, uint8_t len, void *data);   // read from EEPROM memory
    void writeEEPROM(uint16_t addr, uint8_t len, const void *data);   // write to EEPROM memory
    void encrypt(uint8_t key, uint8_t len, void *data);     // encrypt data with the key of the given index (len must be a multiple of 4; the data pointer is used for both plaintext in and ciphertext out)
    void decrypt(uint8_t key, uint8_t len, void *data);     // decrypt data with the key of the given index (len must be a multiple of 4; the data pointer is used for both ciphertext in and plaintext out)
    void hash(uint8_t iv, uint8_t len, const void *data, uint32_t *out);   // hash the given data using the initial vector of the given index
    void getButtonHash(uint8_t len, uint32_t *data);        // get navigation hash value based on debounced data
    void seedRandom(void);                                  // seed the random number generator using analog data from the ATTINY88
    uint32_t _getNavHistory(uint8_t n);                     // dirty hack, going away in the next version
  private:
    boolean _refilterCapSense;                              // true if TinyUI capacitive touch filtering algorithm is newer than ATTINY88 code; if so, TinyUI uses its own filtering to detect button presses
    int _csPin, _echPin;                                    // chip select pin and extra channel pin
    uint8_t _btn[TINYUI_BUTTON_COUNT];                      // buffer of button press counts from the ATTINY (the ATTINY increments the corresponding counter each time a button is pressed)
    uint8_t _press[TINYUI_BUTTON_COUNT];                    // button press counter for acknowledged presses (this is incremented when getButton() returns the corresponding button until it is equal to _btn)
    uint16_t _capAvg[TINYUI_BUTTON_COUNT];                  // capacitive data for each button; this is the moving average filtered data provided by the ATTINY88 and needs to be thresholded to determine button state
    uint16_t _capThrPress[TINYUI_BUTTON_COUNT];             // threshold for detecting a pressed button (used only if _refilterCapSense is true)
    uint16_t _capThrRelease[TINYUI_BUTTON_COUNT];           // threshold for detecting a released button (used only if _refilterCapSense is true)
    uint8_t _btnMask;                                       // currently pressed buttons (used only if _refilterCapSense is true)
    uint16_t _pwr[TINYUI_POWER_COUNT];                      // buffer of supply voltages received from the ATTINY (multiply each of these values by 5.5 to get a result in millivolts)
    uint8_t _dim[TINYUI_LED_COUNT];                         // buffer of LED dimming values
    uint8_t _pulse[TINYUI_LED_COUNT];                       // buffer of LED pulsing periods
    uint8_t _trans[TINYUI_LED_COUNT];                       // buffer of transition lengths
    uint8_t _len[TINYUI_LED_COUNT];                         // buffer of pulse lengths
    uint8_t _bling[TINYUI_PAYLOAD_LENGTH];                  // parameters for bling modes
    uint32_t _navHistory;                                   // buttons pressed (this is handled here because of the debouncing bug)
    boolean _dimDirty;                                      // set to true when a dimming value has been changed, then reset after the new dimming values have been sent to the ATTINY
    boolean _pulseDirty;                                    // set to true when a pulsing value has been changed, then reset after the new pulsing values have been sent to the ATTINY
    boolean _lenDirty;                                      // set to true when a pulse length has been changed, then reset after the new pulse lengths have been sent to the ATTINY
    boolean _blingDirty;                                    // set to true when a bling mode has been changed, then reset after the new bling data has been sent to the ATTINY
    uint8_t _pressMask;                                     // mask of buttons that were pressed in a previous button state packet and have not yet been acknowledged
    uint8_t _pressAck;                                      // mask of button presses that have been acknowledged; if they are not present in the next button state packet, they will be removed from _pressMask
    boolean _isExtraChannels;                               // set to true if the ATTINY is in control of the RX and TX LEDs (pins will be set low on each call to update(uint8_t) because Arduino sometimes messes with them)
    uint8_t _rxFlags;                                       // used during update(uint8_t) to track which update packet types need to be received; bits are cleared as the requested packet types are received
    uint8_t _rxOpcode;                                      // packet currently being parsed
    uint8_t _rxPtr;                                         // offset into packet currently being parsed
    uint8_t _rxBuf[TINYUI_PAYLOAD_LENGTH];                  // packet currently being received
    uint8_t _nvmBuf[TINYUI_PAYLOAD_LENGTH];                 // NavHash and NVM packet buffer
    void _rxBegin(void);                                    // internal method to reset received packet parsing state
    void _rxByte(uint8_t b);                                // internal method to parse a received data byte
    void _txPacket(uint8_t op, uint8_t len, const void *ptr);   // transmit packet with opcode op, data length len, and payload data at ptr
    void _nvmOp(uint8_t opcode, uint8_t len, uint8_t outlen, uint16_t addr, void *rdata, const void *wdata);   // perform NVM operation
};

#endif

