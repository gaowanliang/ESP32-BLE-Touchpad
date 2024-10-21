#include "PS2Mouse.h"

/*
• $FF Reset
• $FE Resend
• $FC Error
• $FA Acknowledge
• $F6 Set Defaults
• $F5 Disable
• $F4 Enable
• $F3 Set Sample Rate (argument $00-$FF)
• $F2 Read Device Type
• $F0 Set Remote Mode
• $EE Set Wrap Mode
• $EC Reset Wrap Mode
• $EB Read Data
• $EA Set Stream Mode
• $E9 Status Request
• $E8 Set Resolution (argument 0-3)
• $E7 Set Scaling 2:1
• $E6 Set Scaling 1:1
*/

/*
 The Synaptics TouchPad has a small set of configurable features that are encapsulated in the TouchPad mode byte,
 an 8-bit field which the host can set to any value using a special command.

    0x00 Always OK Relative mode                                     // 0xE8 0x00 0xE8 0x00 0xE8 0x00 0xE8 0x00 0xF3 0x14
    0x04 Always OK Relative mode with gestures disabled              // 0xE8 0x00 0xE8 0x00 0xE8 0x01 0xE8 0x00 0xF3 0x14
    0x40 Always OK Relative mode with high packet rate               // 0xE8 0x01 0xE8 0x00 0xE8 0x00 0xE8 0x00 0xF3 0x14
    0x80 capExtended = 0 Absolute mode                               // 0xE8 0x02 0xE8 0x00 0xE8 0x00 0xE8 0x00 0xF3 0x14
  ✔ 0x81 capExtended = 1 Absolute mode with W                        // 0xE8 0x02 0xE8 0x00 0xE8 0x00 0xE8 0x01 0xF3 0x14
    0xC0 capExtended = 0 Absolute mode with high packet rate         // 0xE8 0x03 0xE8 0x00 0xE8 0x00 0xE8 0x00 0xF3 0x14
    0xC1 capExtended = 1 Absolute mode with W, high packet rate      // 0xE8 0x03 0xE8 0x00 0xE8 0x00 0xE8 0x01 0xF3 0x14
    0x0C capSleep = 1 Low-power sleep mode                           // 0xE8 0x00 0xE8 0x00 0xE8 0x03 0xE8 0x00 0xF3 0x14

  *** the function send_tp_arg is intended for this, but it does not always work.
*/

PS2Mouse::PS2Mouse(uint8_t scl, uint8_t sda)
{
  _scl = scl;
  _sda = sda;
}

PS2Mouse::PS2Mouse()
{
  _scl = 5;
  _sda = 23;
}

bool PS2Mouse::begin()
{
  byte minor, major, modelCode;
  for (uint8_t i = 0; i < 3; i++)
  {              // -“how many times have you rebooted your computer?”
    send(0xFF);  // -“three dude, you always say 'overload three times'” =)
    delay(1000); // Note: This may need an extra-long timeout.
    if (get() != 0xAA)
    {
      error("init, expected 0xAA");
      return false;
    }
    if (get() != 0x00)
    {
      error("init, expected 0x00");
      return false;
    }
  }
  send_tp_arg(0x00); // Send “Identify TouchPad” sequence
  send(0xE9);        // Status Request command
  minor = get();     // First status byte: TouchPad minor rev.
  modelCode = get(); // Second infoModelCode, status byte: 0x47 == Synaptics TouchPad
  major = get();     // Third status byte: Major rev in low 4 bits
  Serial.println();
  Serial.print("TouchPad: ");
  (modelCode == 0x47) ? Serial.print("Synaptics") : Serial.print("unknown");
  Serial.print(", id: ");
  Serial.print(modelCode, BIN);
  Serial.print(", minor rev.: ");
  Serial.print(minor, BIN);
  Serial.print(", major: ");
  Serial.print(major, BIN);

  if (modelCode == 0x47)
  {
    send(0xF5); // Disable before activate Absolute mode
    /*Synaptics mode byte (6 Bit)*/
    // send_tp_arg(0x81);                                //it does not always work I do not know the reason
    send(0xE8);
    send(0x02);
    send(0xE8);
    send(0x00);
    send(0xE8);
    send(0x00);
    send(0xE8);
    send(0x01);
    send(0xF3);
    send(0x14);
  }

  // send(0xE6);                                         // E6 Set Scaling 1:1 / E7 Set Scaling 2:1

  // send(0xE8);                                         // E8 Set Resolution (argument 0-3)
  // send(0x00);                                         // Sample Resolution argument of 3

  // send(0xF3);                                         // F3 Set Sample Rate (argument $00-$FF)
  // send(0x14);                                         // Sample Rate argument Valid sample rates are 10, 20, 40, 60, 80, 100, and 200 samples/sec

  send(0xF4);   // Enable command
  set_CLOCK(1); // Release PS/2 bus inhibit
  Serial.println(", state: init completed");
  return true;
}

void PS2Mouse::send(byte value)
{
  byte i, ack, p = 1;
  set_CLOCK(0);
  delayMicroseconds(100);
  set_DATA(0);
  set_CLOCK(1);
  for (i = 0; i < 8; i++)
  {
    wait_CLOCK(0);
    set_DATA(value & 0x01);
    p = p + value;
    wait_CLOCK(1);
    value = value >> 1;
  }
  wait_CLOCK(0);
  set_DATA(p & 0x01);
  wait_CLOCK(1);
  wait_CLOCK(0);
  set_DATA(1);
  wait_CLOCK(1);
  wait_CLOCK(0);
  if (read_DATA() == 1)
  {
    error("Missing line control bit!");
  }
  wait_CLOCK(1);
  ack = get();
  if (ack != 0xFA)
  {
    error("Probably got an FE or FC error code");
  }
}

byte PS2Mouse::get(bool handler)
{
  byte i, bit, value = 0, p = 0;
  set_CLOCK(1);
  wait_CLOCK(0);
  wait_CLOCK(1);
  for (i = 0; i < 8; i++)
  {
    wait_CLOCK(0);
    bit = read_DATA();
    value = value + (bit << i);
    p = p + bit;
    wait_CLOCK(1);
  }
  wait_CLOCK(0);
  p = p + read_DATA();
  if ((p & 0x01) == 0)
  {
    error("Parity!");
  }
  wait_CLOCK(1);
  wait_CLOCK(0);
  if (read_DATA() == 0)
  {
    error("Framing!");
  }
  delayMicroseconds(50);
  if (!handler)
  {
    set_CLOCK(0);
  }
  return value;
}

void PS2Mouse::error(const char *e)
{
  Serial.print("Error: ");
  Serial.println(e);
}

void PS2Mouse::wait_CLOCK(byte state)
{
  unsigned long saveTime = millis();
  while (digitalRead(_scl) != state)
  {
    if (millis() - saveTime > 50)
    {
      error("Timeout SCL");
      break;
    }
  }
}

void PS2Mouse::set_CLOCK(byte state)
{
  state == 1 ? high(_scl) : low(_scl);
}

byte PS2Mouse::read_DATA()
{
  return digitalRead(_sda);
}

void PS2Mouse::set_DATA(byte data)
{
  data == 1 ? high(_sda) : low(_sda);
}

void PS2Mouse::send_tp_arg(byte arg)
{
  byte i;
  for (i = 0; i < 4; i++)
  {
    send(0xE8);
    send((arg >> (6 - 2 * i)) & 3);
  };
}

void PS2Mouse::high(uint8_t pin)
{
  pinMode(pin, INPUT);
  digitalWrite(pin, HIGH);
}

void PS2Mouse::low(uint8_t pin)
{
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void PS2Mouse::getAbsoluteAxis(byte *raw, uint16_t &x, uint16_t &y)
{
  x = ((raw[3] & 0x10) >> 4) << 12 | (raw[1] & 0xF) << 8 | raw[4];
  y = ((raw[3] & 0x20) >> 5) << 12 | ((raw[1] & 0xF0) << 4) | raw[5];
}
