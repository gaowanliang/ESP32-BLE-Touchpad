#include "SynapticsHandler.h"

SynapticsHandler::SynapticsHandler(int clock_pin, int data_pin)
{
    _clock_pin = clock_pin;
    _data_pin = data_pin;
    _initialized = false;
    _absolute_mode = false;
    _has_multi_finger = false;
    _has_palm_detect = false;
}

bool SynapticsHandler::initialize()
{

    int counter = 0;
    int return_value = 0;
    // poll mouse to get a connection
    do
    {
        return_value = try_initialise();
        counter++;
    } while ((return_value != 0) && (counter < 10));
    return return_value;
}

bool SynapticsHandler::try_initialise()
{
    // Reset pins to idle state
    pull_high(_clock_pin);
    pull_high(_data_pin);
    delay(500); // Power-up delay

    // Reset the touchpad
    if (!send_command(SYNAPTICS_RESET))
    {
        return false;
    }
    delay(500); // Wait for reset

    // Read BAT completion code
    uint8_t bat_code = read_byte();
    if (bat_code == 0xAA)
    {
        return false;
    }

    // Detect if it's a Synaptics touchpad
    if (!detect_touchpad())
    {
        return false;
    }

    // Set default mode (relative)
    if (!set_touchpad_mode(TOUCHPAD_RELATIVE))
    {
        return false;
    }

    _initialized = true;
    return true;
}

bool SynapticsHandler::detect_touchpad()
{
    // Send the identify command
    if (!send_command(SYNAPTICS_IDENTIFY))
    {
        return false;
    }

    // Read identification bytes
    uint8_t id_bytes[3];
    for (int i = 0; i < 3; i++)
    {
        id_bytes[i] = read_byte();
    }

    // Check if it's a Synaptics device
    if ((id_bytes[0] & 0xFC) != SYNAPTICS_ID)
    {
        return false;
    }

    // Parse version information
    _major_ver = (id_bytes[0] & 0x0F) >> 4;
    _minor_ver = id_bytes[0] & 0x0F;
    _model_id = id_bytes[1];

    // Check capabilities
    _has_multi_finger = (id_bytes[2] & TOUCHPAD_MULTI_FINGER) != 0;
    _has_palm_detect = (id_bytes[2] & TOUCHPAD_PALM_DETECT) != 0;

    return true;
}

bool SynapticsHandler::set_absolute_mode(bool enable)
{
    uint8_t mode = enable ? TOUCHPAD_ABSOLUTE : TOUCHPAD_RELATIVE;
    if (!set_touchpad_mode(mode))
    {
        return false;
    }
    _absolute_mode = enable;
    return true;
}

bool SynapticsHandler::get_data()
{
    if (!_initialized)
    {
        return false;
    }

    // Request data packet
    if (!send_command(0xEB))
    {
        return false;
    }

    // Read acknowledge byte
    if (read_byte() != 0xFA)
    {
        return false;
    }

    // Parse data based on current mode
    if (_absolute_mode)
    {
        parse_absolute_packet();
    }
    else
    {
        parse_relative_packet();
    }

    return true;
}

void SynapticsHandler::parse_absolute_packet()
{
    uint8_t packet[6];
    for (int i = 0; i < 6; i++)
    {
        packet[i] = read_byte();
    }

    _status = packet[0];

    // Parse absolute position (12-bit values)
    _x_abs = ((packet[1] & 0x0F) << 8) | packet[2];
    _y_abs = ((packet[3] & 0x0F) << 8) | packet[4];
    _z_pressure = packet[5];
}

void SynapticsHandler::parse_relative_packet()
{
    uint8_t packet[3];
    for (int i = 0; i < 3; i++)
    {
        packet[i] = read_byte();
    }

    _status = packet[0];
    _x_rel = packet[1];
    _y_rel = packet[2];

    // Sign extend relative movements
    if (_x_rel & 0x80)
        _x_rel |= 0xFF00;
    if (_y_rel & 0x80)
        _y_rel |= 0xFF00;
}

// Low level communication methods (similar to PS2MouseHandler)
void SynapticsHandler::write(uint8_t data)
{
    char parity = 1;
    unsigned long start_time = millis();

    pull_high(_data_pin);
    pull_high(_clock_pin);
    delayMicroseconds(300);
    pull_low(_clock_pin);
    delayMicroseconds(300);
    pull_low(_data_pin);
    delayMicroseconds(10);
    pull_high(_clock_pin);

    // Wait for device to take control of clock
    while (digitalRead(_clock_pin))
    {
        if (millis() - start_time >= 100)
        {
            return; // Timeout
        }
    }

    // Send data bits
    for (int i = 0; i < 8; i++)
    {
        if (data & 0x01)
        {
            pull_high(_data_pin);
        }
        else
        {
            pull_low(_data_pin);
        }

        while (!digitalRead(_clock_pin))
        {
        }
        while (digitalRead(_clock_pin))
        {
        }

        parity ^= (data & 0x01);
        data >>= 1;
    }

    // Send parity bit
    if (parity)
    {
        pull_high(_data_pin);
    }
    else
    {
        pull_low(_data_pin);
    }

    while (!digitalRead(_clock_pin))
    {
    }
    while (digitalRead(_clock_pin))
    {
    }

    pull_high(_data_pin);
    while (digitalRead(_data_pin))
    {
    }
    while (digitalRead(_clock_pin))
    {
    }
    while (!digitalRead(_clock_pin) && !digitalRead(_data_pin))
    {
    }
}

// Other helper methods remain similar to PS2MouseHandler
void SynapticsHandler::pull_low(int pin)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void SynapticsHandler::pull_high(int pin)
{
    pinMode(pin, INPUT);
    digitalWrite(pin, HIGH);
}

uint8_t SynapticsHandler::read_byte()
{
    _last_operation = "read_byte";
    uint8_t data = 0;
    unsigned long timeout = millis() + 100; // 100ms超时

    pull_high(_clock_pin);
    pull_high(_data_pin);
    delayMicroseconds(50);

    // 等待时钟低电平，带超时检查
    while (digitalRead(_clock_pin))
    {
        if (millis() > timeout)
        {
            _last_byte = 0xFF;
            return 0xFF; // 超时返回
        }
    }

    for (int i = 0; i < 8; i++)
    {
        bitWrite(data, i, read_bit());
    }

    read_bit(); // 奇偶校验位
    read_bit(); // 停止位

    _last_byte = data;
    return data;
}

int SynapticsHandler::read_bit()
{
    while (digitalRead(_clock_pin))
    {
    }
    int bit = digitalRead(_data_pin);
    while (!digitalRead(_clock_pin))
    {
    }
    return bit;
}

// 在 SynapticsHandler.cpp 文件中添加以下方法实现：

bool SynapticsHandler::enable_palm_detection(bool enable)
{
    if (!_has_palm_detect)
    {
        return false; // 设备不支持掌部检测
    }

    // 发送开启/关闭掌部检测的命令
    uint8_t cmd = enable ? 0xF8 : 0xF9;
    if (!send_command(cmd))
    {
        return false;
    }

    // 等待确认
    return (read_byte() == 0xFA);
}

bool SynapticsHandler::enable_multi_finger(bool enable)
{
    if (!_has_multi_finger)
    {
        return false; // 设备不支持多指触控
    }

    // 发送开启/关闭多指触控的命令
    uint8_t cmd = enable ? 0xF6 : 0xF7;
    if (!send_command(cmd))
    {
        return false;
    }

    // 等待确认
    return (read_byte() == 0xFA);
}

bool SynapticsHandler::set_sample_rate(uint8_t rate)
{
    // 发送设置采样率命令
    if (!send_command(0xF3))
    {
        return false;
    }

    // 等待确认
    if (read_byte() != 0xFA)
    {
        return false;
    }

    // 发送采样率值
    if (!send_command(rate))
    {
        return false;
    }

    // 等待最终确认
    return (read_byte() == 0xFA);
}

bool SynapticsHandler::send_command(uint8_t command)
{
    _last_operation = "send_command";
    write(command);
    delayMicroseconds(100);

    uint8_t ack = read_byte();
    if (ack != 0xFA)
    {
        Serial.print("Command 0x");
        Serial.print(command, HEX);
        Serial.print(" failed, received: 0x");
        Serial.println(ack, HEX);
        return false;
    }
    return true;
}

bool SynapticsHandler::set_touchpad_mode(uint8_t mode)
{
    // 先禁用数据报告
    if (!send_command(0xF5))
    {
        return false;
    }

    // 发送模式设置命令
    if (!send_command(SYNAPTICS_MODES))
    {
        return false;
    }

    // 发送模式参数
    if (!send_command(mode))
    {
        return false;
    }

    // 重新启用数据报告
    return send_command(0xF4);
}