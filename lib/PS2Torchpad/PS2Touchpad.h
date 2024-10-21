#ifndef PS2TOUCHPAD_H
#define PS2TOUCHPAD_H

#include <Arduino.h>

class PS2Touchpad
{
public:
    PS2Touchpad(int dataPin, int clockPin);
    void begin();
    void init();

private:
    int _dataPin;
    int _clockPin;

    void sendCommand(uint8_t command);
    uint8_t readResponse();
    void log(const char *message);
    void logHex(const char *message, uint8_t value);
    void logError(const char *message);
};

#endif
