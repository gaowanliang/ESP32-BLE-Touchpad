#ifndef SynapticsHandler_h
#define SynapticsHandler_h

#include <Arduino.h>

// Synaptics Commands
#define SYNAPTICS_ID 0x47 // Synaptics TouchPad ID
#define SYNAPTICS_MODES 0xF5
#define SYNAPTICS_STATUS 0xF2
#define SYNAPTICS_RESET 0xFF
#define SYNAPTICS_IDENTIFY 0xE9

// Modes and capabilities
#define TOUCHPAD_ABSOLUTE 0x80
#define TOUCHPAD_RELATIVE 0x00
#define TOUCHPAD_PALM_DETECT 0x01
#define TOUCHPAD_MULTI_FINGER 0x02

class SynapticsHandler
{
private:
  int _clock_pin;
  int _data_pin;
  int _mode;
  bool _initialized;
  bool _absolute_mode;

  // TouchPad information
  uint8_t _major_ver;
  uint8_t _minor_ver;
  uint8_t _model_id;
  bool _has_multi_finger;
  bool _has_palm_detect;

  // Current state
  uint8_t _status;
  int16_t _x_abs;
  int16_t _y_abs;
  int16_t _z_pressure;
  int16_t _x_rel;
  int16_t _y_rel;

  uint8_t _last_byte;          // 存储最后收到的字节
  const char *_last_operation; // 存储最后执行的操作名称

  // Private methods
  void pull_high(int pin);
  void pull_low(int pin);
  bool try_initialise();
  void write(uint8_t data);
  uint8_t read_byte();
  int read_bit();
  bool send_command(uint8_t command);
  bool read_touchpad_id();
  bool set_touchpad_mode(uint8_t mode);
  void parse_absolute_packet();
  void parse_relative_packet();


public:
  SynapticsHandler(int clock_pin, int data_pin);

  // Initialization
  bool initialize();
  bool detect_touchpad();

  // Configuration
  bool set_absolute_mode(bool enable);
  bool enable_palm_detection(bool enable);
  bool enable_multi_finger(bool enable);
  bool set_sample_rate(uint8_t rate);

  // Status and movement
  bool get_data();
  uint8_t get_status() const { return _status; }

  // Absolute positioning
  int16_t get_x_abs() const { return _x_abs; }
  int16_t get_y_abs() const { return _y_abs; }
  int16_t get_pressure() const { return _z_pressure; }

  // Relative movement
  int16_t get_x_movement() const { return _x_rel; }
  int16_t get_y_movement() const { return _y_rel; }

  // Capability checks
  bool has_multi_finger() const { return _has_multi_finger; }
  bool has_palm_detect() const { return _has_palm_detect; }

  // Version information
  uint8_t get_major_version() const { return _major_ver; }
  uint8_t get_minor_version() const { return _minor_ver; }
  uint8_t get_model_id() const { return _model_id; }

  // Debugging
  uint8_t get_last_byte() const { return _last_byte; }
  const char *get_last_operation() const { return _last_operation; }
};

#endif