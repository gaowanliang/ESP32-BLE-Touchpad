[简体中文](README.zh-CN.md) | English

# ESP32-BLE-Touchpad

An ESP32-based Bluetooth touchpad project that drives a Synaptics controller touchpad to connect to a computer or mobile phone via Bluetooth, providing mouse functionality.

## Motivation

I bought an old Synaptics TM2334 touchpad from a Lenovo S41 laptop on Taobao (AliExpress) for only CNY 16 (in China, approx. USD 2.5) including shipping. I wanted to make a Bluetooth touchpad that could connect to a phone or computer. This touchpad is a clickpad, not a traditional button-style touchpad, so I needed a module to simulate mouse left and right buttons. I happened to have an ESP32 development board that I got from a roommate (for free), so I decided to use the ESP32 to implement this functionality.

Most of today's laptop touchpads are made by Synaptics and usually use the PS/2 interface. By default (i.e., without any special drivers), they typically can mimic a regular PS/2 mouse, that is, they can report finger movements and button clicks, but that's it. To implement the multi-finger gestures we commonly use on laptops, we need to implement them ourselves.

## Feature List

> [!Note]
> Since I use Windows systems daily, I have only implemented mouse functions under Windows. Other systems have not been tested yet.

- [x] Mouse movement
- [ ] Pressing the lower left area as the mouse left button, pressing the lower right area as the mouse right button (to be implemented after casing, currently it's difficult to press)
- [x] Tap to click
  - [x] Single-finger tap as mouse left click
  - [x] Two-finger tap as mouse right click
  - [x] Three-finger tap as mouse middle click
  - [x] Palm rejection
- [x] Scrolling
  - [x] Two-finger vertical swipe for vertical scrolling
  - [x] Two-finger horizontal swipe for horizontal scrolling
- [ ] Three-finger gestures
  - [ ] Three-finger swipe left/right to switch applications (implemented by sending Alt + Tab)
  - [ ] Three-finger swipe up/down to show desktop or return to application (implemented by sending Win + Tab and Win + D)
- [x] Tap and drag to enable dragging
- [ ] Zoom in and out
- [ ] Sleep mode

## Compilation

This program is compiled using PlatformIO. You can compile it by installing the PlatformIO plugin in VSCode. The project has already configured the `platformio.ini` file. You just need to open the project folder and click the PlatformIO `Build` button to compile.

You need to modify the 48th and 49th lines in the `src/main.cpp` file, change `CLOCK_PIN` and `DATA_PIN` to your actual pins.

```cpp
const int CLOCK_PIN = 23;
const int DATA_PIN = 5;
```

## Contribution

You're welcome to submit issues and pull requests to improve the project.

## Acknowledgments

[@delingren's synaptics_touchpad project](https://github.com/delingren/synaptics_touchpad)

[@T-vK's ESP32-BLE-Mouse project](https://github.com/T-vK/ESP32-BLE-Mouse)

## License

This project is based on the MIT license. For details, please refer to the [LICENSE](LICENSE) file.
