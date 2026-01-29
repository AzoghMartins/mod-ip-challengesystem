# Testing (Temporary Aura Switches)

For early development, Challenge restrictions can be toggled by applying GM auras.
This is a temporary stand-in for real tier state tracking.

## Config

Set these keys in `mod-ip-challengesystem.conf`:

- `ChallengeSystem.TestAura.Hardcore` (spell aura ID)
- `ChallengeSystem.TestAura.NoTrade` (spell aura ID)

If a key is `0`, the restriction is treated as inactive.

## In-game usage

1) Set the config values to valid spell aura IDs.
2) Apply the aura to a character to simulate the restriction:
   - `.aura <spellId>`
3) Remove the aura when done:
   - `.unaura <spellId>`

## TODO

Replace aura-based testing with real tier/restriction state from the database.
