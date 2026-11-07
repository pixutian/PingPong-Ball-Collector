# Changelog · 版本记录

本项目遵循语义化版本；日期为打包日期。

## v0.2.0 — 2026-11-07

**新增**

- 完整固件工程 `firmware/Fusion_OpenCar/` 入库（Keil MDK，STM32F407VET6，StdPeriph）。
- 底盘 **7 档车速**：`400 ± 100 r/min`，中间档 400，越界回中间档；`X/Y` 升降档，200 ms 去抖。
- 叶片 **7 档调速**：`70%–100%`，**每档 5%**，上电 100%，中间档 85%；`T/U` 升降档。
- 自研 Android App **V3.2**（`com.pxt.carcontrol`）：圆盘摇杆、底盘/叶片档位条与加减键、
  停车、状态回传解析、运行日志；260 ms 指令队列避免固件去抖吞步。
- 文档：通信协议、快速上手、兼容性、故障排查全部更新；README 采用
  `README.md`（English）+ `README.zh-CN.md`（中文）双语结构。

**许可与合规**

- 补齐第三方许可全文：`Driver/LICENSE-ST.txt`（ST SLA0044，StdPeriph）、`Driver/LICENSE-CMSIS.txt`（Apache-2.0，CMSIS）；
  `Driver/NOTICE-ST.md`、`THIRD_PARTY_NOTICES.md` 与 README 同步更新。

**变更**

- 叶片档位由早期“直接 ±10% 占空比”改为固定 7 档、每档 5%。
- 底盘速度由 `SpeedSet 30–480` 改为 7 档 `100–700 r/min`。

**已知问题**

- 固件仍无通信超时停车（断开蓝牙后保持最后方向）。
- 叶片无转速反馈，App 显示的是占空比而非实测 RPM。
- App 未实现断线自动重连。

## v0.1.0 — 2026-11-03

- 首个开源整理版：机械设计（SLDPRT/STL）、BOM、装配与接线文档、通信协议、
  早期固件说明与自研 App（`com.example.hc02serial` 1.0）。
