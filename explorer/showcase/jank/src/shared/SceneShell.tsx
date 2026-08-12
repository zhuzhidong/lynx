// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import type { ReactElement, ReactNode } from "@lynx-js/react";

interface SceneShellProps {
  title: string;
  /** Short description of the root cause this scene reproduces. */
  description: string;
  /** Controls rendered below the header (typically <IntensityToggle/>). */
  controls?: ReactNode;
  /** The main scene body. */
  children?: ReactNode;
}

/**
 * Common page chrome for every jank scene: a title bar, a one-line root-cause
 * description, a slot for the intensity controls, then the scene body.
 */
export function SceneShell(props: SceneShellProps): ReactElement {
  return (
    <view style={{ width: "100%", height: "100vh", display: "flex", flexDirection: "column" }}>
      <view style={{ padding: "10px 12px", backgroundColor: "#ffffff", borderBottom: "1px solid #e1e4e8" }}>
        <text style={{ fontSize: "18px", fontWeight: "bold", color: "#222222" }}>{props.title}</text>
        <text style={{ fontSize: "12px", color: "#666666", marginTop: "4px" }}>{props.description}</text>
      </view>
      {props.controls}
      <view style={{ flex: 1, overflow: "scroll" }}>{props.children}</view>
    </view>
  );
}
