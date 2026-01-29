# KardinalWoW — Challenge System Module (V1)

**Goal:** Add an opt-in, server-side Challenge System for KardinalWoW without any client modifications (no MPQs). Players can select multiple challenges at tier entry points. Challenges are composed from atomic restrictions. Rewards (Marks/titles) are out of scope for V1 design details, but the system must record milestones cleanly.

---

## 1. Non-Negotiables

* **No client modifications** required.
* **Quest-driven UX** for selection and confirmation.
* **Atomic restrictions** (single-purpose rules) compose into **Challenges (presets)**.
* **Multiple challenges can stack** if no conflicts.
* **Tiered runs**:

  * **Tier I:** Level 1 → 60
  * **Tier II:** Level 60 → 70
  * **Tier III:** Level 70 → 80
* **Selection allowed only at tier entry** (Level 1 / 60 / 70).
* **No opt-out once confirmed** (commitment).
* Enforcement prioritizes:

  1. **Block** invalid actions
  2. **Warn** if an action attempt is blocked
  3. **Grace timer** only when blocking is not possible (rare)
  4. Only **death** fails Hardcore; rule-breaking should not fail a run (generally)

---

## 2. Core Concepts

### 2.1 Atomic Restriction

A single enforceable rule that applies during a tier run.

* Has a stable **RestrictionID**
* Declares which server hooks it needs
* Defines response type: **BLOCK**, **WARN**, **GRACE_TIMER**

### 2.2 Challenge Preset

A named bundle of atomic restrictions.

* Convenience for players
* No special enforcement logic
* Players may select multiple presets; server resolves a **union** of restrictions

### 2.3 Tier Run

A run is tracked per tier, per character.

* Status: `NotStarted | Selecting | Active | Completed | Failed`
* Stores restriction set snapshot (resolved)
* Stores start/end time, and optional failure reason

---

## 3. V1 Challenge Presets

Players may stack any of these at tier entry:

1. **Hardcore**

   * `PERMADEATH`
   * `NO_PLAYERBOTS` *(with DF/LFG exemption; see section 4.5)*
   * `HC_MANUAL_GROUP_ONLY_WITH_HC_TIER` *(DF/LFG groups exempt)*

2. **Solo Self-Found (SSF)**

   * `NO_TRADE`
   * `NO_MAIL`
   * `NO_AUCTION`
   * `NO_GUILD_BANK` *(only if guild bank is enabled)*

3. **Lone Adventurer**

   * `SOLO_ONLY`
   * `NO_SUMMONS`

4. **Ascetic**

   * `NO_CONSUMABLES_IN_COMBAT`
   * `NO_ELIXIRS_FLASKS`

---

## 4. Atomic Restrictions (V1)

### 4.1 Economy / Interaction

* **NO_TRADE**

  * Blocks opening/accepting player trade.

* **NO_MAIL**

  * Blocks sending and receiving mail.

* **NO_AUCTION**

  * Blocks browsing, bidding, buyout, and posting auctions.

* **NO_GUILD_BANK** *(conditional)*

  * Blocks guild bank deposit/withdraw.

### 4.2 Grouping / Movement

* **SOLO_ONLY**

  * Blocks party/raid invites and acceptance.
  * If already in group (edge-case): apply grace timer to leave.

* **NO_SUMMONS**

  * Blocks accepting summons.

* **HC_MANUAL_GROUP_ONLY_WITH_HC_TIER**

  * Only relevant if Hardcore is active.
  * Blocks **manual** invites/accepts with non-HC-tier players.
  * **DF/LFG groups are exempt** (Hardcore players may participate in DF/LFG groups).
  * Edge-case: if a mixed manual group exists anyway, apply grace timer and auto-correct if possible.

### 4.3 Combat / Power

* **NO_CONSUMABLES_IN_COMBAT**

  * Blocks use of potions/bandages/healthstones while in combat.
  * Prefer BLOCK/REVERT + warning message.

* **NO_ELIXIRS_FLASKS**

  * Blocks consumption of elixirs and flasks.

### 4.4 Death

* **PERMADEATH**

  * On player death: fail the current tier run (Hardcore only).
  * Character continues normally; enforcement stops for that tier.

### 4.5 Playerbots

* **NO_PLAYERBOTS**

  * If Hardcore active: disallow **recruited/summoned/added** bots and bot-control commands that provide combat advantage.
  * **Dungeon Finder / LFG groups are allowed** for Hardcore even if they contain RNDBOT-style bots, because these bots cannot be re-specced/auto-geared and cannot be freely summoned on demand.
  * Players may still group with their own alts if those characters are logged in normally and also Hardcore-tier.

**Implementation guidance (no core/playerbots edits):**

* Intercept bot recruitment/add/login commands at the command entry (e.g. `.playerbots bot add|addaccount|addclass|login`) before they reach PlayerbotMgr.
* Filter bot-control chat commands that grant combat advantage before they reach PlayerbotAI (attack/cast/cheat/max dps/etc.).
* Detect DF/LFG-created groups and exempt them from `HC_MANUAL_GROUP_ONLY_WITH_HC_TIER` checks.

---

## 5. Hardcore Guild Strategy

Hardcore uses a guild for social visibility + enforcement simplicity.

### 5.1 Guild Identity

* Create a guild named **Hardcore** (or tiered guilds if needed later: `Hardcore (T1)`, etc.)

### 5.2 Enrollment

When a player **confirms** Hardcore for a tier:

* Auto-invite / auto-add player to **Hardcore** guild
* Mark `HC_ACTIVE_TIER = {1|2|3}` on character

### 5.3 While Hardcore Tier is Active

* Leaving guild should be blocked or immediately re-applied (implementation choice).
* Grouping rules:

  * **BLOCK** inviting non-HC-tier players
  * **BLOCK** accepting invites from non-HC-tier players
  * If for any reason a mixed group is formed:

    * Start **grace timer** (e.g., 30–60 seconds)
    * Notify: “Hardcore rule: Leave group immediately.”
    * If not resolved by timer: auto-kick (preferred) or apply escalating warnings

### 5.4 Tier Matching

* Grouping allowed only with players with the **same HC tier active**.

---

## 6. Player UX (Quest-Driven)

### 6.1 Visibility (no edits to existing NPCs)

* On first login (Tier I) and upon reaching level 60/70 (Tier II/III):

  * Auto-add a breadcrumb quest: **“The Trials Await”** → directs player to the Challenge Registrar.

### 6.2 Challenge Registrar NPCs

Faction-neutral new NPC(s), not modifying existing NPCs.

Placement plan:

* **Tier I:** One Registrar per starting zone (maximum visibility)
* **Tier II:** Near Dark Portal / Stair of Destiny / Shattrath-adjacent
* **Tier III:** Near Borean/Howling start points or major Northrend transit hub

### 6.3 Selection Flow per Tier

1. **Choose Your Trials (Tier X)** (intro quest)
2. Player accepts one or more **Trial** quests (one per preset)
3. **Confirm Your Trials (Tier X)** locks the tier

Important: quests are **UI only**. The source of truth is **server-side run state**.

---

## 7. State & Data Model (draft)

### 7.1 Character Tier Run

Per character, per tier:

* `tier` (1/2/3)
* `status` (NotStarted/Selecting/Active/Completed/Failed)
* `restrictions_mask` (or normalized table)
* `started_at`, `ended_at`
* `failure_reason` (nullable)

### 7.2 Milestones

Record minimal milestones for V1:

* Tier started
* Level threshold reached (tier completion at 60/70/80)
* Hardcore failed (death)

---

## 8. Enforcement Hooks (draft)

Minimal hook list for V1:

* Login / FirstLogin
* LevelChanged
* Death
* GroupInvite / GroupAccept / GroupJoin *(distinguish manual groups vs DF/LFG)*
* TradeStart
* MailSend / MailReceive
* Auction actions (browse/bid/buyout/post)
* Summon accept
* ItemUse (consumables)
* Elixir/Flask use
* Playerbot recruit/summon/command entry points

---

## 9. Guard Rails & Messaging (policy)

* Prefer **BLOCK + brief message** (“Challenge rule: Auction House disabled.”)
* If blocking is not feasible, use **Grace Timer**:

  * Start timer
  * Repeated warning
  * Auto-correct (kick/leave/revert) if possible
* Do not fail runs for rule attempts unless explicitly defined (Hardcore fails only on death).

---

## 10. Open Decisions (to resolve before implementation)

1. **Guild leave enforcement:** hard block vs immediate re-invite.
2. **Grace timer duration** for mixed group edge-case.
3. **NO_CONSUMABLES_IN_COMBAT scope:** which items are included (potions, bandages, healthstones; exclude food outside combat).
4. **NO_ELIXIRS_FLASKS scope:** define item classes/IDs or aura categories.
5. **Guild bank usage:** confirm enabled/used in your setup.
6. **Registrar placement coordinates** (list by race/zone).

---

## 11. Next Steps

1. Convert this design into a module repository structure (AzerothCore module).
2. Define DB migration SQL (tables + initial NPC + quests).
3. Implement ChallengeManager skeleton + restriction registry.
4. Implement V1 restrictions in order: economy → grouping → consumables → hardcore death → playerbot locks.
5. Draft Codex-Sable prompt that describes architecture + acceptance criteria.
