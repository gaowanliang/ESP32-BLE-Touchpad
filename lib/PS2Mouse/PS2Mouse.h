#ifndef MOUSE_H_
#define MOUSE_H_
#include <Arduino.h>

class PS2Mouse
{
private:
    void high(uint8_t pin);
    void low(uint8_t pin);
    uint8_t _scl, _sda;
    void error(const char *e);
    void wait_CLOCK(byte state);
    void set_CLOCK(byte state);
    byte read_DATA();
    void set_DATA(byte data);
    void send_tp_arg(byte arg);
    void send(byte value);

public:
    PS2Mouse(uint8_t scl, uint8_t sda);
    PS2Mouse();
    bool begin();
    byte get(bool handler = false);
    void getAbsoluteAxis(byte *raw, uint16_t &x, uint16_t &y);
};
#endif
