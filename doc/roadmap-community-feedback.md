# LMMS 社区呼声与可实操功能路线图（长期参考）

> **性质**：信息收集 + 可实操性评估。**未修改代码、未构建、未提交。**
> **快照日期**：2026-11-27（GitHub API 实时拉取；投票数会变化，建议定期重跑文末"复现"命令更新）。
> **范围**："社区呼声"查的是**上游 LMMS/lmms**（非本 fork）；"可实操"以**本仓代码**为准。
> **仓库**：本文件位于 `doc/`；本 checkout 是 `lmms-modern` fork，`origin`→`zmwcmr/lmms-modern`，`upstream`→`LMMS/lmms`，本地 `develop` 比 `upstream/master`（含 #7878）领先 **235 commits**，已有分支 `feature/vst3` / `feature/vst-improvements` / `feature/ui-ux-improvements` 及 `doc/` 设计文档。

---

## 0. LMMS 是什么

**LMMS = Linux MultiMedia Studio**，开源跨平台 DAW（Qt，GPLv2+）。不是 LLM 评测框架。特性：song editor / pattern editor / piano roll / mixer、15+ 内置 synth、VST2 + SoundFont2 + LADSPA + LV2、MIDI 导入导出、track automation。上游 `master` 稳定 1.2.2、`1.3` milestone 82、`1.3+` 69、总 issue 量级 1.3k；README 要求**大 feature 先 file issue + Discord 讨论**。

---

## 1. 数据来源与证据等级

**已核实（GitHub API，2026-11-27）**：`search/issues?q=repo:LMMS/lmms+is:open&sort=reactions-+1`（按 👍 取前 50）；`label:"good first issue"`；单 issue 详情；Discussions #7653；milestones。**已核实上游 vs 本 fork**：`git show upstream/master:...` / `git ls-tree upstream/master` 对照。**社区**：lmms.io 论坛 Suggestions + t=34235（2021 "A bunch..."）+ VST3/CLAP 帖 + progress reports；Discord 不可索引未覆盖；Reddit r/LMMS 403 仅搜索摘要未完整核实。**推断**："可实操性/工作量"是基于读代码的判断，非维护者背书；已标注文件路径可复核。

---

## 2. 上游社区呼声最高清单（按 👍 降序）

### 2.1 插件格式 / 托管（最集中）

| 👍 | Issue | 内容 | 上游 | 本仓 |
|---|---|---|---|---|
| 34 | [#562](https://github.com/LMMS/lmms/issues/562) LV2 support | 正文空；57 评论 | open，但 **LV2 已内建** `WANT_LV2 ON` + `plugins/Lv2Effect/Instrument` | LV2 UI 已做，见下 |
| 31 | [#4715](https://github.com/LMMS/lmms/issues/4715) VST3 Support | 重开 #2352；Steinberg 停发 VST2 SDK | **open；上游只有 VST2** | **本 fork 原生 VST3** |
| 25 | [#6560](https://github.com/LMMS/lmms/issues/6560) CLAP support | free-audio/clap | open；无原生 | 仅 bundled Carla 间接；可复用 VST3 架构 |
| 10 | [#3277](https://github.com/LMMS/lmms/issues/3277) SFZ plugin | 目前靠 VST 加载 `.sfz` | open | 无原生；仓内 SFZero |
| 8 | [#5433](https://github.com/LMMS/lmms/issues/5433) VST2 Issues | meta | open meta | `plugins/Vst*`/`Vestige`；`feature/vst-improvements` |
| 5 | [#6707](https://github.com/LMMS/lmms/issues/6707) Implement Lv2 UI | marker | open | **本 fork SUIL X11 + 独立 `lmms-lv2-host`** |
| 0 | [#7475](https://github.com/LMMS/lmms/issues/7475) VST3 + **sidechain** | 绑定两诉求 | open | VST3 有；**通用 sidechain 输入无**（仅 LV2 端口） |

- 论坛："Is VST3 (or CLAP) support coming?"、"Suggestion: LMMS as a VST3 (or CLAP) for use in other DAWs"（t=37694；**另起项目，非 issue 级**）；Reddit 榜首 VST3+LV2+双位 [未完整核实]。
- **meta 治理（非功能）**：[#8478](https://github.com/LMMS/lmms/issues/8478) **No-AI policy** 34👍/34👎，**总 reactions 第 2**；meta/conversation/bot-skip；本文不评价该政策；论坛 t=38054 "Modernizing LMMS with AI (like Claude)?"。

### 2.2 工作流 / 界面

| 👍 | Issue | 内容 |
|---|---|---|
| 26 | [#4877](https://github.com/LMMS/lmms/issues/4877) Better Workflow | **meta 汇总**；"won't be closed by a quick patch" |
| 22 | [#4929](https://github.com/LMMS/lmms/issues/4929) Make developing more attractive | meta |
| 18 | [#1911](https://github.com/LMMS/lmms/issues/1911) LMMS redesign concept | 整体 redesign |
| 15 | [#3981](https://github.com/LMMS/lmms/issues/3981) JSON project format | `.mmpz` 带 offset 不通用；JSON+ZIP；本仓仍 XML |
| 12 | [#735](https://github.com/LMMS/lmms/issues/735) Collapsible Song Editor Groups | 可折叠轨道组 |
| 10 | [#3069](https://github.com/LMMS/lmms/issues/3069) Loop button more obvious | |
| 8 | [#5869](https://github.com/LMMS/lmms/issues/5869) XDG Base Directory | 本仓 `~/.lmmsrc.xml`，无 XDG |
| 8 | [#6614](https://github.com/LMMS/lmms/issues/6614) Qt 6 support | Qt5.15 EOL；experimental + `qt6` 分支；本 fork 有 `WANT_QT6 OFF` |
| 7 | [#2510](https://github.com/LMMS/lmms/issues/2510) hiDPI | + [#7542](https://github.com/LMMS/lmms/issues/7542)（5👍 scale options）；本仓仅 `AA_EnableHighDpiScaling`；**fork 审计 S1 像素硬编码** |
| 7 | [#4804](https://github.com/LMMS/lmms/issues/4804) Ctrl+drag auto-create automation track | |
| 6 | [#4949](https://github.com/LMMS/lmms/issues/4949) Effects chain solo | `EffectChain` 仅 `enabled` |
| 4 | [#5006](https://github.com/LMMS/lmms/issues/5006) Automation in piano roll | |
| 4 | [#5887](https://github.com/LMMS/lmms/issues/5887) multi-track piano roll | `GuiApplication::m_pianoRoll` 单例 |
| 4 | [#5669](https://github.com/LMMS/lmms/issues/5669) docking windows | |
| 5 | [#5361](https://github.com/LMMS/lmms/issues/5361) Sf2Player→Soundfont Player | good first issue |
| 7 | [#453](https://github.com/LMMS/lmms/issues/453) "Siloed" B/B editor | |
| 7 | [#5044](https://github.com/LMMS/lmms/issues/5044) Loop Launcher / live recorder | 关联 #4992 |
| — | [discussion #7653](https://github.com/LMMS/lmms/discussions/7653) What features do you want the most? | **multi midi editor 11 votes**；**snap to scale**；**hear other tracks**；theme；automation clamping；SF2 volume envelope（FluidSynth）；SlicerT clear slices（**#7850 已加，本仓 `SlicerTView.cpp`**）；1.3 已含部分 |
| — | **#5495 Workflow and general improvements** + **#4877** | 12+ 条列表（spacebar、play、windows、cut/copy/paste、effects、automation…）；维护者称多已有/1.3 规划 |

**2021 论坛 t=34235 未完成项**（与 issue 重叠；多数已部分存在）：MIDI 1:1、sample-track 内剪切/复制、effects drag&drop、**保存/加载 effect chain preset**（`EffectChain` 存项目 XML 但无独立 preset）、spacebar 全局 play、Esc、记住窗口尺寸/位置（[#7842](https://github.com/LMMS/lmms/issues/7842)）、fx chain on/off…；维护者回复见原帖。**导出 muted/分轨等已部分有**（`RenderManager::renderTracks`）。

### 2.3 音频引擎 / 混音 / 自动化

| 👍 | Issue | 内容 | 本仓 |
|---|---|---|---|
| 13 | [#1165](https://github.com/LMMS/lmms/issues/1165) Humanization/Randomization | timing(需 read-ahead)/volume/pitch；**upstream in progress** | 无现成 |
| 12 | [#1472](https://github.com/LMMS/lmms/issues/1472) Better MIDI support | meta；"mostly feature-complete" | `MidiImport/Export`；1.3 改进 |
| 6 | [#4510](https://github.com/LMMS/lmms/issues/4510) **Plugin delay compensation** | 参照 Ardour | **上游无；本 fork 已实现** `Mixer::updateLatencyCompensation`+`DelayLine`；`doc/pdc-design.md` |
| — | P6.3 sample-accurate automation | automation 对齐 sample | `doc/automation-timing-design.md`+`valueBuffer`+frame-offset MIDI；**本 fork 增量** |
| 4 | [#6666](https://github.com/LMMS/lmms/issues/6666) Freeze audio to sample track | bounce | 无；可复用 renderTracks + SampleTrack |
| 7 | [#3880](https://github.com/LMMS/lmms/issues/3880) Arpeggiator overhaul | bug+enh meta | 需拆子项 |
| 5 | [#3233](https://github.com/LMMS/lmms/issues/3233) ADSR Sf2/GigPlayer | FluidSynth built-in gen | `plugins/Sf2Player` 未暴露；`GigPlayer` |
| 5 | [#6516](https://github.com/LMMS/lmms/issues/6516) Monophonic + portamento | 仅 LB302 | `Instrument`/`NotePlayHandle`/`Lb302` |
| 4 | [#4162](https://github.com/LMMS/lmms/issues/4162) Background rendering | good first issue | `ProjectRenderer` 同步 |
| — | [#4589](https://github.com/LMMS/lmms/issues/4589) CLI render loop markers | milestone 1.3 | `src/core/main.cpp` |

### 2.4 采样器 / 格式 / 其它

- 7 👍 [#4179](https://github.com/LMMS/lmms/issues/4179) Zyn-Fusion；5 👍 [#4634](https://github.com/LMMS/lmms/issues/4634) Carla as effect（`CarlaRack/Patchbay` 已 effect）；4 👍 [#4309](https://github.com/LMMS/lmms/issues/4309) sample locating；4 👍 [#4922](https://github.com/LMMS/lmms/issues/4922) savefile layout；8 👍 [#3100](https://github.com/LMMS/lmms/issues/3100) linked patternblocks（**"linked"语义未完全核实**）；11 👍 [#4314](https://github.com/LMMS/lmms/issues/4314) **tracker view**（大）。

### 2.5 good first issue（已核实 API 输出）

[#4162] background rendering · [#5361] · [#5103] deselect all · [#7842] · [#5772] browser sort · [#2156] dir watching · [#1813] MIDI auto-detect · [#7767] SVG assets · [#3370] · [#5016] master pitch range · [#5533] · [#5703] · [#5748] · [#6327] · [#7814] · [#7824] · [#8038] · [#1132] · [#266] · [#4602] · [#6121] —— 以 API 为准，下文命令可重跑。

---

## 3. 本 fork 已有 / 正在做（别重复造轮子；宣称前核实）

| 社区诉求 | 本仓证据 | 仍缺 / 非目标 |
|---|---|---|
| VST3 #4715 | `WANT_VST3 ON`；`plugins/Vst3Base/Vst3Effect/Vst3Instrument`；`feature/vst3`；`doc/vst3-implementation-plan.md`（P6 complete） | note expression/MIDI2、multi-out、surround、32-bit VST3 Linux、preset browser parity、**跨平台打包 deferred**；sidechain 独立 |
| PDC #4510 | `src/core/Mixer.cpp` `updateLatencyCompensation` + `DelayLine` + `m_latencyByChannel/m_outputLatencyByChannel` + clamp `maxFrames` + saturation（15 hits；上游 0）；`doc/pdc-design.md` P6.2 | 需边界测试后再宣称完成；sample-accurate 见 P6.3 |
| sample-accurate automation | `doc/automation-timing-design.md`；`AutomatableModel::valueBuffer`；frame-offset MIDI | VST2 仍块级；LV2 control-port 块级（doc 承认） |
| LV2 UI #6707 | SUIL `ui:X11UI`；独立 `lmms-lv2-host`；`doc/lv2-plugin-ui.md`、`doc/lv2-host-design.md`；`LMMS_HAVE_SUIL` | `instance-access`→单进程；embedding 移除；XWayland |
| Qt6 #6614 | `WANT_QT6` experimental OFF | 做 CI→修 deprecated→翻默认 |
| hiDPI/UX #2510/#7542/#4877/#1911 | `doc/ui-ux-audit.md`（S1/S2+可复现 grep）、`doc/ui-ux-tasks.md`、`doc/ui-ux-gestures.md`、`feature/ui-ux-improvements` | **像素硬编码、无 design token、手绘控件、快捷键硬编码**为根因 |
| Scale **显示**≠snap | `PianoRoll::m_scaleModel`+`Chord::isScale`+`markSemiTone` | **snap-to-scale 需新增** |
| SlicerT clear | `plugins/SlicerT/SlicerTView.cpp`；#7850 | |
| LV2/VST2 host | `plugins/Lv2*`/`Vst*`/`Vestige`；`doc/vst-plugin-hosting.md` | |
| Stems/export | `ProjectRenderer`（WAV/FLAC/Ogg/MP3）；`RenderManager::renderTracks`；`ExportProjectDialog::Mode::ExportTracks` | freeze 需新增 |
| 录音 | `src/tracks/SampleTrack.cpp`；`src/core/audio/AudioSampleRecorder.cpp` | live/loop launcher 需新增 |
| Effect preset(XML) | `EffectChain::saveSettings/loadSettings` | 独立 preset 文件需新增 |
| 稳定性/测试 | `tests/`、`build-asan`、fuzz/stress commits、`doc/` stability report（中译） | |

---

## 4. 可实操性分档（推断；给出本仓落点）

定义"可实操"：边界清晰可拆小 PR · 不依赖不可控外部 · **本仓有落点文件** · 可用现有 tests/构建/fuzz-stress 验证 · 与 Qt GUI + `AutomatableModel` + `Mixer` + 插件框架 + XML 兼容。

### 4.1 小步快做（低风险独立 PR）

| 候选 | 信号 | 落点 | 备注 |
|---|---|---|---|
| Effects chain solo #4949 | 6👍 | `src/core/EffectChain.cpp`；参照 track/channel solo | 局部；可测旁路 |
| Ctrl+drag→auto automation track #4804 | 7👍 | SongEditor drop + `AutomatableModel`+`AutomationTrack/Clip` | 复用现有 |
| Snap notes to scale (#7653) | 社区票选 | `PianoRoll::m_scaleModel`+`Chord::isScale`+`quantization()` | 已有高亮 |
| Automation range clamp + guide (#7653) | 社区 | `AutomatableModel` min/max + `AutomationEditor` | |
| Loop button obvious #3069 | 10👍 | PianoRoll/SongEditor | 纯 UI |
| Deselect all #5103 | good first | selection | 极小 |
| Delete key song editor #1500 | | 对齐 piano roll | 先核实 |
| Remember window size/pos + tab #7842/forum | good first+社区 | `SubWindow`+`QSettings`/`ConfigManager` | |
| Rename Sf2Player #5361 | good first | `plugins/Sf2Player` | **项目文件兼容** |
| XDG Base Directory #5869 | 8👍 | `src/core/ConfigManager.cpp:742/748/782` `~/.lmmsrc.xml` | **迁移+fallback；Linux** |
| Save/load effect chain preset | 社区 | `EffectChain` XML | |
| FX drag&drop reorder | 社区 | `EffectChain`+GUI | |
| Background rendering #4162 | good first | `ProjectRenderer`/`RenderManager` | 线程隔离工程状态 |
| CLI render loop markers #4589 | milestone 1.3 | `src/core/main.cpp` | |
| ADSR Sf2Player #3233 | 5👍+社区 | `plugins/Sf2Player`；FluidSynth `fluid_synth_set_gen` | 序列化加字段 |
| Sample-track inner cut/copy/paste | 社区"We need that true" | `SampleClip`/`SampleTrack` | 先核实 |
| Spacebar global play / Esc | 社区+ #5495 | `MainWindow` keyboard | 处理焦点冲突 |
| Browser sort #5772 / dir watch #2156 | good first | `FileBrowser` | |
| SVG assets #7767 / design token | good first+S1 | `data/themes`+QSS | 配 hiDPI |
| Monophonic mode #6516(前半) | 5👍 | `Instrument`+`NotePlayHandle` | 先 mono 后 portamento |

### 4.2 中等工程（先写设计文档；仿 `doc/*-design.md`）

1. **Freeze/bounce track→sample track** #6666：复用 `ProjectRenderer::renderTracks`+`SampleTrack`；与 **PDC/export** 对齐。**本 fork 因已有 PDC 更易做对**。
2. **Humanization** #1165：timing(read-ahead)/volume/pitch；`Instrument::process`/`NotePlayHandle`/MIDI-out；**先 volume+pitch，timing 最后**。
3. **Monophonic + portamento** #6516 后半。
4. **SFZ native** #3277：封装 `plugins/CarlaBase/.../sfzero`；或文档化经 Carla。
5. **JSON project format** #3981：复用 `QDomDocument`；**并行 JSON、保持 `.mmp/.mmpz` 兼容**；RFC。
6. **sidechain** #7475 后半：mixer sidechain 输入路由（LV2 端口已有）；**不与 VST3 绑定**。
7. **多 piano-roll / 多轨编辑** #7653-11 + #5887：`GuiApplication::m_pianoRoll` 单例→先**多轨只读+编辑单轨**，再多实例。
8. **Qt6 收尾** #6614；**hiDPI 系统性** #2510/#7542+审计 S1；**MIDI 细分** #1472。
9. **effects preset + reset**；**collapsible groups** #735；**linked patternblocks** #3100（先复现语义）。

### 4.3 大型/长期（高呼声≠小步；需设计文档+社区共识；README 先开 issue）

CLAP 原生 #6560 · **tracker view** #4314 · **loop launcher** #5044 · **redesign** #1911 / **Godot UI** #6194（与 token 路线冲突，先定方向） · **Zyn-Fusion** #4179 · **siloed B/B** #453 · **LMMS-as-plugin**（另起项目） · **MSVC #3999**（CI 矩阵）。

### 4.4 不当作可实操功能

- **meta/aggregator**：#4877 #5495 #1472 #5433 #4929 #3534 #3880(未拆) · **#8478 No-AI**（治理，不是代码；不评价）。
- **上游已有**：LV2 本体 · VST2 · recording · stems · FLAC/Ogg/MP3 · scale 高亮 · clone · SlicerT clear · themes · **先 grep**。
- **VST3 文档非目标**；**fork 稳定性**已有报告不重复；**纯主题/艺术**。

---

## 5. 落地顺序建议（可操作）

1. **先跑通本仓构建+测试基线**（`build*/`、`tests/`、fork stress/fuzz）；`git status` 不覆盖用户工作。
2. **先做 4.1 小步快做**，每个小 PR + 回归测试；**改名/XDG 注意向后兼容**。
3. **对已实现 VST3/PDC/LV2-UI/Qt6/UI-UX 做"宣称前核实"**：读对应 `doc/` 非目标 + 补边界测试；**上游 issue 仍 open ≠ 本 fork 没做**。
4. **中等工程先写设计文档**；上游对应 issue 对齐（README：大 feature 先 issue + Discord）。
5. 拿不准就问维护者；**meta 不当功能排期**。

---

## 6. 复现本次查询

```bash
# 上游按 👍
curl -s "https://api.github.com/search/issues?q=repo:LMMS/lmms+is:issue+is:open&sort=reactions-%2B1&order=desc&per_page=50" \
 | jq -r '.items[]|[.reactions["+1"],.number,.title,(.labels|map(.name)|join("|"))]|@tsv'
# good first issue
curl -s "https://api.github.com/search/issues?q=repo:LMMS/lmms+is:open+label:%22good+first+issue%22&per_page=30" | jq -r '.items[]|[.number,.title]|@tsv'
# 单 issue / discussion / 论坛
curl -s https://api.github.com/repos/LMMS/lmms/issues/4715 | jq .
open https://github.com/LMMS/lmms/discussions/7653
open https://lmms.io/forum/viewforum.php?f=4
open https://lmms.io/forum/viewtopic.php?t=34235
# 本 fork vs 上游
git ls-tree upstream/master plugins/ --name-only | grep -i vst3 || echo none
git show upstream/master:src/core/Mixer.cpp | grep -c latencyByChannel
git show upstream/master:CMakeLists.txt | grep -iE 'WANT_LV2|WANT_VST3|WANT_QT6'
git rev-list --count upstream/master..HEAD
```

---

## 7. 诚实性备注 + 维护建议

- 快照 2026-11-27；**数字会变**。Reddit 403；Discord 未覆盖；论坛 2021 帖"已有/规划"未逐条复核当前 1.3；"linked"等未完全核实处已标注。
- 已区分**上游 LMMS** vs **本 `lmms-modern` fork**；可实操以本仓为准，标注路径可复核。
- 本文件**不构成承诺**；落地前请 `grep`、跑测试、读 `doc/` 非目标；必要时问维护者。**本次未动任何代码。**

### 长期维护

- 定期重跑 **§6** 更新 §2 票数；把已落地项从 §4 移入 §3；把新核实/新 issue 增补；本 fork 新增分支/`doc/` 设计时同步 §3；**meta/No-AI 单独记，不混入功能排期**。

---

### 一句话

上游最高声量 **VST3/CLAP/LV2(UI)** + **工作流/自动化细节/humanization/PDC/hiDPI/Qt6/JSON/SFZ/sidechain/freeze/tracker/loop launcher**；meta 治理非功能。在**你的 fork**，VST3/PDC/LV2-UI/Qt6 实验/UI-UX 审计/sample-accurate 设计/稳定性测试**已有代码或设计**，最划算是 **§3 核实补强 + §4.1 小步快做**，而非从零做大件。
