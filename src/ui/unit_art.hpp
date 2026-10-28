#pragma once

// ---------------------------------------------------------------------------
// Unit art: one authored image per land class, and one anchor per kit slot
// (docs/ui/ledgers/military.md § The designer).
//
// THE FILES. `assets/units/<class>.png` is the class's image; `assets/units/
// <class>.anchors` beside it names, one line per slot, where on the image that
// slot's equipment sits:
//
//     # slot  x     y        (image-relative, 0..1 from the top-left)
//     0       0.78  0.38
//
// The images in the tree are placeholders (`tools/art/make_unit_placeholders.py`
// draws a labelled silhouette per class and writes both files). Replacing one with
// real art is replacing the PNG and, if the parts moved, its .anchors file: no
// code changes. A missing or unreadable image draws an outlined placeholder panel;
// a missing anchor falls back to a point spread down the image's middle.
//
// Loaded once at start-up by app (it owns the renderer); the UI only reads.
// ---------------------------------------------------------------------------

#include "world/unit_roster.hpp" // land_class

struct SDL_Renderer;

namespace ui {

/// One class's art as the UI draws it.
struct unit_art
{
    void* tex    = nullptr; ///< ImTextureID for the SDLRenderer3 backend (an SDL_Texture*); null = no image.
    int   width  = 0;       ///< Pixels; 0 with no image.
    int   height = 0;
    /// Image-relative anchor per slot, 0..1. Always filled for the class's slots.
    float anchor_x[3] = {0.5f, 0.5f, 0.5f};
    float anchor_y[3] = {0.3f, 0.5f, 0.7f};
};

/// The file stem for @p c ("infantry", "armour", ...); "" for `none`.
const char* unit_art_stem(land_class c);

/// Load every class's image and anchors from `assets/units/` (cwd-relative, as the
/// fonts are). Logs what it could not load and carries on. Safe to call twice.
void unit_art_load(SDL_Renderer* r);

/// Destroy the textures. Call before the renderer goes.
void unit_art_release();

/// The art for @p c. Never null; an unloaded class has no texture.
const unit_art& unit_art_for(land_class c);

} // namespace ui
