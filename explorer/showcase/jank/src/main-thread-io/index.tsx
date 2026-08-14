// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root, useEffect, useRef, useState } from "@lynx-js/react";
import { useJankTrace } from "@shared/useJankTrace";
import { IntensityToggle, type IntensityLevel } from "@shared/IntensityToggle";
import { SceneShell } from "@shared/SceneShell";

// Root cause ⑤: 主线程同步等外部资源 (main thread synchronously blocked on
// an external resource). Unlike Long Task (root cause ①, which blocks the
// JS/Lepus thread and leaves UI fps near 60 while the app freezes), this scene
// blocks the *UI/main* (ArkTS) thread: every burst, the JankModule native
// method runs on the UI thread and synchronously reads a rawfile `reads` times
// via `resourceManager.getRawFileContentSync` — a real blocking file read on
// the frame thread. The call is async-dispatched by the bridge (no
// `syncMethods`), so the JS thread stays free; only the frame thread stalls,
// and UI fps drops.
//
// The burst is intermittent (every ~500ms), so unlike the sustained-slow UI
// scenes (Layout Explosion / List Scroll / Animation Reflow) — where the drop
// buckets stay 0 because drop is hitch-based and blind to持续慢渲染 — the drop
// buckets actually light up here. This is the one scene that shows the drop
// metric working for intermittent jank.

// `NativeModules` is a host-injected global. The ReactLynx Lepus env explicitly
// leaves it `undefined` during module eval / first render and only populates
// it with the real module proxy once the runtime has booted (see
// `@lynx-js/react` `setupLynxEnv`). So it MUST be read lazily inside event
// handlers (taps), never at module top-level or render time — exactly how the
// homepage bundle reads `NativeModules` in its tap handlers. `JankModule` is
// only registered on Harmony (see pages/Lynx.ets), so on other platforms it is
// undefined and the scene shows an "unavailable" notice instead of crashing.
declare const NativeModules: {
  JankModule?: {
    readRawFileSync(path: string, times: number, cb: (bytes: number) => void): void;
  };
} | undefined;

const LEVELS: IntensityLevel[] = [
  { label: "Low", hint: "10 sync reads/burst — light stutter, drop1" },
  { label: "Med", hint: "100 sync reads/burst — visible freeze, drop3/drop7" },
  { label: "High", hint: "500 sync reads/burst — long freeze, drop7/drop25" },
];
const READS = [10, 100, 500];
const BURST_MS = 500;
const BUNDLE_PATH = "showcase/jank/long-task.lynx.bundle";

type Status = "idle" | "running" | "unavailable";

function MainThreadIO() {
  const trace = useJankTrace("main-thread-io");
  const [level, setLevel] = useState(0);
  const [status, setStatus] = useState<Status>("idle");
  const [lastBytes, setLastBytes] = useState(0);
  const intervalRef = useRef(0);
  const levelRef = useRef(0);
  levelRef.current = level;

  useEffect(() => () => {
    if (intervalRef.current) clearInterval(intervalRef.current);
  }, []);

  // Lazily resolve the module on each use — never at module-eval / render time
  // (NativeModules is undefined until the runtime boots). Guarded so a missing
  // module degrades to a notice instead of throwing.
  const getMod = () => NativeModules?.JankModule;

  const burst = () => {
    const mod = getMod();
    if (!mod) return;
    const n = READS[levelRef.current];
    trace.mark("sync-io", { reads: String(n) });
    mod.readRawFileSync(BUNDLE_PATH, n, (bytes) => {
      setLastBytes(bytes);
    });
  };

  const start = () => {
    if (!getMod()) {
      setStatus("unavailable");
      return;
    }
    setLastBytes(0);
    setStatus("running");
    intervalRef.current = setInterval(burst, BURST_MS);
  };
  const stop = () => {
    setStatus("idle");
    if (intervalRef.current) clearInterval(intervalRef.current);
    intervalRef.current = 0;
  };

  const running = status === "running";

  return (
    <SceneShell
      title="Main-thread Sync I/O (UI thread blocked)"
      description="A native call reads a rawfile synchronously on the UI/main thread each burst. Frame thread blocked on file I/O -> fps drops / drop buckets light up. Contrast with Long Task (JS thread, fps stays ~60)."
      controls={<IntensityToggle levels={LEVELS} onChange={setLevel} />}
    >
      <view style={{ padding: "16px" }}>
        <view
          bindtap={running ? stop : start}
          style={{
            padding: "12px",
            alignItems: "center",
            backgroundColor: running ? "#d4380d" : "#1677ff",
            borderRadius: "8px",
          }}
        >
          <text style={{ color: "#ffffff", fontSize: "16px" }}>
            {running
              ? `Stop (last burst = ${lastBytes} bytes)`
              : status === "unavailable"
                ? "Native module unavailable on this platform"
                : `Run (last burst = ${lastBytes} bytes)`}
          </text>
        </view>
      </view>
    </SceneShell>
  );
}

root.render(<MainThreadIO />);
if (import.meta.webpackHot) {
  import.meta.webpackHot.accept();
}
