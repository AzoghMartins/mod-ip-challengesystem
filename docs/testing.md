# Testing (GM Commands + Dev Auras)

The challenge system now reads active tier/flags from `character_settings`.
Use GM commands for testing; auras are a fallback override.

## GM commands (preferred)

Commands apply to the selected player if one is targeted, otherwise to yourself.

- `.ipchallenge set tier <0-3> flags <mask>`
  - Example: `.ipchallenge set tier 1 flags 5`
- `.ipchallenge clear`
- `.ipchallenge status`
- `.ipchallenge createguild`

Flag bitmask (locked):
- Hardcore = 1
- Solo Only = 2
- No Trade = 4
- No Mail = 8
- No AH = 16
- No Summons = 32
- Permadeath = 64
- Low Quality Only = 128
- Self Crafted = 256
- Poverty = 512
- No Guild Bank = 1024
- No Mounts = 2048
- No Buffs = 4096
- No Talents = 8192
- No Quest XP = 16384
- Only Quest XP = 32768
- Half XP = 65536
- Quarter XP = 131072
- No Bots = 262144

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
- `ChallengeSystem.TestAura.LowQualityOnly`
- `ChallengeSystem.TestAura.SelfCrafted`
- `ChallengeSystem.TestAura.Poverty`
- `ChallengeSystem.TestAura.NoGuildBank`
- `ChallengeSystem.TestAura.NoMounts`
- `ChallengeSystem.TestAura.NoBuffs`
- `ChallengeSystem.TestAura.NoTalents`
- `ChallengeSystem.TestAura.NoQuestXP`
- `ChallengeSystem.TestAura.OnlyQuestXP`
- `ChallengeSystem.TestAura.HalfXP`
- `ChallengeSystem.TestAura.QuarterXP`
- `ChallengeSystem.TestAura.NoBots`

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
- `ChallengeSystem.Permadeath.BroadcastMessage`
- `ChallengeSystem.Permadeath.CountPvPDeaths`
- `ChallengeSystem.Permadeath.CountBattlegroundDeaths`
- `ChallengeSystem.Permadeath.CountArenaDeaths`
- `ChallengeSystem.Permadeath.CountDuelDeaths`
- `ChallengeSystem.Permadeath.GhostMap`
- `ChallengeSystem.Permadeath.GhostX`
- `ChallengeSystem.Permadeath.GhostY`
- `ChallengeSystem.Permadeath.GhostZ`
- `ChallengeSystem.Permadeath.GhostO`

## Restriction tuning config

- `ChallengeSystem.LowQualityOnly.MaxQuality`
- `ChallengeSystem.Poverty.GoldCap.Tier1`
- `ChallengeSystem.Poverty.GoldCap.Tier2`
- `ChallengeSystem.Poverty.GoldCap.Tier3`
- `ChallengeSystem.XP.HalfMultiplier`
- `ChallengeSystem.XP.QuarterMultiplier`
- `ChallengeSystem.NoBuffs.AllowPassive`
- `ChallengeSystem.NoBuffs.AllowSpells`
- `ChallengeSystem.NoBuffs.ScanIntervalMs`
- `ChallengeSystem.Hardcore.AllowLfg`
- `ChallengeSystem.SoloOnly.AllowLfg`
- `ChallengeSystem.Hardcore.GuildName`

## Message overrides

- `ChallengeSystem.Message.GroupBlocked`
- `ChallengeSystem.Message.TradeBlocked`
- `ChallengeSystem.Message.MailBlocked`
- `ChallengeSystem.Message.AuctionBlocked`
- `ChallengeSystem.Message.SummonBlocked`
- `ChallengeSystem.Message.EquipBlocked`
- `ChallengeSystem.Message.GuildBankBlocked`
- `ChallengeSystem.Message.ResurrectBlocked`
- `ChallengeSystem.Message.Permadeath.Lockout`
- `ChallengeSystem.Message.HardcoreGuildJoined`
- `ChallengeSystem.Message.HardcoreGuildMissing`
- `ChallengeSystem.Message.HardcoreGuildOtherGuild`
- `ChallengeSystem.Message.HardcoreGuildJoinFailed`
- `ChallengeSystem.Message.BotsBlocked`
- `ChallengeSystem.Message.RndBotsBlocked`
- `ChallengeSystem.Message.BotsRequireHardcore`

## Notes

Warning: `NO_SUMMONS` currently blocks summon accepts (e.g., warlock/meeting stone) via the teleport hook.
Some teleport/portal paths may bypass this until deeper hooks are added. TODO: expand summon/portal detection coverage.
