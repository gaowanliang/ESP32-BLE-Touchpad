#include "PS2Touchpad.h"

PS2Touchpad::PS2Touchpad(int dataPin, int clockPin)
    : _dataPin(dataPin), _clockPin(clockPin) {}

void PS2Touchpad::begin()
{
    pinMode(_dataPin, INPUT_PULLUP);
    pinMode(_clockPin, INPUT_PULLUP);
    log("PS/2 communication initialized.");
}

void PS2Touchpad::init()
{
    log("Initializing PS/2 Touchpad...");
    // Step 0: reset
    sendCommand(0xFF);
    uint8_t response = readResponse();

    // Step 1: Read Device Type (0xF2)
    sendCommand(0xF2);
    response = readResponse();
    if (response == 0xFA)
    {
        log("ACK received for 0xF2 (Read Device Type)");
    }
    else
    {
        logError("No ACK for 0xF2");
    }

    response = readResponse();
    logHex("Device Type Response: ", response);

    delay(100); // Possible delay for stability

    // Step 2: Set Defaults (0xF6)
    sendCommand(0xF6);
    response = readResponse();
    if (response == 0xFA)
    {
        log("ACK received for 0xF6 (Set Defaults)");
    }
    else
    {
        logError("No ACK for 0xF6");
    }

    // Step 3: Set Sample Rate (0xF3) - with different data values
    sendCommand(0xF3);
    response = readResponse();
    if (response == 0xFA)
    {
        log("ACK received for 0xF3 (Set Sample Rate)");
    }
    else
    {
        logError("No ACK for 0xF3");
    }
    sendCommand(0x0A); // Sample rate = 10
    response = readResponse();
    logHex("Sample rate set to 10: ", response);

    // Step 4: Set Resolution (0xE8)
    sendCommand(0xE8);
    response = readResponse();
    if (response == 0xFA)
    {
        log("ACK received for 0xE8 (Set Resolution)");
    }
    else
    {
        logError("No ACK for 0xE8");
    }
    sendCommand(0x00); // Resolution = 0
    response = readResponse();
    logHex("Resolution set to 0: ", response);

    // Repeat some more sample rate changes with different values
    uint8_t sampleRates[] = {0x14, 0x3C, 0x28, 0x14, 0x14};
    for (int i = 0; i < 5; i++)
    {
        sendCommand(0xF3);
        response = readResponse();
        if (response == 0xFA)
        {
            log("ACK received for 0xF3 (Set Sample Rate)");
        }
        else
        {
            logError("No ACK for 0xF3");
        }
        sendCommand(sampleRates[i]);
        response = readResponse();
        logHex("Sample rate set to: ", sampleRates[i]);
    }

    // Step 5: Status Request (0xE9)
    sendCommand(0xE9);
    response = readResponse();
    if (response == 0xFA)
    {
        log("ACK received for 0xE9 (Status Request)");
    }
    else
    {
        logError("No ACK for 0xE9");
    }
    uint8_t status[3];
    for (int i = 0; i < 3; i++)
    {
        status[i] = readResponse();
    }
    logHex("Status byte 1: ", status[0]);
    logHex("Status byte 2: ", status[1]);
    logHex("Status byte 3: ", status[2]);

    // Step 6: Reset (0xFF)
    sendCommand(0xFF);
    response = readResponse();
    if (response == 0xFA)
    {
        log("ACK received for 0xFF (Reset)");
    }
    else
    {
        logError("No ACK for 0xFF");
    }

    delay(100); // Possible delay for device to reset
}

void PS2Touchpad::sendCommand(uint8_t command)
{
    // PS/2 Command sending protocol
    pinMode(_clockPin, OUTPUT);
    pinMode(_dataPin, OUTPUT);
    digitalWrite(_clockPin, HIGH);
    digitalWrite(_dataPin, HIGH);
    delayMicroseconds(300); // Bus idle

    digitalWrite(_clockPin, LOW); // Start condition
    delayMicroseconds(300);       // Hold low to start frame

    // Send each bit of the command (8 bits)
    for (int i = 0; i < 8; i++)
    {
        digitalWrite(_dataPin, (command & 1) ? HIGH : LOW);
        delayMicroseconds(30);         // Setup time
        digitalWrite(_clockPin, HIGH); // Clock high
        delayMicroseconds(30);         // Data read time
        digitalWrite(_clockPin, LOW);  // Clock low
        command >>= 1;
    }

    // Parity bit
    digitalWrite(_dataPin, HIGH); // Assume parity bit is 1 for now
    delayMicroseconds(30);
    digitalWrite(_clockPin, HIGH);
    delayMicroseconds(30);
    digitalWrite(_clockPin, LOW);

    // Stop condition
    digitalWrite(_dataPin, HIGH); // Data line released
    delayMicroseconds(30);
    digitalWrite(_clockPin, HIGH); // Release clock line
}

uint8_t PS2Touchpad::readResponse()
{
    pinMode(_dataPin, INPUT);
    pinMode(_clockPin, INPUT);

    uint8_t response = 0;
    for (int i = 0; i < 8; i++)
    {
        while (digitalRead(_clockPin) == LOW)
            ;                                     // Wait for clock line to go high
        delayMicroseconds(15);                    // Small delay
        response |= (digitalRead(_dataPin) << i); // Read the data bit
        while (digitalRead(_clockPin) == HIGH)
            ; // Wait for clock line to go low
    }

    // Read parity bit (ignored for now)
    while (digitalRead(_clockPin) == LOW)
        ;
    while (digitalRead(_clockPin) == HIGH)
        ;

    // Stop bit
    while (digitalRead(_clockPin) == LOW)
        ;
    while (digitalRead(_clockPin) == HIGH)
        ;

    logHex("Response received: ", response);
    return response;
}

void PS2Touchpad::log(const char *message)
{
    Serial.println(message);
}

void PS2Touchpad::logHex(const char *message, uint8_t value)
{
    Serial.print(message);
    Serial.println(value, HEX);
}

void PS2Touchpad::logError(const char *message)
{
    Serial.print("Error: ");
    Serial.println(message);
}
