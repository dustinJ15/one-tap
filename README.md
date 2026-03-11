# One-Tap

A lightweight Counter-Strike-inspired tactical shooter. Written in C. Built on raylib.

## Vision

One-Tap is a love letter to old-school Counter-Strike — the movement, the feel, the tension of a round economy, and the purity of a 5v5 with no abilities, no battle pass, no bloom. The target is a game that runs at 1000 FPS on any modern machine, looks intentionally minimal (think picmip CS 1.6), and plays like the real thing.

Not a AAA game. Not a remake. A focused clone of a specific feeling.

## Design Pillars

- **Own every line** — built on raylib for window/input/OpenGL, every game system written from scratch
- **Performance first** — 1000+ FPS should be trivially achievable on any modern machine
- **Minimal aesthetics** — flat textures, simple geometry, no bloom, no particles beyond what's necessary
- **Authentic movement** — CS-style air acceleration, crouch mechanics, no abilities or gimmicks
- **Round-based economy** — buy menu, kill rewards, team win conditions
- **Bomb defusal** — the classic CT vs T win condition

## Stack

- **Language:** C
- **Foundation:** [raylib](https://raylib.com) — window, input, OpenGL draw calls, audio. Nothing hidden.
- **Maps:** Custom map format (Trenchbroom-compatible workflow, milestone 10+)
- **Models:** Simple geometry to start, .obj or .md2 format later

## Project Structure

```
one-tap/
├── src/
│   ├── main.c               # entry point, game loop
│   ├── game/                # round system, economy, weapons, teams
│   ├── physics/             # CS-style movement — the heart of the game
│   ├── render/              # renderer: world geometry, players, HUD
│   ├── net/                 # UDP networking, player sync
│   └── audio/               # sound effects wrapper
│
├── lib/
│   └── raylib/              # raylib source (git submodule)
│
├── content/
│   ├── maps/                # map files
│   ├── textures/            # flat textures, solid colors
│   ├── sounds/              # weapon sounds, footsteps, etc.
│   └── models/              # player and weapon geometry (later milestones)
│
├── tools/
│   ├── trenchbroom/         # Trenchbroom game config (milestone 10+)
│   └── scripts/             # build helpers
│
└── docs/
    ├── game-design.md
    └── movement-tuning.md
```

## Milestones

| # | Goal | Checkpoint | Status |
|---|------|-----------|--------|
| 1 | Window + game loop | raylib window opens, FPS counter visible | ✓ |
| 2 | CS-style movement | Move in the void — air accel, crouch, gravity feel right | ✓ |
| 3 | Walk around a room | Hardcoded geometry, collision, perspective correct | ✓ |
| 4 | Shoot something | Hitscan raycast, hit walls, bullet hole, gunshot sound | ✓ |
| 5 | Two players | UDP networking, clients see and can shoot each other | ✓ |
| 6 | Rounds | Teams, death, elimination win condition, round timer | ✓ |
| 7 | Economy | Money, kill rewards, buy menu, starting pistol | ✓ |
| 8 | Weapon arsenal | 4-5 weapons with distinct feel, ammo system | |
| 9 | Bomb objective | Plant, defuse, explosion — full CT vs T loop | |
| 10 | Real map | Trenchbroom workflow, map file format, proper de_ layout | |

## Tools

| Tool | Purpose |
|------|---------|
| [raylib](https://raylib.com) | Foundation — window, input, OpenGL, audio |
| [Trenchbroom](https://trenchbroom.github.io) | Map editor (milestone 10+) |

## License

GPL v2
