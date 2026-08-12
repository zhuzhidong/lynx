// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root } from '@lynx-js/react';
import { AppContextProvider } from '@explorer/lib';
import { ItemProps } from '@components/menu-item';
import { Menu } from '@components/menu';

// Jank / frame-drop showcase. Each entry reproduces one root-cause category
// of frame drops and ships with an in-scene intensity toggle. The bundle
// filenames below must stay in sync with the entry keys in
// `explorer/showcase/jank/lynx.config.mjs`.
const ITEMS: ItemProps[] = [
  {
    title: 'Long Task',
    description: 'JS-thread blocking: heavy sync loop in RAF',
    url: 'file://lynx?local://showcase/jank/long-task.lynx.bundle',
  },
  {
    title: 'Layout Explosion',
    description: 'Deep nested flexbox + scroll-triggered relayout',
    url: 'file://lynx?local://showcase/jank/layout-explosion.lynx.bundle',
  },
  {
    title: 'SetState Storm',
    description: 'High-frequency data updates -> cross-thread dispatch',
    url: 'file://lynx?local://showcase/jank/setstate-storm.lynx.bundle',
  },
  {
    title: 'List Scroll',
    description: 'Large list, heavy cells, no virtualization',
    url: 'file://lynx?local://showcase/jank/list-scroll.lynx.bundle',
  },
  {
    title: 'Image Decode',
    description: 'Large images decoded on scroll, no downsampling',
    url: 'file://lynx?local://showcase/jank/image-decode.lynx.bundle',
  },
  {
    title: 'Animation Reflow',
    description: 'Animate width/height vs transform/opacity',
    url: 'file://lynx?local://showcase/jank/animation-reflow.lynx.bundle',
  },
  {
    title: 'GC Pressure',
    description: 'Allocate large arrays in RAF -> GC spikes',
    url: 'file://lynx?local://showcase/jank/gc-pressure.lynx.bundle',
  },
];

root.render(
  <AppContextProvider>
    <Menu items={ITEMS} />
  </AppContextProvider>
);
