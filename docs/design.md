# mod-ip-challengesystem — V1 Design (Living Document)

## Goal
Introduce an opt-in, server-side Challenge System for KardinalWoW without client modifications. Players can stack multiple challenges. Challenges are tiered to align with Individual Progression so raid plateaus are safe from Hardcore failure.

---

## Non-Negotiables
- No client modifications (no MPQs, no required addons).
- Quest-driven UX for challenge selection and confirmation.
- Atomic restrictions compose into challenge presets.
- Multiple presets may be selected and stacked per tier.
- Tiered runs:
  - Tier I: Level 1 → 60
  - Tier II: Level 60 → 70
  - Tier III: Level 70 → 80
- Selection only at tier entry (1 / 60 / 70).
- No opt-out once confirmed (commitment).
- Enforcement priority: BLOCK > WARN > GRACE TIMER (only when blocking isn’t feasible).
- Hardcore fails only on death; rule attempts should not fail a run.

---

## Core Concepts

### Atomic Restriction
A single enforceable rule. Restrictions define:
- RestrictionID
- Required hooks
- Response (BLOCK/WARN/GRACE TIMER)

### Challenge Preset
A named bundle of restrictions (convenience only). No special enforcement logic.

### Tier Run
Tracked per character, per tier:
- Status: NotStarted / Selecting / Active / Completed / Failed
- Resolved restriction set snapshot
- Start/end times
- Optional failure reason

---

## V1 Presets (Stackable)

### Hardcore
- PERMADEATH
- NO_PLAYERBOTS
- GROUP_ONLY_WITH_HC_TIER
- Implemented with Hardcore guild strategy (below)

### Solo Self-Found (SSF)
- NO_TRADE
- NO_MAIL
- NO_AUCTION
- NO_GUILD_BANK (if enabled)

### Lone Adventurer
- SOLO_ONLY
- NO_SUMMONS

### Ascetic
- NO_CONSUMABLES_IN_COMBAT
- NO_ELIXIRS_FLASKS

---

## Atomic Restrictions (V1)

### Economy / Interaction
- NO_TRADE — block player trade
- NO_MAIL — block send/receive mail
- NO_AUCTION — block browse/bid/buyout/post
- NO_GUILD_BANK — block deposit/withdraw (if enabled)

### Grouping / Movement
- SOLO_ONLY — block party/raid invites/accept; grace timer if already grouped via edge-case
- NO_SUMMONS — block accepting summons
- GROUP_ONLY_WITH_HC_TIER — only relevant when Hardcore active; block grouping with non-HC or different tier; grace timer if mixed group exists anyway

### Combat / Power
- NO_CONSUMABLES_IN_COMBAT — block/revert potions/bandages/healthstones while in combat
- NO_ELIXIRS_FLASKS — block consumption of elixirs/flasks

### Death
- PERMADEATH — on death, fail current tier run (Hardcore only). Character continues normally; enforcement stops for that tier.

### Playerbots
- NO_PLAYERBOTS — if Hardcore active: disallow bot recruit/summon and bot commands that provide combat advantage
- Players may still group with their own alts if logged in normally and also HC for the same tier.

---

## Hardcore Guild Strategy

### Overview
Hardcore uses a guild for visibility and for easier grouping enforcement.

### Enrollment
On Hardcore confirmation for a tier:
- Auto-add to guild “Hardcore”
- Set HC_ACTIVE_TIER (1/2/3)

### While Hardcore Active
- Block invites/accepts with non-HC-tier players
- If a mixed group exists anyway:
  - start grace timer (e.g. 45s)
  - warn player to leave immediately
  - auto-kick as fallback if not resolved

### Tier Matching
Grouping allowed only with same HC tier active.

---

## Player UX (Quest-driven)

### Visibility
At tier entry points:
- Tier I: on first login auto-add breadcrumb quest “The Trials Await”
- Tier II: on reaching level 60 auto-add breadcrumb quest
- Tier III: on reaching level 70 auto-add breadcrumb quest

### Registrar NPCs (neutral, new)
Do not modify existing NPCs.
- Tier I: one registrar per race starting zone (max visibility)
- Tier II: near Dark Portal / Stair of Destiny / Shattrath-adjacent
- Tier III: near Northrend entry hubs

### Selection Flow per Tier
1) Choose Your Trials (Tier X)
2) Accept one or more “Trial: <Preset>” quests (sets restrictions)
3) Confirm Your Trials (Tier X) — locks run and freezes restrictions

Important: quests are UI; server-side run state is the authority.

---

## Data Model (draft)
Per character, per tier:
- tier (1/2/3)
- status (NotStarted/Selecting/Active/Completed/Failed)
- restrictions_mask (or normalized rows)
- started_at, ended_at
- failure_reason (nullable)

Minimal milestones V1:
- tier started
- completion at 60/70/80
- Hardcore fail on death

---

## Enforcement Hooks (draft)
- Login / FirstLogin
- LevelChanged
- Death
- GroupInvite / GroupAccept / GroupJoin
- TradeStart
- MailSend / MailReceive
- Auction actions
- Summon accept
- ItemUse (consumables)
- Playerbot recruit/summon/command entry points

---

## Open Decisions
- Guild leave policy: block vs auto-reinvite
- Grace timer duration
- Exact consumable and elixir/flask inclusion lists
- Guild bank enabled/used?
- Registrar NPC placement coordinates
