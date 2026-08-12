// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { defineConfig } from "@lynx-js/rspeedy";
import { pluginReactLynx } from "@lynx-js/react-rsbuild-plugin";
import { pluginQRCode } from "@lynx-js/qrcode-rsbuild-plugin";
import { pluginSass } from "@rsbuild/plugin-sass";

// One entry per jank root-cause scene. The entry key becomes the output
// bundle filename (e.g. `long-task.lynx.bundle`), which the jank sub-menu
// references as `showcase/jank/<key>.lynx.bundle`. Keep keys kebab-case and
// in sync with `explorer/showcase/menu/sub-menu/jank.tsx`.
export default defineConfig({
  source: {
    entry: {
      "long-task": "./src/long-task/index.tsx",
      "layout-explosion": "./src/layout-explosion/index.tsx",
      "setstate-storm": "./src/setstate-storm/index.tsx",
      "list-scroll": "./src/list-scroll/index.tsx",
      "image-decode": "./src/image-decode/index.tsx",
      "animation-reflow": "./src/animation-reflow/index.tsx",
      "gc-pressure": "./src/gc-pressure/index.tsx",
    },
    alias: {
      "@shared": "./src/shared",
    },
  },
  plugins: [pluginReactLynx(), pluginSass(), pluginQRCode()],
});
