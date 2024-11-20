[English](README.md) | 简体中文

# ESP32-BLE-Touchpad

一个基于 ESP32 的蓝牙触摸板项目，驱动一个 Synaptics 控制器的触摸板，通过蓝牙连接到电脑或者手机，实现鼠标功能。

## 起因

我从淘宝上买了个旧的联想 S41 笔记本上的拆机触摸板 TM2334，带邮费只要 16 块钱。我想做一个蓝牙触摸板，可以连接到手机或者电脑上使用。这个触摸板属于点击板（clickpad），不是传统的按键式触摸板，所以我需要一个模块来模拟鼠标左键和右键。我手里正好有之前从舍友那里拿到（白嫖）的 ESP32 开发板，所以我决定用 ESP32 来实现这个功能。

目前的大多数笔记本触控板由 Synaptics 制造，通常使用 PS/2 接口。默认情况下（即在没有任何特殊驱动程序的情况下），它们通常可以模拟常规的 PS/2 鼠标，即可以报告手指移动和按钮点击，但仅此而已。要实现我们通常在笔记本电脑上用到的多指手势，我们需要自己实现。

## 功能清单

> [!NOTE]
> 由于我日常使用的是 Windows 系统，所以我只实现了 Windows 系统下的鼠标功能，其他系统暂未测试。

- [x] 鼠标移动
- [ ] 按动左下角的区域作为鼠标左键，按动右下角的区域作为鼠标右键（封壳之后再实现，目前按起来比较困难）
- [x] 轻触作为点击
  - [x] 单指轻触作为鼠标左键
  - [x] 两指轻触作为鼠标右键
  - [x] 三指轻触作为鼠标中键
  - [x] 屏蔽手掌误触
- [x] 滚动
  - [x] 两指上下滑动来作为垂直方向的滚动
  - [x] 两指左右滑动来作为水平方向的滚动
- [ ] 三指手势
  - [ ] 三指左右移动来切换应用（通过发送 Alt + Tab 实现）
  - [ ] 三指上下移动来显示桌面或回到应用（通过发送 Win + Tab 和 Win + D 实现）
- [x] 轻触一下，然后移动手指来实现拖拽
- [ ] 放大和缩小
- [ ] 休眠模式

## 编译

该程序通过 PlatformIO 编译，你可以通过 VSCode 安装 PlatformIO 插件来编译。项目已经配置好了 `platformio.ini` 文件，你只需要打开项目文件夹，然后点击 PlatformIO 的 `Build` 按钮即可编译。

## 贡献

欢迎提交问题和拉取请求来改进项目。

## 感谢

[@delingren 的 synaptics_touchpad 项目](https://github.com/delingren/synaptics_touchpad)

[@T-vK 的 ESP32-BLE-Mouse 项目](https://github.com/T-vK/ESP32-BLE-Mouse)

## 许可证

该项目基于 MIT 许可证，详细信息请参阅[LICENSE](LICENSE)文件。