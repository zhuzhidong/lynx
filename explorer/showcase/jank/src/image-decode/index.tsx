// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root, useRef, useState } from "@lynx-js/react";
import { useJankTrace } from "@shared/useJankTrace";
import { IntensityToggle, type IntensityLevel } from "@shared/IntensityToggle";
import { SceneShell } from "@shared/SceneShell";

// Root cause: Render / GPU. Large source images decoded on the main decode
// path with no downsampling cause decode spikes (and memory pressure) when
// they scroll into view.
//
// NOTE: uses remote https://picsum.photos images at the chosen source size, so
// it needs network. The decode cost is local regardless of where the bytes
// come from.

const LEVELS: IntensityLevel[] = [
  { label: "Low", hint: "256px source — smooth" },
  { label: "Med", hint: "1024px source — decode spikes on scroll" },
  { label: "High", hint: "4096px source — drop7/drop25 when big images enter view" },
];
const SIZES = [256, 1024, 4096];
const CELL_COUNT = 30;

function ImageDecode() {
  const trace = useJankTrace("image-decode");
  const [level, setLevel] = useState(0);
  const size = SIZES[level];
  const loadedRef = useRef(0);
  const scrollCountRef = useRef(0);

  const onLoad = () => {
    loadedRef.current++;
    trace.mark("decode", { size: String(size), loaded: String(loadedRef.current) });
  };

  const onScroll = () => {
    if ((scrollCountRef.current++ & 3) === 0) {
      trace.mark("scroll", { size: String(size) });
    }
  };

  const items: any[] = [];
  for (let i = 0; i < CELL_COUNT; i++) {
    // Same displayed size, but the *source* resolution scales with level —
    // the decoder still has to decode the full source bitmap.
    const src = `https://picsum.photos/${size}/${size}?random=${i}`;
    items.push(
      <view key={`img-${i}`} style={{ padding: "6px" }}>
        <image
          src={src}
          mode="aspectFill"
          bindload={onLoad}
          style={{ width: "100%", height: "160px", backgroundColor: "#f0f2f5" }}
        />
      </view>
    );
  }

  return (
    <SceneShell
      title="Image Decode (large source, no downsampling)"
      description="Large source images decoded on scroll. Needs network. Big images cause drop7/drop25 when entering the viewport."
      controls={<IntensityToggle levels={LEVELS} onChange={setLevel} />}
    >
      <scroll-view scroll-y style={{ width: "100%", height: "100%" }} bindscroll={onScroll}>
        {items}
      </scroll-view>
    </SceneShell>
  );
}

root.render(<ImageDecode />);
if (import.meta.webpackHot) {
  import.meta.webpackHot.accept();
}
