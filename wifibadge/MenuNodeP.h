/*
  MenuNodeP.h - Library for handling menu items in program space.
  Created by The Hat, July 24, 2017.
  Released under the MIT License.
*/

#ifndef MenuNodeP_h
#define MenuNodeP_h

#include "Arduino.h"

typedef struct __menu_descr {
  uint32_t id;
  const void *data;
  PGM_P text;
  uint8_t locks;
  uint8_t reserved;
} __menu_descr_t;

#define _PASTE_INDIRECT(A, B) A ## B
#define _PASTE(A, B) _PASTE_INDIRECT(A, B)

/*
#define _PgmMenuNodeHelper(NAME, _DESCRNAME, ID, _DATANAME, DATA, _TEXTNAME, TEXT, CHILDREN...)   \
const PROGMEM uint8_t * const _DATANAME = DATA;                                                   \
const PROGMEM char _TEXTNAME[] = TEXT;                                                            \
const PROGMEM __menu_descr_t _DESCRNAME = {                                                       \
  .id = ID,                                                                                       \
  .data = _DATANAME,                                                                              \
  .text = _TEXTNAME                                                                               \
};                                                                                                \
MenuNodeP NAME(&_DESCRNAME, CHILDREN)

#define PgmMenuNode(NAME, ID, DATA, TEXT, CHILDREN...)   _PgmMenuNodeHelper(NAME, _PASTE(__MENU_DESCR_, __LINE__), ID, _PASTE(__MENU_DATA_, __LINE__), DATA, _PASTE(__MENU_TITLE_, __LINE__), TEXT, ## CHILDREN, NULL)
*/

#define _PgmMenuNodeHelper(NAME, _DESCRNAME, ID, LOCKS, _TEXTNAME, TEXT, CHILDREN...)          \
const PROGMEM char _TEXTNAME[] = TEXT;                                                         \
const PROGMEM __menu_descr_t _DESCRNAME = {                                                    \
  .id = ID,                                                                                    \
  .data = NULL,                                                                                \
  .text = _TEXTNAME,                                                                           \
  .locks = LOCKS,                                                                              \
  .reserved = 0                                                                                \
};                                                                                             \
MenuNodeP NAME(&_DESCRNAME, CHILDREN)
#define PgmMenuNode(NAME, ID, LOCKS, TEXT, CHILDREN...)   _PgmMenuNodeHelper(NAME, _PASTE(__MENU_DESCR_, __LINE__), ID, LOCKS, _PASTE(__MENU_TITLE_, __LINE__), TEXT, ## CHILDREN, NULL)

#define _PgmMenuLeafHelper(NAME, _DESCRNAME, ID, LOCKS, _TEXTNAME, TEXT, _DATANAME, DATA...)   \
const PROGMEM uint8_t _DATANAME[] = { DATA };                                                  \
const PROGMEM char _TEXTNAME[] = TEXT;                                                         \
const PROGMEM __menu_descr_t _DESCRNAME = {                                                    \
  .id = ID,                                                                                    \
  .data = _DATANAME,                                                                           \
  .text = _TEXTNAME,                                                                           \
  .locks = LOCKS,                                                                              \
  .reserved = 0                                                                                \
};                                                                                             \
MenuNodeP NAME(&_DESCRNAME, NULL)
#define PgmMenuLeaf(NAME, ID, LOCKS, TEXT, DATA...)       _PgmMenuLeafHelper(NAME, _PASTE(__MENU_DESCR_, __LINE__), ID, LOCKS, _PASTE(__MENU_TITLE_, __LINE__), TEXT, _PASTE(__MENU_DATA_, __LINE__), ## DATA)

#define PgmMenuText(NAME, ID, LOCKS, TEXT)                _PgmMenuNodeHelper(NAME, _PASTE(__MENU_DESCR_, __LINE__), ID, LOCKS, _PASTE(__MENU_TITLE_, __LINE__), TEXT, NULL)

class MenuNodeP
{
  public:
    MenuNodeP(const __menu_descr_t *descr, ...);
    ~MenuNodeP(void);
    static void setLocks(uint8_t locks);
    uint32_t getId(void);
    uint8_t getLocks(void);
    const void *getData(void);
    boolean hasData(void);
    uint8_t getDataByte(int n);
    PGM_P getText(void);
    char *readText(char *buf, size_t n);
    MenuNodeP *getParent(void);
    int getChildCount(void);
    MenuNodeP *getChild(int n);
  private:
    static uint8_t _locks;
    const __menu_descr_t *_descr;
    MenuNodeP *_parent;
    int _childCount;
    MenuNodeP **_children;
    uint8_t _lastLocks;
    uint8_t _unlockedCount;
    MenuNodeP **_unlockedChildren;
    void _checkLocks(void);
};

#endif

