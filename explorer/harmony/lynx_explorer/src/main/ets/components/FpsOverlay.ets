// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import displaySync from '@ohos.graphics.displaySync';
import display from '@ohos.display';
import hidebug from '@ohos.hidebug';
import {
  TraceController,
  TraceConfig,
  TraceEvent,
  TraceCategory,
} from '@lynx/lynx';

const TAG: string = 'FpsOverlay';
const INVALID_TIMESTAMP: number = -1;
// Lynx LynxFpsTracer keeps time in units of ms * 100 to avoid floats.
const OFFSET_TO_MS: number = 100;
const TIME_MS_TO_NS: number = 1000000;
const FACTOR: number = TIME_MS_TO_NS / OFFSET_TO_MS; // 100000
const DESIRED_FRAME_INTERVAL: number = 1000 * OFFSET_TO_MS / 60; // 1666 (~16.66ms)
const NS_PER_SECOND: number = 1_000_000_000;
// Overlay width as a fraction of screen width (~75% of the previous 0.42).
const OVERLAY_WIDTH_RATIO: number = 0.31;

/**
 * Floating performance overlay for the Jank showcase. Counts frames via
 * OHOS displaySync (modeled on platform/harmony/.../fluency/LynxFpsTracer.ets)
 * and shows fps + drop{1,3,7,25} per second + RSS, with a Start/Stop Trace
 * button that drives Lynx TraceController to dump a perfetto file.
 *
 * Drop buckets are mutually exclusive (a frame counts once, in its worst
 * bucket): drop1 = [1x,3x), drop3 = [3x,7x), drop7 = [7x,25x), drop25 = [25x,+).
 */
@Component
export struct FpsOverlay {
  @State fps: number = 0;
  @State drop1: number = 0;
  @State drop3: number = 0;
  @State drop7: number = 0;
  @State drop25: number = 0;
  @State rssKb: number = 0;
  @State tracing: boolean = false;
  @Prop visible: boolean = true;

  // Draggable position (vp). @State so .position() re-renders on drag.
  @State posX: number = 8;
  @State posY: number = 48;
  private dragStartX: number = 8;
  private dragStartY: number = 48;
  // Screen bounds (vp) for clamping the overlay within the viewport.
  private screenW: number = 0;
  private screenH: number = 0;
  private overlayW: number = 0;

  private ds: displaySync.DisplaySync = displaySync.create();
  private lastTimestampNanos: number = INVALID_TIMESTAMP;
  private lastTargetTimestampNanos: number = INVALID_TIMESTAMP;
  private framesInWindow: number = 0;
  private windowStartNanos: number = 0;
  private wDrop1: number = 0;
  private wDrop3: number = 0;
  private wDrop7: number = 0;
  private wDrop25: number = 0;
  private traceAvailable: boolean = false;

  // The frame callback MUST be a stored field so off('frame', cb) uses the
  // same reference as on('frame', cb) (ArkTS strict mode), exactly like
  // LynxFpsTracer.frameCallback.
  private frameCallback: Callback<displaySync.IntervalInfo> = (
    info: displaySync.IntervalInfo,
  ) => {
    if (this.lastTimestampNanos !== INVALID_TIMESTAMP) {
      let hitch: number = Math.floor(
        (info.timestamp - this.lastTargetTimestampNanos) / FACTOR,
      );
      if (hitch < 0) {
        hitch = 0;
      }
      if (hitch >= 25.0 * DESIRED_FRAME_INTERVAL) {
        this.wDrop25 += 1;
      } else if (hitch >= 7.0 * DESIRED_FRAME_INTERVAL) {
        this.wDrop7 += 1;
      } else if (hitch >= 3.0 * DESIRED_FRAME_INTERVAL) {
        this.wDrop3 += 1;
      } else if (hitch >= DESIRED_FRAME_INTERVAL) {
        this.wDrop1 += 1;
      }
      this.framesInWindow += 1;
    }
    this.lastTimestampNanos = info.timestamp;
    this.lastTargetTimestampNanos = info.targetTimestamp;

    // Emit a 1s rolling window.
    if (this.windowStartNanos === 0) {
      this.windowStartNanos = info.timestamp;
    }
    if (info.timestamp - this.windowStartNanos >= NS_PER_SECOND) {
      this.fps = this.framesInWindow;
      this.drop1 = this.wDrop1;
      this.drop3 = this.wDrop3;
      this.drop7 = this.wDrop7;
      this.drop25 = this.wDrop25;
      this.framesInWindow = 0;
      this.wDrop1 = 0;
      this.wDrop3 = 0;
      this.wDrop7 = 0;
      this.wDrop25 = 0;
      this.windowStartNanos = info.timestamp;
      this.refreshMem();
    }
  };

  aboutToAppear() {
    this.traceAvailable = TraceEvent.enableTrace();
    this.setupBounds();
    this.ds.on('frame', this.frameCallback);
    this.ds.start();
    this.refreshMem();
  }

  // Compute screen size in vp (for clamping) and the overlay's own width.
  private setupBounds() {
    const d = display.getDefaultDisplaySync();
    const density = d.densityPixels > 0 ? d.densityPixels : 1;
    this.screenW = d.width / density;
    this.screenH = d.height / density;
    this.overlayW = this.screenW * OVERLAY_WIDTH_RATIO;
    // Re-clamp the initial position in case the default won't fit.
    this.posX = this.clampX(this.posX);
    this.posY = this.clampY(this.posY);
  }

  private clampX(x: number): number {
    if (this.screenW <= 0) {
      return x;
    }
    const maxX = this.screenW - this.overlayW;
    return x < 0 ? 0 : (x > maxX ? maxX : x);
  }

  private clampY(y: number): number {
    if (this.screenH <= 0) {
      return y;
    }
    // Leave a little height headroom; the overlay isn't tall, so clamp to
    // screen height minus a margin.
    const maxY = this.screenH - 80;
    return y < 0 ? 0 : (y > maxY ? maxY : y);
  }

  aboutToDisappear() {
    this.ds.off('frame', this.frameCallback);
    this.ds.stop();
    if (this.tracing) {
      this.toggleTrace();
    }
  }

  private refreshMem() {
    try {
      const mem: hidebug.NativeMemInfo = hidebug.getAppNativeMemInfo();
      // rss is bigint (KB). Convert via string to avoid bigint->number
      // coercion restrictions in ArkTS strict mode.
      this.rssKb = parseInt(mem.rss.toString(), 10);
    } catch (e) {
      // keep previous value
    }
  }

  toggleTrace() {
    if (!this.tracing) {
      const ctx = getContext(this);
      const cfg: TraceConfig = {
        filePath: ctx.filesDir + '/jank.trace',
        bufferSize: 8 * 1024 * 1024,
        enableCompress: 'true',
      };
      TraceController.getInstance().startTracing(cfg);
      this.tracing = true;
      TraceEvent.instant(TraceCategory.Vitals, 'jank:trace-start', {});
    } else {
      TraceEvent.instant(TraceCategory.Vitals, 'jank:trace-stop', {});
      TraceController.getInstance().stopTracing();
      this.tracing = false;
    }
  }

  @Builder
  metricRow(label: string, value: string, color: ResourceColor) {
    Row() {
      Text(label).fontSize(10).fontColor('#aaa').width('40%')
      Text(value).fontSize(11).fontColor(color).width('60%')
    }.width('100%').height(16)
  }

  build() {
    if (this.visible) {
      Column() {
        Row() {
          Text(`FPS ${this.fps}`)
            .fontSize(14)
            .fontColor(this.fps >= 55 ? '#52c41a' : (this.fps >= 30 ? '#faad14' : '#ff4d4f'))
            .fontWeight(FontWeight.Bold)
          Blank()
          Text(this.tracing ? '● REC' : '')
            .fontSize(10)
            .fontColor('#ff4d4f')
        }.width('100%')

        this.metricRow('drop1', `${this.drop1}`, '#faad14')
        this.metricRow('drop3', `${this.drop3}`, '#ff7a45')
        this.metricRow('drop7', `${this.drop7}`, '#ff4d4f')
        this.metricRow('drop25', `${this.drop25}`, '#cf1322')
        this.metricRow(
          'RSS',
          this.rssKb > 0 ? `${Math.round(this.rssKb / 1024)} MB` : '--',
          '#d4d4d4',
        )

        Button(this.tracing ? 'Stop Trace' : 'Start Trace')
          .fontSize(10)
          .height(24)
          .padding({ left: 8, right: 8 })
          .enabled(this.traceAvailable)
          .opacity(this.traceAvailable ? 1 : 0.4)
          .onClick(() => this.toggleTrace())
      }
      .padding(6)
      .border({ width: 1, color: '#66000000', radius: 6 })
      .backgroundColor('#CC000000')
      .position({ x: this.posX, y: this.posY })
      .width(`${OVERLAY_WIDTH_RATIO * 100}%`)
      .gesture(
        PanGesture()
          .onActionStart(() => {
            this.dragStartX = this.posX;
            this.dragStartY = this.posY;
          })
          .onActionUpdate((e: GestureEvent) => {
            this.posX = this.clampX(this.dragStartX + e.offsetX);
            this.posY = this.clampY(this.dragStartY + e.offsetY);
          })
      )
    } else {
      Column() {}.width('0%').height('0%')
    }
  }
}
