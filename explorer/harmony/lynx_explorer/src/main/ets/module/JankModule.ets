// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { LLog, LynxModule } from '@lynx/lynx';
import resourceManager from '@ohos.resourceManager';

const TAG = 'JankModule';

/**
 * Native module backing the "main-thread sync I/O" jank scene (root cause ⑤:
 * 主线程同步等外部资源).
 *
 * `readRawFileSync` is async-dispatched by the bridge (it is deliberately NOT
 * declared in `syncMethods`), so it runs on the UI/main (ArkTS) thread while
 * the JS (Lepus) thread stays free. Inside, it calls
 * `resourceManager.getRawFileContentSync` `times` times — a real synchronous
 * file read that blocks the frame thread on I/O. This is the intentional
 * contrast with the Long Task scene (root cause ①), which blocks the JS
 * thread and leaves UI fps near 60.
 *
 * `syncMethods` (PostSyncTask) is avoided on purpose: it would block the JS
 * thread too and can deadlock when the thread strategy collapses JS and UI
 * onto one thread (ALL_ON_UI). Async dispatch blocks only the frame thread.
 *
 * The host `Context` (needed to reach `resourceManager`) is handed in via the
 * module `param` from `pages/Lynx.ets` `aboutToAppear`, so the module does not
 * depend on `getContext(this)` resolving for a framework-constructed instance.
 */
export class JankModule extends LynxModule {
  public readRawFileSync(path: string, times: number,
    callback: (bytes: number) => void) {
    try {
      const ctx = this.param as Context;
      const mgr: resourceManager.ResourceManager = ctx.resourceManager;
      let total = 0;
      for (let i = 0; i < times; i++) {
        // Synchronous rawfile read — blocks the UI/main thread on file I/O.
        const data = mgr.getRawFileContentSync(path);
        total += data.byteLength;
      }
      callback(total);
    } catch (e) {
      LLog.e(TAG, `readRawFileSync failed: ${JSON.stringify(e)}`);
      callback(0);
    }
  }
}
