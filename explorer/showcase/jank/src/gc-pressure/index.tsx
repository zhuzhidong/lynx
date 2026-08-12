// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root, useEffect, useRef, useState } from "@lynx-js/react";
import { useJankTrace } from "@shared/useJankTrace";
import { IntensityToggle, type IntensityLevel } from "@shared/IntensityToggle";
import { SceneShell } from "@shared/SceneShell";

// Root cause: Memory / GC. Allocating large arrays every tick creates
// allocation pressure; when the GC runs it pauses the JS thread, producing
// periodic large freezes (drop25 clusters) between otherwise smooth stretches.
//
// NOTE: uses setInterval (not requestAnimationFrame) because ReactLynx runs
// component logic on the Lepus background thread, where RAF is not defined.

const LEVELS: IntensityLevel[] = [
  { label: "Low", hint: "~0.5 MB/tick allocated — occasional gc pause" },
  { label: "Med", hint: "~4 MB/tick — frequent gc freezes" },
  { label: "High", hint: "~16 MB/tick — heavy allocation, regular drop25 clusters" },
];
// 0.5MB / 4MB / 16MB in float64 (8 bytes) element counts.
const ALLOC_BYTES = [0.5 * 1024 * 1024, 4 * 1024 * 1024, 16 * 1024 * 1024];
const TICK_MS = 16;

function GcPressure() {
  const trace = useJankTrace("gc-pressure");
  const [level, setLevel] = useState(0);
  const [running, setRunning] = useState(false);
  const [acc, setAcc] = useState(0);
  const intervalRef = useRef(0);
  const levelRef = useRef(0);
  levelRef.current = level;
  const accRef = useRef(0);
  accRef.current = acc;

  useEffect(() => () => {
    if (intervalRef.current) clearInterval(intervalRef.current);
  }, []);

  const tick = () => {
    const bytes = ALLOC_BYTES[levelRef.current];
    const len = Math.floor(bytes / 8); // float64 elements
    trace.mark("alloc", { bytes: String(bytes) });
    // Allocate a fresh large array each tick and immediately discard it.
    // The garbage collector has to reclaim the previous one, pausing JS.
    const arr = new Array(len);
    for (let i = 0; i < len; i++) {
      arr[i] = Math.sqrt(i);
    }
    accRef.current = (accRef.current + arr[len - 1]) % 1000;
    setAcc(accRef.current);
  };

  const start = () => {
    setRunning(true);
    intervalRef.current = setInterval(tick, TICK_MS);
  };
  const stop = () => {
    setRunning(false);
    if (intervalRef.current) clearInterval(intervalRef.current);
    intervalRef.current = 0;
  };

  return (
    <SceneShell
      title="GC Pressure (per-tick large allocations)"
      description="Each tick allocates a large array and discards it. GC pauses cause periodic drop25 clusters between smooth stretches."
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
            {running ? "Stop" : "Run"} (acc = {acc})
          </text>
        </view>
      </view>
    </SceneShell>
  );
}

root.render(<GcPressure />);
if (import.meta.webpackHot) {
  import.meta.webpackHot.accept();
}
