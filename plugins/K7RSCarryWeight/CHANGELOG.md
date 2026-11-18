# Changelog

All notable changes to K7 - Race&Sex Carry Weight (SKSE plugin) are recorded here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), the versions follow
[Semantic Versioning](https://semver.org/).

The Russian version is [CHANGELOG.ru.md](CHANGELOG.ru.md); the two files are kept together and must say the same.

## [0.2.2] - 2026-11-18

### Fixed

- Avoid repeating the same error on every Survival Mode poll after a damaged co-save disables carry-weight recalculation.
- Localize settings diagnostics and write failure messages in the menu, with file and line information; detailed diagnostics remain in the plugin log.
- Detect external user-file changes against the version loaded into the menu. Serialize cooperating configuration writers and use unique temporary files.

- Stop carry-weight recalculation when the saved contribution is damaged or non-finite, and preserve that state across saves. Reject non-finite recalculation results.

### Changed

- Before replacing an existing configuration, save a unique verified dated backup outside the mod folder; retain any existing `.bak` file.

## [0.2.1] - 2026-11-10

### Fixed

- The settings menu shows the syntax error of `config.user.toml` with its line number when the file breaks while the game is running. Before, only the "file is not readable" line was shown, and the error appeared nowhere in the menu.

## [0.2.0] - 2026-11-07

First release on the mirror.

[0.2.2]: https://github.com/vorxa/K7RSCarryWeight/compare/K7RSCarryWeight-v0.2.1...K7RSCarryWeight-v0.2.2
[0.2.1]: https://github.com/vorxa/K7RSCarryWeight/compare/K7RSCarryWeight-v0.2.0...K7RSCarryWeight-v0.2.1
[0.2.0]: https://github.com/vorxa/K7RSCarryWeight/releases/tag/K7RSCarryWeight-v0.2.0
