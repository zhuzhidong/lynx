# Lynx 丢帧卡顿示例集(Jank Showcase)

一组可交互的 Lynx 场景,用于在 HarmonyOS 上**复现、观察、定位**丢帧卡顿。每个场景对应一类根因,带 Low/Med/High 强度调节,并叠加一个实时 FPS 浮窗,便于把"卡顿现象 ↔ 根因 ↔ 指标"三者对上。

> 运行载体:Lynx Explorer(`explorer/harmony`)。进入路径:首页 → **Jank** → 选场景。

---

## 一、为什么要单独做这套场景

丢帧卡顿是性能测试的高频命题,但"卡顿"本身是个模糊词——它可能是 UI 线程被阻塞、可能是 JS 线程长任务、可能是布局爆炸、也可能是 GC 停顿。它们的观测信号和修复方向完全不同。

这套示例把线上最常见的几类根因**各做一个最小可复现场景**,并让每个场景的强度可调,目的是:

- 让性能测试有一个**确定性的卡顿源**(而不是依赖偶现卡顿)。
- 让"根因 → 指标变化"的对应关系可教学、可对照。
- 作为 Lynx 自身性能回归的压测素材。

---

## 二、丢帧根因分类

Lynx 在 HarmonyOS 上的线程模型(关键背景):

- **UI / 主线程** = App 主线程(ArkTS / ArkUI),负责合成与绘制。
- **JS 线程(Lepus)** = 跑 ReactLynx 组件逻辑、reconciler、JS 业务。
- **TASM / Layout 线程** = 模板解析与布局计算。
- 跨线程通信经 LynxActor / NAPI 桥接。

因此"卡顿"至少分两大类:**UI 线程卡顿**(直接掉合成帧)与 **JS 线程卡顿**(响应性丧失,UI 帧未必掉)。这是本套示例最重要的认知前提。

按根因,丢帧通常落在以下几类:

| 类别                 | 典型表现                                    |
| -------------------- | ------------------------------------------- |
| JS 线程阻塞          | 长同步任务、大数据运算、同步 I/O 饿死帧回调 |
| Layout / TASM 瓶颈   | 深嵌套 flex、滚 动触发 relayout、节点爆炸   |
| 跨线程 dispatch 风暴 | 高频 setData,JS→TASM→UI 通路被打爆          |
| 列表 / 滚动          | 大列表无虚拟化、重 cell、滚动中构建         |
| 渲染 / GPU           | 大图解码、过度绘制、昂贵绘制                |
| 帧调度               | 动画驱动 layout 属性(非 transform/opacity)  |
| 内存 / GC            | 大量分配触发 GC 停顿                        |
| 系统竞争             | 温控降频、后台抢占                          |

本套示例覆盖前 7 类。

---

## 三、场景清单与原理

每个场景都是独立 bundle(`src/<场景>/index.tsx`),挂在 Jank 子菜单下。共性结构:

- `SceneShell`:标题 + 根因描述 + 内容区。
- `IntensityToggle`:Low/Med/High(或自定义)强度切换,即时生效。
- `useJankTrace`:挂载时开 `lynx.performance.profileStart('jank:<场景>')`,卸载时 `profileEnd()`;热路径 `mark(label)` 发 instant,用 flowId 关联回该段。`isProfileRecording()` 为 false 时零开销。

> 说明:ReactLynx 组件逻辑跑在 **Lepus(JS)线程**。该线程全局**没有** `requestAnimationFrame`(那是主线程全局),也没有 `Date.now`。所以凡需要周期循环的场景统一用 `setInterval`(后台线程可用)。这也是为什么纯 JS 长任务不一定会让 UI 掉帧——见下文每个场景的"信号"。

### 1. Long Task — JS 线程阻塞

**根因**:每 tick 跑一个紧凑同步 `for` 循环(Math.sqrt),占满 Lepus 线程,饿死事件循环与帧回调。

**强度**:5M / 20M / 100M 次迭代/帧。

**机制要点**:末尾把累加结果写入 `sink` 防止循环被优化掉;`mark('sync-loop')` 标记每次循环。

**观测信号**:

- **响应性丧失**:点 Run/Stop 后要等当前 tick 跑完才响应(High 档 tick ~1s)——这本身就是病灶。
- 浮窗 FPS **可能仍是 60**(见 §四):因为 UI 线程闲着,仍按 16.66ms 合成;但 App 实际僵死。
- perfetto:Lepus 线程上一条长 `jank:long-task:sync-loop` 段。

### 2. Layout Explosion — Layout/TASM 瓶颈

**根因**:递归建一棵 N 层深的 flex 子树(×40 cell),`bindscroll` 里翻转子节点 width,强制整棵子树 re-measure。

**强度**:深度 10 / 30 / 60。

**机制要点**:`mark('relayout')`(按事件计数节流,避免自造 trace 噪声)。

**观测信号**:fling 时 drop3/drop7 突发;DevEco Profiler 的 Layout slice 在 TASM/UI 线程膨胀;深度越深越明显。这是真正会让 UI 掉帧的场景(布局在主线程相关路径上)。

### 3. SetState Storm — 跨线程 dispatch 风暴

**根因**:以 N Hz 调 `setState` 翻转 120 行数组,洪水般跨线程 setData(JS→TASM→UI),reconciler 与 dispatch 远超屏幕能呈现的频率。

**强度**:10 / 60 / 240 次/秒。

**机制要点**:`mark('dispatch')` 每 tick。

**观测信号**:稳定低 fps + 频繁 drop1;`onPerformanceEvent` 的 PipelineEntry 高频;trace 里 `data_update`/`js_to_native` slice 密集。

### 4. List Scroll — 列表/滚动

**根因**:一次性渲染 N 个重嵌套 cell(多层 view/text/嵌套 flex),无虚拟化,滚动中持续 cell layout。

**强度**:50 / 500 / 2000 cell。

**机制要点**:`mark('scroll')` 于 `bindscroll`(节流)。

**观测信号**:High 首屏 drop25(一次性建树太重);fling 中 List/Layout slice 占主;fps 随 fling 下跌。

### 5. Image Decode — 渲染/GPU

**根因**:scroll-view 内 30 张大源图,源分辨率随强度变化(256/1024/4096 px),无下采样,滚动入视口时解码 spike。

> 需网络:用 `https://picsum.photos` 远程图;解码成本是本地的,与字节来源无关。

**机制要点**:`mark('decode')` 于 `<image>` 的 `bindload`。

**观测信号**:大图进视口时 drop7/drop25;图片解码线程忙;RSS 随大图缓存堆积上升。

### 6. Animation Reflow — 帧调度

**根因**:对比动画驱动 layout 属性(width/height)与 compositor-only 属性(transform/opacity)的成本差异。

**强度**:Reflow / Composite 两档。

**机制要点**:60 个 cell,Reflow 模式每 tick 动 width/height(触发 layout+paint),Composite 模式动 transform/opacity(GPU 合成);`mark(mode)`。

**观测信号**:Reflow 每帧 drop1/drop3、Layout slice 每帧出现;Composite 60fps。两档切换对比直观,是"动画该用 transform 而非 width"的教学场景。

### 7. GC Pressure — 内存/GC

**根因**:每 tick `new Array(len)` 大数组并丢弃,制造分配压力;GC 回收时停顿 Lepus 线程。

**强度**:0.5 / 4 / 16 MB/帧。

**机制要点**:`mark('alloc')` 每 tick。

**观测信号**:每隔几秒一次大冻(drop25 簇),其间 fps 正常;RSS 锯齿振荡(分配↑、GC↓);trace 里 GC slice 周期出现。与 Long Task 类似,这是 JS 线程卡顿,UI 帧未必掉,看 RSS 锯齿 + 周期冻帧。

---

## 四、浮窗指标说明(FpsOverlay)

浮窗挂在 `pages/Lynx.ets` 的 `Stack` 上,所有 Lynx 页面都会显示。指标基于宿主 `@ohos.graphics.displaySync` 帧回调(样板取自 `LynxFpsTracer`),**测的是 UI/主线程流畅度**。

### FPS

过去 1 秒成功渲染的帧数。60 满帧;≥55 绿,≥30 黄,否则红。

### drop1 / drop3 / drop7 / drop25

按"这帧迟到了几个理想帧时长"分桶(理想帧间隔 16.66ms):

| 桶     | 判定          | 迟到      | 直观     |
| ------ | ------------- | --------- | -------- |
| drop1  | ≥ 1× ≈ 17ms   | 17–50ms   | 轻微掉帧 |
| drop3  | ≥ 3× ≈ 50ms   | 50–116ms  | 可见卡顿 |
| drop7  | ≥ 7× ≈ 116ms  | 116–416ms | 明显卡顿 |
| drop25 | ≥ 25× ≈ 416ms | ≥ 0.4s    | 严重冻帧 |

浮窗显示的是**过去 1 秒落入各桶的帧数**,桶之间**互斥**(一帧只算最严重桶)。口径与 Lynx 上报的 `lynxsdk_fluency_drop{1,3,7,25}` 一致(Lynx 自家用累加式,这里为好读改互斥)。

### RSS

`@ohos.hidebug.getAppNativeMemInfo()` 读 `/proc/<pid>/smaps_rollup` 的 Rss,即进程常驻物理内存。趋势比绝对值更有意义:持续涨=可能泄漏;锯齿振荡=典型 GC 压力。

### Start/Stop Trace

驱动 `@lynx/lynx` 的 `TraceController` 写 perfetto 文件到 `ctx.filesDir/jank.trace`。需 `--dev` 构建(`enable_trace="perfetto"`)才可用,否则按钮置灰(`TraceEvent.enableTrace()` 返回 false)。

### 关键局限:浮窗只覆盖 UI 流畅度

**这是最重要的认知点。** 浮窗 FPS 来自 UI 线程 vsync。Long Task / GC Pressure 阻塞的是 **Lepus(JS)线程**,不是 UI 线程。UI 线程闲着仍按 16.66ms 合成(画面虽不更新但照合成),所以:

- **FPS 可能仍是 60、drop=0**,但 App 实际僵死(按钮不响应、内容不刷新)。
- 这类场景的正确信号是 **tap 延迟 + 僵死** 与 perfetto 里 Lepus 线程的长段,不是 UI 帧率。
- Lynx 自家也有两套:LynxFpsTracer(UI 流畅度,本浮窗用)与 C++ `FluencyTracer`(`core/services/fluency/fluency_tracer.cc`,报 `lynxsdk_javascript_fluency_event`,即 JS 线程流畅度)。本浮窗暂只覆盖前者。

因此:Layout Explosion / List Scroll / SetState Storm / Animation Reflow 会反映在浮窗 drop 上;Long Task / GC Pressure 主要靠响应性 + trace + RSS 看。

---

## 五、构建与运行

```bash
# 1. 构建 JS bundle(菜单 + 7 场景)并拷贝到 5 平台
python3 explorer/showcase/build_and_copy.py

# 2. 构建 Lynx Harmony SDK(含 trace 支持,--dev 开启 perfetto)
python3 explorer/harmony/script/build.py --dev

# 3. 在 DevEco Studio 里 Build & Run(HAP 打包,会自动经 gn task 重建 bundle)
```

无需改 native 即可加场景:`pages/Lynx.ets` + `ExampleTemplateResourceFetcher` 已能加载任意 `showcase/jank/*.lynx.bundle`。

---

## 六、场景 ↔ 根因 ↔ 信号 对照速查

| 场景             | 根因类别        | 浮窗是否动    | 主要信号                   |
| ---------------- | --------------- | ------------- | -------------------------- |
| Long Task        | JS 线程阻塞     | 否(UI 帧不掉) | tap 延迟、僵死、trace 长段 |
| Layout Explosion | Layout/TASM     | 是            | fling 时 drop3/drop7       |
| SetState Storm   | 跨线程 dispatch | 是            | 稳定低 fps + 频繁 drop1    |
| List Scroll      | 列表/滚动       | 是            | High 首屏 drop25           |
| Image Decode     | 渲染/GPU        | 是            | 大图入视口 drop7/drop25    |
| Animation Reflow | 帧调度          | 是            | Reflow 每帧 drop1/drop3    |
| GC Pressure      | 内存/GC         | 否(周期冻)    | drop25 簇 + RSS 锯齿       |

---

## 七、文件结构

```txt
explorer/showcase/jank/
  lynx.config.mjs          # 7 场景多入口
  package.json             # @showcase/jank
  src/
    shared/
      useJankTrace.ts      # profileStart/End/Mark 封装
      IntensityToggle.tsx  # 强度选择器
      SceneShell.tsx       # 场景外壳
      index.scss
    long-task/index.tsx
    layout-explosion/index.tsx
    setstate-storm/index.tsx
    list-scroll/index.tsx
    image-decode/index.tsx
    animation-reflow/index.tsx
    gc-pressure/index.tsx
explorer/showcase/menu/sub-menu/jank.tsx    # 子菜单
explorer/harmony/lynx_explorer/src/main/ets/
  components/FpsOverlay.ets                 # FPS 浮窗
  pages/Lynx.ets                            # 挂浮窗
```
