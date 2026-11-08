#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2025 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
"""Lynx Explorer 页面滚动场景驱动(scenario driver script 模式)。

契约(reference/scenario_driver.md §1):
- argv: {sn} {report_dir} {page_url}(scenario.py 占位符替换后传入);
- env FFRT_UNDER_TRACE=1 时遵循 cold_start=scenario 语义:窗口内重启即冷启动;
- stdout 仅一条 SCENARIO_WINDOW <t0> <t1>(UTC epoch 秒);
- exit 0 = 本轮场景成功。

场景: cold_start=scenario 模式——窗口内 force-stop 后 aa start 携带
lynx_initial_url 冷启动直达目标页(Index.ets aboutToAppear 仅在创建时
消费该参数,onNewWant 重入不触发跳转,故必须冷启动)→ 等首帧布局 →
6 次上滑浏览。窗口覆盖: 冷启动加载(排版热点) + 滚动。
"""

import re
import subprocess
import sys
import time

BUNDLE = "org.lynxjs.explorer"
ABILITY = "EntryAbility"

LOAD_WAIT_S = 4.0     # 首帧 + 布局完成等待
SWIPE_COUNT = 6       # 上滑次数
SWIPE_INTERVAL_S = 1.2
SWIPE_SPEED_PPS = 1200  # uitest uiInput swipe 速度(px/s)


def hdc(sn, *args, timeout=30):
    cmd = ["hdc", "-t", sn, "shell", *args]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)


def get_display_size(sn):
    out = hdc(sn, "hidumper", "-s", "RenderService", "-a", "screen").stdout
    m = re.search(r"render resolution=(\d+)x(\d+)", out)
    if m:
        return int(m.group(1)), int(m.group(2))
    return 1316, 2832


def main():
    sn = sys.argv[1]
    page_url = sys.argv[3]
    width, height = get_display_size(sn)

    # 场景初态:冷启动直达 markdown 页(cold_start=scenario,窗口内重启被测
    # app,pid 变化由 scenario.py 看门狗时间线记录)
    hdc(sn, "aa", "force-stop", BUNDLE)
    time.sleep(1.0)
    hdc(sn, "aa", "start", "-a", ABILITY, "-b", BUNDLE,
        "--ps", "lynx_initial_url", page_url)
    t0 = time.time()

    time.sleep(LOAD_WAIT_S)

    cx = width // 2
    y_from = int(height * 0.75)
    y_to = int(height * 0.35)
    for _ in range(SWIPE_COUNT):
        hdc(sn, "uitest", "uiInput", "swipe",
            str(cx), str(y_from), str(cx), str(y_to), str(SWIPE_SPEED_PPS))
        time.sleep(SWIPE_INTERVAL_S)

    t1 = time.time()
    print(f"SCENARIO_WINDOW {t0:.2f} {t1:.2f}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
