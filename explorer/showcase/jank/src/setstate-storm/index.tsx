// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root, useEffect, useRef, useState } from "@lynx-js/react";
import { useJankTrace } from "@shared/useJankTrace";
import { IntensityToggle, type IntensityLevel } from "@shared/IntensityToggle";
import { SceneShell } from "@shared/SceneShell";

// Root cause: JS-thread bottleneck + cross-thread dispatch storm. High-
// frequency setData updates force the JS -> TASM -> UI pipeline to re-render
// far more often than the screen can present, flooding LynxActor dispatches.

const LEVELS: IntensityLevel[] = [
  { label: "Low", hint: "~10 updates/sec — mild" },
  { label: "Med", hint: "~60 updates/sec — steady jank" },
  { label: "High", hint: "~240 updates/sec — dispatch storm, frequent drop1" },
];
const HZ = [10, 60, 240];
const ROWS = 120;

function SetStateStorm() {
  const trace = useJankTrace("setstate-storm");
  const [level, setLevel] = useState(0);
  const [rows, setRows] = useState<number[]>(() => Array.from({ length: ROWS }, (_, i) => i));
  const [running, setRunning] = useState(false);
  const levelRef = useRef(0);
  levelRef.current = level;
  const intervalRef = useRef(0);

  useEffect(() => () => { if (intervalRef.current) clearInterval(intervalRef.current); }, []);

  const start = () => {
    setRunning(true);
    const tick = () => {
      trace.mark("dispatch", { hz: String(HZ[levelRef.current]) });
      // Flip every row on each tick — a full re-render of the data array,
      // serialized across the JS -> native thread boundary each update.
      setRows((prev) => prev.map((v) => -v));
    };
    // setInterval ms = 1000 / hz
    intervalRef.current = setInterval(tick, Math.max(1, Math.floor(1000 / HZ[levelRef.current])));
  };

  const stop = () => {
    setRunning(false);
    if (intervalRef.current) clearInterval(intervalRef.current);
    intervalRef.current = 0;
  };

  return (
    <SceneShell
      title="SetState Storm (cross-thread dispatch)"
      description="High-frequency data updates flood the JS -> TASM -> UI pipeline. Steady low fps + frequent drop1 on High."
      controls={<IntensityToggle levels={LEVELS} onChange={setLevel} />}
    >
      <view style={{ padding: "12px" }}>
        <view
          bindtap={running ? stop : start}
          style={{
            padding: "12px", alignItems: "center",
            backgroundColor: running ? "#d4380d" : "#1677ff", borderRadius: "8px",
          }}
        >
          <text style={{ color: "#ffffff", fontSize: "16px" }}>{running ? "Stop" : "Run"}</text>
        </view>
      </view>
      <view>
        {rows.slice(0, 40).map((v, i) => (
          <view key={`r-${i}`} style={{ padding: "4px 12px", flexDirection: "row" }}>
            <text style={{ fontSize: "12px", color: v < 0 ? "#d4380d" : "#333333" }}>
              row {i}: {v}
            </text>
          </view>
        ))}
      </view>
    </SceneShell>
  );
}

root.render(<SetStateStorm />);
if (import.meta.webpackHot) {
  import.meta.webpackHot.accept();
}
