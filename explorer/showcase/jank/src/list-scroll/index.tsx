// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root, useRef, useState } from "@lynx-js/react";
import { useJankTrace } from "@shared/useJankTrace";
import { IntensityToggle, type IntensityLevel } from "@shared/IntensityToggle";
import { SceneShell } from "@shared/SceneShell";

// Root cause: List / scroll. A large list of heavy cells with no
// virtualization creates a huge node tree and lays out every cell up front;
// scrolling triggers continuous cell layout work.

const LEVELS: IntensityLevel[] = [
  { label: "Low", hint: "50 heavy cells — light" },
  { label: "Med", hint: "500 heavy cells — scroll jank" },
  { label: "High", hint: "2000 heavy cells — first-screen drop25, fling dips" },
];
const COUNTS = [50, 500, 2000];

// A deliberately heavy cell: nested views + multiple text/image nodes.
function HeavyCell({ index }: { index: number }) {
  const colors = ["#fff7e6", "#e6f4ff", "#f6ffed", "#fff1f0"];
  return (
    <list-item item-key={`i-${index}`} key={`i-${index}`}>
      <view style={{ padding: "8px", backgroundColor: colors[index % 4] }}>
        <view style={{ flexDirection: "row", marginBottom: "6px" }}>
          <view style={{ width: "40px", height: "40px", backgroundColor: "#1677ff", borderRadius: "20px" }} />
          <view style={{ marginLeft: "8px", flex: 1 }}>
            <text style={{ fontSize: "14px", fontWeight: "bold" }}>Item {index}</text>
            <text style={{ fontSize: "12px", color: "#888888" }}>heavy nested cell</text>
          </view>
        </view>
        <view style={{ height: "80px", backgroundColor: "#ffffff", borderWidth: 1, borderColor: "#e1e4e8" }}>
          <text style={{ fontSize: "12px", padding: "6px", color: "#555555" }}>
            body content row {index}
          </text>
        </view>
      </view>
    </list-item>
  );
}

function ListScroll() {
  const trace = useJankTrace("list-scroll");
  const [level, setLevel] = useState(0);
  const count = COUNTS[level];
  const scrollCountRef = useRef(0);

  const onScroll = () => {
    if ((scrollCountRef.current++ & 3) === 0) {
      trace.mark("scroll", { count: String(count) });
    }
  };

  const items: any[] = [];
  for (let i = 0; i < count; i++) {
    items.push(<HeavyCell index={i} />);
  }

  return (
    <SceneShell
      title="List Scroll (no virtualization, heavy cells)"
      description="A large list of heavy nested cells laid out up front. Scroll to trigger continuous cell layout work."
      controls={<IntensityToggle levels={LEVELS} onChange={setLevel} />}
    >
      <list
        scroll-orientation="vertical"
        list-type="single"
        span-count={1}
        style={{ width: "100%", height: "100%" }}
        bindscroll={onScroll}
      >
        {items}
      </list>
    </SceneShell>
  );
}

root.render(<ListScroll />);
if (import.meta.webpackHot) {
  import.meta.webpackHot.accept();
}
