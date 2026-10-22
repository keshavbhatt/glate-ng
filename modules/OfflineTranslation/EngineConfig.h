#pragma once

#include "ModelStore.h"

#include <string>

namespace glate::offline {

// The engine's YAML configuration for one installed direction. The settings
// match what Firefox uses for the same models (greedy decoding, soft
// alignment off the hot path, paragraph sentence splitting).
[[nodiscard]] std::string engineConfig(const InstalledModel& model);

} // namespace glate::offline
