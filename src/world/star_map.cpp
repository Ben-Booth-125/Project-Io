#include "star_map.hpp"

// ---------------------------------------------------------------------------
// The authored sky. See star_map.hpp for the contract.
//
// Hand-placed rather than generated, and deliberately so: a generated sky would
// reroll with the campaign, and the point of this one is that it does not. The
// constellations are grouped loosely along a galactic band running across the
// middle of the projection, with the field thinning toward the poles, because
// that is what a galaxy looks like from inside one.
//
// Names are invented — the standing rule forbids drawing them from any Earth
// list, and the sky is no exception.
// ---------------------------------------------------------------------------

namespace {

// The bright, named stars. Ordered by constellation so the index blocks below
// stay readable; nothing depends on the order.
const star_map_star k_stars[] = {
    // --- The Ferryman (0-6), a long chain low in the galactic band -----------
    { "Vethra",    0.075f, 0.520f, 0.6f, star_class::blue   },
    { "Oskelin",   0.128f, 0.575f, 1.9f, star_class::white  },
    { "Marrend",   0.171f, 0.610f, 2.4f, star_class::yellow },
    { "Tsurel",    0.214f, 0.585f, 1.4f, star_class::white  },
    { "Ibhan",     0.262f, 0.540f, 3.1f, star_class::orange },
    { "Korrath",   0.309f, 0.505f, 2.2f, star_class::red    },
    { "Sevallis",  0.352f, 0.462f, 1.1f, star_class::blue   },

    // --- The Kiln (7-11), a compact quadrilateral with one outlier ----------
    { "Adruun",    0.455f, 0.395f, 1.7f, star_class::yellow },
    { "Pellith",   0.507f, 0.372f, 2.8f, star_class::white  },
    { "Nokhet",    0.523f, 0.437f, 2.1f, star_class::orange },
    { "Yssarine",  0.471f, 0.459f, 3.4f, star_class::red    },
    { "Drevan",    0.556f, 0.331f, 0.9f, star_class::blue   },

    // --- The Spindle (12-17), a near-vertical run crossing the band --------
    { "Callowen",  0.648f, 0.244f, 1.3f, star_class::white  },
    { "Rhesk",     0.661f, 0.318f, 2.6f, star_class::yellow },
    { "Ulvaine",   0.673f, 0.397f, 1.8f, star_class::blue   },
    { "Thanneth",  0.685f, 0.471f, 3.0f, star_class::orange },
    { "Xoreth",    0.694f, 0.548f, 2.3f, star_class::red    },
    { "Belisaur",  0.706f, 0.629f, 1.6f, star_class::white  },

    // --- The Drowned Crown (18-23), an arc high in the north ---------------
    { "Ashenvar",  0.812f, 0.196f, 1.2f, star_class::blue   },
    { "Quenneth",  0.858f, 0.168f, 2.7f, star_class::white  },
    { "Solvarr",   0.905f, 0.181f, 2.0f, star_class::yellow },
    { "Merrick",   0.941f, 0.228f, 3.3f, star_class::orange },
    { "Isvane",    0.952f, 0.291f, 2.5f, star_class::red    },
    { "Talgorrim", 0.925f, 0.349f, 1.5f, star_class::white  },

    // --- The Anvil (24-28), southern, squat and bright ----------------------
    { "Hurrowen",  0.198f, 0.782f, 0.8f, star_class::blue   },
    { "Cadresh",   0.256f, 0.812f, 2.9f, star_class::yellow },
    { "Velmoth",   0.318f, 0.798f, 1.7f, star_class::white  },
    { "Ondrave",   0.291f, 0.741f, 3.2f, star_class::orange },
    { "Sarrowin",  0.232f, 0.727f, 2.4f, star_class::red    },

    // --- The Lantern-Bearer (29-33), southern, trailing the Anvil ----------
    { "Ekhelan",   0.548f, 0.766f, 1.0f, star_class::white  },
    { "Vorrunde",  0.601f, 0.803f, 2.3f, star_class::blue   },
    { "Ymber",     0.643f, 0.762f, 3.5f, star_class::red    },
    { "Kasslith",  0.612f, 0.712f, 1.9f, star_class::yellow },
    { "Nethuun",   0.559f, 0.706f, 2.8f, star_class::orange },

    // --- Anonymous field stars, thinning toward the poles (34-59) ----------
    { "", 0.041f, 0.331f, 4.6f, star_class::white  },
    { "", 0.093f, 0.688f, 5.1f, star_class::red    },
    { "", 0.147f, 0.421f, 4.2f, star_class::yellow },
    { "", 0.206f, 0.658f, 5.4f, star_class::orange },
    { "", 0.239f, 0.294f, 4.8f, star_class::blue   },
    { "", 0.288f, 0.881f, 5.7f, star_class::red    },
    { "", 0.341f, 0.352f, 4.4f, star_class::white  },
    { "", 0.377f, 0.671f, 5.2f, star_class::yellow },
    { "", 0.408f, 0.512f, 4.1f, star_class::orange },
    { "", 0.437f, 0.238f, 5.5f, star_class::red    },
    { "", 0.489f, 0.617f, 4.7f, star_class::white  },
    { "", 0.531f, 0.868f, 5.9f, star_class::blue   },
    { "", 0.578f, 0.481f, 4.3f, star_class::yellow },
    { "", 0.619f, 0.126f, 5.6f, star_class::red    },
    { "", 0.666f, 0.842f, 5.0f, star_class::orange },
    { "", 0.718f, 0.386f, 4.5f, star_class::white  },
    { "", 0.744f, 0.708f, 5.3f, star_class::yellow },
    { "", 0.771f, 0.271f, 4.9f, star_class::blue   },
    { "", 0.803f, 0.591f, 5.8f, star_class::red    },
    { "", 0.836f, 0.452f, 4.0f, star_class::orange },
    { "", 0.871f, 0.744f, 5.5f, star_class::white  },
    { "", 0.897f, 0.096f, 6.0f, star_class::red    },
    { "", 0.923f, 0.617f, 4.6f, star_class::yellow },
    { "", 0.958f, 0.478f, 5.1f, star_class::blue   },
    { "", 0.981f, 0.742f, 5.7f, star_class::orange },
    { "", 0.017f, 0.882f, 5.9f, star_class::red    },
};
constexpr int k_star_count = static_cast<int>(sizeof(k_stars) / sizeof(k_stars[0]));

// Constellation membership, as indices into k_stars.
const int k_ferryman[]  = { 0, 1, 2, 3, 4, 5, 6 };
const int k_ferryman_e[] = { 0,1, 1,2, 2,3, 3,4, 4,5, 5,6 };

const int k_kiln[]      = { 7, 8, 9, 10, 11 };
const int k_kiln_e[]    = { 0,1, 1,2, 2,3, 3,0, 1,4 };

const int k_spindle[]   = { 12, 13, 14, 15, 16, 17 };
const int k_spindle_e[] = { 0,1, 1,2, 2,3, 3,4, 4,5 };

const int k_crown[]     = { 18, 19, 20, 21, 22, 23 };
const int k_crown_e[]   = { 0,1, 1,2, 2,3, 3,4, 4,5 };

const int k_anvil[]     = { 24, 25, 26, 27, 28 };
const int k_anvil_e[]   = { 0,1, 1,2, 2,3, 3,4, 4,0 };

const int k_lantern[]   = { 29, 30, 31, 32, 33 };
const int k_lantern_e[] = { 0,1, 1,2, 2,3, 3,4, 4,0 };

const star_map_constellation k_constellations[] = {
    { "The Ferryman",      k_ferryman, 7, k_ferryman_e, 6 },
    { "The Kiln",          k_kiln,     5, k_kiln_e,     5 },
    { "The Spindle",       k_spindle,  6, k_spindle_e,  5 },
    { "The Drowned Crown", k_crown,    6, k_crown_e,    5 },
    { "The Anvil",         k_anvil,    5, k_anvil_e,    5 },
    { "The Lantern-Bearer",k_lantern,  5, k_lantern_e,  5 },
};
constexpr int k_constellation_count =
    static_cast<int>(sizeof(k_constellations) / sizeof(k_constellations[0]));

// The deep sky. `seen_gya` is when the light of the event reached the campaign
// system — the number the biography's flavour lines date themselves from, so a
// remnant in the sky and the line about it agree.
const star_map_object k_objects[] = {
    { "Ith-Karan",          0.386f, 0.148f, 0.012f, deep_sky_kind::quasar,            0.0f  },
    { "The Ember Wake",     0.166f, 0.556f, 0.030f, deep_sky_kind::supernova_remnant, 2.85f },
    { "Sarrow's Lamp",      0.679f, 0.434f, 0.024f, deep_sky_kind::supernova_remnant, 1.12f },
    { "The Cold Sister",    0.598f, 0.744f, 0.021f, deep_sky_kind::supernova_remnant, 0.43f },
    { "The Weaver's Veil",  0.491f, 0.523f, 0.055f, deep_sky_kind::nebula,            0.0f  },
    { "Hollow Court",       0.855f, 0.262f, 0.040f, deep_sky_kind::nebula,            0.0f  },
    { "The Seed Hoard",     0.271f, 0.769f, 0.033f, deep_sky_kind::cluster,           0.0f  },
    { "Ashaneth's Wheel",   0.058f, 0.198f, 0.048f, deep_sky_kind::galaxy,            0.0f  },
};
constexpr int k_object_count = static_cast<int>(sizeof(k_objects) / sizeof(k_objects[0]));

} // namespace

const star_map_star* star_map_stars(int& count)
{
    count = k_star_count;
    return k_stars;
}

const star_map_constellation* star_map_constellations(int& count)
{
    count = k_constellation_count;
    return k_constellations;
}

const star_map_object* star_map_objects(int& count)
{
    count = k_object_count;
    return k_objects;
}

const char* star_class_name(star_class k)
{
    switch (k)
    {
        case star_class::blue:   return "blue";
        case star_class::white:  return "white";
        case star_class::yellow: return "yellow";
        case star_class::orange: return "orange";
        case star_class::red:    return "red";
    }
    return "?";
}

uint32_t star_class_rgb(star_class k)
{
    // Eyeballed toward real blackbody colours but pulled toward white: a star
    // rendered at its true chromaticity reads as a coloured dot rather than a
    // star, because the eye sees bright point sources as near-white.
    switch (k)
    {
        case star_class::blue:   return 0xCBD9FFu;
        case star_class::white:  return 0xF2F4FFu;
        case star_class::yellow: return 0xFFF3D6u;
        case star_class::orange: return 0xFFD9AEu;
        case star_class::red:    return 0xFFB894u;
    }
    return 0xFFFFFFu;
}

const char* deep_sky_kind_name(deep_sky_kind k)
{
    switch (k)
    {
        case deep_sky_kind::quasar:            return "quasar";
        case deep_sky_kind::supernova_remnant: return "supernova remnant";
        case deep_sky_kind::nebula:            return "nebula";
        case deep_sky_kind::cluster:           return "cluster";
        case deep_sky_kind::galaxy:            return "galaxy";
    }
    return "?";
}
