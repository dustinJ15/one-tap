# One-Tap

A lightweight Counter-Strike-inspired tactical shooter written in C, built on [raylib](https://raylib.com). Every game system above the library level — physics, networking, renderer, economy — is written from scratch.

---

## Screenshots

> *(coming soon)*

---

## What's Built

Through 8 of 10 milestones:

- **CS-authentic movement** — air acceleration, crouch, ground friction, and jump velocity tuned to CS 1.6 values
- **Hitscan shooting** — slab-method ray vs AABB, bullet hole decals, procedural gunshot audio
- **Multiplayer over UDP** — dedicated server model (`--server` / `--connect`), 64 Hz tick rate, server-authoritative hit detection
- **Round system** — team assignment, death-as-spectator, elimination win condition, 1:55 timer
- **Economy** — kill rewards, round win/loss bonuses, loss streak, buy phase
- **Weapon arsenal** — Pistol, AK, M4, AWP with distinct stats, CS:GO-faithful spray patterns, movement inaccuracy, reload times
- **Settings menu** — sensitivity slider using the Source engine formula (`delta × sensitivity × 0.022`)

Still in progress: bomb plant/defuse (milestone 9), Trenchbroom map workflow (milestone 10).

---

## Design Pillars

- **Own every line** — raylib handles window/input/OpenGL; everything above it is written from scratch
- **Performance first** — 1000+ FPS on any modern hardware, trivially
- **Minimal aesthetics** — flat textures, simple geometry, no bloom, no particles beyond necessity
- **Authentic movement** — CS-style air acceleration, crouch, no abilities or gimmicks
- **Round-based economy** — buy menu, kill rewards, team win conditions
- **Bomb defusal** — the classic CT vs T objective *(in progress)*

---

## Stack

| | |
|---|---|
| **Language** | C |
| **Foundation** | [raylib](https://raylib.com) — window, input, OpenGL, audio |
| **Networking** | UDP sockets, hand-rolled packet protocol |
| **Map editor** | [Trenchbroom](https://trenchbroom.github.io) *(milestone 10+)* |

---

## Build

**Fedora / RHEL:**
```sh
sudo dnf install mesa-libGL-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel libXext-devel
```

**Ubuntu / Debian:**
```sh
sudo apt install libgl1-mesa-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev
```

```sh
git clone --recurse-submodules https://github.com/dustinJ15/one-tap
cd one-tap
make
./one-tap
```

**Multiplayer (LAN / localhost):**
```sh
./one-tap --server          # terminal 1
./one-tap --connect 127.0.0.1   # terminal 2
```

---

## Milestones

| # | Goal | Status |
|---|------|--------|
| 1 | Window + game loop | ✅ |
| 2 | CS-style movement | ✅ |
| 3 | Walk around a room | ✅ |
| 4 | Shoot something | ✅ |
| 5 | Two players (UDP) | ✅ |
| 6 | Rounds | ✅ |
| 7 | Economy | ✅ |
| 8 | Weapon arsenal | ✅ |
| 9 | Bomb objective | 🔨 |
| 10 | Real map (Trenchbroom) | 🔨 |

---

## License

GPL v2
