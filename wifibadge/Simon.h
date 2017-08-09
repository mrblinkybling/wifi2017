/*
  Simon.h - Library for handling the Simon game.
  Created by The Hat, July 26, 2017.
  Released under the MIT License.
*/

#ifndef Simon_h
#define Simon_h

#include <Adafruit_SSD1306.h>
#include "Arduino.h"
#include "TinyUI.h"

#define SIMON_MAX_ROUNDS        10
#define SIMON_FLASH_FRAMES      16
#define SIMON_FLASH_DT         240
#define SIMON_WRONG_FRAMES      12
#define SIMON_WRONG_DT         120
#define SIMON_PLAY_DT         2500
#define SIMON_WRONG_COUNT       16
#define SIMON_ROUND_DT        1000
#define SIMON_WINNER_PAUSE     250
#define SIMON_WINNER_FRAMES     16
#define SIMON_WINNER_DT         25
#define SIMON_WINNER_CYCLES      8

#define SIMON_STATE_SHOW         0
#define SIMON_STATE_PLAY         1
#define SIMON_STATE_WRONG        2
#define SIMON_STATE_WINNER       3

typedef struct {
  long tAdvance;
  uint8_t gameState;
  uint8_t symbolCount;
  uint8_t curSymbol;
  uint8_t flashCount;
  uint8_t symbol[SIMON_MAX_ROUNDS];
} SimonGameData;

class Simon
{
  public:
    Simon(void);
    void setUi(TinyUI *ui, Adafruit_SSD1306 *display);
    void start(void);
    boolean play(uint8_t btn);
    void release(void);
    boolean isWinner(void);
  private:
    TinyUI *_ui;
    Adafruit_SSD1306 *_display;
    SimonGameData *_gameData;
    void _flashSymbol(uint8_t sym, uint8_t frames);
    boolean _addSymbol(void);
};

#endif

