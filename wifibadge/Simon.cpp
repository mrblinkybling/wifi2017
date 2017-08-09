/*
  Simon.c - Library for handling the Simon game.
  Created by The Hat, July 26, 2017.
  Released under the MIT License.
*/

#include <Adafruit_SSD1306.h>
#include "Arduino.h"
#include "TinyUI.h"
#include "Simon.h"

Simon::Simon(void)
{
  _ui = NULL;
}

void Simon::setUi(TinyUI *ui, Adafruit_SSD1306 *display)
{
  _ui = ui;
  _display = display;
  _gameData = NULL;
  _ui->seedRandom();
}

void Simon::start(void)
{
  _gameData = (SimonGameData *) malloc(sizeof(SimonGameData));
  memset(_gameData, 0, sizeof(SimonGameData));
  _display->clearDisplay();
  _display->display();
  _addSymbol();
  _ui->blingOff();
  _ui->buttonFeedbackOff();
  _ui->update(TINYUI_GET_DEFAULT);
  _ui->seedRandom();
  _gameData->tAdvance = millis() + SIMON_ROUND_DT;
}

boolean Simon::play(uint8_t btn)
{
  long t;
  if (!_gameData) {
    start();
  }
  t = millis();
  if (btn && (_gameData->gameState == SIMON_STATE_PLAY)) {
    if (btn == _gameData->symbol[_gameData->curSymbol]) {
      _flashSymbol(btn, SIMON_FLASH_FRAMES);
      _gameData->curSymbol++;
      if (_gameData->curSymbol < _gameData->symbolCount) {
        _gameData->tAdvance = t + SIMON_PLAY_DT;
      } else if (_addSymbol()) {
        _gameData->curSymbol = 0;
        _gameData->gameState = SIMON_STATE_SHOW;
        _gameData->tAdvance = t + SIMON_ROUND_DT;
      } else {
        _gameData->gameState = SIMON_STATE_WINNER;
        _gameData->tAdvance = t + SIMON_WINNER_PAUSE;
        _gameData->flashCount = 0;
        _gameData->curSymbol = 0;
      }
    } else {
      _gameData->gameState = SIMON_STATE_WRONG;
      _flashSymbol(_gameData->symbol[_gameData->curSymbol], SIMON_WRONG_FRAMES);
      _gameData->tAdvance = t + SIMON_WRONG_DT;
      _gameData->flashCount = 0;
    }
  } else if (t >= _gameData->tAdvance) {
    switch (_gameData->gameState) {
    case SIMON_STATE_SHOW:
      if (_gameData->curSymbol < _gameData->symbolCount) {
        _flashSymbol(_gameData->symbol[_gameData->curSymbol], SIMON_FLASH_FRAMES);
        _gameData->curSymbol++;
        _gameData->tAdvance = t + SIMON_FLASH_DT;
      } else {
        _gameData->curSymbol = 0;
        _gameData->gameState = SIMON_STATE_PLAY;
        _gameData->tAdvance = t + SIMON_PLAY_DT;
      }
      break;
    case SIMON_STATE_PLAY:
      _gameData->gameState = SIMON_STATE_WRONG;
      _flashSymbol(_gameData->symbol[_gameData->curSymbol], SIMON_WRONG_FRAMES);
      _gameData->tAdvance = t + SIMON_WRONG_DT;
      _gameData->flashCount = 0;
      break;
    case SIMON_STATE_WRONG:
      if (++_gameData->flashCount < SIMON_WRONG_COUNT) {
        _flashSymbol(_gameData->symbol[_gameData->curSymbol], SIMON_WRONG_FRAMES);
        _gameData->tAdvance = t + SIMON_WRONG_DT;
      } else {
        return false;
      }
      break;
    case SIMON_STATE_WINNER:
      _ui->setPixel(_gameData->curSymbol, 255);
      _ui->update(TINYUI_GET_DEFAULT);
      _ui->setPixelTransition(_gameData->curSymbol, 0, SIMON_WINNER_FRAMES);
      if (++_gameData->curSymbol >= TINYUI_LED_COUNT) {
        _gameData->curSymbol = 0;
        if (++_gameData->flashCount >= SIMON_WINNER_CYCLES) {
          return false;
        }
      }
      _gameData->tAdvance = t + SIMON_WINNER_DT;
      break;
    default:
      _gameData->gameState = SIMON_STATE_SHOW;
      break;
    }
  }
  return true;
}

void Simon::release(void)
{
  if (_gameData) {
    free(_gameData);
    _gameData = NULL;
  }
}

boolean Simon::isWinner(void)
{
  return _gameData && (_gameData->gameState == SIMON_STATE_WINNER);
}

void Simon::_flashSymbol(uint8_t sym, uint8_t frames)
{
  if (sym == TINYUI_BUTTON_SELECT) {
    _ui->setPixel(0, 255);
    _ui->setPixel(3, 255);
    _ui->setPixel(6, 255);
    _ui->setPixel(9, 255);
    _ui->update(TINYUI_GET_DEFAULT);
    _ui->setPixelTransition(0, 0, frames);
    _ui->setPixelTransition(3, 0, frames);
    _ui->setPixelTransition(6, 0, frames);
    _ui->setPixelTransition(9, 0, frames);
  } else if (sym == TINYUI_BUTTON_UP) {
    _ui->setPixel(11, 255);
    _ui->setPixel(0, 255);
    _ui->setPixel(1, 255);
    _ui->update(TINYUI_GET_DEFAULT);
    _ui->setPixelTransition(11, 0, frames);
    _ui->setPixelTransition(0, 0, frames);
    _ui->setPixelTransition(1, 0, frames);
  } else if (sym == TINYUI_BUTTON_RIGHT) {
    _ui->setPixel(2, 255);
    _ui->setPixel(3, 255);
    _ui->setPixel(4, 255);
    _ui->update(TINYUI_GET_DEFAULT);
    _ui->setPixelTransition(2, 0, frames);
    _ui->setPixelTransition(3, 0, frames);
    _ui->setPixelTransition(4, 0, frames);
  } else if (sym == TINYUI_BUTTON_DOWN) {
    _ui->setPixel(5, 255);
    _ui->setPixel(6, 255);
    _ui->setPixel(7, 255);
    _ui->update(TINYUI_GET_DEFAULT);
    _ui->setPixelTransition(5, 0, frames);
    _ui->setPixelTransition(6, 0, frames);
    _ui->setPixelTransition(7, 0, frames);
  } else if (sym == TINYUI_BUTTON_LEFT) {
    _ui->setPixel(8, 255);
    _ui->setPixel(9, 255);
    _ui->setPixel(10, 255);
    _ui->update(TINYUI_GET_DEFAULT);
    _ui->setPixelTransition(8, 0, frames);
    _ui->setPixelTransition(9, 0, frames);
    _ui->setPixelTransition(10, 0, frames);
  }
}

boolean Simon::_addSymbol(void)
{
  if (_gameData->symbolCount < SIMON_MAX_ROUNDS)
  {
    _gameData->symbol[_gameData->symbolCount++] = 1 << random(TINYUI_BUTTON_COUNT);
    return true;
  }
  else
  {
    return false;
  }
}

