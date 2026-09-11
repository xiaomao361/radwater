# 开发 Radwater

仓库只包含钓鱼游戏。代码直接位于根目录下的 `src/`、`include/`，原 PocketFishing 的三次游戏提交保留为独立历史；其他个人构想不在此仓库。

## 目录

| 目录 | 职责 |
| --- | --- |
| `src/game.cpp` | 版本化生成器、短收线、事件选择、防重复、存档编解码 |
| `src/main.cpp` | Cardputer 按键、屏幕、声音、SD 与阅读逻辑 |
| `src/view.cpp` | 240×135 软件绘图、安静岸边与椅子开场，设备与主机共用 |
| `src/lore.cpp`、`src/events.cpp` | 原创鱼/物品故事、批注、随机事件 |
| `include/journal.h`、`include/notebook.h` | 收藏与独立手记的追加式存储 |
| `tests/` | ASan/UBSan、模拟存储、C++/Python 兼容样本与渲染 |
| `tools/` | 字体子集、测试、预览、只读日志工具和打包 |
| `assets/` | 首页预览；可从同一绘图代码重新生成 |
| `firmware/` | 当前可下载应用镜像及 SHA256 |
| `releases/` | 版本清单；历史 PocketFishing 名称保留用于校验旧包 |

`.venv/`、`.pio/`、`build/`、`dist/`、真实 `*.pfj`/`*.pfn` 和凭据均不跟踪。`tests/fixtures/legacy-v1.pfj` 是合成测试样本，不是玩家存档。

## 完整构建

从仓库根目录执行；Python 3.10+，Clang 或 GCC，C++17。首次获取依赖需要网络。

```sh
set -e
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
mkdir -p build dist
.venv/bin/python tools/make_font.py
sh tools/test.sh > build/test-results.txt 2>&1
.venv/bin/pio run > build/firmware-build.log 2>&1
.venv/bin/python tools/previews.py
.venv/bin/python tools/package.py
```

普通编译可直接 `pio run`，字体子集已跟踪。修改中文或生成预览时运行 `make_font.py`，首次下载 Noto Sans SC；保留其许可证和源哈希。

`package.py` 检查完整测试标记、构建结果、ESP32-S3 镜像分段校验及内嵌 SHA256，生成 app-only BIN、ZIP、安装说明与清单。已有同版本 BIN 若字节不同会拒绝覆盖；需要采用新的版本号。构建路径、工具环境可能影响镜像哈希。

发布时将新清单复制到 `releases/vX.Y.Z.json`，将当前镜像与校验清单放到 `firmware/`，更新首页链接、CHANGELOG 和预览。旧版本记录不改写。

## 数据约定

显示名可改，`/PocketFishing/catches-v1.pfj` 与 `/PocketFishing/reading-v1.pfn` 继续保留。更名不是存档迁移。生成器 v1/v2/v3 的历史行为有回归样本；新版本修改规则时显式版本化。不要自动截断、清空或覆盖坏档。

源码测试与编译不能代替真实设备验收。主机生成的图是渲染预览，不能标成设备截图。当前只明确支持 Cardputer ADV，未把普通 Cardputer 列为已验证目标。

## 椅子开场

`Game::arrivalAge` 和 `shoreIdle` 仅为本局显示状态，不写入存档、不消耗 RNG。开场由 1.2 秒展开和 1.4 秒坐下组成，输入立即跳过，继续处理原动作。`quietShore` 直接在既有 RGB565 帧缓冲中绘制，不加载概念图或视频，没有新增屏幕缓冲；窗口常亮，倒影和水纹缓慢变化。问候在 2.6–7 秒显示并淡出，提示在 7–12 秒出现；之后只留水岸。操作后提示显示 4 秒。

`dist/arrival.gif` 是同一状态机和绘图函数产生的 15 秒放大预览，`rest-240x135.png` 为原分辨率。ASan/UBSan 覆盖三个场景、四种收藏保存状态和动画阶段。设备帧率、按键实际手感仍需真机验收。
