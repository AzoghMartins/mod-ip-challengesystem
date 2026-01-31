# Testing (GM Commands + Dev Auras)

The challenge system now reads active tier/flags from `character_settings`.
Use GM commands for testing; auras are a fallback override.

## GM commands (preferred)

Commands apply to the selected player if one is targeted, otherwise to yourself.

- `.ipchallenge set tier <0-3> flags <mask>`
  - Example: `.ipchallenge set tier 1 flags 5`
- `.ipchallenge clear`
- `.ipchallenge status`

Flag bitmask (locked):
- Hardcore = 1
- Solo Only = 2
- No Trade = 4
- No Mail = 8
- No AH = 16
- No Summons = 32
- Permadeath = 64

## Aura override (DEV fallback)

If a test aura is configured AND the player has that aura, the restriction is treated as active.

Set these keys in `mod-ip-challengesystem.conf`:
- `ChallengeSystem.TestAura.Hardcore`
- `ChallengeSystem.TestAura.SoloOnly`
- `ChallengeSystem.TestAura.NoTrade`
- `ChallengeSystem.TestAura.NoMail`
- `ChallengeSystem.TestAura.NoAuction`
- `ChallengeSystem.TestAura.NoSummons`
- `ChallengeSystem.TestAura.Permadeath`

Usage:
1) Apply the aura to a character:
   - `.aura <spellId>`
2) Remove when done:
   - `.unaura <spellId>`

## Permadeath behavior + config

When Permadeath is active and the configured death context counts, the character is marked dead in
`ip_permadeath`, release/resurrection are denied during the kick delay, and the session is disconnected.
Permadead characters are locked out on login.
If the player releases spirit during the pending window, the ghost is teleported to the configured
Permadeath.Ghost* location.

Config keys:
- `ChallengeSystem.Permadeath.Enable`
- `ChallengeSystem.Permadeath.KickDelaySeconds`
- `ChallengeSystem.Permadeath.Broadcast`
- `ChallengeSystem.Permadeath.CountPvPDeaths`
- `ChallengeSystem.Permadeath.CountBattlegroundDeaths`
- `ChallengeSystem.Permadeath.CountArenaDeaths`
- `ChallengeSystem.Permadeath.CountDuelDeaths`
- `ChallengeSystem.Permadeath.GhostMap`
- `ChallengeSystem.Permadeath.GhostX`
- `ChallengeSystem.Permadeath.GhostY`
- `ChallengeSystem.Permadeath.GhostZ`
- `ChallengeSystem.Permadeath.GhostO`

## Notes

Warning: `NO_SUMMONS` currently blocks summon accepts (e.g., warlock/meeting stone) via the teleport hook.
Some teleport/portal paths may bypass this until deeper hooks are added. TODO: expand summon/portal detection coverage.
