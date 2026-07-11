// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2022 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// TODO(fml): ConcurrentMessageLoop is being converted to a facade over the
// ConcurrentLoopBackend abstraction (see base/docs/concurrent_loop_backend_*).
// The previous std::thread-pool implementation has been ported to
// base/src/fml/concurrent_loop_backend_std.cc. This translation unit is
// intentionally a no-op stub for now; the facade wiring will be added in
// Task 5 of the plan. Keep the copyright headers above and the namespace
// below intact so downstream translation units that include this header
// (directly or transitively) continue to link cleanly during the migration.

namespace lynx {
namespace fml {
// Placeholder. Implementation lives in concurrent_loop_backend_std.cc.
}  // namespace fml
}  // namespace lynx