# Fusion OpenCar

四轮差速小车的整车控制程序，基于 STM32F407VET6 + 标准外设库。手机 APP 通过蓝牙
（USART2 + JDY-33 模块）下发摇杆方向与档位指令，板端在 200Hz 控制节拍里完成编码器
采集、四路速度环 PI 运算和 PWM 输出，同时驱动 OLED 显示运行状态并回传数据给 APP。

## 功能概览

- 四轮差速底盘：左列 A/C、右列 B/D，八方向遥控（前进 / 后退 / 四个斜向 / 原地左右转）
- 7 档速度循环调节（400 ± 100 r/min），到顶/到底后回到中间档；上电默认中间档 400
- 叶片电机（分机电机）同样 7 档循环调速，每档 5% 占空比（70%–100%）
- 四路独立增量式 PI 速度环，200Hz 控制频率
- 九轴姿态（ICM-20948 + AK09916），加速度计 + 陀螺仪互补滤波解算 roll / pitch / yaw
- 128×64 OLED 实时显示目标转速、实际转速、档位、舵机角、电池电压、调试参数
- 电池电压采样与电量百分比换算
- 一路独立的分机电机（TIM8_CH1）常转输出

## 目录结构

```
Fusion_OpenCar/
├─ App/                     业务逻辑层
│   ├─ app_config.h         整车参数（传动比、档位、PI、PWM 时序、电池）
│   ├─ app_chassis.c/.h     档位管理、方向指令、速度环 PI、电机输出
│   ├─ app_link.c/.h        蓝牙帧解析、模块应答过滤、数据回传
│   ├─ app_main.c           初始化流程、主循环、200Hz 控制节拍
│   └─ stm32f4xx_it.c       Cortex-M4 内核异常
├─ Hardware/                板级驱动层
│   ├─ board.h              引脚 / 定时器 / 串口资源映射表
│   ├─ clock.c/.h           1ms 系统节拍与延时
│   ├─ io.c/.h              指示灯、蜂鸣器、按键
│   ├─ motor.c/.h           四路电机 H 桥 PWM、舵机、分机电机
│   ├─ encoder.c/.h         四路编码器四倍频计数
│   ├─ adc.c/.h             电池电压采样
│   ├─ serial.c/.h          调试串口与蓝牙串口
│   ├─ i2c_soft.c/.h        GPIO 模拟 IIC
│   ├─ imu.c/.h             九轴传感器读写与姿态解算
│   ├─ imu_reg.h            ICM-20948 寄存器表
│   ├─ screen.c/.h          SSD1306 屏幕驱动与排版
│   └─ font.h               6×12 ASCII 点阵字库
├─ Driver/                  ST 官方资料（保持原样，版权归 STMicroelectronics）
│   ├─ CMSIS/               内核头文件、启动文件、system_stm32f4xx.c
│   └─ StdPeriph/           标准外设库（inc / src）
├─ Objects/                 Keil 编译输出（hex / axf / 日志）
└─ Fusion_OpenCar.uvprojx   Keil MDK 工程
```

## 硬件资源

| 功能 | 引脚 / 外设 |
| --- | --- |
| A 轮 PWM | TIM10_CH1 (PB8) / TIM11_CH1 (PB9) |
| B 轮 PWM | TIM9_CH1 (PE5) / TIM9_CH2 (PE6) |
| C 轮 PWM | TIM1_CH2 / TIM1_CH1 (PE9 / PE11) |
| D 轮 PWM | TIM1_CH4 / TIM1_CH3 (PE13 / PE14) |
| 舵机 | TIM12_CH2 (PB14 / PB15)，中位 90° |
| 叶片电机 | TIM8_CH1 (PC6)，7 档占空比可调 |
| 编码器 A/B/C/D | TIM2 (PA15/PB3)、TIM3 (PB4/PB5)、TIM4 (PB6/PB7)、TIM5 (PA0/PA1) |
| 蓝牙 | USART2 (PD5/PD6)，9600 |
| 调试串口 | USART1 (PA9/PA10)，115200 |
| OLED | GPIOD 11=D/C、12=RST、13=SDA、14=SCL |
| 九轴 IMU | 软件 IIC，GPIOB 10=SCL、11=SDA，器件地址 0x68 |
| 电池采样 | ADC1_IN8 (PB0)，分压比 11 |
| 指示灯 / 蜂鸣器 | PE8 / PA8 |

## 控制逻辑

1. **节拍**：TIM7 每 5ms 进一次中断（200Hz），依次做 IMU 采样与姿态解算、指示灯翻转、
   电池采样标志、按当前方向指令刷新四轮目标转速、执行速度环并输出 PWM。主循环约 100Hz，
   负责电池换算、屏幕刷新和 APP 回传。
2. **档位**：档位号 0~6，目标转速 = `GEAR_BASE_RPM + (档位 - GEAR_CENTER) * GEAR_STEP_RPM`，
   默认 100/200/300/400/500/600/700 rpm。档位只存序号，不存累加值，因此不会出现
   "按住按键速度一直涨"的问题；越界时回落到中间档，形成循环。
3. **叶片电机档位**：0~6 档对应占空比 40/50/60/70/80/90/100%，第 3 档（70%）为中间档，
   到顶或到底后再按一次回到中间档。档位同样只存序号，按下即按新占空比重写 `TIM8->CCR1`，
   所以不会连续累加。上电默认停在第 6 档（100% 满速）。
4. **速度环**：增量式 PI（`Kp=5.0`，`Ki=1.6`），输出限幅 ±16700；右侧 B/D 编码器读回取负号，
   使四路转速都以同一方向为正。
5. **通讯**：帧格式 `{...}$`。方向键为单字节（0x41~0x48 对应 1~8），`X`/`Y` 底盘升/降档，
   `T`/`U` 叶片电机升/降档（两类按键都用 200ms 去抖，一次按下只调一档），
   `I`/`J`/`K` 切换 APP 页面。回传帧 `{A:a:b:c:d}$`（两轮转速、电量、航向）、
   `{B:p:r:y}$`（姿态角）、`{C:p1:p2}$`（调试参数）。

## 编译

1. 用 Keil MDK（ARMCC V5.06）打开 `Fusion_OpenCar.uvprojx`
2. 直接 Build，输出在 `Objects/`，生成 `Fusion_OpenCar.hex`
3. 器件选型 STM32F407VETx，宏定义 `STM32F40_41xxx,USE_STDPERIPH_DRIVER`

## 常改参数

全部集中在 `App/app_config.h`，两处最常动的已经用 `★` 标出来了：

| 参数 | 含义 |
| --- | --- |
| `GEAR_BASE_RPM` | **小车初始速度**（上电默认车速 = 中间档目标转速） |
| `GEAR_STEP_RPM` | 每档之间的转速差 |
| `AUX_GEAR_STEP` | **叶片电机每档间距**（默认 10% 占空比） |
| `AUX_GEAR_TOP_DUTY` | 叶片电机最高档占空比（默认 100%） |
| `AUX_GEAR_BOOT` | 叶片电机上电所在档位（默认 6 = 100%；改 3 则上电 70%） |
| `GEAR_COUNT` / `GEAR_CENTER` | 档位总数与中间档序号 |
| `TURN_RATE_SCALE` | 转向时目标转速的放大倍数 |
| `SPEED_PI_KP` / `SPEED_PI_KI` | 速度环 PI 参数 |
| `MOTOR_GEAR_RATIO` / `ENCODER_PULSE_PER_REV` | 减速比与编码器线数，决定转速换算 |
| `SERVO_TRAVEL_DEG` | 180° 或 270° 舵机 |

### 两个常改点的代码位置

**初始速度**

1. `App/app_config.h` → `GEAR_BASE_RPM`（唯一改点，400 即中间档 400rpm）
2. `App/app_chassis.c` → `chassis_init()` 复位档位、`chassis_gear_change()` 用
   `GEAR_BASE_RPM + (档位 - 3) * GEAR_STEP_RPM` 换算当前车速

**叶片电机转速**

1. `App/app_config.h` → `AUX_GEAR_STEP`（每档 10%）、`AUX_GEAR_TOP_DUTY`（最高档 100%）、
   `AUX_GEAR_BOOT`（上电档位，默认 6 = 100%）
2. `App/app_chassis.c` → `aux_gear_to_duty()` 档位换算、`aux_gear_change()` 升/降档与循环、
   `chassis_init()` 上电套用初始档位
3. `App/app_link.c` → `T`/`U` 键的 200ms 去抖（`s_aux_key_tick`）
4. `Hardware/motor.c` → `s_aux_duty`（当前占空比）、`aux_motor_init()`（上电写入 TIM8->CCR1）、
   `aux_motor_set_duty()`（按档位重写比较值）
5. 比较值换算：`CCR1 = 占空比 * (AUX_PWM_ARR + 1) / 100`，100% → 10000（满占空比）

## 版权说明

- `App/`、`Hardware/` 为本项目自行编写的代码，遵循仓库根目录的 LICENSE。
- `Driver/CMSIS/`、`Driver/StdPeriph/` 来自 STMicroelectronics 的 STM32F4 标准外设库与
  CMSIS，版权归 STMicroelectronics 所有，按原样保留，仅用于本工程编译。
