#pragma once

#include "world/world.hpp"

#include <string>
#include <vector>

namespace ui {

/// One line in the comms log. `from == null_entity` marks a system line (epoch
/// notices), rendered dimmed. Otherwise `from` is either a NATION (BL-212: the
/// only author the Public channel ever shows — see step_economy's nation-voiced
/// aggregation) or a corporation (a Counsel channel, player-only), and the line
/// carries that entity's identity colour. Rendering resolves which kind `from`
/// is by trying the nation map first, then the corp map (see draw_chat_panel).
struct chat_message
{
    int         day;     ///< Sim day the message was posted (m_sim_loop.day_tick()).
    entity_id   from;    ///< Authoring nation or corp, or null_entity for a system line.
    int         channel; ///< Index into chat_state::channels.
    std::string text;
};

/// A comms channel. Channel 0 is always **Public** (`members` empty) — as of
/// BL-212 the only speakers ever posted there are NATIONS, never corporations,
/// so a rival's internals never leak through comms (DISCOVERY.md's
/// competitor-visibility rule). Further channels are player-created groups over
/// an arbitrary corp subset (BL-205 — "arbitrary groups can be made"), plus the
/// per-corp Counsel channels (BL-207); the player is an implicit member of any
/// group they create.
struct chat_channel
{
    std::string            name;
    std::vector<entity_id> members; ///< Empty = everyone (Public).
};

/// UI-side comms state, owned by app (session-local; not serialised in slice 1 —
/// messages re-derive from deterministic sim events, see backlog BL-205).
struct chat_state
{
    std::vector<chat_channel> channels{{"Public", {}}};
    std::vector<chat_message> messages;
    int  active_channel = 0;
    char input[192]     = {};

    // Group-creation draft (the "+" popup): proposed name + per-corp membership.
    char group_name[64] = {};
    std::vector<std::pair<entity_id, bool>> group_draft;
};

/// Append a message to the log, dropping the oldest lines beyond the cap.
///
/// @param chat    Comms state.
/// @param day     Sim day stamp.
/// @param from    Authoring corp, or null_entity for a system line.
/// @param channel Target channel index (clamped to Public if out of range).
/// @param text    Message body.
void chat_post(chat_state& chat, int day, entity_id from, int channel, std::string text);

/// Draw the comms panel — the channel-based chat log. It first replaced the
/// Explorer placeholder in the right shell band; BL-227 (2026-07-30) re-homed it to
/// the **bottom-left tile of the screen's bottom strip** (`comms_dock_rect`,
/// `foldout_column.hpp`), sharing the Selection band's top edge and height. One
/// control row (label + channel selector + the "+" group-create popup), a scrolling
/// message log for the active channel, and a message input. Player input posts to
/// the active channel with no mechanical effect yet — the hook the AI-opponent
/// C-route consumes later (docs/ai/AI_OPPONENT.md § 7).
///
/// Rows, not width, are the scarce axis in that host (BL-216): the log uses a
/// one-line-per-message form and the channels sit in a combo rather than a
/// silently-clipping row of tabs. See docs/ui/CHAT.md.
///
/// @param w      World (corp names + identity colours).
/// @param chat   Comms state to draw and mutate.
/// @param day    Current sim day (stamps player-sent messages).
/// @param x      Left edge of the panel (the icon rail's right edge).
/// @param y      Top edge of the panel (shared with the Selection band).
/// @param width  Panel width (three quarters of the fold-out column's).
/// @param height Panel height (the Selection band's, by design).
void draw_chat_panel(const world& w, chat_state& chat, int day,
                     float x, float y, float width, float height);

} // namespace ui
