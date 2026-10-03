# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Language

Communicate with the user in Russian.

Write all new code comments in English (existing Spanish comments can stay as they are).

## Git

**Never push to any remote** (`git push` in any form, including `--force`, tags, or pushing via `gh`). The user pushes personally. Committing locally is fine when asked.

## Bug reports from play sessions

When the user reports a bug they hit while playing (draft, game, mascot, overlay), archive the session **before** investigating: Hearthstone keeps only its last couple of log sessions and the tracker log rotates on every restart.

```sh
python3 tools/collect_session.py -n "<short English description>"
```

Add `-c` when the user says they just took a screenshot (their screenshots go to the clipboard). Then fill the `Expected` / `Actual` sections of the fixture's `note.md` from what the user said. Fixtures live in `~/Desktop/hs fixtures`, outside the repo: they contain BattleTags, never commit them.

## Project

Arena Tracker (AT) is a Qt 6 / C++ desktop deck tracker for Hearthstone, focused on Arena drafting. Single qmake project, no test suite, no linter. Code comments are frequently in Spanish.

## Build

Dependencies: Qt (modules `core gui network widgets websockets`), OpenCV (found via pkg-config as `opencv`), libzip, zlib. The README targets OpenCV 2.4.x.

```sh
qmake ArenaTracker.pro && make        # or open ArenaTracker.pro in Qt Creator, Release build
```

- Every new `.cpp`/`.h` must be added to `SOURCES`/`HEADERS` in `ArenaTracker.pro`.
- `Sources/Utils/capturemanager.*` is only compiled on Linux (Wayland screen capture via the external `Extra/captureHelper` binary).
- Resources (images, fonts) are bundled through `arenatracker.qrc`; the main UI layout lives in `mainwindow.ui`.
- App version is `VERSION` in `Sources/versionchecker.h`.

Debug toggles (compile-time) are the `DEBUG_*` defines in `Sources/utility.h`.

## Architecture

**Wiring hub:** `MainWindow` (`Sources/mainwindow.cpp`, ~5k lines) creates every handler in `create*Handler()` methods and connects them with old-style `SIGNAL()/SLOT()` string connections. Handlers are mostly decoupled from each other; cross-handler data flow goes through signals connected in `MainWindow`. When adding a signal, remember to wire it there (and handlers commonly re-emit `pDebug`, `startProgressBar`, etc. to MainWindow).

**Game-state pipeline (Hearthstone log parsing):**
1. `LogLoader` finds the HS logs dir and creates one `LogWorker` per component (`LoadingScreen`, `Power`, `Zone`, `Arena`, `Asset`), each tailing its log file.
2. Lines are optionally merged/sorted across components by timestamp (`sortLogs`) and emitted as `newLogLineRead(LogComponent, line, ...)`.
3. `GameWatcher` parses lines with regexes (`processPower`, `processZone`, `processArena`, ...) and emits high-level signals (`newArena`, `startGame`, `playerCardDraw`, `enemySecretPlayed`, `playerMinionZonePlayAdd`, ...).
4. Handlers consume those signals: `DeckHandler` (the drafted deck, synced with Hearthstone's deck snapshots), `DraftHandler`, `ArenaHandler` (the run record in `ArenaTrackerStats.json`) and the mascot reactions in `MainWindow`.

**Drafting:** `DraftHandler` (largest file) screen-captures the draft, locates card slots via template images in `Extra/*Template*.png`, and identifies cards by OpenCV histogram comparison against downloaded card images. On macOS it first reads each card's name banner with Apple Vision (`Sources/Utils/macocr.mm`, `DraftHandler::readCardNames`) and fuzzy-matches it against `cardsNameMap`; a matched name overrides the histogram (needed for animated golden cards). It scores picks from two sources (`DraftMethod`/`ScoreSource` enums: HearthArena tier list and Firestone winrates via `WinratesDownloader`), combined by `PickRating` into the hands on the plates and the mascot's advice.

**UI:** the user sees only `MascotWindow` (the mascot and its speech bubble), the draft overlays (`DraftScoreWindow` with the `ScorePlate`s under the cards, `DraftHeroWindow` on the hero choice) and `SplashWindow`. The old main window (`mainwindow.ui`, tabs, config, `ThemeHandler` defaults) is still created but never shown (`OLD_TRACKER_WINDOWS` is off in `utility.h`).

**Card model:** Card data comes from HearthstoneJSON `cards.json` (cached); card metadata lookups are static helpers in `Utility`. Card-ID constants (secrets, special cards) are in `Sources/constants.h`. `Sources/Cards/` holds the card list-item types (`DeckCard`, `DraftCard`).

**Settings/storage:** `QSettings("Arena Tracker", "Arena Tracker")`; user data dir is `~/Arena Tracker` (Win/Mac) or `~/.local/share/Arena Tracker` (Linux).

## Repo data served to running clients

The installed app downloads data directly from this repo's `master` branch via `raw.githubusercontent.com` (URLs in `mainwindow.h`, `versionchecker.h`, `hscarddownloader.h`). Committing to these directories affects all live users immediately:

- `Version/version.json` — latest version + download URLs; `versionFree` lists versions allowed without premium.
- `Arena/arenaVersion.json` — current arena card sets, `trustHA`, reset counters.
- `HearthArena/hearthArena.json` + `haVersion.json`, `LightForge/`, `CardsJson/`, `Extra/`, `Images/`, `HearthstoneCards/`, `HearthstoneSignatureCards/`, `Premium/premium.json`.

Clients only re-download a JSON when its companion `*Version.json` number increases — bump the version number whenever the data file changes.

`tools/update_data.py` refreshes cards.json, arena sets, card images and the HearthArena tier list (bumping the versions). `.github/workflows/update-data.yml` runs it on the 1st and 15th of each month (or manually from the Actions tab) and opens a "Data update" pull request.


