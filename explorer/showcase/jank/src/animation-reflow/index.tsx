// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root, useEffect, useRef, useState } from "@lynx-js/react";
import { useJankTrace } from "@shared/useJankTrace";
import { IntensityToggle, type IntensityLevel } from "@shared/IntensityToggle";
import { SceneShell } from "@shared/SceneShell";

// Root cause: Frame scheduling. Animating layout-triggering properties
// (width/height/top) forces layout+paint every frame, vs compositor-only
// properties (transform/opacity) which the GPU can cheaply composite.
// Toggle the mode to see the contrast side by side.
//
// NOTE: uses setInterval (not requestAnimationFrame) because ReactLynx runs
// component logic on the Lepus background thread, where RAF is not defined.

const LEVELS: IntensityLevel[] = [
  { label: "Reflow", hint: "animate width/height — layout+paint per tick, drop1/drop3" },
  { label: "Composite", hint: "animate transform/opacity — smooth 60fps" },
];
type Mode = "reflow" | "composite";
const TICK_MS = 16;

function AnimationReflow() {
  const trace = useJankTrace("animation-reflow");
  const [level, setLevel] = useState(0);
  const mode: Mode = level === 0 ? "reflow" : "composite";
  const [phase, setPhase] = useState(0); // 0..1 animation phase
  const [running, setRunning] = useState(false);
  const phaseRef = useRef(0);
  phaseRef.current = phase;
  const dirRef = useRef(1);
  const intervalRef = useRef(0);
  const modeRef = useRef<Mode>(mode);
  modeRef.current = mode;

  useEffect(() => () => {
    if (intervalRef.current) clearInterval(intervalRef.current);
  }, []);

  const tick = () => {
    trace.mark(modeRef.current);
    let p = phaseRef.current + dirRef.current * 0.04;
    if (p >= 1) { p = 1; dirRef.current = -1; }
    else if (p <= 0) { p = 0; dirRef.current = 1; }
    phaseRef.current = p;
    setPhase(p);
  };

  const toggle = () => {
    if (running) {
      setRunning(false);
      if (intervalRef.current) clearInterval(intervalRef.current);
      intervalRef.current = 0;
    } else {
      setRunning(true);
      intervalRef.current = setInterval(tick, TICK_MS);
    }
  };

  // size interpolates 40px -> 120px
  const size = 40 + Math.floor(phase * 80);
  const scale = 0.5 + phase * 1.0; // 0.5 -> 1.5
  const op = 0.3 + phase * 0.7;

  const cells: any[] = [];
  for (let i = 0; i < 60; i++) {
    if (mode === "reflow") {
      // Animating width/height: every tick triggers layout on each cell.
      cells.push(
        <view key={`c-${i}`} style={{ margin: "4px", flexDirection: "row" }}>
          <view style={{ width: `${size}px`, height: `${size}px`, backgroundColor: "#1677ff" }} />
        </view>
      );
    } else {
      // Animating transform/opacity: compositor-only, no layout.
      cells.push(
        <view key={`c-${i}`} style={{ margin: "4px", flexDirection: "row" }}>
          <view
            style={{
              width: "80px",
              height: "80px",
              backgroundColor: "#52c41a",
              opacity: op,
              transform: `scale(${scale.toFixed(3)})`,
            }}
          />
        </view>
      );
    }
  }

  return (
    <SceneShell
      title="Animation Reflow (layout vs composite properties)"
      description="Animating width/height triggers layout+paint every tick; transform/opacity is compositor-only. Toggle modes and compare fps."
      controls={<IntensityToggle levels={LEVELS} initial={0} onChange={setLevel} />}
    >
      <view style={{ padding: "12px", flexDirection: "row" }}>
        <view
          bindtap={toggle}
          style={{ padding: "12px", alignItems: "center", backgroundColor: "#1677ff", borderRadius: "8px", marginRight: "12px" }}
        >
          <text style={{ color: "#ffffff", fontSize: "14px" }}>{running ? "Pause" : "Play"}</text>
        </view>
        <text style={{ fontSize: "12px", color: "#666666", alignSelf: "center" }}>
          mode: {mode} · phase: {phase.toFixed(2)}
        </text>
      </view>
      <view style={{ flex: 1, flexDirection: "row", flexWrap: "wrap", padding: "8px" }}>
        {cells}
      </view>
    </SceneShell>
  );
}

root.render(<AnimationReflow />);
if (import.meta.webpackHot) {
  import.meta.webpackHot.accept();
}
