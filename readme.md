# ESP32-BLE-Touchpad

这是一个基于ESP32的项目，目的是实现 Synaptics 触摸板的蓝牙化使用，使用PlatformIO进行构建和管理。

## 目录结构

```
.gitignore
.pio/
.vscode/
include/
lib/


platformio.ini


src/
test/
```

- `.gitignore`: Git忽略文件配置。
- `.pio/`: PlatformIO生成的构建文件夹。
- `.vscode/`: Visual Studio Code配置文件。
- `include/`: 包含头文件的目录。
- `lib/`: 项目依赖的库文件。
- `platformio.ini`: PlatformIO项目配置文件。
- `src/`: 源代码文件夹。
- `test/`: 测试代码文件夹。

## 依赖库

项目依赖以下库：

- Arduino-PS2-Mouse-Handler
- ESP32-BLE-Mouse

## 快速开始

1. 克隆仓库到本地：

    ```sh
    git clone <repository-url>
    ```

2. 使用PlatformIO打开项目：

    ```sh
    platformio init --board esp32dev
    ```

3. 编译并上传代码到ESP32：

    ```sh
    platformio run --target upload
    ```

## 文件说明

### [src/main.cpp](src/main.cpp)

这是项目的主文件，包含了主要的逻辑代码。以下是文件中的主要功能：

- 初始化任务和看门狗定时器。
- 在主循环中重置看门狗定时器。

```cpp
NULL,           // 任务句柄
      1               // 在核心1上运行
  );

  // 将当前运行的核心（通常是核心0）添加到看门狗
  esp_task_wdt_add(NULL);
  // bleMouse.begin();
}

void loop()
{
  // 主循环喂狗
  esp_task_wdt_reset();

  // 可以在这里添加其他非关键任务
  delay(1000);
}
```

## 贡献

欢迎提交问题和拉取请求来改进项目。

## 许可证

该项目基于MIT许可证，详细信息请参阅[LICENSE](lib/Arduino-PS2-Mouse-Handler-main/LICENSE)文件。
