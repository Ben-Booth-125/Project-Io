# Project Io — Claude Reference

Project Io is a near-future space-based 4X grand strategy game. The player controls a corporate entity competing through resource extraction, trade, and military conflict across an Earth-like solar system. The project is in prototype phase, solo-developed in C++ with Lua scripting.

Read the documents below before responding to any request. They are the authoritative source for all design and technical decisions.

---

## Documents

**`docs/CONCEPT.md`**
The player identity, core mechanics, and campaign design. Start here for questions about what the game is and how it should feel.

**`docs/SYSTEMS.md`**
Every game system, how they relate to one another, and which are load-bearing. Read this for questions about scope, system design, or how a feature fits into the whole.

**`docs/GLOSSARY.md`**
Canonical definitions for all project terms. Use these terms consistently. If a term is defined here, do not substitute alternatives.

**`docs/tech/TECH_FOUNDATIONS.md`**
All settled technical decisions: language, framework, architecture, tick model, data model, UI approach, and serialisation. Read this before writing any code or making any architectural suggestion. It also defines the prototype scope and what is explicitly excluded.

**`docs/development/INITIAL_INSTRUCTIONS.md`**
The prototype build sequence and the constraints that govern development assistance. Read this alongside TECH_FOUNDATIONS when working on implementation.

**`docs/development/DEVELOPMENT_PRACTICES.md`**
Testing framework (Catch2), naming conventions, documentation standards, and how Claude should handle each.

**`docs/development/DEVLOG.md`**
Running session log — chronological record of what was built each session and
in-session decisions. Consult when asked about prior work, open items, or why
a specific implementation choice was made.