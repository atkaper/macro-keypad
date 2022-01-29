#include "Keyboard.h"
#include <Keypad.h>
#include <EEPROM.h>

// KeyPad, 8 keys, with (single color) leds in them.
//
// Arduino Leonardo Compatible - ATMEGA 32U4-AU/MU - Pro micro.
// USB Device HID/Mouse/Keyboard/etc...
// Board info: https://learn.sparkfun.com/tutorials/pro-micro--fio-v3-hookup-guide/all
//
// Arduino IDE board manager url: https://raw.githubusercontent.com/sparkfun/Arduino_Boards/master/IDE_Board_Manager/package_sparkfun_index.json
// Choose board "SparkFun Pro Micro", CPU 16 MHZ, 5V.
//
// Linux/Ubuntu, show which key has been pressed: "sudo showkey -s" or "sudo showkey -k"
// Note: The keypad will also work on windows, it is just simulating an USB keyboard.
// 
// Thijs Kaper, January 29, 2022.

// Enable next line if you want double click on key 8 to toggle between keymap 1 and 2, and key 7 to show startup animation (was only for testing).
//#define DOUBLE_CLICK_TEST

// Just some random version number (can be queried via serial command interface).
const char VERSION[] PROGMEM = "MacroPad-1.0, " __DATE__ " " __TIME__;

// Key matrix size setup
const byte ROWS = 2;
const byte COLS = 4;
const byte KEYCOUNT = (ROWS * COLS);

// Key matrix pin connections:
const byte rowPins[ROWS] = { 10, 16 }; //connect to the row pinouts of the keypad.
const byte colPins[COLS] = { A1, A0, 15, 14 }; //connect to the column pinouts of the keypad.

// Led to pin mapping
const byte ledPin[KEYCOUNT] = { 2, 3, 4, 5, 6, 7, 8, 9 }; // Led nr is index (0 based), value is controller pin number.

// Assign indexes to keymap, we use the indexes as pointer into keyMap1 or keyMap2 (after subtracting 1).
// The index starts at 1, because reading the keypad will return 0 if there was none, so we can not use 0 as index.
// Normally you would just put the "real" keys in this array, but I wanted to be able to choose between 2 keymaps,
// so I did just put in index in here, to point into my two keymaps.
const char keys[ROWS][COLS] = {
  { 1, 2, 3, 4 },
  { 5, 6, 7, 8 },
};

// The data in this configuration struct can be modified at runtime via serial port, and persisted in EEPROM for use on fresh boot.
struct ConfigData {
  // keyMap1 defintion, the one used by default after power on. You can fill in special keynames
  // as seen in arduino-*/libraries/Keyboard/src/Keyboard.h, or just use ascii code chars, e.g. '0' or 'a'.
  // My defintion, F13-F22. Skipping two f-keys, as these do not seem to work on linux mint? (F19/F20 moved to F21/F22).
  char keyMap1[KEYCOUNT] = { KEY_F13, KEY_F14, KEY_F15, KEY_F16, KEY_F17, KEY_F18, KEY_F21, KEY_F22 };
  
  // Alternate keymap, just for testing the keys, sending normal digits. Same comment as for keyMap1.
  char keyMap2[KEYCOUNT] = { '1', '2', '3', '4', '5', '6', '7', '8' };
  
  // Configuration of led action when key is pressed. Initial start, set to auto toggle (t).
  // Options: t = toggle, e = enable(on), d = disable(off), f = flash 150 ms.
  char ledConfig1[KEYCOUNT] = { 't', 't', 't', 't', 't', 't', 't', 't' };
  char ledConfig2[KEYCOUNT] = { 't', 't', 't', 't', 't', 't', 't', 't' };
} config;

// Startup animation, led numbers to light, 1 based to easily match up keys (not zero based).
// Animation lights up 2 leds at a time.
const byte ledAnimation[][2] = {
  { 1, 8 },
  { 2, 7 },
  { 3, 6 },
  { 4, 5 },
  { 8, 1 },
  { 7, 2 },
  { 6, 3 },
  { 5, 4 },
  { 1, 8 },
};

// Toggle flag to change keymap. Default starts using keyMap1. You can toggle to keyMap2 using double click on key 8 (if DOUBLE_CLICK_TEST defined), or by sending a command from PC via serial.
// Note: the second keymap was not meant for any serious use (just testing), as double clicking key 8 will also send the configured key function on its first press.
boolean useAlternateKeyMap=false;

// Initialze keypad functions.
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

// Led state memory.
byte ledState[KEYCOUNT]; // on = 1 / off = 0

// Helper function to determine length of array.
#define membersof(x) (sizeof(x) / sizeof(x[0]))

// memorize when last keypress occurred.
unsigned long startTime = millis();

// memorize last key press.
char lastKey = '-';

// Serial data buffer.
char readBuffer[80];
char *readBufferPtr;

// Reset serial data buffer.
void clearReadBuffer() {
  readBuffer[0] = 0;
  readBufferPtr = readBuffer;
}

// Initialize all.
void setup() {
  Serial.begin(115200);

  // Load config data from EEPROM.
  handleLoad();
  
  // Initialize readBuffer to blank/empty-string.
  clearReadBuffer();
  
  // Set LED pins as output, and turn off.
  for (int i=0;i<KEYCOUNT;i++) {
    pinMode(ledPin[i], OUTPUT);
    setLed(i, 0);
  }

  // Start USB keyboard.
  Keyboard.begin();

  // Show startup lightshow.
  startupLedAnimation();
}

// Animation code
void startupLedAnimation() {
  // Backup current led state, in case we re-start animation during use.
  static byte currentLedState[ROWS*COLS];
  for(int i=0;i<KEYCOUNT;i++) {
    currentLedState[i] = ledState[i];
  }

  // Run through animation "frames".
  for (int i=0; i< membersof(ledAnimation); i++) {
    // Turn leds on
    setLed(ledAnimation[i][0]-1, 1);
    setLed(ledAnimation[i][1]-1, 1);
    if (i>0) {
      // Turn previous leds off
      setLed(ledAnimation[i-1][0]-1, 0);
      setLed(ledAnimation[i-1][1]-1, 0);
    }
    delay(250);
  }
  // Turn last ones off.
  setLed(ledAnimation[membersof(ledAnimation)-1][0]-1, 0);
  setLed(ledAnimation[membersof(ledAnimation)-1][1]-1, 0);

  // Restore led state, in case we ran animation during use.
  for(int i=0;i<KEYCOUNT;i++) {
    setLed(i, currentLedState[i]);
  }
}

// Set (zero based index) led on or off; 1 = on, 0 = off.
void setLed(int nr, int onOff) {
  if (nr<0 || nr>=KEYCOUNT || onOff<0 || onOff>1) return;
  ledState[nr] = onOff;
  digitalWrite(ledPin[nr], (onOff==1)?LOW:HIGH);
}

// Toggle led (zero based index).
void toggleLed(int nr) {
  if (nr<0 || nr>=KEYCOUNT) return;
  if (ledState[nr] == 0) {
    setLed(nr, 1);
  } else {
    setLed(nr, 0);
  }
}

// Change led state, on key-press, according to what has been confgiured for it.
void executeLedConfig(int nr) {
  if (nr<0 || nr>=KEYCOUNT) return;
  char setting;
  if (useAlternateKeyMap) {
    setting = config.ledConfig2[nr];
  } else {
    setting = config.ledConfig1[nr];
  }
  // Options: t = toggle, e = enable(on), d = disable(off), f = flash 150 ms.
  if (setting == 't') {
    toggleLed(nr);
  }
  if (setting == 'e') {
    setLed(nr, 1);
  }
  if (setting == 'd') {
    setLed(nr, 0);
  }
  if (setting == 'f') {
    setLed(nr, 1);
    delay(150);
    setLed(nr, 0);
    delay(150);
  }
}

// Read number from the serial data buffer.
int parseNumber(int minValue, int maxValue) {
  // Parse next word into a (zero or positive) number.
  char *nextword = strtok(NULL, " ");
  if (nextword == NULL) {
    Serial.print("Error, missing number\n");
    return -1;
  }
  int nr = atoi(nextword);
  if (nr<minValue || nr>maxValue) {
    Serial.print("Error, expected number from ");
    Serial.print(minValue);
    Serial.print(" to ");
    Serial.println(maxValue);
    return -1;
  }
  return nr;
}

// Create function "template".
typedef int (*HandlerFunction)();

// Create struct with function, name, description.
struct HandlerEntry {
  char *shortcode;
  char *command;
  char *help;
  HandlerFunction handler;
};

// Stored all long text messages in PROGMEM, this does require them to be copied back using strncpy_P to use them.
const char MSG_HELP_START[]   PROGMEM = "Valid commands:\n";
const char MSG_HELP[]         PROGMEM = "help                    : [or ?] show this help";
const char MSG_VERSION[]      PROGMEM = "version                 : [or v] show the macropad version";
const char MSG_GETLED[]       PROGMEM = "getled <NR>             : [or g] get led <NR> state (return 0 for off or 1 for on)";
const char MSG_TOGGLE[]       PROGMEM = "toggle <NR>             : [or t] toggle led <NR> state (<NR> is 1..8)";
const char MSG_ON[]           PROGMEM = "on <NR>                 : [or e] turn led <NR> on/enable (<NR> is 1..8)";
const char MSG_OFF[]          PROGMEM = "off <NR>                : [or d] turn led <NR> off/disable (<NR> is 1..8)";
const char MSG_FLASH[]        PROGMEM = "flash <NR> <MS>         : [or f] turn led <NR> on, sleep <MS> milliseconds, turn off, and sleep same time again (<NR> is 1..8, <MS> is 10..500)";
const char MSG_ANIMATION[]    PROGMEM = "animation               : [or a] run startup led animation";
const char MSG_USEMAP[]       PROGMEM = "usemap <NR>             : [or u] enable keymap <NR> (<NR> is 1 or 2)";
const char MSG_CONFIGTOGGLE[] PROGMEM = "configtoggle <NR> <SET> : [or c] configure key led toggle for key <NR> (1..8), <SET> is one of [t,e,d,f] where flash will be 150ms";
const char MSG_SHOWSETTINGS[] PROGMEM = "showsettings            : [or s] show settings";
const char MSG_SETKEYCODE[]   PROGMEM = "setkeycode <NR> <CODE>  : [or k] set key code of key <NR> (1..8) to <CODE> 0..255";
const char MSG_SETKEYCHAR[]   PROGMEM = "setkeychar <NR> <CHAR>  : [or h] set key character of key <NR> (1..8) to <CHAR> (a single char)";
const char MSG_PERSIST[]      PROGMEM = "persist                 : [or p] write config to EEPROM (use if you change keys or configtoggles)";
const char MSG_LOAD[]         PROGMEM = "load                    : [or l] load config from EEPROM (automatically done on each startup)";
const char MSG_HELP_END[]     PROGMEM = "\nYou can put multiple commands in one line - up to 79 characters (space separated), example: 'on 1 toggle 7 off 3'\n";

// List of all possible commands.
const HandlerEntry handlers[] = {
  { "?", "help",          MSG_HELP,         &handleHelp },
  { "v", "version",       MSG_VERSION,      &handleVersion },
  { "g", "getled",        MSG_GETLED,       &handleGetLed },
  { "t", "toggle",        MSG_TOGGLE,       &handleToggle },
  { "e", "on",            MSG_ON,           &handleOn },
  { "d", "off",           MSG_OFF,          &handleOff },
  { "f", "flash",         MSG_FLASH,        &handleFlash },
  { "a", "animation",     MSG_ANIMATION,    &handleAnimation },
  { "u", "usemap",        MSG_USEMAP,       &handleActivateKeyMap },
  { "c", "configtoggle",  MSG_CONFIGTOGGLE, &handleConfigToggle },
  { "s", "showsettings",  MSG_SHOWSETTINGS, &handleShowSettings },
  { "k", "setkeycode",    MSG_SETKEYCODE,   &handleSetKeyCode },
  { "h", "setkeychar",    MSG_SETKEYCHAR,   &handleSetKeyChar },
  { "p", "persist",       MSG_PERSIST,      &handlePersist },
  { "l", "load",          MSG_LOAD,         &handleLoad },
};

// Print char* from PROGMEM (copy to normal memory, and then print)
void println_P(char *data_P) {
  // Would be nice if we could determine length of string in progmem, and define buffer of that size.
  // Shortcut for now, just set 200 as max, and fix when needed.
  char tmp[200];
  strncpy_P(tmp, data_P, 199);
  Serial.println(tmp);
}

// Help command, show list.
int handleHelp() {
  println_P(MSG_HELP_START);
  for(int i=0; i<membersof(handlers);i++) {
    println_P(handlers[i].help);
  }
  println_P(MSG_HELP_END);
  return 0;
}

// Show version number.
int handleVersion() {
  println_P(VERSION);
  return 0;
}

// Get led state.
int handleGetLed() {
  int nr = parseNumber(1, KEYCOUNT);
  if (nr == -1) return 1;
  Serial.println(ledState[nr-1]);
  return 0;
}

// Toggle led.
int handleToggle() {
  int nr = parseNumber(1, KEYCOUNT);
  if (nr == -1) return 1;
  toggleLed(nr-1);
  return 0;
}

// Turn on led.
int handleOn() {
  int nr = parseNumber(1, KEYCOUNT);
  if (nr == -1) return 1;
  setLed(nr-1, 1);
  return 0;
}

// Turn off led.
int handleOff() {
  int nr = parseNumber(1, KEYCOUNT);
  if (nr == -1) return 1;
  setLed(nr-1, 0);
  return 0;
}

// Flash led.
int handleFlash() {
  int nr = parseNumber(1, KEYCOUNT);
  if (nr == -1) return 1;
  int ms = parseNumber(10, 500);
  if (ms == -1) return 1;
  setLed(nr-1, 1);
  delay(ms);
  setLed(nr-1, 0);
  delay(ms);
  return 0;
}

// Execute led animation.
int handleAnimation() {
  startupLedAnimation();
  return 0;
}

// Switch to keymap.
int handleActivateKeyMap() {
  int nr = parseNumber(1, 2);
  if (nr == -1) return 1;
  useAlternateKeyMap = (nr == 2);
  return 0;
}

// Configure led toggle behavior.
int handleConfigToggle() {
  int nr = parseNumber(1, KEYCOUNT);
  if (nr == -1) return 1;
  char *setting = strtok(NULL, " ");
  if (setting == NULL || strstr("tedf", setting) == NULL || strlen(setting) != 1) {
    Serial.println("Expected setting flag, one of [t,e,d,f].");
    return 1;
  }
  if (useAlternateKeyMap) {
    config.ledConfig2[nr-1] = setting[0];
  } else {
    config.ledConfig1[nr-1] = setting[0];
  }
  return 0;
}

// Show configuration (for active keymap).
int handleShowSettings() {
  Serial.println(useAlternateKeyMap?"map 2 active":"map 1 active");
  for(int i=0; i<KEYCOUNT; i++) {
    static char buffer[60];
    if(useAlternateKeyMap) {
      sprintf(buffer, "key %d, led: %s, ledconfig: %c, keycode: %d", i+1, ledState[i]?"on":"off", config.ledConfig2[i], (uint8_t) config.keyMap2[i]);
    } else {
      sprintf(buffer, "key %d, led: %s, ledconfig: %c, keycode: %d", i+1, ledState[i]?"on":"off", config.ledConfig1[i], (uint8_t) config.keyMap1[i]);
    }
    Serial.println(buffer);
  }
  return 0;
}

// Change the function of a macro key, by code, e.g, 65 = A, 240 = F13.
int handleSetKeyCode() {
  int nr = parseNumber(1, KEYCOUNT);
  if (nr == -1) return 1;
  int code = parseNumber(0, 255);
  if (code == -1) return 1;
  if(useAlternateKeyMap) {
    config.keyMap2[nr-1] = (char) (code & 255);
  } else {
    config.keyMap1[nr-1] = (char) (code & 255);
  }
  return 0;
}

// Change the function of a macro key, by char.
int handleSetKeyChar() {
  int nr = parseNumber(1, KEYCOUNT);
  if (nr == -1) return 1;
  char *value = strtok(NULL, " ");
  if (value == NULL || strlen(value) != 1) {
    Serial.println("Expected single character.");
    return 1;
  }
  if(useAlternateKeyMap) {
    config.keyMap2[nr-1] = value[0];
  } else {
    config.keyMap1[nr-1] = value[0];
  }
  return 0;
}

// Store config settings in EEPROM.
int handlePersist() {
  int eeAddress = 0;
  int version = 2306;
  EEPROM.put(eeAddress, version);
  eeAddress += sizeof(version);
  EEPROM.put(eeAddress, config);
  Serial.println("Config persisted.");
  return 0;
}

// Load config settings from EEPROM, also called on startup.
int handleLoad() {
  // Load config from EEPROM if there is a valid checksum there.
  int eeAddress = 0;
  int version;
  EEPROM.get(eeAddress, version);
  if (version == 2306) {
    eeAddress += sizeof(version);
    EEPROM.get(sizeof(version), config);
    Serial.println("Config loaded.");
  }
  return 0;
}

// Find commands in serial input, and execute them.
void executeSerialCommands() {
  char *word;
  word = strtok(readBuffer, " ");
  while (word != NULL) {
    boolean ok = false;

    for(int i=0; i<membersof(handlers);i++) {
      if(strcmp(handlers[i].command, word) == 0 || strcmp(handlers[i].shortcode, word) == 0) {
        if(handlers[i].handler()) return;
        ok = true;
        break;
      }
    }
    
    if (!ok) {
      Serial.print("Unknown Command Error\n");
      // Ignore the rest of the line.
      return;
    }

    // Check if there is a next command.
    word = strtok(NULL, " ");
  }
}

// Check and store serial data.
void handleSerialReceive() {
  while(Serial.available()>0) {
    char ser = Serial.read();
    // Collect received data in readBuffer, until someone sends "enter" (cr and or lf).
    if (strlen(readBuffer) < (sizeof(readBuffer)-1) && ser != 13 && ser !=10) {
      // Add new char to buffer.
      *(readBufferPtr++) = ser;
      *readBufferPtr = 0;
    }
    // "enter" (cr and or lf) pressed? handle data (if any).
    if ((ser == 13 || ser == 10) && strlen(readBuffer) > 0) {
      executeSerialCommands();
      clearReadBuffer();
    }
  }
}

// main loop
void loop() {
  handleSerialReceive();
  
  // key returns our 1 based index 1..8, or 0 if no key was pressed.
  char key = keypad.getKey();
 
  if (key){
    char keyIndex = key - 1;

#ifdef DOUBLE_CLICK_TEST
    // Check if key'8' was pressed twice quickly, if so, toggle keymap.
    if (key == 8 && lastKey == 8 && ((millis()-startTime) < 600)) {
      // toggle keymap between keyMap1 and keyMap2.
      useAlternateKeyMap = !useAlternateKeyMap;
      toggleLed(useAlternateKeyMap?0:4); delay(250); toggleLed(useAlternateKeyMap?0:4); // flash led 1 or 5 to confirm toggle (1 = numeric keys, 5 = function keys)
      delay(250);
      toggleLed(useAlternateKeyMap?0:4); delay(250); toggleLed(useAlternateKeyMap?0:4); // flash led 1 or 5 to confirm toggle (1 = numeric keys, 5 = function keys)
      startTime = millis();
      delay(20);
      return;
    }

    // Check if key'7' was pressed twice quickly, if so show startup led animation.
    if (key == 7 && lastKey == 7 && ((millis()-startTime) < 600)) {
      startupLedAnimation();
      startTime = millis();
      delay(20);
      return;
    }
#endif

    // Send key press to PC via USB
    Keyboard.write(useAlternateKeyMap ? config.keyMap2[keyIndex] : config.keyMap1[keyIndex]);
    lastKey = key;
    executeLedConfig(keyIndex);
    startTime = millis();
  }
  delay(20);
}
