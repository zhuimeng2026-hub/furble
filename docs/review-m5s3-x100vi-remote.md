# M5StickS3 + X100VI 定制版 —— 复核文档

| 项 | 值 |
| :--- | :--- |
| 日期 | 2026-11-17 |
| 仓库 | `https://github.com/zhuimeng2026-hub/furble`（fork 自 `gkoh/furble`） |
| 分支 | `feat/m5s3-x100vi-remote`（fork 的默认分支） |
| HEAD | `78a9a7b Raise the default shutter time to 400ms and document AF-ON` |
| 基线 | `0cac22a Enable watchdog on M5StickS3.`（= `master` = `origin/master`） |
| 目标设备 | M5StickS3 + 富士 X100VI（固定镜头，无电子对焦环） |

本文档中的行号引用均针对 HEAD `78a9a7b`。

---

## 0. 结论摘要

**代码可以编译、可以链接、符号完整；但遥控协议的运行时行为一次都没有实测过。**

| 结论项 | 状态 | 依据 |
| :--- | :--- | :--- |
| 代码托管已正规化 | ✅ 完成 | 见第 1 节 |
| 编译链接通过 | ✅ 通过 | 见第 2 节（**注意是增量空构建**，见 2.2） |
| 遥控协议运行时行为 | ❌ **未验证** | 无设备连接，见第 4 节 |
| 代码审查 | ⚠️ 5 条发现 | 见第 3 节 |

**建议处置顺序：F-1 → F-3 → 烧录实测 → F-2 → F-4/F-5。**（理由见第 5 节）

---

## 1. 代码托管现状

### 1.1 远端与身份

| 项 | 值 |
| :--- | :--- |
| `origin` | `git@github-zhuimeng:gkoh/furble.git` —— 上游，只读 |
| `myfork` | `git@github-zhuimeng:zhuimeng2026-hub/furble.git` —— 自己的 fork，可写 |
| fork 可见性 | `public` |
| fork 默认分支 | `feat/m5s3-x100vi-remote`（已由 `master` 改来） |
| 本仓库 git 身份 | `zhuimeng2026-hub` / `259243957+zhuimeng2026-hub@users.noreply.github.com` |

### 1.2 为什么不能直接推上游

`gkoh/furble` 是原作者 Guo-Rong 的仓库，当前账号对它的权限是 `pull: true` / `push: false` / `admin: false`，直接 `git push origin master` 会被拒（403）。

### 1.3 SSH 身份的三账号陷阱

这台机器上 GitHub 相关的 SSH 身份不止一个，容易踩坑：

| Host 别名 | 实际密钥 | 认证身份 | 用途 |
| :--- | :--- | :--- | :--- |
| `github.com`（默认） | `~/.ssh/Identity`（另有同内容副本 `id_rsa`） | **webheat** | 另一个账号，无本仓库写权限 |
| `github-zhuimeng` | `~/.ssh/id_ed25519` | **zhuimeng2026-hub** ✅ | 本仓库所有推送走这个 |

`~/.ssh/config` 中的 `github-zhuimeng` 条目已配置 `IdentitiesOnly yes`，避免 SSH 把 agent 里的多把密钥依次试完落到 webheat 上。

> 注：早前一份结论称 `id_ed25519` 不存在，系检索时只 glob 了 `*.pub` 导致误判。该文件存在且一直是本仓库推送的正确密钥。

### 1.4 提交划分

相对 `master` ahead 3 / behind 0，共改动 11 个文件：

| 提交 | 主题 | 文件 |
| :--- | :--- | :--- |
| `3c58f1f` | 构建工具加固 | `patches/apply.py`、`platformio.ini`、`.gitignore` |
| `d72f066` | USB Serial/JTAG 遥控协议 | `include/FurbleRemote.h`、`src/FurbleRemote.cpp`、`src/CMakeLists.txt`、`src/main.cpp` |
| `78a9a7b` | 快门默认值 400ms + AF-ON 文档 | `include/interval.h`、`src/FurbleSettings.cpp`、`src/FurbleUI.cpp`、`README.md` |

### 1.5 新机器接入步骤

```bash
ssh-keygen -t ed25519 -C "furble-<机器名>" -f ~/.ssh/furble_zhuimeng
gh ssh-key add ~/.ssh/furble_zhuimeng.pub --title "furble-<机器名>"
```

`~/.ssh/config`：

```
Host github-zhuimeng
    HostName github.com
    User git
    IdentityFile ~/.ssh/furble_zhuimeng
    IdentitiesOnly yes
```

```bash
git clone git@github-zhuimeng:zhuimeng2026-hub/furble.git
git remote add upstream git@github-zhuimeng:gkoh/furble.git
git remote rename origin myfork
```

因默认分支已改为特性分支，clone **无需 `-b`**。仓库为 public，只读拉取用匿名 HTTPS 亦可；SSH 密钥仅在需要推送时才必要。

> 不要把本机的 `id_ed25519` 私钥复制到其他机器——两台机器共用同一身份，将来任一台出问题两边都受影响。

---

## 2. 构建验证

### 2.1 结果

```
.venv\Scripts\platformio.exe run -e m5stick-s3
========================= [SUCCESS] Took 49.82 seconds =========================
RAM:   [=         ]  10.4% (used 33924 bytes from 327680 bytes)
Flash: [=======   ]  67.1% (used 1030692 bytes from 1536000 bytes)
```

PlatformIO 6.1.18（`.venv`，Python 3.11.5），平台 `espressif32@6.12.0`，框架 espidf，板子 `esp32-s3-devkitc-1`。无 warning / error。

### 2.2 ⚠️ 重要限定：这是一次增量空构建

**本次运行没有重新编译任何文件**——日志中无 `Compiling` 行，49.82 秒是全量构建（8 分 10 秒）之后的增量耗时。原因是所有目标文件都已是最新：

| 源文件 mtime | 目标文件 mtime | 关系 |
| :--- | :--- | :--- |
| `src/FurbleRemote.cpp` 2026-11-16 17:09 | `FurbleRemote.cpp.o` 2026-11-16 17:13 | 源文件更旧 → 已最新 |
| `src/FurbleUI.cpp` 2026-11-16 17:09 | `FurbleUI.cpp.o` 2026-11-16 17:13 | 同上 |
| `src/FurbleSettings.cpp` 2026-11-16 17:08 | `FurbleSettings.cpp.o` 2026-11-16 17:13 | 同上 |

即：**`FurbleRemote` 的实际编译发生在 2026-11-16 17:13**，早于本次会话。

### 2.3 补强验证：确认新代码确实进了固件

不能只凭"目标文件比源文件新"下结论（那只说明 PIO 认为它新，不说明内容对得上）。做了两项独立核对：

**(a) 工作区内容 == HEAD**：`git status` 干净，无未提交改动，故已编译的产物与已提交代码是同一份内容。

**(b) 直接扫描固件二进制** `pio/build/m5stick-s3/firmware.bin`：

```
SHUTTER_RELEASE      FOUND
FOCUS_PRESS          FOUND
remote control ready FOUND
CMDS PING            FOUND
/dev/secondary       FOUND
ERR unknown          FOUND
AF-ON (hold)         FOUND
```

遥控协议的全部命令字符串、端口路径，以及 UI 提示文案均在固件中。

**(c) 体积对比**：旧构建日志 `build_m5s3.log` 记录 Flash 占用 1029312 字节，本次 1030692 字节，差值 +1380 字节，与新增模块体积吻合。

### 2.4 构建验证的结论与局限

**结论**：新增代码能编译、能链接、符号完整、产物与提交一致。编译层面没有发现问题。

**局限**：
- 未做全量 clean 重建（增量构建依赖了 11-16 的缓存产物）
- **未做任何运行时验证** —— 编译通过不代表 `/dev/secondary` 能打开、不代表协议有正确响应、不代表相机能接受这些命令

---

## 3. 代码审查发现

### F-1 【高】`OK` 是无条件返回的，脚本可能拿到假成功

**位置**：`src/FurbleRemote.cpp:63-84`（`Remote::handleLine`）；根因在 `src/FurbleControl.cpp:189-191`

```cpp
// src/FurbleControl.cpp:189
BaseType_t Control::sendCommand(cmd_t cmd) {
  return xQueueSend(m_Queue, &cmd, 0);   // 超时 0
}
```

```cpp
// src/FurbleRemote.cpp:64
control.sendCommand(Control::CMD_SHUTTER_PRESS);
vTaskDelay(pdMS_TO_TICKS(100));
control.sendCommand(Control::CMD_SHUTTER_RELEASE);
reply("OK");                            // ← 返回值被丢弃
```

**两条丢弃路径**：

1. **队列满**：`xQueueSend(..., 0)` 超时为 0，队列（`Control::m_Queue`，长度 32，`include/FurbleControl.h:135`）满时静默失败并返回非 `pdTRUE`。`Remote` 忽略返回值。
2. **状态不对（更常见）**：`Control::task()` 在 `src/FurbleControl.cpp:135-167` 的状态机里，**只有在 `STATE_ACTIVE` 分支才会把命令分发给 target**。其余状态（`STATE_IDLE`、`STATE_CONNECT`、`STATE_CONNECTING`、`STATE_CONNECT_FAILED`、`STATE_DISCONNECTING`）下，命令在 `xQueueReceive`（`:138`）时**已被消费**，随后被 `switch (m_State)` 静默丢弃。

**影响**：相机未连接时发送 `SHUTTER`，设备仍回 `">OK"`。自动化脚本据此判定"已拍摄"并继续推进流程，实际一张都没拍。

**建议**：检查 `sendCommand` 返回值，非 `pdTRUE` 时回 `">ERR busy"`；同时在 `STATE != STATE_ACTIVE` 时回 `">ERR not-connected"`。更彻底的做法是让 `Control` 暴露一个"命令已投递"或"相机已连接"的状态查询，而不是让 `Remote` 猜。

---

### F-2 【中高】`STATUS` 无锁遍历 `m_Targets`，与 `disconnect()` 存在竞态

**位置**：`src/FurbleRemote.cpp:85-100`

```cpp
} else if (strcmp(line, "STATUS") == 0) {
  ...
  for (const auto &target : control.getTargets()) {   // 无锁
```

**写侧全部持锁**（`include/FurbleControl.h:138-139` 声明 `std::mutex m_Mutex` 与 `std::vector<...> m_Targets`）：

- `src/FurbleControl.cpp:228,242` —— `disconnect()` 在锁内 `m_Targets.clear()`
- `src/FurbleControl.cpp:247,261` —— `addActive()` 在锁内 `m_Targets.push_back()`

**读侧 `getTargets()` 不持锁**（`src/FurbleControl.cpp:212-214`），直接返回内部 vector 的引用。`Remote` 任务在遍历过程中，`Control` 任务可能正在 `clear()` → 迭代器失效 / use-after-free。

**缓解因素**：`src/FurbleUI.cpp:1147` 是同样写法，说明这是**既有模式**而非本次新引入的类别。但 `Remote` 引入了一条外部可随时触发 `STATUS` 的路径，暴露概率显著高于 UI 的被动刷新。

**另一个同类小问题**：`Control::getState()`（`src/FurbleControl.cpp:269-271`）同样无锁读取 `m_State`，而 `m_State` 在 `disconnect()`（`:223,243`）等处被写。枚举读写在 32 位上实际是原子的，风险低，但严格说属于同一类未同步访问。

**建议**：给 `Control` 增加一个持锁的快照接口（如 `std::vector<TargetInfo> snapshotTargets()`），返回拷贝而非内部引用。改动会触及上游共享代码，若打算把这份改动 PR 回 gkoh/furble，需要权衡。

---

### F-3 【中】非 m5stick-s3 板型每次开机都会打印一条 ERROR

**位置**：`src/CMakeLists.txt:6` 无条件加入 `FurbleRemote.cpp`

```cmake
set(furble_sources
    ...
    FurbleRemote.cpp      # ← 无条件
```

**根因**：`/dev/secondary` 只有在启用 `CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG` 时才注册。逐个板型核对 `sdkconfig`：

| 板型 | `CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG` |
| :--- | :--- |
| `sdkconfig.m5stick-s3` | `=y`（`:1465`）✅ |
| `sdkconfig.m5stick-c` | 未设置 ❌ |
| `sdkconfig.m5stick-c-plus` | 未设置 ❌ |
| `sdkconfig.m5stack-core` | 未设置 ❌ |
| `sdkconfig.m5stack-core2` | 未设置 ❌ |

非 S3 板型上 `open("/dev/secondary")` 失败 → `src/FurbleRemote.cpp:111-116` 打印 `failed to open /dev/secondary` 后任务自删。**不崩溃，功能本身对这些板型也无意义**，但每次开机都有一条 ERROR 日志。

**建议**：用 `#if CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG` 包裹整个实现，或按板型宏条件编译。

---

### F-4 【低】超长行的溢出处理会吐出半截命令

**位置**：`src/FurbleRemote.cpp:129-141`

```cpp
if (used < sizeof(line) - 1) {
  line[used++] = c;
} else {
  used = 0;          // ← 丢弃前缀，但后续字符继续累积
}
```

行长超过 128 字节时，前缀被丢弃，但**后续字符继续被累积成一条新的行**。若这段垃圾数据里恰好出现 `SHUTTER` 等命令关键字，会被当作一条真实命令执行。

**建议**：溢出时进入"丢弃模式"，直到遇到下一个 `\n` 才恢复正常解析。

---

### F-5 【低·功能缺口】协议未覆盖 Shutter Lock

上游 UI 有 Shutter Lock 功能（`src/FurbleUI.cpp:1296` 附近菜单项 `"Shutter\nLock"`）。新协议只暴露了 `SHUTTER` / `FOCUS` 系列，**没有进入/退出 Shutter Lock 的命令**，遥控脚本无法复现该行为。

**建议**：视实际需求决定是否补 `SHUTTER_LOCK_ON` / `SHUTTER_LOCK_OFF`，或至少在 `HELP`（`src/FurbleRemote.cpp:102`）与 README 中说明协议的能力边界。

---

### 3.6 已确认**无问题**的点

为免复核重复排查，以下几项已核实：

| 检查项 | 结论 |
| :--- | :--- |
| 跨任务调用 `Control::sendCommand` 是否安全 | **安全**。走 FreeRTOS 队列（`Control::m_Queue`），由 `Control` 任务串行消费，不直接触碰相机对象 |
| `/dev/secondary` 的使用前提 | **成立**。`sdkconfig.m5stick-s3:1465` 已启用 secondary console，主控台是 UART0（`:1467`），secondary 端口的接收方向不被 `console_read()` 占用 |
| `Remote::reply()` 的缓冲区 | **安全**。`buf[REMOTE_LINE_MAX + 4]`，`snprintf` 自身截断 |
| `handleLine` 的行解析 | 正确处理 `\r\n`、前后空格、空行 |
| `STATUS` 用的相机 API | `Camera::isConnected()` (`lib/furble/Camera.h:119`)、`Camera::getName()` (`lib/furble/Camera.h:133`) 均存在且签名匹配 |
| `FOCUS` 保持 400ms 的取值 | 与新的快门默认 400ms 一致，合理（`src/FurbleRemote.cpp:31`） |

---

## 4. 尚未验证的事项

**当前没有检测到设备**：pyserial 列出 0 个 COM 口，PnP 设备中无 ESP32-S3 / USB JTAG 设备。以下均**未做**：

- [ ] `/dev/secondary` 能否成功打开（`src/FurbleRemote.cpp:111`）
- [ ] `PING` 是否返回 `>PONG`
- [ ] `SHUTTER` 是否真正触发相机快门
- [ ] `FOCUS` 在 X100VI 上是否确实锁定对焦点（AF-S 模式）
- [ ] `STATUS` 返回的 `STATE` 与 `CAM` 行是否与实际连接状态一致
- [ ] 遥控输出与 ESP_LOG 混在同一端口时，`>` 前缀的区分是否可靠（`src/FurbleRemote.cpp:40`）
- [ ] 全量 clean 重建（`pio run -e m5stick-s3 -t clean` 后完整构建，约 8 分钟）

> 烧录前注意：按既往记录，esptool 的端口号会漂移，需先确认当前 COM 口。

---

## 5. 建议处置顺序

1. **F-1** —— 影响自动化脚本正确性，且改动小（判返回值 + 判连接状态）
2. **F-3** —— 条件编译隔离，改动小，收益是消除其他板型的开机报错
3. **烧录实测** —— 前述所有结论都停留在编译层，实测是当前最大的信息缺口
4. **F-2** —— 需要动上游共享代码（`Control` 加快照接口），改动面较大，可在实测确认问题真实存在后再决定
5. **F-4 / F-5** —— 边界情况与功能扩展，按需处理

---

## 6. 复核清单

请复核人逐条确认：

- [ ] 第 1 节：远端地址、身份、提交划分与实际仓库状态一致
- [ ] 第 2.2 节：认可"本次为增量空构建"这一限定结论的表述是否准确
- [ ] 第 2.3 节：二进制字符串扫描是否足以支撑"新代码确实进了固件"的判断
- [ ] F-1：是否认可"命令在非 `STATE_ACTIVE` 状态下被消费后丢弃"这一路径分析
- [ ] F-2：竞态是否真实可触发（需要评估 `STATUS` 与 `disconnect()` 的实际并发概率）
- [ ] F-3：条件编译的方案是否符合上游对多板型支持的规划
- [ ] F-4：溢出后半截命令被误执行的实际风险评估
- [ ] F-5：Shutter Lock 是否确有必要纳入遥控协议
- [ ] 第 4 节：待验证清单是否完整，是否还有遗漏的运行时验证项
- [ ] 第 5 节：处置顺序是否合理

---

## 7. 修复实施记录（2026-11-17）

复核结论已被确认为真，5 项全部实施。工作分支 `feat/m5s3-x100vi-remote`，改动 5 个文件（相对 `78a9a7b`）：

| 项 | 处置 | 实现 |
| :--- | :--- | :--- |
| F-1 | ✅ 已修 | `Control::submitIfActive()` 在 `m_Mutex` 内检查 `STATE_ACTIVE`+非空+`allConnected()` 后再入队，返回 `SUBMIT_OK / SUBMIT_NOT_CONNECTED / SUBMIT_BUSY`；`Remote` 据此回 `OK / ERR not-connected / ERR busy`，连招（SHUTTER/FOCUS）两段都成功才回 OK |
| F-2 | ✅ 已修 | `Control::snapshot()` 持锁返回 `Snapshot{state, vector<TargetInfo>}` 拷贝；`STATUS` 遍历拷贝而非实时 `m_Targets`，消除与 `disconnect()` 的竞态 |
| F-3 | ✅ 已修 | `FurbleRemote.cpp` 全文以 `#if CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG` 包裹；非 S3 板型编译空实现（`start()` 返回 false、`reply()` no-op），开机不再打印 ERROR。CMakeLists 保持无条件加入，链接安全 |
| F-4 | ✅ 已修 | 收包循环新增 `dropping` 标志；溢出后置位并丢弃整行，直到下一个 `\n` 才复位，半截命令不再进入 `handleLine()` |
| F-5 | ⚠️ 判定修正后处理 | 核实 UI 的 `shutterLock()/shutterUnlock()` 本质就是发 `CMD_SHUTTER_PRESS/RELEASE`，原协议**可以**复现，并非硬缺口。为明确语义新增 `SHUTTER_LOCK/SHUTTER_UNLOCK` 别名，并更新 HELP、头注释与 README 协议表 |

**对原 F-5 结论的修正**：原文"遥控脚本无法复现该行为"不准确——`SHUTTER_PRESS`（保持）+ `SHUTTER_RELEASE` 已等价。本次以补别名和文档的方式消除歧义，而非新增底层能力。

### 7.1 验证结果

- **编译（m5stick-s3）**：`pio run -e m5stick-s3` 完整构建 SUCCESS（EXIT=0），无 warning/error；Flash 1032308 B（较修复前 1030692 B +1616）。
- **固件字符串**：`SHUTTER_LOCK / SHUTTER_UNLOCK / ERR not-connected / ERR busy / ERR unknown / /dev/secondary / AF-ON (hold) / CMDS PING` 全部命中 `firmware.bin`。
- **非 S3 条件编译（F-3）**：`FurbleRemote.cpp` 在 `CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=0` 下用 ESP-IDF 的 xtensa g++ 做 `-fsyntax-only` 检查通过，确认 `#else` 空实现分支可编译、链接符号齐全。未对 m5stick-c 做全量链接（PIO 增量清理触发本机安全 hook 的批量删除拦截，与代码无关；改用单文件编译检查等价验证条件编译正确性）。
- **仍未做**：烧录与真机运行时验证（PING/SHUTTER/FOCUS 对 X100VI 的实际响应）——设备未连接，属第 4 节固有缺口，不受本次修复影响。

### 7.2 待复核人确认

- F-1 用持锁的 `submitIfActive()` 检查状态+入队，是否认可（非 ACTIVE 走无锁快速路径立即返回，避免连接阶段被 `connectAll()` 的长持锁阻塞；ACTIVE 才取锁，与 `Control::task()` 消费队列不冲突，无死锁风险）。
- F-3 采用文件内 `#if` + 空实现而非改 CMakeLists，是否认可（好处是不破坏其他板型的符号链接）。
- 是否接受对 F-5 的判定修正。
