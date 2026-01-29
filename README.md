# mod-ip-challengesystem

A tiered, opt-in Challenge System for AzerothCore (3.3.5a), designed to work
hand-in-hand with the **Individual Progression** module.

This module allows players to voluntarily take on additional restrictions
(Hardcore, Solo Self-Found, Solo-only, Ascetic, etc.) in exchange for prestige,
recognition, and future rewards — all **without any client modifications**.

---

## Core Design Goals

- No client mods, no MPQs, no addons required
- Quest-driven UX using native WoW systems
- Atomic, server-enforced restrictions
- Multiple challenges may be stacked
- Tiered challenge runs aligned with Individual Progression
- Hardcore mode that respects raid progression plateaus
- Playerbot-aware design (no trivialized Hardcore runs)

---

## Challenge Tiers

Challenges are divided into **three independent tiers**:

| Tier | Level Range | Expansion Scope |
|-----|-------------|-----------------|
| Tier I | 1 → 60 | Classic |
| Tier II | 60 → 70 | The Burning Crusade |
| Tier III | 70 → 80 | Wrath of the Lich King |

Players may opt into challenges **only at the start of a tier**.
Once confirmed, challenges **cannot be disabled** for that tier.

Failure in one tier does not affect completed tiers.

---

## V1 Challenge Presets

Players may stack any number of these presets when starting a tier.

### Hardcore
- Permadeath (death fails the current tier)
- Playerbot usage disabled
- Grouping restricted to other Hardcore players of the same tier

### Solo Self-Found (SSF)
- No trading
- No mail
- No auction house
- No guild bank (if enabled)

### Lone Adventurer
- No grouping or raids
- No summons

### Ascetic
- No consumables in combat
- No elixirs or flasks

---

## Enforcement Philosophy

- Invalid actions are **blocked whenever possible**
- Players receive clear warnings when rules prevent an action
- Grace timers are used only when blocking is not technically feasible
- Hardcore runs fail **only on character death**, not on rule attempts

---

## Player Experience

- Challenge selection is handled via **quests**
- A neutral **Challenge Registrar NPC** is used
- Players are guided via breadcrumb quests at tier entry points
- Existing NPCs and quest flows are not modified

---

## Requirements

- AzerothCore (3.3.5a)
- Individual Progression module
- PlayerBots module (optional, supported)

---

## Installation (when available)

1. Place the module in `modules/mod-ip-challengesystem`
2. Re-run CMake and rebuild the core
3. Import SQL files from `sql/`
4. Start the server

---

## Status

🚧 **Early development**

- V1 design locked
- Implementation in progress

See `/docs/design.md` for the full living design document.

---

## License

MIT

