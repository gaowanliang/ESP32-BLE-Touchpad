// touchpad_demo.ino
#include <ps2.h>
#include <synaptics.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <BleMouse.h>
#include <freertos/queue.h>

BleMouse bleMouse;

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef sign
#define sign(x) ((x) > 0 ? (1) : ((x) < 0 ? (-1) : (0)))
#endif

// 定义GPIO引脚
const int CLOCK_PIN = 23; // ESP32的GPIO23
const int DATA_PIN = 5;   // ESP32的GPIO5

// 防抖和优化相关常量
const float noise_threshold_tracking_mm = 0.08;
const float noise_threshold_scrolling_mm = 0.09;
const int frames_delay = 20;
const int frames_stablization = 15;
const float scale_tracking_mm = 12.0;
const float scale_scroll_mm = 1.6;
const float slow_scroll_threshold_mm = 2.0;
const float max_delta_mm = 3;
const int proximity_threshold_mm = 15;
const float slow_scroll_amount = 0.20F;
const TickType_t xDelay = pdMS_TO_TICKS(10);

// 全局变量
volatile uint64_t g_received_packet = 0;
volatile bool g_packet_ready = false;
static unsigned long global_tick = 0;
static unsigned long session_started_tick = 0;
static unsigned long button_released_tick = 0;
int last_y = 0;
unsigned long three_finger_start = 0;
bool middle_button_pressed = false;
unsigned long finger_down_time = 0;

// Tap as click 轻触作为点击
bool tap_detected = false;
unsigned long tap_start_tick = 0;
const unsigned long tap_time_threshold = 25; // 轻触时间tick
float total_movement = 0;
const float tap_tracking_threshold = 15;   // 防止手抖
const short tap_z_threshold = 100;         // 防止手掌误触，z是触摸宽度，当手掌压上去时，z值会很大
short max_tap_z = 0;                       // 记录最大的z值
uint16_t button_state_count = 0;           // 记录超过一定次数的按钮状态
const uint16_t button_state_threshold = 5; // 按钮状态超过一定次数，才认为是点击
short tap_button = 0;                      // 记录轻触时的按钮状态

// 定义消息队列句柄
static QueueHandle_t mouseEventQueue = NULL;

struct TouchInfo
{
  int x;
  int y;
  int z;
  int fingers;
  bool button;
};

struct finger_state
{
  SimpleAverage<int, 5> x;
  SimpleAverage<int, 5> y;
  short z;
};

struct report
{
  uint8_t buttons;
  int8_t x;
  int8_t y;
  int8_t scroll;
};

RingBuffer<report, 32> reports;
static finger_state finger_states[2]; // 0 is primary, 1 is secondary
static short finger_count = 0;
static uint8_t button_state = 0;
// 变量
float scale_tracking_x, scale_tracking_y;
float scale_scroll;
float noise_threshold_tracking_x, noise_threshold_tracking_y;
float noise_threshold_scrolling_y;
float max_delta_x, max_delta_y;
float slow_scroll_threshold;
float proximity_threshold_x, proximity_threshold_y;

void IRAM_ATTR byte_received(uint8_t data)
{
  static uint64_t buffer = 0;
  static int index = 0;

  // Ignore all bytes until we see the start of a packet, otherwise the
  // packets may get out of sequence and things will get very confusing.
  if (index == 0 && (data & 0xc8) != 0x80)
  {
    Serial.print("Unexpected byte0 data ");
    Serial.println(data, HEX);

    index = 0;
    buffer = 0;
    return;
  }

  if (index == 24 && (data & 0xc8) != 0xc0)
  {
    Serial.print("Unexpected byte3 data ");
    Serial.println(data, HEX);

    index = 0;
    buffer = 0;
    return;
  }

  buffer |= ((uint64_t)data) << index;
  index += 8;
  if (index == 48)
  {
    if (mouseEventQueue != NULL)
    { // 确保队列存在
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      uint64_t temp = buffer; // 创建临时变量，因为buffer会被清零
      xQueueSendFromISR(mouseEventQueue, &temp, &xHigherPriorityTaskWoken);
      if (xHigherPriorityTaskWoken)
      {
        portYIELD_FROM_ISR();
      }
    }
    index = 0;
    buffer = 0;
  }
}

// 将值转换为HID值
float to_hid_value(float value, float threshold, float scale_factor)
{
  const float hid_max = 127.0F;
  if (abs(value) < threshold)
  {
    return 0;
  }
  return sign(value) * min(max(abs(value) * scale_factor, 1.0F), hid_max);
}

void tap_as_click_reset(int flag)
{
  tap_detected = false;
  total_movement = 0;
  max_tap_z = 0;
  button_state_count = 0;
  Serial.printf("Tap as click reset, flag: %d\n", flag);
}

void queue_report(uint8_t buttons, int8_t x, int8_t y, float scroll)
{
  static float scroll_amount_rollover = 0;
  report item = {.buttons = buttons};
  if (button_released_tick != 0 &&
      global_tick - button_released_tick < frames_stablization)
  {
    item.x = 0;
    item.y = 0;
    item.scroll = 0;
  }
  else
  {
    if (scroll > -1.0F && scroll < 1.0F)
    {
      scroll_amount_rollover += scroll;
      if (scroll_amount_rollover >= 1.0F)
      {
        scroll = 1.0F;
        scroll_amount_rollover -= 1.0F;
      }
      else if (scroll_amount_rollover <= -1.0F)
      {
        scroll = -1.0F;
        scroll_amount_rollover += 1.0F;
      }
      else
      {
        scroll = 0;
      }
    }
    item.x = x;
    item.y = y;
    item.scroll = scroll;
  }
  reports.push_back(item);
}

void parse_primary_packet(uint64_t packet, int w)
{
  global_tick++;
  // Reference: Section 3.2.1, Figure 3-4
  int x = (packet >> 32) & 0x00FF | (packet >> 0) & 0x0F00 |
          (packet >> 16) & 0x1000;
  int y = (packet >> 40) & 0x00FF | (packet >> 4) & 0x0F00 |
          (packet >> 17) & 0x1000;
  short z = (packet >> 16) & 0xFF; // z 是宽度，手掌压上去z就大，手指轻轻触摸z就小
  // w is width only if it >= 4. otherwise it encodes finger count
  short width = max(w, 4);

  // A clickpad reprots its button as a middle/up button. This logic needs to
  // change completely if the touchpad is not a clickpad (i.e. it has physical
  // buttons).
  bool button = (packet >> 24) & 0x01;
  int new_finger_count = 0;
  if (z == 0)
  {
    new_finger_count = 0;
  }
  else if (w >= 4)
  {
    new_finger_count = 1;
  }
  else if (w == 0)
  {
    new_finger_count = 2;
  }
  else if (w == 1)
  {
    new_finger_count = 3;
  }

  if (finger_count == 0 && new_finger_count > 0)
  {
    session_started_tick = global_tick;
  }

  /* Mechanisms to smooth the movements. */

  // When a button is pressed, we retrospectively freeze the previous frames,
  // since the movements tend to be jerky when releasing a button.
  if (button && button_state == 0)
  {
    int size = reports.size();
    for (int i = 0; i < size; i++)
    {
      reports[i].x = 0;
      reports[i].y = 0;
      reports[i].scroll = 0;
    }
  }

  // When a button is released, we freeze the next few frames, since the
  // movements tend to be jerky when pressing a button.
  if (!button && button_state != 0)
  {
    button_released_tick = global_tick;
  }

  // When a finger is lifted, we restrospectively freeze the previous
  // frames, since the movements tend to be jerky when lifting a finger.
  if (new_finger_count < finger_count)
  {
    for (int i = 0; i < reports.size(); i++)
    {
      reports[i].x = 0;
      reports[i].y = 0;
      reports[i].scroll = 0;
    }
  }

  /* Update state variables. */
  if (new_finger_count > finger_count)
  {
    // A finger has been added. Reset state for that finger.
    finger_states[1].x.reset();
    finger_states[1].y.reset();
    if (finger_count == 0)
    {
      finger_states[0].x.reset();
      finger_states[0].y.reset();
    }
  }

  int delta_x = 0, delta_y = 0;

  if (new_finger_count > 0)
  {
    finger_states[0].z = z;

    int prev_x = finger_states[0].x.average();
    int new_x = finger_states[0].x.filter(x);
    if (prev_x > 0 && new_finger_count == finger_count)
    {
      delta_x = new_x - prev_x;
    }

    int prev_y = finger_states[0].y.average();
    int new_y = finger_states[0].y.filter(y);
    if (prev_y > 0 && new_finger_count == finger_count)
    {
      delta_y = new_y - prev_y;
    }
  }

  if (new_finger_count < finger_count)
  {
    // A finger has been released.
    if (finger_count == 2)
    {
      // 2 fingers -> 1 finger
      // When a primary finger is released, the secondary becomes the primary.
      // We determine if that is happening by checking if the finger location is
      // close to the previous secondary finger. proximity_threshold is an
      // emperical number and not always reliable. We err on the conservative
      // side. If we can't be sure, just reset the state. This could result in a
      // slightly jerky cursor movement.
      if (abs(x - finger_states[0].x.average()) >= proximity_threshold_x ||
          abs(y - finger_states[0].y.average()) >= proximity_threshold_y)
      {
        if (abs(x - finger_states[1].x.average()) < proximity_threshold_x &&
            abs(y - finger_states[1].y.average()) < proximity_threshold_y)
        {
          finger_states[0] = finger_states[1];
        }
        else
        {
          finger_states[0].x.reset();
          finger_states[0].y.reset();
          // tap_as_click_reset();
        }
      }
    }
    else
    {
      // 3 fingers -> 2 fingers or 1 finger, or all fingers have been lifted.
      // Let's not bother. Just reset both fingers.
      finger_states[0].x.reset();
      finger_states[0].y.reset();
      finger_states[1].x.reset();
      finger_states[1].y.reset();
      // tap_as_click_reset();
    }
  }

  if (finger_count == 1 && new_finger_count == 1 &&
      (abs(delta_x) >= max_delta_x || abs(delta_y) >= max_delta_y))
  {
    // In rare occasions where a finger is released and another is pressed in
    // the same frame, we don't see a finger count change but a big jump in
    // finger position. In this case, reset the position and start over.
    // This solution isn't ideal. A rather big jump could happen when the
    // fingers are moving very fast. A more reliable approach would be based
    // on the recent velocity of the finger movements. But it's complicated
    // and expensive. Both scenarios just described are edge cases and the user
    // is probably just fooling around.
    finger_states[0].x.reset();
    finger_states[0].y.reset();
    delta_x = 0;
    delta_y = 0;
  }

  // Serial.printf("=== tick: %d, Fingers: %d, X: %d, Y: %d, Z: %d, Width: %d, Button: %d, DeltaX: %d, DeltaY: %d, Per_Finger: %d ===\n", global_tick, new_finger_count, x, y, z, width, button, delta_x, delta_y, finger_count);

  // 轻触作为点击，包括单击、双击和三击
  if (finger_count == 0 && new_finger_count > 0)
  {
    tap_start_tick = global_tick;
    tap_as_click_reset(1);
    button_state = new_finger_count;
    tap_detected = true;
    tap_button = button_state;
  }
  else if (finger_count >= 0 && new_finger_count == 0)
  {
    if (tap_detected)
    {
      // Serial.printf("Tap detected: %d, total_movement: %f, max_tap_z: %d\n", global_tick - tap_start_tick, total_movement, max_tap_z);
      if (global_tick - tap_start_tick <= tap_time_threshold)
      {

        if (total_movement < tap_tracking_threshold && max_tap_z < tap_z_threshold)
        {
          queue_report(tap_button, 0, 0, 0);
          button_state = 0;
          tap_button = 0;
          tap_as_click_reset(2);
        }
      }
      else
      {
        Serial.println("Tap timeout");
        tap_as_click_reset(3);
      }
    }
  }

  finger_count = new_finger_count;

  /* State machine logic */
  if (finger_count == 0 && new_finger_count == 0)
  {
    // // idle
    // if (button_state == 0 && !click_is_send)
    // {
    //   button_state = new_finger_count < 2 ? LEFT_BUTTON : RIGHT_BUTTON;
    //   queue_report(button_state, 0, 0, 0);
    // }
    // else if (button_state != 0 && !button)
    // {
    //   button_state = 0;
    //   queue_report(0, 0, 0, 0);
    // }
  }
  else if (finger_count >= 2)
  {
    // scrolling
    // if (button)
    // {
    //   // It's OK to change between left and right while scrolling.
    //   button_state = new_finger_count > 1 ? RIGHT_BUTTON : LEFT_BUTTON;
    // }
    // else
    // {
    //   button_state = 0;
    // }

    // Since we're scrolling, we are here every other frame. So we should double
    // the noise threshold.
    total_movement += abs(delta_x) + abs(delta_y);
    if (z > max_tap_z)
    {
      max_tap_z = z;
    }
    float scroll_amount =
        to_hid_value(delta_y, noise_threshold_scrolling_y, scale_scroll);
    if (abs(delta_y) <= slow_scroll_threshold)
    {
      scroll_amount = sign(scroll_amount) * slow_scroll_amount;
    }
    if (tap_button < 2)
    {
      tap_button = 2;
    }
    if (scroll_amount != 0)
    {
      button_state = 0;
      // Serial.printf("Scroll amount: %f\n", scroll_amount);
      queue_report(button_state, 0, 0, scroll_amount);
    }
  }
  else if (finger_count == 1)
  {
    // 1-finger tracking or 2-finger tracking
    if (button || (abs(delta_x) < noise_threshold_tracking_x && abs(delta_y) < noise_threshold_tracking_y))
    {
      // If the button is already pressed, we don't change between left and
      // right while dragging.
      // if (button_state == 0)
      // {
      //   button_state = new_finger_count > 1 ? RIGHT_BUTTON : LEFT_BUTTON;
      // }
      total_movement += abs(delta_x) + abs(delta_y);
      if (z > max_tap_z)
      {
        max_tap_z = z;
      }
    }
    else
    {
      button_state = 0;
    }
    // else
    // {
    //   button_state = 0;
    // }
    // If there are multiple fingers pressed, normal packets and secondary
    // packets are alternated. So we should double the threshold.
    float threshold_multiplier = finger_count == 1 ? 1.0 : 2.0;
    // Serial.print("Width: ");
    // Serial.print(width);
    // Serial.print(" Pressure: ");
    // Serial.println(z);

    if (width > 4)
    {
      // Fat finger
      threshold_multiplier *= 1.0F + (width - 4.0F) / 4.0F;
    }
    if (z >= 60)
    {
      // Heavy finger
      threshold_multiplier *= 1.0F + (z - 60.0F) / 40.0F;
    }

    float delta_x_mm = ((float)delta_x) / ((float)synaptics::units_per_mm_x);
    float delta_y_mm = ((float)delta_y) / ((float)synaptics::units_per_mm_y);
    // Precision for low speed and range for high speed. If there are more than
    // one finger the speed needs to be doubled.
    float velocity = sqrt(delta_x_mm * delta_x_mm + delta_y_mm * delta_y_mm);
    if (finger_count > 1)
    {
      velocity *= 2;
    }
    float scale_multiplier = 1.0F + velocity * 0.5F; // Emperical constant

    int8_t delta_x_hid =
        to_hid_value(delta_x, noise_threshold_tracking_x * threshold_multiplier,
                     scale_tracking_x * scale_multiplier);
    int8_t delta_y_hid = -to_hid_value(
        delta_y, noise_threshold_tracking_y * threshold_multiplier,
        scale_tracking_y * scale_multiplier);
    if (abs(delta_x_hid) > 0 || abs(delta_y_hid) > 0)
    {
      // Serial.printf("DeltaX: %d, DeltaY: %d\n", delta_x_hid, delta_y_hid);
      button_state = 0;
      queue_report(button_state, delta_x_hid, delta_y_hid, 0);
      if (abs(delta_x_hid) > 1 || abs(delta_y_hid) > 1)
        tap_as_click_reset(4);
    }
  }
}

void parse_extended_packet(uint64_t packet)
{
  uint8_t packet_code = (packet >> 44) & 0x0F;
  if (packet_code == 1)
  {
    // Reference: Section 3.2.9.2. Figure 3-14
    int x = (packet >> 7) & 0x01FE | (packet >> 23) & 0x1E00;
    int y = (packet >> 15) & 0x01FE | (packet >> 27) & 0x1E00;
    short z = (packet >> 39) & 0x1D | (packet >> 23) & 0x60;

    if (x == 0 || y == 0 || z == 0)
    {
      finger_states[1].x.reset();
      finger_states[1].y.reset();
      return;
    }

    int prev_x = finger_states[1].x.average();
    int new_x = finger_states[1].x.filter(x);
    int delta_x = prev_x == 0 ? 0 : new_x - prev_x;

    int prev_y = finger_states[1].y.average();
    int new_y = finger_states[1].y.filter(y);
    int delta_y = prev_y == 0 ? 0 : new_y - prev_y;

    if (abs(delta_x) >= max_delta_x || abs(delta_y) >= max_delta_y)
    {
      // Sometimes when a 2nd or 3rd finger is released, we receive a secondary
      // finger position before the finger count change. In this case, the new
      // secondary finger is not necessarily the same physical finger as
      // previous one. Not sure if this is by design or due to a packet loss.
      // In either case, we should not report this position change to avoid
      // jerky movements. Instead, reset the secondary finger state and start
      // over.
      finger_states[1].x.reset();
      finger_states[1].y.reset();
      delta_x = 0;
      delta_y = 0;
    }

    finger_states[1].x.filter(x);
    finger_states[1].y.filter(y);
    finger_states[1].z = z;

    // TODO: use velocity and z value to adjst the multiplier here too, just
    // like the primary frames. We don't have width info though.
    if (finger_count >= 2 && button_state == 0)
    {
      // Since we are parsing secondary packets, we are here every other frame,
      // so we should double the noise threshold.
      float scroll_amount =
          to_hid_value(delta_y, noise_threshold_scrolling_y, scale_scroll);
      if (abs(delta_y) <= slow_scroll_threshold)
      {
        scroll_amount = sign(scroll_amount) * slow_scroll_amount;
      }
      // Serial.printf("Wmode Scroll amount: %f\n", scroll_amount);
      queue_report(button_state, 0, 0, scroll_amount);
    }
    else
    {
      int8_t delta_x_hid = to_hid_value(
          delta_x, noise_threshold_tracking_x * 2.0F, scale_tracking_x);
      int8_t delta_y_hid = -to_hid_value(
          delta_y, noise_threshold_tracking_y * 2.0F, scale_tracking_y);
      Serial.printf("Wmode DeltaX: %d, DeltaY: %d\n", delta_x_hid, delta_y_hid);
      // queue_report(button_state, delta_x_hid, delta_y_hid, 0);
      queue_report(0, delta_x_hid, delta_y_hid, 0);
    }
  }
}

void touchpadTask(void *pvParameters)
{
  uint64_t packet;

  if (mouseEventQueue == NULL)
  {
    Serial.println("Queue not initialized!");
    vTaskDelete(NULL);
    return;
  }

  while (1)
  {
    // 在解析数据包时，我们将报告排队，而不是直接发送它们。
    // 然后，我们延迟几帧后再发送报告，以便我们有机会回溯性地修改报告。
    // 我们每帧最多只生成一个报告。因此，我们每帧只需要发送一个待发送的报告。
    // 一旦所有活动停止，触控板会继续发送包含 x、y 和 z 都设置为 0 的数据包，持续一秒钟。
    // 我们只报告第一个数据包。这意味着我们有足够的时间清空报告队列，这是我们需要做的。
    // 否则，队列很快就会堵塞，报告会泄漏到下一次会话中，导致奇怪的行为。
    global_tick++;
    if (global_tick - session_started_tick >= frames_delay)
    {
      if (!reports.empty())
      {
        report item = reports.pop_front();
        // hid::report(item.buttons, item.x, item.y, item.scroll);
        Serial.printf("Buttons: %d, X: %d, Y: %d, Scroll: %d\n", item.buttons, item.x, item.y, item.scroll);

        // 如果和上次的一样，就不传了
        // if (item.buttons == previousItem.buttons && item.x == previousItem.x && item.y == previousItem.y && item.scroll == previousItem.scroll)
        // {
        //   continue;
        // }

        if (bleMouse.isConnected())
        {
          if (item.buttons > 0)
          {
            if (item.buttons == 1)
            {
              bleMouse.click(MOUSE_LEFT);
            }
            else if (item.buttons == 2)
            {
              bleMouse.click(MOUSE_RIGHT);
            }
          }
          else
          {
            if (item.scroll != 0)
            {
              bleMouse.move(0, 0, item.scroll);
            }
            else if (item.x != 0 || item.y != 0)
            {
              bleMouse.move(item.x, item.y);
            }
          }
        }
        // previousItem = item;
      }
    }

    if (xQueueReceive(mouseEventQueue, &packet, pdMS_TO_TICKS(10)))
    {
      // 处理packet数据
      uint8_t w = (packet >> 26) & 0x01 | (packet >> 1) & 0x2 | (packet >> 2) & 0x0C;
      switch (w) // 文档 3.2.6 节，Figure 3-9
      {
      case 3: // 当w=3时，表示是Pass-Through encapsulation packet（直通式封装数据包）
        break;
      case 2: // 当w=2时，表示是Extended W mode packet（扩展W模式数据包）
        parse_extended_packet(packet);
        break;
      default: // 当w=0或w=1时，表示是capMultiFinger，0是两根手指，1是三根及以上手指
        parse_primary_packet(packet, w);
        break;
      }
    }

    // vTaskDelay(xDelay);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 Touchpad Test");
  bleMouse.begin();

  // 创建队列 - 在使用之前必须先创建
  mouseEventQueue = xQueueCreate(32, sizeof(uint64_t)); // 32是队列长度
  if (mouseEventQueue == NULL)
  {
    Serial.println("Queue creation failed!");
    while (1)
      ; // 如果队列创建失败，停止运行
  }

  // 初始化PS2通信
  ps2::begin(CLOCK_PIN, DATA_PIN, byte_received);
  ps2::reset();
  synaptics::init();

  // 初始化变量
  scale_tracking_x = scale_tracking_mm / synaptics::units_per_mm_x;
  scale_tracking_y = scale_tracking_mm / synaptics::units_per_mm_y;
  scale_scroll = scale_scroll_mm / synaptics::units_per_mm_y;
  noise_threshold_tracking_x =
      noise_threshold_tracking_mm * synaptics::units_per_mm_x;
  noise_threshold_tracking_y =
      noise_threshold_tracking_mm * synaptics::units_per_mm_y;
  noise_threshold_scrolling_y =
      noise_threshold_scrolling_mm * synaptics::units_per_mm_y;
  max_delta_x = max_delta_mm * synaptics::units_per_mm_x;
  max_delta_y = max_delta_mm * synaptics::units_per_mm_y;
  slow_scroll_threshold = slow_scroll_threshold_mm * synaptics::units_per_mm_y;
  proximity_threshold_x = proximity_threshold_mm * synaptics::units_per_mm_x;
  proximity_threshold_y = proximity_threshold_mm * synaptics::units_per_mm_y;

  // 初始化任务看门狗
  esp_task_wdt_init(100, true); // 100ms超时，任务看门狗启用

  // 创建触摸板处理任务
  xTaskCreatePinnedToCore(
      touchpadTask,             // 任务函数
      "TouchpadTask",           // 任务名称
      4096,                     // 堆栈大小
      NULL,                     // 参数
      configMAX_PRIORITIES - 1, // 优先级
      NULL,                     // 任务句柄
      0                         // 在核心0上运行
  );

  // 将当前运行的核心（通常是核心0）添加到看门狗
  esp_task_wdt_add(NULL);
}

void loop()
{
  // 主循环喂狗
  esp_task_wdt_reset();

  // 可以在这里添加其他非关键任务
  delay(1000);
}