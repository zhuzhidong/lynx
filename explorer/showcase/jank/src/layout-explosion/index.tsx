// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root, useRef, useState } from "@lynx-js/react";
import { useJankTrace } from "@shared/useJankTrace";
import { IntensityToggle, type IntensityLevel } from "@shared/IntensityToggle";
import { SceneShell } from "@shared/SceneShell";

// Root cause: Layout / TASM bottleneck. A deeply nested flexbox subtree is
// forced to re-measure on every scroll event, so the layout pass explodes.
//
// Toggle nesting depth; scroll the list to trigger relayout storms.

const LEVELS: IntensityLevel[] = [
  { label: "Low", hint: "depth 10 — light" },
  { label: "Med", hint: "depth 30 — noticeable jank on scroll" },
  { label: "High", hint: "depth 60 — heavy relayout, drop3/drop7 bursts" },
];
const DEPTHS = [10, 30, 60];

function Nest({ depth }: { depth: number }) {
  // Recursively nest <view> flex containers to `depth` levels.
  if (depth <= 0) {
    return (
      <view style={{ width: "40px", height: "40px", backgroundColor: "#ff7a45" }} />
    );
  }
  return (
    <view
      style={{
        display: "flex",
        flexDirection: "row",
        padding: "4px",
        backgroundColor: depth % 2 === 0 ? "#fff7e6" : "#e6f4ff",
        borderWidth: 1,
        borderColor: "#d0d3d9",
      }}
    >
      <Nest depth={depth - 1} />
    </view>
  );
}

function LayoutExplosion() {
  const trace = useJankTrace("layout-explosion");
  const [level, setLevel] = useState(0);
  const depth = DEPTHS[level];
  // Mutated on scroll to force a relayout of the whole subtree each tick.
  const [toggle, setToggle] = useState(false);
  const scrollCountRef = useRef(0);

  const onScroll = () => {
    // Throttle trace marks by event count so we don't add self-inflicted noise.
    if ((scrollCountRef.current++ & 3) === 0) {
      trace.mark("relayout", { depth: String(depth) });
    }
    // Flipping a child style forces the deep subtree to re-measure.
    setToggle((t) => !t);
  };

  // Render many deep subtrees so the scroll surface is large.
  const cells: any[] = [];
  for (let i = 0; i < 40; i++) {
    cells.push(
      <view key={`c-${i}`} style={{ marginBottom: "8px" }}>
        <Nest depth={depth} />
      </view>
    );
  }

  return (
    <SceneShell
      title="Layout Explosion (deep flexbox relayout)"
      description="A deeply nested flex subtree is forced to re-measure on every scroll event. Fling the list and watch drop3/drop7."
      controls={<IntensityToggle levels={LEVELS} onChange={setLevel} />}
    >
      <scroll-view
        scroll-y
        style={{ width: "100%", height: "100%" }}
        bindscroll={onScroll}
      >
        <view style={{ width: `${toggle ? "98%" : "100%"}`, padding: "8px" }}>
          {cells}
        </view>
      </scroll-view>
    </SceneShell>
  );
}

root.render(<LayoutExplosion />);
if (import.meta.webpackHot) {
  import.meta.webpackHot.accept();
}
