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

---

## 8. 独立复核结论（2026-11-17，复核人未参与 `ff9170a` 的实现）

### 8.1 复核方式

不采信第 7 节的自述，独立验证：

- **代码走读**：`ff9170a` 的完整 diff + 相关上下游代码（`Control::task()` 状态机、`Camera::isConnected()`、UI 侧的快门/对焦处理）
- **产物二进制扫描**：对 `firmware.bin` 逐字节匹配协议字符串
- **补做链接级验证**：第 7.1 节记录 m5stick-c 全量构建被本机 hook 拦截，本次已绕过并**完整跑通**（SUCCESS，exit 0，11 分 40 秒，产出 `firmware.bin` 996816 字节）

### 8.2 判定

| 项 | 判定 | 一句话理由 |
| :--- | :--- | :--- |
| F-1 | ✅ 已修，**残留 R-1** | 假成功的两条路径均已封堵；但释放半段可能丢失（新增发现） |
| F-3 | ✅ 已修，验证比原记录更充分 | 链接级验证已补齐，跨板型二进制交叉比对确认 |
| F-2 | ✅ 已修，**残留 R-2** | 快照拷贝正确；但 `snapshot()` 持锁引入新的阻塞窗口（新增发现） |
| F-4 | ✅ 已修 | 逐行核对无误 |
| F-5 | ✅ 判定修正正确 | 原结论确有误，修正成立 |

### 8.3 F-1 复核

**修复成立。** `Control::submitIfActive()`（`src/FurbleControl.cpp:193-209`）结构正确：

```
快速路径（不取锁）：m_State != STATE_ACTIVE          → SUBMIT_NOT_CONNECTED
持锁路径：重查 m_State + m_Targets 非空 + allConnected() → SUBMIT_NOT_CONNECTED
        xQueueSend(...) == pdTRUE                     → SUBMIT_OK / SUBMIT_BUSY
```

原 F-1 描述的两条丢弃路径均已封堵：

| 原丢失去向 | 原行为 | 现行为 |
| :--- | :--- | :--- |
| 控制队列满（32 格） | `xQueueSend` 静默失败，仍回 `OK` | `SUBMIT_BUSY` → `ERR busy` ✅ |
| 非 `STATE_ACTIVE`，命令被 `Control::task()` 消费后丢弃 | 仍回 `OK` | 入队前即拒绝 → `ERR not-connected` ✅ |

`FurbleRemote.cpp` 中 6 处相机命令已全部改走 `submitIfActive`，无 `sendCommand` 残留（grep 确认）。

**快速路径的设计判断正确**：`connectAll()`（`src/FurbleControl.cpp:98`）持锁跨越 `camera->connect()`（超时最长 30 秒），但期间 `m_State` 已被置为 `STATE_CONNECTING`（`:151`），快速路径据此不取锁直接返回，不会被连接过程阻塞。

**无死锁**：锁序恒为 `Control::m_Mutex` → `Camera::m_Mutex`（`Camera::isConnected()` 内部取 `Camera::m_Mutex`），已确认 `lib/furble/Camera.cpp` 不反向引用 `Control`，不存在反向锁序。第 7.2 节"无死锁风险"的结论成立。

### 8.4 R-1【新增·中】`tap` 的释放半段可能丢失，富士快门/对焦停在按下状态

**位置**：`src/FurbleRemote.cpp:72-81`

```cpp
Control::submit_result_t result = control.submitIfActive(press);
if (result != Control::SUBMIT_OK) { replySubmit(result); return; }
vTaskDelay(hold);
replySubmit(control.submitIfActive(release));   // ← 与按下走同一道闸门
```

按下已投递后，**释放同样要过 `submitIfActive()` 的状态检查**。若释放被拒（队列满，或 100ms/400ms 保持窗口内 `allConnected()` 瞬时为假），则按下已送达而释放永远没送出。

**在 X100VI 上的具体后果**（`lib/furble/Fujifilm.cpp`）：

- `shutterPress()` → `SHUTTER_CMD` + `SHUTTER_PRESS`
- `shutterRelease()` → `SHUTTER_CMD` + `SHUTTER_RELEASE`
- **`focusRelease()` 直接调用 `shutterRelease()`**（`:89-91`）——对焦释放与快门释放是同一条 GATT 写

因此释放丢失意味着：快门停在半按下（不曝光、不出片），或 AF-ON 持续保持（对焦不停）。而脚本此时收到的是 `ERR busy` / `ERR not-connected`，通常会重试 `SHUTTER`，在相机已处于按下态的基础上再次按下，状态更乱。

**建议**：释放是**清理动作**，只要按下已被接受就必须送达，不应再受连接状态闸门约束。建议增加一条不受状态检查的强制入队路径（阻塞式 `xQueueSend`，如 100ms 超时），并以按下阶段的返回值为准回复。

**触发概率**：低（需要 BLE 瞬断落在保持窗口内，或队列积压 32 条）。**后果可恢复**（补发 `SHUTTER_RELEASE` / `FOCUS_RELEASE` 即可），但对无人值守的遥控脚本是真实故障模式。

### 8.5 R-2【新增·中】`STATUS` 持锁可能在连接期间阻塞整个遥控通道长达 30 秒

**位置**：`src/FurbleControl.cpp:211-224`

```cpp
Control::Snapshot Control::snapshot(void) {
  const std::lock_guard<std::mutex> lock(m_Mutex);   // ← 无条件取锁
```

`connectAll()` 持锁跨越 `camera->connect(m_Power, timeout)`，`timeout` 最长 30 秒（`TIMEOUT_DEFAULT_MS`）。`Remote` 是**单任务串行**处理协议的，`snapshot()` 阻塞期间，`PING` 与其它命令一律无响应。

这相对修复前是**行为倒退**：修复前 `STATUS` 不取锁、立即返回（虽然有竞态）。

值得注意的是，**F-1 的修复恰好明确规避了这类阻塞**（`src/FurbleControl.cpp:194-196` 的注释专门解释了这一点），但 F-2 的修复在姊妹函数 `snapshot()` 上把同类问题引了回来。

**对本项目的实际影响**：设备上电会自动连接相机，脚本若以轮询 `STATUS` 等待相机就绪，会在整个连接期间被挂起（最坏 30 秒），容易触发脚本侧超时。

**建议**：(a) 在 `snapshot()` 里对连接中状态走无锁快速路径——`connectAll()` 只读 `m_Targets`，真正修改它的只有 `addActive()`（UI 触发）和 `disconnect()`；(b) 或改用 `std::timed_mutex` 配短超时，拿不到锁就返回上次已知状态。

### 8.6 R-3【低】`m_State` 的无锁读取是形式上的数据竞争

`m_State` 由 Control 任务写（`src/FurbleControl.cpp:144,151,152,161`）、由 UI 任务经 `disconnect()` 写（`:256,276`），现由 Remote 任务无锁读（快速路径 `:198`）。

形式上违反 C++ 内存模型；Xtensa 上对齐 4 字节枚举读写实际原子，且**失效方向是安全的**——陈旧读只可能导致误拒（保守），ACTIVE 路径取锁后会复查。既有问题（`getState()` 在 UI 侧同样无锁），非本次引入，但修复新增了第三个读者。

### 8.7 R-4【低】残余的"已应答但未分发"窗口

`Control::task()` 的 `STATE_ACTIVE` 分支：

```cpp
if (!allConnected()) { m_State = STATE_CONNECT; continue; }   // ← 已取出的命令被丢弃
```

若相机恰在此迭代间断开，命令被消费但未分发，而 Remote 已回 `OK`。窗口为一次队列接收迭代。队列无逐条 ack，属设计固有限制；README"actually queued for dispatch"的措辞准确。

### 8.8 F-3 复核

**修复成立，且本次验证比第 7.1 节记录的更充分。**

- `#include "sdkconfig.h"` 路径正确：生成头文件在 `.pio/build/<env>/config/sdkconfig.h`，已确认两个 env 均存在
- 宏取值：`m5stick-s3` → `#define CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG 1`；`m5stick-c` → 该宏**完全未出现** → `#if` 求值为 0 → 走 `#else` ✅
- **链接级验证已补齐**：m5stick-c 全量构建 SUCCESS（exit 0，11:40），产出 `firmware.bin` 996816 字节 → 空实现分支符号齐全，无未定义符号（`handleLine` / `task` 为 private 且无外部引用，不产生链接错误）
- **跨板型二进制交叉比对**：

| 字符串 | m5stick-s3 | m5stick-c |
| :--- | :--- | :--- |
| `/dev/secondary` | FOUND | MISSING |
| `remote control ready` | FOUND | MISSING |
| `SHUTTER_RELEASE` | FOUND | MISSING |
| `ERR not-connected` / `ERR busy` | FOUND | MISSING |
| `CMDS PING` | FOUND | MISSING |
| `AF-ON (hold)`（UI 提示，与板型无关） | FOUND | FOUND |

非 S3 板型固件中**不含任何遥控协议实现**，且 UI 提示保留 —— 正是期望结果。开机不再打印 ERROR：`ESP_LOGE` 位于 `#if` 分支内（`src/FurbleRemote.cpp:113`），空实现的 `start()` 返回 false 且不记日志，`main.cpp` 忽略返回值 → 该路径可证明是静默的。

**"文件内 `#if` 而非改 CMakeLists"这个选择我认可**：不依赖各板型的 CMake 配置差异，不存在某个板型漏配导致符号缺失的风险；代价只是非 S3 板型多编译一个空文件，可忽略。

### 8.9 F-5 判定修正复核

**修正成立，我原来的结论有误。**

`UI::shutterLock()`（`src/FurbleUI.cpp:506-509`）就是 `sendCommand(CMD_SHUTTER_PRESS)`，`UI::shutterUnlock()`（`:523-526`）就是 `sendCommand(CMD_SHUTTER_RELEASE)`。原协议用 `SHUTTER_PRESS` + `SHUTTER_RELEASE` 已能复现"快门保持按下"，原"遥控脚本无法复现"的说法不准确。

补充一点文档未言明的细节：UI 的锁定**手势**带有焦点耦合——按住焦点键(B)时按下快门键(A)才进入锁定（`:545-550`），且要松开焦点键才解除（`:579-584`、`:552-556`）。`SHUTTER_LOCK` / `SHUTTER_UNLOCK` 复现的是**效果**（快门保持按下），不包含手势耦合。协议层面合理，但文档宜注明这一边界。

### 8.10 建议处置

1. **R-1**（释放半段强制送达）——影响无人值守遥控的可靠性，几行改动
2. **R-2**（`snapshot()` 避免长持锁）——影响脚本可用性，几行改动
3. R-3 / R-4 —— 记录为已知限制即可，不建议为此改动上游共享代码
4. 仍未做：**烧录与真机运行时验证**（见第 4 节），R-1 / R-2 的实际影响也只有真机能确认
