# Project Io — Explorer

There is **no Explorer surface** in Io. The Explorer was a specified placeholder — a pinning /
quick-navigation panel reserving the right middle shell band — and its band is not reserved:
the right middle of the shell is canvas. The diplomacy-as-communication surface that once took
that band, the **comms chat log** (`CHAT.md`, `src/ui/chat_panel.{hpp,cpp}`,
`docs/ai/AI_OPPONENT.md` § 7), lives in the **bottom-left dock**.

**Pinning is not a player affordance.** `highlight.hpp` carries a `pinned` tier and the palette
an amber slot for it, and nothing sets either. If a pinning affordance is ever wanted it is
designed fresh — chat-adjacent or Selection-driven — rather than reviving a reserved band. This
file exists so the name resolves; the original spec is in git history.
