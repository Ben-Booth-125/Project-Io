-- Home-body census: who actually holds ground where the player stands.
--
-- Written 2026-08-28 to settle a question the Corporation/Company lens split
-- raised rather than answered. After the split the Company lens (background
-- firms) came back populated and the Corporation lens (player + rivals) came
-- back nearly EMPTY on the home body — the opposite of what was expected. The
-- combined lens had been hiding that, because it drew every non-player corp and
-- the background firms filled it in.
--
-- PARKED BY LOCATION, deliberately: this is a diagnostic, not a check. It
-- asserts nothing and would only slow --verify-all down. Run it by hand:
--   build/ProjectIo.exe --verify scripts/verify/parked/home_body_census.lua
--
-- It reports through error() because the verify Lua sandbox exposes no print or
-- log binding — the message is the product, and a non-zero exit is expected.

verify.goto_surface("home")
verify.econ_step(12)

-- Which body is "home" is not exposed directly, but every building row carries
-- its body, and goto_surface("home") has already put us on it: the body under
-- the player's own holdings is the home body by construction.
local home_body = nil
for _, b in ipairs(verify.buildings()) do
    if b.player then home_body = b.body; break end
end

local n_player, n_rival, n_company = 0, 0, 0
local corps_rival, corps_company = {}, {}
local n_rival_corps, n_company_corps = 0, 0
-- Off-body totals too: an empty Corporation lens on THIS body says nothing about
-- whether rivals exist at all, and that is the distinction that matters.
local off_rival, off_company = 0, 0
local all_rival, all_company = {}, {}
local n_all_rival, n_all_company = 0, 0

for _, b in ipairs(verify.buildings()) do
    local on_home = (b.body == home_body)
    if b.player then
        if on_home then n_player = n_player + 1 end
    elseif b.background then
        if on_home then
            n_company = n_company + 1
            if not corps_company[b.corp] then corps_company[b.corp] = true; n_company_corps = n_company_corps + 1 end
        else
            off_company = off_company + 1
        end
        if not all_company[b.corp] then all_company[b.corp] = true; n_all_company = n_all_company + 1 end
    else
        if on_home then
            n_rival = n_rival + 1
            if not corps_rival[b.corp] then corps_rival[b.corp] = true; n_rival_corps = n_rival_corps + 1 end
        else
            off_rival = off_rival + 1
        end
        if not all_rival[b.corp] then all_rival[b.corp] = true; n_all_rival = n_all_rival + 1 end
    end
end

error("HOME-BODY CENSUS"
   .. " || ON HOME BODY -- player buildings: " .. n_player
   .. " | rival buildings: " .. n_rival .. " across " .. n_rival_corps .. " corporation(s)"
   .. " | background-firm buildings: " .. n_company .. " across " .. n_company_corps .. " company(s)"
   .. " || ELSEWHERE -- rival buildings: " .. off_rival
   .. " | background-firm buildings: " .. off_company
   .. " || WORLD TOTALS -- corporations with any holding: " .. n_all_rival
   .. " | companies with any holding: " .. n_all_company)
