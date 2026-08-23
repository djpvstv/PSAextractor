# PSAextractor — Requirements

Cross-platform C++17 CLI for reading and (eventually) writing Brawl PSA
`.pac` fighter files. Serves as an editable replacement for the closed-source,
Windows-only PSA Compressor, and adds workflows PSAC either lacks or
handles incorrectly.

Status legend: **Done** | **Partial** | **Planned**

---

## R1 — Extract a PSA `.pac` to human-readable text

Convert a fighter `.pac` into a plain-text representation suitable for
review, diffing under version control, and hand-editing.

**Acceptance:**
- Given any character `.pac` (e.g. `FitMario.pac`), the tool emits a text
  document that names each event by its PSA command and formats every
  argument by type (Scalar as decimal float, Variable as
  `MemClass-DataType[decimal_index]`, Value/Pointer as `0x…`, etc.).
- The raw wire form (`E=CMD_ID:type-value,…`) is preserved alongside each
  pretty event so a user can copy any line back into PSAC.

**Status:** Partial. `--subaction <id> [tab]` and `--events <off>` decode
individual streams. A full-moveset dump to a single text file is not yet
wired up.

---

## R2 — Comments in the text format

The text output must permit inline user comments so notes travel with the
moveset.

**Acceptance:**
- Grammar reserves `#` (or equivalent) as a line-comment leader.
- Round-trip (R8) must preserve comments verbatim.

**Status:** Planned. The extraction already emits a trailing `# raw` on
every event; the syntax exists but a formal grammar spec and comment
preservation across write-back is not yet defined.

---

## R3 — Read all Actions, Subactions, and Subroutines

Every entry point defined in the moveset must be reachable and dumpable.

**Acceptance:**
- Enumerate every SubAction ID with its animation name (from
  `SubActionFlags[i].anim_name_ptr`).
- Enumerate every Action (Common, Special, Extra) with its Pre / Main /
  Interrupts / Overrides sub-lists.
- Enumerate every Subroutine that any event references, including
  transitively via `Subroutine` events.
- Every listed entry must decode its event stream.

**Status:** Partial. Subactions across all four tabs (Main/GFX/SFX/Other)
are covered. Actions and Subroutines are not yet enumerated.

---

## R4 — Read Articles

Articles (projectiles / spawned objects) have their own Actions,
SubActions, parameters, and data-offset table.

**Acceptance:**
- Enumerate every Article referenced by the character (e.g. Cape,
  Fireball, FLUDD, FLUDD's Water, Mario Finale for Mario).
- For each Article: dump its own Actions and SubActions, and expose its
  Parameters (matching PSAC's Articles → Parameters tab).
- Handle "ID inheritance" cases (PSAC shows e.g. "Article 3 (ID=0 in
  Article 2)") — the tool needs to make the same identity distinction.

**Status:** Planned.

---

## R5 — Read `Fighter.pac` (shared base)

Every fighter inherits behaviour from `Fighter.pac`. This file has a
different layout from a character `.pac` (4 ARC entries, two `MiscData`
sections, no ext-sub imports) and its moveset appears to consist mainly of
Actions (few or no SubActions).

**Acceptance:**
- Detect `Fighter.pac` layout automatically and read both `MiscData`
  entries.
- Enumerate the shared Actions/Subroutines that character files import
  via `External=…`.
- Cross-reference: a character file's `External=<name>` should resolve to
  a specific offset inside `Fighter.pac` when both are loaded together.

**Status:** Partial. Parses the outer container and MISC section headers
for `Fighter.pac`. The moveset layout inside the two MiscData entries has
not been decoded.

---

## R6 — Search for variable usage

Given a variable (e.g. `RA-Basic[16]`), find every SubAction, Subroutine,
and Article code path that reads or writes it.

**Acceptance:**
- CLI: `psax <pac> --find-var <descriptor>` where descriptor is either
  the raw hex (`0x22000010`) or the pretty form (`RA-Bit[16]`).
- Report format includes: SubAction ID + animation name + tab, or
  Subroutine ID, plus the specific event line (pretty + raw).
- Search must traverse into Subroutine calls, not just the top-level
  event lists.

**Status:** Planned. The event decoder + tab enumerator + audit-style
scan pattern (see R7) are all in place; this is mostly wiring a new
predicate.

---

## R7 — Search for SFX usage

List every place a sound effect is triggered across the entire moveset.

**Acceptance:**
- CLI: `psax <pac> --audit-sfx`.
- Detects both direct triggers (`SoundEffect`, `SoundEffectTransient`)
  and indirect triggers (`OffensiveCollision` /
  `SpecialOffensiveCollision` combined with writes to `RA-Basic[8..10]`
  that customize hitbox sound registers).
- Each hit is annotated with SubAction ID, tab name, and animation name.

**Status:** Done for SubActions (all 4 tabs). Missing: traversal into
Subroutines and Actions.

---

## R8 — Write a character `.pac` back from text

Round-trip: parse the text format and produce a binary `.pac` that
behaves identically in-game.

**Acceptance:**
- Input: an existing target `.pac` (as a template) plus the edited text
  file.
- Output: a `.pac` that Dolphin / a real Wii accepts and plays
  correctly, with edits from the text applied.
- Preserves all bytes we didn't decode into text (headers, unknown
  sections, animation data, etc.) — the text file is deliberately partial.
- Preserves R2 comments across the round-trip (they only live in the
  text file, not the `.pac`).

**Status:** Planned. Non-trivial: requires a parser for the text
grammar plus a bytewise editor that patches the existing `.pac`
in-place rather than reconstructing from scratch.

---

## R9 — Write `Fighter.pac` back from text

PSAC saves `Fighter.pac` incorrectly and breaks the game; the current
workaround is manual ASM inject patches. This tool must produce a
`Fighter.pac` that actually works.

**Acceptance:**
- Round-trip a `Fighter.pac` unmodified: the output byte-matches the
  input (or matches a game-verified reference).
- Edited round-trip: apply a small text edit (e.g., change one event's
  argument) and confirm the resulting file loads in Dolphin without
  crashing.
- Documented list of PSAC's failure modes on `Fighter.pac` and how this
  tool avoids them.

**Status:** Planned. Depends on R5 (decode) and R8 (writeback
infrastructure). Explicitly a stretch goal — it's the reason this tool
exists as an alternative to PSAC, so getting it right matters more than
speed of shipping other features.

---

## Non-goals

- **General Brawl editor.** Textures, models, animations, stage data,
  and non-PSA archives are out of scope.
- **Full replacement of BrawlCrate / BrawlBox.** Their strengths
  (bones, particles, REFF) stay theirs.
- **In-game runtime hooks.** ASM injects and Gecko codes are handled by
  other tooling (Combo-Mode-Plus, GCTRealMate).

## Cross-cutting concerns

- **Cross-platform.** C++17 + CMake. No Windows-only APIs in core.
- **Testable.** Every new decode/encode rule lands with a doctest case
  that pins output byte-for-byte or string-for-string.
- **PSAC ground truth.** When uncertain about a binary layout, verify
  against a PSAC screenshot before shipping the code (see
  `psac_ground_truth.md` in memory).
- **VSCode extension** (future). The text grammar (R1, R2) will be
  designed with an eye toward autocomplete, hover-tooltips (via
  `command_description()`), and go-to-definition for Subroutine calls.
