# Quick Start · 从零到第一次遥控

> 目标：在 1–2 小时内完成装配、烧录、连接与第一次架空测试。
> 机械细节见 [../hardware/assembly.md](../hardware/assembly.md)，接线见 [wiring.md](wiring.md)。

---

## 0. 准备清单

**硬件**

- 淘宝购置的轮趣 R1 四驱小车底盘（实车电机为 4× MG310P20 编码减速电机）
- C30D V2.2 主控板（STM32F407VET6）
- 12 V 锂电池、TB6612FNG、5 V 叶片电机、JDY-33/HC-02 蓝牙模块、0.96" OLED
- 3D 打印件（见 [../hardware/](../hardware/)）与 M3 紧固件、轴承 605ZZ

**软件**

- Keil MDK 5 + ARM Compiler 5（烧录固件）
- Android 6.0+ 手机（安装 App）
- 可选：ST-Link、USB 转 TTL（调试）

---

## 1. 装配

1. 按厂家手册完成 R1 小车底盘与 C30D 的机械/电气基础连接。
2. 安装自研收球机构：左右收拢爪 → 三叶片 → 前壳体 → 后储球箱 → 支架。
3. 检查叶片与壳体间隙、紧固件、轴承同轴度；用手轻转叶片应无卡滞。

---

## 2. 接线

按 [wiring.md](wiring.md) 完成，重点：

| 连接 | 说明 |
|---|---|
| C30D `PC6` → TB6612 `PWMA` | 叶片调速 PWM（TIM8_CH1） |
| C30D `3V3` → TB6612 `STBY` | 使能 |
| C30D `5V` → TB6612 `VM` | 叶片电机电源 |
| TB6612 `AO1/AO2` → 叶片电机 | 转向按需对调 |
| 蓝牙模块 `TXD/RXD` → C30D `PD6/PD5` | 交叉连接，9600 |
| 电池 → C30D 电源输入 | 注意极性与电压 |

> 上电前用万用表确认 5 V 与 12 V 没有短路；首次上电架空车轮。

---

## 3. 烧录固件

1. Keil 打开 `firmware/Fusion_OpenCar/Fusion_OpenCar.uvprojx`。
2. Build（0 error），ST-Link 或厂家无线方式下载。
3. 上电后 OLED 应显示转速、档位、叶片占空比与电池电压。

详见 [../firmware/README.md](../firmware/README.md)。

---

## 4. 安装 App

1. 手机系统蓝牙中先配对模块（模块名以实物为准）。
2. 安装 `app/CarControl-v3.2.apk`（允许未知来源）。
3. 打开 App → 授予蓝牙权限 → 刷新 → 选择模块 → 连接。
4. 状态显示“已连接”后进入测试。

详见 [../app/README.md](../app/README.md)。

---

## 5. 首次测试（务必架空）

1. 按 App 的“停止”，确认四轮目标为 0。
2. 圆盘轻推“前”，确认四轮同向；逐个验证 8 个方向。
3. 松手，确认立即停止。
4. 按“底盘 ＋/－”，确认档位与 OLED 显示一致（第 3 档 400 r/min）。
5. 按“叶片 ＋/－”，确认占空比 70%→100% 变化，OLED 的 Y 值同步。
6. 落地，低速试跑，再做实球收集。

---

## 6. 常见问题

- 连不上：先在系统蓝牙配对；确认模块未被其他手机占用；重新上电。
- 方向反了：对调该电机两根线，或在 `app_chassis.c` 中调整符号。
- 叶片不转：检查 TB6612 的 PWMA/STBY/VM 与 5 V 供电。
- App 档位与车端不一致：确认固件为 v0.2（7 档 / 5% 步进），并按 App 的加/减重新对齐。

更多见 [troubleshooting.md](troubleshooting.md)。
