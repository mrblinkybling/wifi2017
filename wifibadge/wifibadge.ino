/*
  wifibadge.ino - Main source file for Mr. Blinky Bling 2017 WiFi badge.
  Created by Mr. Blinky Bling, July 24, 2017.
  Released under the MIT License.
*/

#include <avr/pgmspace.h>       // The necessary C functions for accessing things stored in Program Space on the MCU
#include <SPI.h>                // Necessary when using SPI
#include <Wire.h>               // Arduino I2C communication library
#include <Adafruit_GFX.h>       // The library necessary for fonts and graphics on the screen
#include <Adafruit_SSD1306.h>   // Talks to the specific screen we are using
#include "TinyUI.h"             // The user interface object - documented in TinyUI.h and below
#include "MenuNodeP.h"          // The menu object - documented below
#include "EspModule.h"          // The ESP module interface
#include "Simon.h"              // Simon Says game

// Maximum (zero-based) line number in the menu display (this should probably not be changed unless you change the font on the screen)
#define MENU_HEIGHT 3

// The number of WiFi channels that can be scanned for
#define CHANNEL_COUNT 14

// Milliseconds between WiFi scans
#define SCAN_INTERVAL 5000

// Maximum amount of activity on a channel; e.g. if there are more than 16 accesss points in a channel it will hold the LED on solid instead of attempting to flash
#define DEFAULT_MAX_ACTIVITY 16

// Shift for multiplying channel activity; this is actually a bitshift meaning it multiplies activity by 4 making it more apparent when flashing the LEDs (you should probably not change this)
#define ACTIVITY_SH 2

// Too many wifi networks can overflow the RAM... (you should probably not change this unless you make it smaller) -- NOTE Max = 256
#define MAX_NETWORKS_RAM   256

// Too many secrets
#define SECRET_ANIMATE_MILLIS   1000

// Easter egg stuff; good hunting!
#define RED_PILL_INTERVAL   250
#define RED_PILL_FRAMES      16

// Setup hardware SPI parameters (you probably shouldn't change these)
#define OLED_DC     5
#define OLED_CS    13
#define OLED_RESET  4
Adafruit_SSD1306 display(OLED_DC, OLED_RESET, OLED_CS);

// Configuration options for the screensaver with the falling logos effect
#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2

// Configuration variables for the logo; in our case it is 16x13 pixels
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  13

#define HowBigIsThisArray(x) (sizeof(x) / sizeof(x[0])) // Handy array tool you might like to use; you're welcome hacker!

// The Mr Blinky Bling logo, in byte array glory:
static const unsigned char PROGMEM mr_blinky_bling_logo_bmp[] =
{ B10000001, B00000001,
  B01000001, B00000010,
  B00100001, B00000100,
  B00010001, B00001000,
  B00000110, B00100000,
  B00000101, B10100000,
  B11110100, B01101111,
  B00000101, B10100000,
  B00000110, B00100000,
  B00010001, B00001000,
  B00100001, B00000100,
  B01000001, B00000010,
  B10000001, B00000001 };

// Checks for the correct screen height.  (You should probably not change this unless you enjoy breaking things)
#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

// Menu constants used in the menu objects so that the menu can be processed properly.
//
// Here is an example if you wanted to add Flappy Birds to the menu:
// #define MENU_GAME_FLAPPY    0x02    (This is the next unused number under the GAME section)
// We will see how these declarations are used later.
#define MENU_TYPE_TEXT           0x00
#define MENU_TYPE_SCANNER        0x01
#define MENU_TYPE_BLING          0x02
#define MENU_TYPE_GAME           0x03
#define MENU_TYPE_SETTING        0x04
#define MENU_TYPE_SECRET         0x05
#define MENU_BLING_OFF           0x00
#define MENU_BLING_SPIN          0x01
#define MENU_BLING_HEARTBEAT     0x02
#define MENU_BLING_SPARKLE       0x03
#define MENU_BLING_SWEEP         0x04
#define MENU_BLING_BUTTONS       0x08
#define MENU_GAME_SIMON          0x01
#define MENU_SETTING_REGION      0x01
#define MENU_REGION_US           0x01
#define MENU_REGION_EU           0x02
#define MENU_REGION_JP           0x03
#define MENU_SECRET_RABBIT       0x01
#define MENU_SECRET_RED_PILL     0x02

// These settings are used in navigateInto() to do extra menu setup; you probably shouldn't change these but you can add more if you are adding a menu that has a selection saved
#define MENU_ID_ROOT             0x00000000
#define MENU_ID_BLING            0xbf7adf6c
#define MENU_ID_SETTING_REGION   0xba9c9844

// Easter Egg bits; good hunting!
#define NO_LOCKS        0
#define UNLOCK_SIMON    (1<<0)
#define UNLOCK_RABBIT   (1<<1)

// Begin menu setup here:
//
// The parameters are:
// variable name, random unique identifier, NO_LOCKS, "Text to display"
// "NO_LOCKS" has to do with whether or not this menu option is locked as an Easter Egg
PgmMenuText(m_title, 0x289ffb00, NO_LOCKS, "Mr. Blinky Bling");
PgmMenuText(m_subtitle, 0xf44763d8, NO_LOCKS, " - DC25 -");
PgmMenuLeaf(m_scan, 0x42efc888, NO_LOCKS, "Scanner", MENU_TYPE_SCANNER);
PgmMenuLeaf(m_bling_off, 0x2e9b072a, NO_LOCKS, "Off", MENU_TYPE_BLING, MENU_BLING_OFF);
PgmMenuLeaf(m_bling_spin, 0x471289df, NO_LOCKS, "Spin", MENU_TYPE_BLING, MENU_BLING_SPIN);
PgmMenuLeaf(m_bling_heartbeat, 0xff77c01d, NO_LOCKS, "Heartbeat", MENU_TYPE_BLING, MENU_BLING_HEARTBEAT);
PgmMenuLeaf(m_bling_sparkle, 0x5089880f, NO_LOCKS, "Sparkle", MENU_TYPE_BLING, MENU_BLING_SPARKLE);
PgmMenuLeaf(m_bling_sweep, 0xeebec5f8, NO_LOCKS, "Sweep", MENU_TYPE_BLING, MENU_BLING_SWEEP);
PgmMenuNode(m_bling, MENU_ID_BLING, NO_LOCKS, "Bling", &m_bling_off, &m_bling_spin, &m_bling_heartbeat, &m_bling_sparkle, &m_bling_sweep);
PgmMenuLeaf(m_games_simon, 0x5cfbe589, NO_LOCKS, "Simon", MENU_TYPE_GAME, MENU_GAME_SIMON);
PgmMenuNode(m_games, 0x490162ad, NO_LOCKS, "Games", &m_games_simon);
// In our earlier example where we were adding in Flappy Bird we would add this line above:
// PgmMenuLeaf(m_games_flappy, 0x7893cef1, NO_LOCKS, "Flappy Bird", MENU_TYPE_GAME, MENU_GAME_FLAPPY);
// We then need to change the existing line:
// PgmMenuNode(m_games, 0x490162ad, NO_LOCKS, "Games", &m_games_simon);
// to read:
// PgmMenuNode(m_games, 0x490162ad, NO_LOCKS, "Games", &m_games_simon, &m_games_flappy);
// 
// NOTE:
// There are THREE functions involved; one defines text messages that don't go anywhere and the other two define nodes and leafs on the menu tree
// Use PgmMenuText to output a non-selectable text line into the menu tree as seen in the first two options
// Use PgmMenuNode to define a menu option that leads to a sub-menu
// Use PgmMenuLeaf to define a menu option that leads to an action

// This is part of the menu left as an exercise for the reader; feel free to let us know if you get this working and we'll add your code in.
/*
PgmMenuLeaf(m_settings_region_us, 0x751bea52, NO_LOCKS, "US (11 ch)", MENU_TYPE_SETTING, MENU_SETTING_REGION, MENU_REGION_US);
PgmMenuLeaf(m_settings_region_eu, 0x5564de88, NO_LOCKS, "EU (13 ch)", MENU_TYPE_SETTING, MENU_SETTING_REGION, MENU_REGION_EU);
PgmMenuLeaf(m_settings_region_jp, 0xa2cea3b0, NO_LOCKS, "JP (14 ch)", MENU_TYPE_SETTING, MENU_SETTING_REGION, MENU_REGION_JP);
PgmMenuNode(m_settings_region, MENU_ID_SETTING_REGION, NO_LOCKS, "Region", &m_settings_region_us, &m_settings_region_eu, &m_settings_region_jp);
PgmMenuNode(m_settings, 0xc91d02ec, NO_LOCKS, "Settings", &m_settings_region);
*/

// More menu options here:
PgmMenuText(m_info_1, 0xca976999, NO_LOCKS, "mrblinkybling.com");
PgmMenuText(m_info_2, 0x7f1d7b0b, NO_LOCKS, "GET NOTICED!");
PgmMenuText(m_info_3, 0xa4ab3c29, NO_LOCKS, "@blenster");
PgmMenuText(m_info_4, 0x151e9dc8, NO_LOCKS, "@tweetthehat");
PgmMenuLeaf(m_info_5, 0xb5b5c242, NO_LOCKS, "FollowTheWhiteRabbit", MENU_TYPE_SECRET, MENU_SECRET_RABBIT);
PgmMenuNode(m_info, 0xa53d1abb, NO_LOCKS, "Info", &m_info_1, &m_info_2, &m_info_3, &m_info_4, &m_info_5);
PgmMenuLeaf(m_red_pill, 0xd78881ff, UNLOCK_RABBIT, "Take the red pill", MENU_TYPE_SECRET, MENU_SECRET_RED_PILL);
PgmMenuNode(m_root, MENU_ID_ROOT, NO_LOCKS, "", &m_title, &m_subtitle, &m_scan, &m_bling, &m_games, /*&m_settings,*/ &m_info, &m_red_pill);

// holds menu strings - limited to 24 characters to avoid menu scrolling issues. If you adjust this to be larger more than 24 will require rewriting scrolling.
char buffer[24];

// Setup the menu
int menu_position = 0;            // How many rows down the menu we have scrolled
int menu_selected = 0;            // Where the inverted "cursor" appears on the menu
MenuNodeP* menu_level = &m_root;  // What level are we in the menu?

/*
 * Here's how these two menu variables work:
 *
 * Example:
 *
 * Menu Option 1  <-- menu_position == 0 (zero indexed)
 * Menu Option 2
 * MENU OPTION 3  <-- menu_selected == 2 (zero indexed)
 * Menu Option 4
 *
 * After some scrolling down, and once back up
 *
 * Menu Option 3  <-- menu_position == 2 (zero indexed)
 * Menu Option 4
 * MENU OPTION 5  <-- menu_selected == 4 (zero indexed)
 * Menu Option 6
 *
 * menu_position shows which entry is currently at the top of the screen and you can probably ignore this in your code
 * menu_selected shows the currently highlighted menu option and this is the one that will be activated
 *
 */

// Global settings in a struc for convenience; these may be pushed to EEPROM in the future
struct {
  uint8_t blingMode;
  uint8_t region;
  uint8_t maxActivity;
  uint8_t unlocked;
  uint8_t spinSpeed;
  uint8_t spinN;
  uint8_t heartbeatSpeed;
  uint8_t heartbeatPeriod;
  uint8_t sparkleSpeed;
  uint8_t sparkleFreq;
  uint8_t sweepSpeed;
  uint8_t sweepPeriod;
} settings;

// Initialize the ATTiny88 communication (You REALLY should not change these)
#define UI_CS       8
#define UI_ECH      9
TinyUI ui(UI_CS, UI_ECH);

// Initialize the ESP module through some ugly hacked up code hidden in EspModule.cpp (Only the brave should look at that mess)
EspModule esp;

// The length of the SSID text we can display; the menu says 24 but we recommend 22 or less here.
char ssidBuffer[22];

// These variables store the information that comes back from the ESP module
typedef struct _NetworkInfo {
  struct _NetworkInfo *next;
  uint8_t security;
  int8_t rssi;
  //uint8_t mac[6];   // not displaying this and it takes up memory...
  uint8_t channel;
  char ssid[];
} NetworkInfo;
NetworkInfo *networkList = NULL;
NetworkInfo *networksRx = NULL;

// This initializes the Simon game object
Simon simon;

// Some variables for the netwrok scanning the ESP will do (You should probably not change these initial values; they get reset anyway)
long nextScan = 0;   // millis() value for next WiFi scan
uint16_t networkRAM = 0;   // Number of bytes being used to store network data

// This is the main setup function - just like any other Arduino Sketch - see arduino.cc documentation for more information
void setup() {
  //Serial.begin(9600);   // If there's no USB connection, this may hang, but if you want to interact through the serial monitor uncomment this line

  // Initial values for user modifiable settings - default settings:
  settings.blingMode = MENU_BLING_OFF;
  settings.region = MENU_REGION_US;
  settings.maxActivity = DEFAULT_MAX_ACTIVITY;
  settings.unlocked = 0;
  settings.spinSpeed = 4;
  settings.spinN = 2;
  settings.heartbeatSpeed = 2;
  settings.heartbeatPeriod = 255;
  settings.sparkleSpeed = 8;
  settings.sparkleFreq = 32;
  settings.sweepSpeed = 6;
  settings.sweepPeriod = 64;
  MenuNodeP::setLocks(settings.unlocked);

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC);
 

  // We're required by the license on this library to display this splash screen; you do you

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(2000);

  // Clear the buffer.
  display.clearDisplay();

  // Setup menu text/font options; you should probably not change these
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // draw the menu on the screen
  draw_menu();

  // Initialize the connection to the ATTiny88
  ui.begin();
  ui.enableExtraChannels(); // Hands control over the RX/TX LEDs to the ATTiny88 for animations and channel information
  ui.update(TINYUI_GET_DEFAULT);  // Syncs up over the SPI connection with a SPI transaction

  // Start talking to the ESP module over serial
  esp.begin();

  // Initialize the Simon game; it needs a reference to the ATTiny88 and the display in order to play the game
  simon.setUi(&ui, &display);
}

// This is the main loop of the Arduino sketch -- see Arduino Documentation
void loop() {
  // These variables are all defined up here because C conditions you to declare all your variables at the top of the function
  uint8_t btn;          // The buttons that have been pressed that come back from the ATTiny88 via TinyUI
  uint8_t menuType;     // One of the identifiers pulled from the current menu object (the 0x0? values defined on lines 86 - 104)
  long t;               // Current value from millis() -- https://www.arduino.cc/en/Reference/Millis Used for general timing
  boolean espData;      // True when the ESP is still recieving data
  boolean refreshData;  // True when we need to update the display
  uint8_t i;            // Loop variable e.g. for (i = 0; i < CHANNEL_COUNT; i++) {...
  
  t = millis();
  
  espData = esp.handleData();  // Returns nothing if it doesn't have an open operation; this is necessary in case the user exits the scanning while an operation is underway
  menuType = menu_level->getDataByte(0);  // Operates the menu off the lower-number constants defined above (lines 86 - 104)

  // Handles data returned by the ESP module; builds a list of the incoming data
  if (networksRx && !espData) {
    releaseNetworkList(networkList);
    networkList = networksRx;
    networksRx = NULL;
    if (menuType != MENU_TYPE_SCANNER) {
      releaseNetworkList(networkList);
      networkList = NULL;
    }
    refreshData = true;
  } else {
    refreshData = false;
  }

  // Grabs any recent button presses, commits any changes to the LEDs (via the ATTiny88 over SPI)
  ui.update(TINYUI_GET_BUTTONS);  // Performes the SPI transaction; keeps asking until it gets new button data
  btn = ui.getButton();           // Grabs the next button press
  
  // If we're scanning:
  if (menuType == MENU_TYPE_SCANNER) {
    if (refreshData) {
      setNetworkActivity();
      drawWifiList();
    }
    if ((t >= nextScan) && !espData) {
      nextScan = t + SCAN_INTERVAL;
      resetNetworksList();
      esp.startListNetworks(&networksRx, networkItem, ssidBuffer, sizeof(ssidBuffer));
    }
    if (btn & TINYUI_BUTTON_UP) {
      if (menu_position) {
        menu_position--;
      }
      drawWifiList();
    }
    if (btn & TINYUI_BUTTON_DOWN) {
      menu_position++;
      drawWifiList();
    }
    if (btn & TINYUI_BUTTON_LEFT) {
      navigateOutOf();
    }
  // If we're going to play a game (If you were adding Flappy Birds here's where you'd want to start adding code below:
  } else if (menuType == MENU_TYPE_GAME)  {
    menuType = menu_level->getDataByte(1);
    if (menuType == MENU_GAME_SIMON) {
      if (!simon.play(btn)) {
        if (simon.isWinner()) {
          settings.unlocked |= UNLOCK_SIMON;
          MenuNodeP::setLocks(settings.unlocked);
        }
        navigateOutOf();
      } 
    }  // You'd add: else if (menuType == MENU_GAME_FLAPPY) { and then more code like you see above.
  // Easter Egg!  Have fun!
  } else if (menuType == MENU_TYPE_SECRET) {
    menuType = menu_level->getDataByte(1);
    if (menuType == MENU_SECRET_RABBIT) {
      if (btn && (menu_position != 0xff)) {
        refreshData = true;
        if ((btn & TINYUI_BUTTON_UP) && (menu_position >= 0x10)) {
          menu_position -= 0x10;
        }
        if ((btn & TINYUI_BUTTON_DOWN) && (menu_position < 0xc0)) {
          menu_position += 0x10;
        }
        if (btn & TINYUI_BUTTON_LEFT) {
          if (menu_position & 0x0f) {
            menu_position--;
          } else {
            navigateOutOf();
            refreshData = false;
          }
        }
        if ((btn & TINYUI_BUTTON_RIGHT) && ((menu_position & 0x0f) < 0x0a)) {
          menu_position++;
        }
        if (refreshData) {
          updateTheMatrixHasYou();
        }
      }
      if ((menu_position == 0xff) && (btn & (TINYUI_BUTTON_LEFT | TINYUI_BUTTON_SELECT))) {
        navigateOutOf();
      }
      if ((ui._getNavHistory(8) == 0x004e58d1) && (menu_position != 0xff)) {
        display.clearDisplay(); // clear the screen/flush the buffer
        display.setCursor(0,0); // Set the cursor back to the top left
        display.setTextColor(WHITE);
        strcpy_P(buffer, PSTR("Never gonna give you"));
        display.println(buffer);
        strcpy_P(buffer, PSTR("up. Never gonna let"));
        display.println(buffer);
        strcpy_P(buffer, PSTR("you down. Never gonna"));
        display.println(buffer);
        strcpy_P(buffer, PSTR("run around and desert"));
        display.println(buffer);
        display.display();
        menu_position = 0xff;
        settings.unlocked = UNLOCK_RABBIT;
        MenuNodeP::setLocks(settings.unlocked);
      }
    } else if (menuType == MENU_SECRET_RED_PILL) {
      if (t >= nextScan) {
        i = random(TINYUI_LED_COUNT);
        ui.setPixelTransition(i, ui.getPixel(i) ? 0 : 255, RED_PILL_FRAMES);
        nextScan = t + RED_PILL_INTERVAL;
      }
      if (btn & (TINYUI_BUTTON_LEFT | TINYUI_BUTTON_SELECT)) {
        navigateOutOf();
      }
    }
  } else {
    handleMenuButton(btn); // Default menu navigation behavior
  }
}

// This variable is only used down here; the number of access points on each channel
uint8_t channelActivity[CHANNEL_COUNT];

// Sets the LED blinking based on what is in channel activity
void setNetworkActivity(void) {
  uint8_t i, p, m;
  m = TINYUI_PULSE_LENGTH + (settings.maxActivity << ACTIVITY_SH);
  for (i = 0; i < CHANNEL_COUNT; i++) {
    if (channelActivity[i]) {
      p = (channelActivity[i] < settings.maxActivity) ? m - (channelActivity[i] << ACTIVITY_SH) : TINYUI_PULSE_LENGTH;
      ui.setPixel(i, 255);
      ui.setPulse(i, p);
    } else {
      ui.setPixel(i, 0);
    }
  }
}

// Releases memory that was used to hold a network list that is no longer needed
void releaseNetworkList(NetworkInfo *head) {
  NetworkInfo *cur;
  NetworkInfo *next;
  cur = head;
  while (cur) {
    next = cur->next;
    free(cur);
    cur = next;
  }
}

// Resets the counters for aggregating data as it parses
void resetNetworksList(void) {
  networkRAM = 0;
  memset(channelActivity, 0, sizeof(channelActivity));
}

// Callback for the ESP module - checks RAM usage and adds the network information to the list of data being received
void *networkItem(void *obj, uint8_t security, const char *ssid, int8_t rssi, const uint8_t *mac, uint8_t channel) {
  NetworkInfo *info;
  uint16_t newRAM;
  if (channel && (channel <= CHANNEL_COUNT)) {
    channelActivity[channel - 1]++;
  }
  newRAM = networkRAM + sizeof(NetworkInfo) + strlen(ssid) + 1;
  if (newRAM <= MAX_NETWORKS_RAM) {
    for (info = networksRx; info; info = info->next) {   // since we have limited RAM, don't waste it on repeated network names
      if (!strcmp(ssid, info->ssid)) {
        return obj;
      }
    }
    networkRAM = newRAM;
    info = (NetworkInfo *) malloc(sizeof(NetworkInfo) + strlen(ssid) + 1);
    info->next = NULL;
    info->security = security;
    info->rssi = rssi;
    info->channel = channel;
    strcpy(info->ssid, ssid);
    *(NetworkInfo **)obj = info;
    return &(info->next);
  } else {
    return obj;
  }
}

// Default menu behavior
void handleMenuButton(uint8_t btn) {
  if (btn & TINYUI_BUTTON_SELECT)
  {
    navigateInto();
  }
  if (btn & TINYUI_BUTTON_UP)
  {
    if (menu_selected) {
      menu_selected--;
      navigateSelect();
    }
  }
  if (btn & TINYUI_BUTTON_RIGHT)
  {
    navigateInto();
  }
  if (btn & TINYUI_BUTTON_DOWN)
  {
    if (menu_selected < (menu_level->getChildCount() - 1)) {
      menu_selected++;
      navigateSelect();
    }
  }
  if (btn & TINYUI_BUTTON_LEFT)
  {
    navigateOutOf();
  }
}

// A new menu item has been selected; apply whatever changes need to be applied
void navigateSelect(void) {
  if (menu_position > menu_selected) {
    menu_position = menu_selected;
  }
  if ((menu_position + MENU_HEIGHT) < menu_selected) {
    menu_position = menu_selected - MENU_HEIGHT;
  }
  // Any actions that should be performed when a menu item is highlighted should be implemented here (without requiring the center button to be pressed)
  draw_menu();
}

// Move into the currently selected submenu item and execute any action that menu option would perform
void navigateInto(void) {
  uint8_t menuType;
  uint32_t tgtid;
  MenuNodeP *tgt;
  tgt = menu_level->getChild(menu_selected);
  if (tgt->hasData()) {
    menuType = tgt->getDataByte(0);
    if (menuType == MENU_TYPE_SCANNER) {
      menu_level = tgt;
      menu_position = 0;
      navigateSelect();
    } else if (menuType == MENU_TYPE_BLING) {
      menuType = tgt->getDataByte(1);
      if (menuType == MENU_BLING_OFF) {
        ui.blingOff();
      } else if (menuType == MENU_BLING_SPIN) {
        ui.blingOff();
        ui.blingSpin(settings.spinSpeed, settings.spinN);
      } else if (menuType == MENU_BLING_HEARTBEAT) {
        ui.blingOff();
        ui.blingHeartbeat(settings.heartbeatSpeed, settings.heartbeatPeriod);
      } else if (menuType == MENU_BLING_SPARKLE) {
        ui.blingOff();
        ui.blingSparkle(settings.sparkleSpeed, settings.sparkleFreq);
      } else if (menuType == MENU_BLING_SWEEP) {
        ui.blingOff();
        ui.blingSweep(settings.sweepSpeed, settings.sweepPeriod);
      } else if (menuType == MENU_BLING_BUTTONS) {
        if (ui.buttonFeedbackEnabled()) {
          ui.buttonFeedbackOff();
        } else {
          ui.buttonFeedbackOn();
        }
      }
      if (menuType != MENU_BLING_BUTTONS) {
        settings.blingMode = menuType;
      }
      navigateOutOf();
    } else if (menuType == MENU_TYPE_GAME) {
      menuType = tgt->getDataByte(1);
      if (menuType == MENU_GAME_SIMON) {
        simon.start();
        menu_level = tgt;
      }
    } else if (menuType == MENU_TYPE_SETTING) {
      menuType = tgt->getDataByte(1);
      if (menuType == MENU_SETTING_REGION) {
        settings.region = tgt->getDataByte(2);
        navigateOutOf();
      }
    } else if (menuType == MENU_TYPE_SECRET) {
      menuType = tgt->getDataByte(1);
      menu_position = 0;
      if (menuType == MENU_SECRET_RABBIT) {
        updateTheMatrixHasYou();
      } else if (menuType == MENU_SECRET_RED_PILL) {
        display.clearDisplay(); // clear the screen/flush the buffer
        display.setCursor(0,0); // Set the cursor back to the top left
        display.display();
      }
      menu_level = tgt;
    }
  } else if (tgt->getChildCount()) {
    tgtid = tgt->getId();
    menu_level = tgt;
    menu_position = 0;
    menu_selected = 0;
    if (tgtid == MENU_ID_BLING) {
      selectSubmenu(1, settings.blingMode);
    } else if (tgtid == MENU_ID_SETTING_REGION) {
      selectSubmenu(2, settings.region);
    }
    navigateSelect();
  }
}

// Move back out of the current menu item to its parent (This is where you'd clean up anything that needs to be cleaned up when you exit a menu)
void navigateOutOf(void) {
  uint8_t i, n;
  uint8_t oldType;
  MenuNodeP *old;
  old = menu_level;
  oldType = old->getDataByte(0);
  if (oldType == MENU_TYPE_SCANNER) {
    releaseNetworkList(networkList);
    networkList = NULL;
    for (i = 0; i < CHANNEL_COUNT; i++) {
      ui.setPixel(i, 0);
      ui.setPulse(i, 0);
    }
  } else if (oldType == MENU_TYPE_GAME) {
    oldType = old->getDataByte(1);
    if (oldType == MENU_GAME_SIMON) {
      simon.release();
    }
  } else if (oldType == MENU_TYPE_SECRET) {
    oldType = old->getDataByte(1);
    if (oldType == MENU_SECRET_RED_PILL) {
      for (i = 0; i < TINYUI_LED_COUNT; i++) {
        ui.setPixelTransition(i, 0, RED_PILL_FRAMES);
      }
    }
  }
  menu_level = menu_level->getParent();
  if (menu_level) {
    menu_position = 0;
    menu_selected = 0;
    n = menu_level->getChildCount();
    for (i = 0; i < n; i++) {
      if (menu_level->getChild(i) == old) {
        menu_selected = i;
        break;
      }
    }
    navigateSelect();
  } else {
    menu_level = old;
  }
}

// Finds and selects the menu item indicated by data index (so if you have activated a certain BLING mode and go back to 
// the BLING menu you will see that it has already highlited the current bling mode; this makes sure you have the correct one
// selected when you return)
void selectSubmenu(uint8_t dataIndex, uint8_t value) {
  uint8_t i, n;
  n = menu_level->getChildCount();
  for (i = 0; i < n; i++) {
    if (menu_level->getChild(i)->getDataByte(dataIndex) == value) {
      menu_selected = i;
      break;
    }
  }
}

// This function decides what the menu should look like based on the environmental variables
void draw_menu(void) {
  // Check to see where we are in the menu versus the display so we know what to text to invert
  int i;
  MenuNodeP *node;
  bool use_invert = 0; // Whether or not we're using the inverted color option

  // setup display
  display.clearDisplay(); // clear the screen/flush the buffer
  display.setCursor(0,0); // Set the cursor back to the top left

  // Display the 4 menu options here:
  for (i = 0; i < 4; i++) {

    // if this is a selected entry highlight it
    if (i + menu_position == menu_selected) {
      display.setTextColor(BLACK, WHITE);
      use_invert = 1;
    }

    // Pull menu character data out of memory and push it to the screen
    // We are using the menu_level pointer to access the menu text data
    if (i + menu_position < menu_level->getChildCount()) {
      node = menu_level->getChild(i + menu_position);
      node->readText(buffer, sizeof(buffer));
    } else {
      buffer[0] = 0;
    }
    display.println(buffer);

    // if we have used the invert to highlight recently we need to disable it now
    if (use_invert) {
      display.setTextColor(WHITE);
      use_invert = 0;
    }

  }
  // update the display
  display.display();
}

// Show the list of WiFi SSIDs that have been found
void drawWifiList(void) {
  uint8_t i;
  NetworkInfo *info;

  // setup display
  display.clearDisplay(); // clear the screen/flush the buffer
  display.setCursor(0,0); // Set the cursor back to the top left

  // find some network names to display
  i = menu_position;
  info = networkList;
  while (i && info) {
    i--;
    info = info->next;
  }
  if (i) {
    menu_position -= i;
  } else {
    for (i = 0; (i <= MENU_HEIGHT) && info; i++) {
      display.println(info->ssid);
      info = info->next;
    }
  }

  // update the display
  display.display();
}

// Wake up Neo...
void updateTheMatrixHasYou(void) {
  display.clearDisplay(); // clear the screen/flush the buffer
  display.setCursor((menu_position & 0x0f) << 1,(menu_position >> 4) << 1); // Set the cursor back to the top left
  display.setTextColor(WHITE);
  strcpy_P(buffer, PSTR("The matrix has you"));
  display.println(buffer);
  display.display();
}

