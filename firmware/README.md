# Firmware · Fusion_OpenCar 整车控制程序

> 完整 Keil 工程位于 [`Fusion_OpenCar/`](Fusion_OpenCar/)。本文说明编译、烧录、参数与协议。
>
> **代码来源**：本工程的全部控制代码（`App/`、`Hardware/`）由团队**独立编写**，没有在厂家示例工程基础上二次开发；`Driver/` 为 ST 官方 CMSIS/StdPeriph 库。
> 早期单文件 `original/yellow_motor.h` 仅作历史参考，当前工程已不使用它。

---

## 1. 平台与工具链

| 项目 | 内容 |
|---|---|
| 主控 | 轮趣科技 C30D V2.2（STM32F407VET6） |
| 开发环境 | Keil MDK 5 / ARM Compiler 5（AC5） |
| 库 | STM32F4 标准外设库（`Driver/StdPeriph`） |
| 工程文件 | `Fusion_OpenCar.uvprojx` |
| 控制周期 | TIM7，5 ms（200 Hz） |
| 蓝牙 | USART2（PD5/PD6），9600 8N1，JDY-33 / HC-02 兼容 SPP |
| 调试串口 | USART1（PA9/PA10），115200 |

---

## 2. 工程结构

```
Fusion_OpenCar/
├── Fusion_OpenCar.uvprojx      Keil 工程
├── App/                        业务逻辑
│   ├── app_config.h            整车参数（传动、档位、PI、PWM、电池）
│   ├── app_chassis.c/.h        档位管理、方向映射、速度环 PI、电机输出
│   ├── app_link.c/.h           蓝牙帧解析、模块噪声过滤、状态回传
│   ├── app_main.c              初始化、主循环、200 Hz 节拍、OLED
│   └── stm32f4xx_it.c          Cortex-M4 异常
├── Hardware/                   板级驱动
│   ├── board.h                 引脚/定时器/串口资源表
│   ├── clock.c/.h              1 ms 节拍与延时
│   ├── motor.c/.h              四路 H 桥 PWM、舵机、叶片电机
│   ├── encoder.c/.h            四路编码器四倍频
│   ├── adc.c/.h                电池采样
│   ├── serial.c/.h             调试/蓝牙串口
│   ├── i2c_soft.c/.h           软件 IIC
│   ├── imu.c/.h                九轴姿态
│   ├── screen.c/.h + font.h    SSD1306 OLED
│   └── io.c/.h                 指示灯/蜂鸣器/按键
└── Driver/                     ST CMSIS + StdPeriph（第三方，原样保留）
```

> `Objects/`、`Listings/`、`*.uvguix.*` 为编译产物/IDE 元数据，已在 `.gitignore` 中排除。

---

## 3. 编译与烧录

1. 安装 Keil MDK 5 与 C30D 设备支持包（或直接使用 STM32F4 器件包）。
2. 打开 `Fusion_OpenCar.uvprojx`，确认 Target 为 STM32F407VET6、编译器为 AC5。
3. `Project → Build Target`，确认 0 error（`Driver/` 为 ST 原版，告警可忽略）。
4. 通过 ST-Link 或厂家无线烧录方式下载。
5. 上电自检：OLED 显示 `A/B` 目标与实际转速、档位、叶片占空比、电池电压。

> 若使用 AC6，需要处理部分 AC5 内联汇编/`__align` 写法；建议直接用 AC5 复现。

---

## 4. 关键参数（`App/app_config.h`）

| 参数 | 值 | 说明 |
|---|---|---|
| `MOTOR_GEAR_RATIO` | 30 | 减速比 |
| `ENCODER_PULSE_PER_REV` | 13 | 霍尔编码器线数 |
| `ENCODER_EDGE_MULT` | 4 | 四倍频 |
| `CONTROL_RATE_HZ` | 200 | 与 TIM7 5 ms 一致 |
| `GEAR_BASE_RPM` | 400 | 中间档（第 3 档）目标转速 |
| `GEAR_STEP_RPM` | 100 | 每档差 |
| `GEAR_COUNT` / `GEAR_CENTER` | 7 / 3 | 档位总数 / 中间档 |
| `SPEED_PI_KP` / `KI` | 5.0 / 1.6 | 四轮速度环 |
| `TURN_RATE_SCALE` | 1.5 | 转向目标转速放大（注释里的 0.7 为旧值，勿照抄） |
| `MOTOR_PWM_ARR` | 16799 | 底盘 PWM 满量程（≈10 kHz） |
| `AUX_GEAR_COUNT` / `CENTER` | 7 / 3 | 叶片档位 |
| `AUX_GEAR_STEP` | **5** | 每档 5% 占空比 |
| `AUX_GEAR_TOP_DUTY` | 100 | 最高档 |
| `AUX_GEAR_BOOT` | 6 | 上电 100% |
| `AUX_PWM_ARR` | 9999 | TIM8，约 16.8 kHz |

**档位换算**

- 底盘：`SpeedSet = 400 + (gear − 3) × 100` → 100…700 r/min
- 叶片：`duty = 100 − (6 − gear) × 5` → 70%…100%
- 两者越界（<0 或 ≥7）都回中间档 3；叶片中间档为 85%

---

## 5. 指令与回传

完整表格见 [../docs/communication.md](../docs/communication.md)。

| 指令 | 动作 |
|---|---|
| `A`–`H` | 8 方向（`g_link_dir = recv − 0x40`） |
| `S` | 停止（落到 `default`，四轮目标清零） |
| `X` / `Y` | 底盘升/降档（7 档循环，200 ms 去抖） |
| `T` / `U` | 叶片升/降档（7 档循环，200 ms 去抖） |
| `I` / `J` / `K` | 切换 APP 页面（兼容厂家 App） |
| `{Q:P}` / `{Q:W}` | 请求回传参数 / 请求保存 |

回传帧：`{A<rpmA>:<rpmB>:<电量>:<航向>}$`、`{B<pitch>:<roll>:<yaw>}$`、`{C<p1>:<p2>}$`。

### 5.1 可选：增加档位回传 `{D}`

APP V3.2 已支持解析，固件加 3 行即可让手机显示与车端完全一致：

```c
/* app_main.c：文件顶部增加缓冲区 */
static char s_frame_d[24];

/* report_to_app()：把 A/B 两帧交替改为 A/B/D 三帧 */
    } else if (showtime == 3) {
        sprintf(s_frame_d, "{D%d:%d}$",
                (int)aux_gear_duty(),   /* 0~100 */
                (int)chassis_gear());   /* 0~6 */
        link_send_text(s_frame_d);
    } else {
        showtime = 0;
    }
```

`aux_gear_duty()` / `chassis_gear()` 已在 `app_chassis.h` 中声明。

---

## 6. 实现要点

- **方向映射**：`App/app_chassis.c` 的 `switch (g_link_dir)`；轮 A/C 为左侧、B/D 为右侧。
- **速度环**：200 Hz 读取四路编码器，`RPM = ΔN × 200 × 60 / (30×13×4)`，增量式 PI 输出 PWM。
- **档位键去抖**：`GEAR_KEY_DEBOUNCE_MS = 200`，按住连发时车端只认第一次。
- **模块噪声过滤**：`module_noise_filter()` 丢弃 JDY-33 的 `+CONNECTING` / `CONNECTED` / `+DISC` 提示，避免被当成方向指令。
- **叶片**：TIM8_CH1（PC6）→ TB6612 PWMA；`CCR1 = duty × 100`（ARR=9999）。

---

## 7. 已知问题

1. **没有通信超时停车**：断开蓝牙后底盘保持最后方向；建议在 `chassis_control_step()` 中加入 500 ms 指令超时。
2. **任意字节都会改写 `g_link_dir`**：非方向字节会落到 `default` 停底盘，但叶片仍保持；不要依赖未知字节急停。
3. **叶片无转速反馈**：当前只能显示占空比，无法显示实际 RPM。
4. **`TURN_RATE_SCALE` 注释与值不一致**：实际 1.5，转向目标高于直行，需按实车整定。

---

## 8. 许可

- `App/`、`Hardware/` 中由本项目独立编写的代码：MIT（见仓库根 `LICENSE`）。
- `Driver/`（CMSIS、StdPeriph）版权归 STMicroelectronics，按原许可使用。
- 本工程不包含厂家 BSP 基线；`Driver/` 仅含 ST 官方库。
