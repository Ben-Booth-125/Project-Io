#pragma once

#include <cstdint>

/// Opaque integer handle for a simulation entity.
/// Zero (null_entity) is reserved as an invalid sentinel.
using entity_id = uint32_t;

static constexpr entity_id null_entity = 0;
