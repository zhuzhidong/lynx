// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { useEffect, useRef, useState } from "@lynx-js/react";
import { root } from "@lynx-js/react";
import { useJankTrace } from "@shared/useJankTrace";
import { IntensityToggle, type IntensityLevel } from "@shared/IntensityToggle";
import { SceneShell } from "@shared/SceneShell";

// Root cause: JS-thread blocking. A long synchronous task running on the JS
// thread starves the per-frame callback (and the rest of the Lynx pipeline:
// JS -> TASM -> UI), so frames are dropped until the task finishes.
//
// Toggle the workload with Low/Med/High, press "Run" to arm a RAF loop whose
// every tick executes the loop synchronously, "Stop" to release it.

const LEVELS: IntensityLevel[] = [
  { label: "Low", hint: "~5M sqrt iterations per frame — light stutter" },
  { label: "Med", hint: "~20M sqrt iterations per frame — visible jank" },
  { label: "High", hint: "~100M sqrt iterations per frame — long freezes, fps crashes" },
];
const ITERATIONS = [5_000_000, 20_000_000, 100_000_000];

// Module-level sink so the compiler cannot elide the work.
const sink = { current: 0 };

function LongTask() {
  const trace = useJankTrace("long-task");
  const [level, setLevel] = useState(0);
  const [running, setRunning] = useState(false);
  const rafIdRef = useRef(0);
  const levelRef = useRef(0);
  levelRef.current = level;

  // cleanup on unmount
  useEffect(() => () => {
    if (rafIdRef.current) cancelAnimationFrame(rafIdRef.current);
  }, []);

  const tick = () => {
    const n = ITERATIONS[levelRef.current];
    trace.mark("sync-loop", { iters: String(n) });
    // Tight synchronous CPU loop — blocks the JS thread for the whole pass.
    let acc = 0;
    for (let i = 0; i < n; i++) {
      acc += Math.sqrt(i);
    }
    sink.current = acc;
    rafIdRef.current = requestAnimationFrame(tick);
  };

  const start = () => {
    setRunning(true);
    rafIdRef.current = requestAnimationFrame(tick);
  };
  const stop = () => {
    setRunning(false);
    if (rafIdRef.current) cancelAnimationFrame(rafIdRef.current);
    rafIdRef.current = 0;
  };

  return (
    <SceneShell
      title="Long Task (JS thread blocking)"
      description="A synchronous CPU loop runs on the JS thread each frame, starving the render pipeline. Watch fps collapse on High."
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
            {running ? "Stop" : "Run"} (acc = {sink.current})
          </text>
        </view>
      </view>
    </SceneShell>
  );
}

root.render(<LongTask />);
if (import.meta.webpackHot) {
  import.meta.webpackHot.accept();
}
