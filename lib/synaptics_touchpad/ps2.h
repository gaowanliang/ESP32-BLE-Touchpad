#ifndef PS2_H
#define PS2_H

#include <Arduino.h>

namespace ps2
{
#define PSMOUSE_CMD_SETSCALE11 0x00e6
#define PSMOUSE_CMD_SETRATE 0x10f3
#define PSMOUSE_CMD_ENABLE 0x00f4
#define PSMOUSE_CMD_DISABLE 0x00f5
#define PSMOUSE_CMD_RESET_BAT 0x02ff
#define PSMOUSE_CMD_SETRES 0x10e8
#define PSMOUSE_CMD_GETINFO 0x03e9

    bool write_byte(uint8_t data);
    void begin(uint8_t clock_pin, uint8_t data_pin, void (*byte_received)(uint8_t));
    bool ps2_command(uint16_t command, uint8_t *args, uint8_t *result);
    void reset();
    void enable();
    void disable();
}

#endif