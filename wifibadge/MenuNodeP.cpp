/*
  MenuNodeP.cpp - Library for handling menu items in program space.
  Created by The Hat, July 24, 2017.
  Released under the MIT License.
*/

#include "Arduino.h"
#include "MenuNodeP.h"

uint8_t MenuNodeP::_locks = 0;

MenuNodeP::MenuNodeP(const __menu_descr_t *descr, ...)
{
  int i;
  va_list args;
  
  _descr = descr;
  _parent = NULL;

  _childCount = 0;
  va_start(args, descr);
  while (va_arg(args, MenuNodeP *))
  {
    _childCount++;
  }
  va_end(args);

  _children = (MenuNodeP **) malloc(sizeof(MenuNodeP*) * _childCount);
  va_start(args, descr);
  for (i = 0; i < _childCount; i++)
  {
    _children[i] = va_arg(args, MenuNodeP *);
    _children[i]->_parent = this;
  }
  va_end(args);
  _lastLocks = ~_locks;
  _unlockedChildren = NULL;
  _checkLocks();
}

MenuNodeP::~MenuNodeP(void)
{
  free(_children);
}

void MenuNodeP::setLocks(uint8_t locks)
{
  _locks = locks;
}

uint32_t MenuNodeP::getId(void)
{
  return pgm_read_dword(&(_descr->id));
}

uint8_t MenuNodeP::getLocks(void)
{
  return pgm_read_byte(&(_descr->locks));
}

const void *MenuNodeP::getData(void)
{
  return pgm_read_ptr(&(_descr->data));
}

boolean MenuNodeP::hasData(void)
{
  return !!getData();
}

uint8_t MenuNodeP::getDataByte(int n)
{
  const void *p = getData();
  return p ? pgm_read_byte(((const uint8_t *)p) + n) : 0;
}

PGM_P MenuNodeP::getText(void)
{
  return pgm_read_ptr(&(_descr->text));
}

char *MenuNodeP::readText(char *buf, size_t n)
{
  return strncpy_P(buf, getText(), n);
}

MenuNodeP *MenuNodeP::getParent(void)
{
  return _parent;
}

int MenuNodeP::getChildCount(void)
{
  _checkLocks();
  return _unlockedCount;   //_childCount;
}

MenuNodeP *MenuNodeP::getChild(int n)
{
  _checkLocks();
  //return (n < _childCount) ? _children[n] : NULL;
  return (n < _unlockedCount) ? (_unlockedChildren ? _unlockedChildren : _children)[n] : NULL;
}

void MenuNodeP::_checkLocks(void)
{
  uint8_t i, l, u;
  if (_lastLocks != _locks) {
    if (_unlockedChildren) {
      free(_unlockedChildren);
      _unlockedChildren = NULL;
    }
    _lastLocks = _locks;
    l = ~_lastLocks;
    u = 0;
    for (i = 0; i < _childCount; i++) {
      if ((_children[i]->getLocks() & l) == 0) {
        u++;
      }
    }
    _unlockedCount = u;
    if (u < _childCount) {
      _unlockedChildren = (MenuNodeP **) malloc(sizeof(MenuNodeP *) * u);
      u = 0;
      for (i = 0; i < _childCount; i++) {
        if ((_children[i]->getLocks() & l) == 0) {
          _unlockedChildren[u++] = _children[i];
        }
      }
    }
  }
}

