// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { useEffect, useRef } from "@lynx-js/react";

/**
 * Wraps `lynx.performance.profileStart/End/Mark` so each jank scene opens a
 * named trace section on mount and closes it on unmount. `profileMark`
 * instants are tied back to the section via a shared `flowId`, so they line
 * up under the `jank:<scene>` span in the perfetto/HiTraceMeter UI.
 *
 * The marks are no-ops when the host is not recording
 * (`lynx.performance.isProfileRecording()` is false), so leaving them in the
 * hot path costs nothing outside of a capture.
 */
export function useJankTrace(scene: string) {
  // A stable flowId for this scene instance. Generated once per mount.
  const flowIdRef = useRef<number>(lynx.performance.profileFlowId());
  const sceneRef = useRef<string>(scene);

  useEffect(() => {
    sceneRef.current = scene;
    lynx.performance.profileStart(`jank:${scene}`, {
      flowId: flowIdRef.current,
    });
    return () => {
      lynx.performance.profileEnd();
    };
  }, [scene]);

  return {
    flowId: flowIdRef.current,
    /** Emit an instant trace mark tied to this scene's section. */
    mark: (label: string, args?: Record<string, string>) => {
      lynx.performance.profileMark(`jank:${sceneRef.current}:${label}`, {
        flowId: flowIdRef.current,
        args,
      });
    },
  };
}
