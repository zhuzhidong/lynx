// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { useState, type ReactElement } from "@lynx-js/react";
import "./index.scss";

export interface IntensityLevel {
  label: string;
  /** Human-readable summary of what this level does, shown under the buttons. */
  hint: string;
}

interface IntensityToggleProps {
  levels: IntensityLevel[];
  /** Called when the selected level changes (0-based index). */
  onChange: (index: number) => void;
  /** Optional initial selection (default 0 = Low). */
  initial?: number;
}

/**
 * A Low/Med/High (or custom) intensity selector shared by every jank scene.
 * Lets the same scene reproduce a range of jank severity for perf testing.
 */
export function IntensityToggle(props: IntensityToggleProps): ReactElement {
  const [selected, setSelected] = useState<number>(props.initial ?? 0);

  const select = (index: number) => {
    setSelected(index);
    props.onChange(index);
  };

  return (
    <view className="toggle">
      <view className="toggle__buttons">
        {props.levels.map((level, index) => (
          <view
            key={level.label}
            className={
              index === selected
                ? "toggle__btn toggle__btn--active"
                : "toggle__btn"
            }
            bindtap={() => select(index)}
          >
            <text className="toggle__btn-text">{level.label}</text>
          </view>
        ))}
      </view>
      <text className="toggle__hint">{props.levels[selected].hint}</text>
    </view>
  );
}
