#pragma once

// BL-1125 (markets can die) — the reading definitions market_census and
// market_gravity_ladder share, so "a major city with no market" means one thing
// in both. READINGS, not rules: nothing in src/world reads these.

/// A MAJOR city: a population centre at scale 3 (city) or above. The curated
/// seeds carry almost no metropolis (scale 4: two pooled over 16 seeds), so the
/// aim "about one market per major city" can only be read at 3.
constexpr int k_major_city_scale = 3;

/// A major city HAS a market when a standing market's centre lies within this
/// many grid tiles of it (column wrapped) — the carve's own proxy radius, the
/// distance a market's site may sit from the centre that seeded it.
constexpr double k_major_city_radius = 8.0;
