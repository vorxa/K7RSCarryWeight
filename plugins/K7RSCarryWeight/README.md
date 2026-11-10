# K7 - Race&Sex Carry Weight (SKSE plugin)

Base carry weight of the player by race and sex for Skyrim SE (1.5.97) and AE (1.6.x, 1.7.x); Skyrim VR is not
supported. Changes: [CHANGELOG.md](CHANGELOG.md) ([RU](CHANGELOG.ru.md)). Русская версия: [README.ru.md](README.ru.md).

## State

Version 0.2.1: stage 1 with the Survival Mode penalty and stage 2, the settings menu, are complete and accepted in game on Skyrim
AE 1.6.1170. Version 0.2.1 fixes the
syntax error line of a broken `config.user.toml` in the settings menu.

## What it does

Every playable race gets its own base carry weight for a man and for a woman, instead of 300 for everyone. The value
replaces only the base: stamina bonuses from levelling, items, spells and other mods still add to it. A new character
without bonuses has exactly the configured value.

The plugin puts the difference between the value and the base of the race into the permanent carry weight modifier and
remembers how much it added in the SKSE co-save (the `.skse` file next to the save). It recounts when a save is loaded,
when the player's race changes and when the character creation menu closes, and applies only the difference, so other
mods that change carry weight add up with it. Vampire versions of a race count as that race. In a beast form (werewolf,
vampire lord) it does not recount and keeps what it applied before the transformation. A race that is not in the
configuration (a race of a mod) is left alone.

## Survival Mode

Survival Mode takes carry weight off the player (150 in the Creation Club file, 100 with Survival Mode Improved -
SKSE). With the low values of this plugin that would leave the player overencumbered all the time, so while the
penalty is on, the plugin cancels it and takes `survival_penalty` points off the value instead (0 by default: the
same carry weight as without Survival). The amount is read from the penalty effect on the player, and the plugin
checks it once a second, because Survival is switched on and off without an event (after a load or in the game's
settings). Nothing is checked when Survival Mode is not installed. A race that is not in the configuration and
`enabled = false` leave the penalty as it is. `bDisableCarryWeightPenalty` of Survival Mode Improved is not needed
any more; either setting works.

## Settings menu

If SKSE Menu Framework is installed, the plugin adds
a section to its window (open it with the framework's key, F1 by default): **K7 - Race&Sex Carry Weight** (in a Russian
game: «K7 - Переносимый вес по расе и полу») → **Settings**. The framework is optional: without it the plugin works
from the files alone and writes one line about it into the log.

The menu shows the character (race, sex, base of the race, carry weight by the settings, the plugin's adjustment, what
else changes carry weight, the total), the files that were read and the errors in them, and edits the settings: the
`[general]` keys, the values of every race for a man and a woman, the race of the character as a new race of a mod,
the race records, the beast forms and the Survival Mode effects. Values outside their range are not accepted; a record
is accepted only if the game has it and it is a race (an effect for Survival Mode).

"Save" writes `config.user.toml` and applies the change at once, without loading a save. Only what differs from
`config.toml` is written; **comments and the order of lines in `config.user.toml` are not kept**, so the previous file is
copied to `config.user.toml.bak` before the first save of a game session. "Reset" on a race row takes its edit out of
the file. The records of the vanilla races and the built-in beast forms and Survival effects are removed only by editing
the file. If `config.user.toml` does not parse, the menu shows the error and does not write over it ("Start with a
clean file" keeps it as `.bak`). If the file changed on disk after the menu read it, the menu asks whether to
overwrite or re-read it.

Texts of the menu are in `Data/SKSE/Plugins/K7RSCarryWeight/lang/english.toml` and `russian.toml` (the language of
the game is used, English otherwise); a translation is one more file with the same keys. The Cyrillic text needs a
font that draws it, for example the one from "SKSE Menu Framework Rus".

## Installation

Requirements: SKSE64 and Address Library for SKSE Plugins for your game version. Install the mod with a mod manager or
copy `SKSE/Plugins/K7RSCarryWeight.dll` into `Data/SKSE/Plugins/`. No ESP, no Papyrus.

On the first start the plugin writes `Data/SKSE/Plugins/K7RSCarryWeight/config.toml` (with Mod Organizer 2 it lands in
`overwrite`) and its log to `Documents/My Games/Skyrim Special Edition/SKSE/K7RSCarryWeight.log`.

## Configuration

`config.toml` holds the built-in values with a comment for every key and is **rewritten when the plugin is updated**.
Put your changes into `config.user.toml` in the same folder: give only what you change, the rest comes from
`config.toml`. Changes take effect after the game is restarted or a save is loaded.

```toml
[general]
enabled   = true     # false: take the plugin's contribution back (see Removal)
min_total = 20       # no race gets a base below this, 1 to 300
log_level = "info"   # trace | debug | info | warn | error
survival_penalty = 0 # instead of the Survival Mode penalty, see Survival Mode

[races.nord]
male = 120           # one field of a vanilla race; female and forms stay as in config.toml

[races.my_mod_race]  # a race of a mod with its own values
forms  = ["MyRace.esp|0x000800", "MyRace.esp|0x000801"]   # its race records: plugin | local form id
male   = 110
female = 95

[beast]              # transformations without recount; replaces the list, so keep the vanilla two
forms = ["Skyrim.esm|0x0CDD84", "Dawnguard.esm|0x00283A", "MyMod.esp|0x000900"]
```

Values of races are whole numbers from 1 to 1000. `forms` given in `config.user.toml` replaces the race's list as a
whole: to add a record to a vanilla race, list its two vanilla records too, otherwise the vanilla race and its vampire
version drop out of the configuration and get no contribution. A race record named by two races belongs to the one
named later (`config.user.toml` wins over `config.toml`). A record that is both in a race and in `[beast]` is treated
as a beast form only, with a warning in the log. A wrong value
is skipped and reported in the log with the file, the key and the line; the rest of the file still applies. A file
that does not parse is not applied and not rewritten.

To find the form of a race of a mod, open the plugin in xEdit (the record's FormID without the load order byte) or
look at the log at `log_level = "debug"`: when the player has a race that is not configured, the recount line names it,
for example `race MyRace.esp|0x000800 [not in configuration]`.

## Removal

The contribution lives in the save, not in the DLL. Removing the DLL alone leaves it there for good, and if the DLL
is installed again later, the plugin no longer knows about it and adds it a second time.

1. Set `enabled = false` in `config.user.toml`.
2. Load the save you play: the log shows `contribution … -> 0` and the carry weight goes back to the vanilla base.
3. Save, quit, and remove the mod.

Do this in the normal form of the character, not as a werewolf or a vampire lord: in a beast form the plugin keeps its
contribution even with `enabled = false` and takes it back only when the character returns to the normal form, so a
save made in a beast form still carries it.

If you already removed it without these steps and put it back, the plugin cannot tell: the contribution is in the save
twice. The log warns only when the carry weight ends up below `min_total`. The extra contribution can be removed with
the console, `player.modav carryweight <amount>` (the amount is the contribution with the opposite sign; the log line
`contribution … -> X` shows it).

## Build

From the repository root, see [the root README](../../README.md#build). The DLL is
`build/<preset>/plugins/K7RSCarryWeight/K7RSCarryWeight.dll`; the dev preset deploys it into
`<mods>/K7RSCarryWeight (dev)` of every Mod Organizer 2 installation listed in `K7_MO2_MODS_DIRS`. Unit tests:
`build/<preset>/plugins/K7RSCarryWeight/tests/K7RSCarryWeight_tests.exe`.

## License

GPL-3.0-or-later, see [LICENSE](../../LICENSE). CommonLibSSE-NG is GPL-3.0-or-later as well, and its
modding exception does not cover plugin code, so the plugin is distributed under the same terms.
