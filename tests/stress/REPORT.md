# 稳定性测试活动报告

日期：2026-11-27（本机 Fedora 45）
分支：`stability/autotest-20261127`

本报告记录本次本地压力/模糊测试活动中发现并修复的问题，以及相应的测试证据。本次工作没有新增功能特性。

## 发现并修复的问题

### 1. 加载 VST2 乐器时的 GUI 死锁

**现象：** 渲染或 GUI 加载包含 ZynAddSubFX 的工程时会间歇性永久卡死（观察到 4/10 次；在负载下后来达到 6/6 次）。

**根因：** `VestigeInstrument::loadFile()` 在构造 `VstInstrumentPlugin` 期间持有 `m_pluginMutex`。VST2 插件启动过程会处理 Qt 事件循环。此时周期性的 `InstrumentTrack` latency 定时器调用 `VestigeInstrument::latency()`，在同一个非递归互斥锁上阻塞。

**修复：**
- 先构造插件，再在 `m_pluginMutex` 保护下发布它；
- 让 `latency()` 非阻塞（`tryLock`，忙时返回 0）；
- 对 `VstEffect` 应用同样的“非阻塞 latency + 锁外构造”模式。

**证据：** 保存的复现工程（`corpus/regressions/vestige-load-deadlock.mmp`）现在 10/10 次都在约 1 秒内完成，不再卡死。

### 2. VST2 插件宿主握手可能永久阻塞

**现象：** 插件宿主连接后从不发送 `IdHostInfoGotten` 或 `IdInitDone` 时，LMMS 会无限等待。

**修复：** `RemotePluginBase::waitForMessage()` 现在带有截止时间（宿主信息/初始化 15 秒，默认 30 秒，音频处理 5 秒）。超时后会使传输层失效，从而把插件视为加载失败。同时加固了协议解析，限制过大的字符串长度/字符串数量，并移除了一个 `std::string` 越界写入。

**证据：** 一个只连接然后休眠的假 `NativeLinuxRemoteVstPlugin64` 不再导致卡死；LMMS 会报告插件加载失败，并在约 16 秒后退出。

### 3. 配置升级会静默清空音频/MIDI 设置

**现象：** 加载任何 `configversion < 3` 的配置都会清空 `audioengine/audiodev`、`mididev`、`samplerate`、`framesperaudiobuffer` 和 `hqaudio`，并丢失旧的 `MidiAlsaRaw/Device` / `app/displaydbv` 值。

**根因：** `ConfigManager::value()` 的默认值是 `""`（一个空但**非 null** 的 `QString`），而升级代码用 `isNull()` 判断旧键是否存在。于是每个缺失的键都被当成“存在”，用空值覆盖了有效设置。

**修复：** 在升级函数中使用显式的 `QString()` 默认值。

**证据：** `ConfigUpgradeTest` 覆盖了音频引擎设置保留、旧 `mixer` 配置迁移、`displaydbv` 重命名和 `MidiAlsaRaw` 重命名。

### 4. NotePlayHandle 缓存泄漏其底层内存块

**现象：** ASan/LSan 在退出时报告泄漏 79,872 字节；每次缓存扩容还会泄漏上一块内存。

**修复：** 跟踪所有由 `malloc` 分配的 note 内存块，并在 `NotePlayHandleManager::free()` 中释放。

### 5. Base64 采样/状态辅助函数

- `base64::decode<T>()` 按 `size / sizeof(T)` 分配元素，却拷贝了 `size` 字节。长度不是元素大小整数倍时会造成堆缓冲区溢出。已改为只拷贝完整元素，并返回截断后的字节数。
- `FreeBoy`、`Watsyn`、`MultitapEcho` 会泄漏解码后的缓冲区（Watsyn 和 MultitapEcho 每次加载分别泄漏 3 块和 1 块）。
- 这些插件（以及 DynamicsProcessor、WaveShaper、Xpressive、Vibed）在拷贝 `graphModel::length()` 个 float 前没有检查解码数据长度。已增加长度校验；过短或损坏的状态会被忽略，而不是越界读取。

**证据：** `Base64Test`，以及使用 FreeBoy 的示例工程的 ASan 运行。

### 6. CLI 渲染泄漏 renderer，并在退出时崩溃

**现象：** ASan 报告 dummy 音频设备及相关对象泄漏；第一次清理尝试还导致崩溃，因为 `RenderManager` 在 `QApplication` 之后才销毁，而 `restoreAudioDevice()` 会向 VST 插件槽发送 `sampleRateChanged`。

**修复：** CLI 模式保留 `RenderManager*`，在 `app->exec()` 之后、删除 `QApplication` 和销毁引擎**之前**删除它。这样可以恢复原始音频设备并释放文件编码器。

**证据：** 内置插件和真实插件工程的 ASan 渲染均无泄漏。

### 7. 非有限/溢出的数值属性导致 SIGFPE 和挂起

**现象：** 一个包含 `timesig_numerator="nan"`、`timesig_denominator="inf"` 的工程在 `MidiClip::beatClipLength()` 中触发 SIGFPE；一个 `bpm="nan"` 的工程在 60 秒内无法完成渲染。

**根因：** `std::clamp(NaN, min, max)` 会保留 NaN。因此 `IntModel`/`FloatModel` 接受了非有限值，随后这些值进入 `TimePos::ticksPerBar()` / `Song::framesPerTick()`，产生零或非有限除数。

**修复：**
- `AutomatableModel::fittedValue()` 在钳制前把 NaN/Inf 替换为模型的初始值。
- `TimePos::ticksPerBar(TimeSig)` 把分子/分母限制在 1..32，`setTicksPerBar()` 永不存储 0。
- `ticksPerBeat()` 使用 `max(1, numerator)`。

**证据：** 两个复现工程都位于 `corpus/regressions/`（`nonfinite-meter-sigfpe.mmp`、`nonfinite-tempo-hang.mmp`），现在可以正常渲染。`ValidationTest` 覆盖非有限模型值和零拍号；`fuzz_numbers.py` 运行了 400 个 release 用例和 30 个 ASan 用例，无崩溃/挂起。

### 8. FIFO/设备节点作为采样源会卡死加载器

**现象：** 工程的 `audiofileprocessor src` 指向命名管道（`mkfifo`）时，LMMS 打开采样会永久阻塞；`/dev/zero` 也会被当作采样路径接受。

**修复：** `SampleDecoder::decode()` 现在要求必须是普通文件（`QFileInfo::isFile()`）才会打开，因此 FIFO、字符设备、目录和套接字都会被立即拒绝。同样的检查也应用于工程文件本身、配置文件的加载/保存，以及音频导出路径的写入。

**证据：** FIFO 复现工程现在约 2 秒内完成并给出警告；`nonregular-sample-devzero.mmp` 回归工程由 `run.py --replay` 重放。

### 9. 零长度 pattern 在创建轨道视图时卡死 GUI

**现象：** 50 次 ASan GUI 运行中有 1 次（工程含 0 长度 pattern 和负数拍号）始终无法退出。主线程停在 `TrackContentWidget::updateBackground()` 中，无限绘制网格线。

**根因：** 当最大 clip 长度为 0 时，PatternEditor 会把 `pixelsPerBar` 设为 0。随后 `TrackContentWidget::updateBackground()` 使用 `x += ppb * resolution`，而 `ppb == 0`，形成无限循环。

**修复：** `TrackContainerView::setPixelsPerBar()` 至少钳制为 1；`updateBackground()` 在 `ppb` 非正时提前返回，并清理非正/NaN 的 snap 尺寸。

**证据：** `corpus/regressions/gui-zero-length-pattern.mmp` 现在约 4 秒内启动并退出；之后的 ASan GUI 矩阵全部通过。

### 10. 超大 pattern 步数 / clip 长度会冻结 GUI

**现象：** 两个数值边界工程在 8 秒内无法完成 GUI 启动。GDB 显示主线程位于 `MidiClipView::paintEvent()`：一个工程有 2,147,483,647 个 pattern steps（数十亿个 step 按钮），另一个有超大 clip 长度（数十亿条小节线）。

**修复：**
- `MidiClip` 把加载的步数限制在 1..1024（由音符驱动的长度同步也做同样限制）。
- `MidiClipView::paintEvent()` 使用安全的非零 clip 长度，并在 `pixelsPerBar < 1` 时跳过节线循环（此时线间距已小于一个像素）。

**证据：** `gui-huge-clip-length.mmp` 和 `gui-unbounded-pattern-steps.mmp` 现在约 3 秒内启动并退出。

### 11. AudioFileProcessor 播放时间歇性 SIGSEGV

**现象：** 一个数值模糊测试用例（非有限自动化值）在 `AudioFileProcessor::playNote()` 中触发 SIGSEGV，当时它把 `_working_buffer + offset` 传给 `Sample::play()`。

**根因：** 损坏/非有限的时间或自动化值可能产生负的 note offset 或负的每周期帧数。播放代码只处理了 offset 过大的情况，没有处理负值，而且 `framesLeftForCurrentPeriod()` 可能返回负数。

**修复：** 在 `NotePlayHandle::play()` 中把负 offset 钳制为 0，并把每周期帧数钳制为至少 0。这保护所有乐器，而不仅是 AudioFileProcessor。

**证据：** 复现工程位于 `corpus/regressions/afp-note-offset-crash.mmp`；修复后分别运行了 20 次 release 和 10 次 ASan，均正常。原始崩溃是间歇性的，因此保留为模糊测试回归用例。

### 12. 深层嵌套 XML 导致工程加载器崩溃

**现象：**
- 100,000 层嵌套元素 → 在 `QXmlSimpleReaderPrivate::parseElement()` 中触发 SIGSEGV（栈溢出）；
- 10,000 层嵌套元素 → 20 秒内无法完成。

**修复：** `DataFile::loadData()` 现在会在把原始/解压后的 XML 交给 Qt 递归解析器之前扫描元素深度，拒绝深度超过 1000 的输入。同时拒绝声称解压后大小超过 512 MB 的 qCompress 头（解压炸弹防护）。

**证据：** 1k/10k/100k 层文件以及压缩后的 100k 层文件现在都会在远小于 1 秒内被优雅拒绝（“project is empty”）。`ValidationTest::deeplyNestedProjectIsRejected` 覆盖该防护。

## 测试证据

### 单元/集成测试

- Release ctest：**29/29 通过**（包括新增的 `Base64Test`、`ConfigUpgradeTest` 和深层 XML 测试）。
- 使用 `QT_QPA_PLATFORM=offscreen` 的 ASan ctest：**29/29 通过**（已抑制 Qt/Wayland/fontconfig 等外部库泄漏）。

### 随机渲染测试（release）

| 运行 | 用例数 | 结果 |
|---|---|---|
| tiny，插件比例 0.3 | 150 | 148 通过，3 个超时（已修复死锁），1 个静音（master 音量被改） |
| medium，插件比例 0.4 | 100 | 98 通过，2 个静音（主通道音量为 -1） |
| tiny + 效果器，插件比例 0.6 | 100 | 100 通过 |
| tiny，插件比例 0.5 | 300 | 300 通过 |
| medium + 效果器，插件比例 0.5 | 100 | 100 通过 |
| medium + 效果器，插件比例 0.5 | 200 | 200 通过 |
| medium + 效果器，插件比例 0.5 | 100 | 100 通过 |

### ASan 测试

- 30 个带真实乐器/效果器的 tiny 用例：30 通过（其中 1 个静音由被改坏的 clip 长度导致；测试框架现在把变异工程的静音视为警告）。
- ASan 下 320 个畸形 XML 用例，release 下 500 个：0 崩溃，0 sanitizer 报告。
- 400 个数值边界工程（NaN/Inf/溢出属性）：0 崩溃，0 挂起（拍号/BPM 修复后）。
- 300 个损坏采样文件（其中 100 个在 ASan 下）：0 崩溃。
- 真实插件探活（Dexed、Zyn、Surge LV2/VST3、OB-Xf、sfizz、Pianoteq）：全部可加载并渲染，ASan 无泄漏。
- 50 次带 Dexed 状态的加载/保存循环：ASan 无泄漏。

### GUI

- 50 次离屏 GUI 启动/加载/退出循环：全部通过（SIGINT 退出码 3）。
- 20 次带真实插件/效果器的 GUI 循环：全部通过。
- 50 次带插件的 ASan GUI 循环：49 通过，1 个超时由零长度 pattern 网格循环导致（已修复）；复现工程现在通过。
- 200 次 release GUI 启动/加载/退出循环：200 通过。
- 30 分钟 ASan GUI 空闲浸泡（含混合真实插件工程）：RSS 稳定在 993–996 MB，SIGINT 干净退出（3），无 LMMS 泄漏。
- GUI 启动/加载/退出已纳入 `gui_smoke.py`。

### 浸泡测试

- 复制构建上的 2 小时 tiny 浸泡：1,781 个用例，RSS 稳定在约 221–224 MB（平均 222.9 MB），无崩溃/挂起。5 个失败是测试框架误报（缺少根节点 `type`、被改坏的 clip 长度/master 音量），均已在生成器中处理；最终重跑干净。
- 最新构建上的 45 分钟最终浸泡：826 个用例，0 失败，平均 RSS 223.6 MB（最大 399 MB）。
- 最新构建上的 1 小时最终浸泡：1,169 个用例，0 失败，平均 RSS 223.1 MB（最大 563 MB）。

### 插件隔离

渲染期间杀死插件宿主进程的测试结果：

- VST3 远程 DSP（`lmms-vst3-host`）：LMMS 退出码 0，输出部分 WAV。
- VST2（`NativeLinuxRemoteVstPlugin64`）：LMMS 退出码 0。
- LV2 远程（`LMMS_LV2_REMOTE=1`，`lmms-lv2-host`）：LMMS 退出码 0。

未观察到 LMMS 崩溃或挂起。

## 容量观察

- 100 个乐器轨道 × 500 个音符：约 65 秒，约 390 MB RSS。
- 一个小节内 10,000 个同时音符：约 9 秒。
- 一个小节内 50,000+ 个同时音符：CPU 受限，超过 5 分钟；不是崩溃。
- 100 次加载/保存循环：约 3.4 秒，最大 RSS 47 MB（1000 次循环无增长）。
- 64 个 Dexed VST3 实例：约 16 秒，2.1 GB RSS，419 个文件描述符。
- 128 个 Dexed VST3 实例：约 37 秒，4.2 GB RSS，803 个文件描述符；无崩溃。
- 深层 XML：在解析器递归前被拒绝。

## 遗留 / 后续工作

- 本机没有 TSAN（未安装 `libtsan` 运行时，且无 root 权限），因此未能检查并发竞态。
- 完整 GUI 交互需要 X server 以及 `xdotool`/`wmctrl`；目前只自动化了离屏启动/加载/退出。
- 损坏 MIDI 文件导入按设计会弹出模态对话框；自动化模糊测试需要额外的对话框自动关闭器。
- LADSPA/Carla 第三方插件未纳入默认矩阵。

## 复现

```bash
# 死锁复现工程
QT_QPA_PLATFORM=offscreen ./build/lmms render \
  tests/stress/corpus/regressions/vestige-load-deadlock.mmp -o /tmp/out.wav

# 深层 XML 回归测试
./build/tests/ValidationTest

# 配置升级回归测试
./build/tests/ConfigUpgradeTest

# base64 回归测试
./build/tests/Base64Test

# 随机渲染 + 真实插件
python3 tests/stress/run.py --build build --profile tiny --cases 50 \
  --seed-start 1 --plugin-ratio 0.5

# 真实插件探活
python3 tests/stress/plugins/probe_plugins.py --build build --include-optional
```
