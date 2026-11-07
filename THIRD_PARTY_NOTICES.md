# Third-Party Notices · 第三方资产声明

> 本文件为双语说明：中文在前，英文在后。英文用于对外发布，中文便于团队与国内读者核对。
> This notice is bilingual; English follows each Chinese section.

本项目基于轮趣科技（WHEELTEC）公开销售的硬件**独立开发**，与轮趣科技**没有官方隶属、赞助或背书关系**。

This project is **independently developed** using commercially available WHEELTEC hardware. The repository's MIT / CERN-OHL-P-2.0 / CC BY 4.0 licences apply **only** to original material explicitly marked as such; third-party hardware, software, and documents remain under their owners' terms. It is **not affiliated with, sponsored by, or endorsed by** 轮趣科技（东莞）有限公司 / WHEELTEC.

---

## 1. 硬件 / Hardware

**中文**：以下为轮趣科技产品，**不是本项目的原创设计**：

- 轮趣 R1 四驱小车底盘（淘宝购置），含底盘板、轮组与紧固件
- 轮趣 C30D V2.2 主控板（STM32F407VET6）
- 轮趣 MG310P20 编码减速电机（随底盘套件提供，实车铭牌）
- 12 V 2550 mAh 锂电池（随套件）
- JDY-33 兼容蓝牙 SPP 模块（第三方通用模块）
- 0.96" OLED（随 C30D 套件）

产品名称与商标归各自权利人所有。本仓库不包含厂家的 CAD、原理图、PCB 布局、固件、手册或厂家蓝牙 App，请从轮趣科技按其条款获取。

**English**: The following are WHEELTEC products and are **not original designs of this project**: the R1 4WD chassis (purchased from Taobao; plates, wheels, fasteners), C30D V2.2 controller (STM32F407VET6), MG310P20 encoder gear motors (as supplied), 12 V battery, JDY-33-compatible Bluetooth SPP module, and 0.96" OLED. Product names and trademarks remain with their owners. This repository does not include vendor CAD, schematics, PCB layouts, firmware, manuals, or the vendor Bluetooth app; obtain them from WHEELTEC under their terms.

---

## 2. 软件 / Software

### 2.1 本项目固件 / Project firmware

**中文**：`firmware/Fusion_OpenCar/` 的 `App/` 与 `Hardware/` 由团队**独立编写**，**没有在厂家示例工程上做二次开发**；仓库不包含厂家的 `CORE/`、`FWLIB/`、`SYSTEM/`、`WHEELTEC_BSP/` 或示例 `main.c`。

**English**: `App/` and `Hardware/` under `firmware/Fusion_OpenCar/` were **written by the team from scratch**, with **no secondary development based on vendor example projects**. The repository does not contain the vendor `CORE/`, `FWLIB/`, `SYSTEM/`, `WHEELTEC_BSP/`, or the example `main.c`.

### 2.2 STM32F4 标准外设库 / STM32F4 StdPeriph

> **发布前注意 / Pre-release note**：`Driver/StdPeriph/` 为 ST 的 StdPeriph V1.8.1 源码。当前文件保留了
> ST 的文件头（`@author MCD Application Team`），但**未附 MCD-ST Liberty SW License 的完整许可文本**。
> 再分发源码时应在仓库内附上该许可文本（或从发布包中移除 `Driver/StdPeriph/`，让使用者自行从 ST 获取）。
>
> The `Driver/StdPeriph/` sources keep ST's file headers but **lack the full MCD-ST Liberty SW License text**.
> Before publishing, include that license in the repository, or remove `Driver/StdPeriph/` and instruct users
> to obtain the library from ST.



**中文**：`Driver/StdPeriph` 为 ST 标准外设库，按 SLA0044 授权（全文 `Driver/LICENSE-ST.txt`）；`Driver/CMSIS` 为 CMSIS 文件，按 Apache-2.0 授权（全文 `Driver/LICENSE-CMSIS.txt`）。

**English**: `Driver/StdPeriph` is ST's STM32F4 standard peripheral library under SLA0044 (full text: `Driver/LICENSE-ST.txt`); `Driver/CMSIS` is under Apache-2.0 (full text: `Driver/LICENSE-CMSIS.txt`). 详细声明见 `firmware/Fusion_OpenCar/Driver/NOTICE-ST.md`。 See also `firmware/Fusion_OpenCar/Driver/NOTICE-ST.md`.

### 2.3 自研 Android App / Self-developed Android app

**中文**：`app/` 为团队**纯自研**的「收集车控制台」V3.2（`com.pxt.carcontrol`），源码与已签名 APK 均在仓库内；**不是厂家蓝牙 App**，也未内置第三方库。厂家 App 仅作兼容对照，不随仓库分发。

**English**: `app/` is the team's **fully self-developed** console V3.2 (`com.pxt.carcontrol`); both source and a signed APK are included. It is **not** the vendor Bluetooth app and bundles no third-party libraries. The vendor app is a compatibility reference only and is not redistributed.

### 2.4 开发工具 / Development tools

**中文**：Keil MDK / ARM Compiler、SolidWorks、Android Studio 均为商业工具，不随仓库分发。

**English**: Keil MDK / ARM Compiler, SolidWorks, and Android Studio are commercial tools and are not redistributed.

---

## 3. 文档与媒体 / Documentation and media

**中文**：以下内容不包含在本仓库中，版权归轮趣科技：R1 用户手册、C30D 手册与原理图、资源分配表、《单片机与 APP 通信协议》、厂家 App 截图与产品照片、厂家底盘/主控 CAD（含 `.stp`/`.SLDASM`）、嵌入了厂家模型的装配文件。项目报告与课程材料同样不包含，以避免公开个人信息与课程限制内容。若需再分发厂家材料，请先取得轮趣科技书面许可。

**English**: The following are **not** included and remain subject to WHEELTEC's copyright: R1 manual, C30D manuals/schematics/resource tables, the "MCU–APP communication protocol" document, vendor app screenshots and product photos, vendor chassis/controller CAD (including `.stp`/`.SLDASM`), and assembly files embedding vendor models. The project report and course materials are also excluded to avoid personal information and course-restricted content. Obtain written permission from WHEELTEC before redistributing any vendor material.

---

## 4. 下游署名 / Attribution for downstream users

**中文**（建议在 fork/衍生作品中保留）：

> 本项目使用轮趣科技公开销售的硬件独立开发。底盘与主控板为轮趣科技产品，不是本项目的原创设计。本项目的开源许可仅覆盖明确标注为原创的内容；第三方硬件、软件与文档仍受其权利人条款约束。本项目与轮趣科技没有官方隶属或背书关系。

**English**:

> This project is independently developed using commercially available WHEELTEC hardware. The chassis and controller are WHEELTEC products and are not original designs of this project. The project's open-source licenses apply only to original materials identified as such. Third-party hardware, software, and documentation remain subject to their respective owners' terms. This project is not officially affiliated with or endorsed by WHEELTEC.

---

## 5. 下架与更正 / Takedown and correction

**中文**：如权利人认为仓库中的某个文件超出了授权范围，请提交 Issue 或联系维护者，我们会尽快删除或替换。

**English**: If you are a rights holder and believe a file exceeds the permission you granted, please open an issue or contact the maintainer; the file will be removed or replaced promptly.
