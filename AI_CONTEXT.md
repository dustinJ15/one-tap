# AI Context — One-Tap

This file is written for AI assistants working on this project. Read it before touching any code.
Update it whenever significant decisions are made, milestones are completed, or the design changes.

---

## What This Project Is

One-Tap is a Counter-Strike-inspired tactical shooter written in C, built on raylib.
The goal is not a feature-rich modern game. The goal is to recreate the specific feel of old-school Counter-Strike:
- CS-authentic movement (air acceleration, crouch mechanics, no abilities)
- Round-based 5v5 with economy (buy menu, kill rewards)
- Bomb defusal win condition
- Minimal visuals — flat textures, simple geometry, intentionally low-fidelity
- 1000+ FPS on any modern machine

The developer is vibecodeing this project — building iteratively, milestone by milestone.
**Keep suggestions focused and achievable. Do not over-engineer. Do not add features outside the current milestone.**

---

## Stack

- **Language:** C
- **Foundation:** raylib (https://raylib.com) — window creation, input handling, OpenGL draw calls, audio
  - raylib is a library, not a framework. You call it; it does nothing you didn't ask.
  - Every game system above raylib is written from scratch: physics, networking, renderer, game logic.
  - raylib goes in `lib/raylib/` as a git submodule.
- **No engine.** No Quake. No Yamagi. The developer owns every line above the library level.

### Why not id Tech 2 / Yamagi?
We started with Yamagi Quake II but switched. Yamagi is ~150k lines of C the developer doesn't own or understand. raylib is a thin, transparent library that does only what you ask. The game code above it is 100% ours.

---

## Project Structure

```
one-tap/
├── src/
│   ├── main.c               # entry point, game loop
│   ├── game/                # round system, economy, weapons, teams
│   ├── physics/             # CS-style movement math
│   ├── render/              # world geometry, player rendering, HUD
│   ├── net/                 # UDP networking, player sync
│   └── audio/               # sound effects wrapper around raylib audio
│
├── lib/
│   └── raylib/              # raylib source (git submodule)
│
├── content/
│   ├── maps/                # map files (format TBD, milestone 10)
│   ├── textures/            # flat textures, solid colors
│   ├── sounds/              # weapon sounds, footsteps, etc.
│   └── models/              # player/weapon geometry (later milestones)
│
├── tools/
│   ├── trenchbroom/         # Trenchbroom game config (milestone 10+)
│   └── scripts/             # build helpers
│
└── docs/
    ├── game-design.md
    └── movement-tuning.md
```

---

## Key Source Files (will grow as milestones are completed)

| File | Role |
|------|------|
| `src/main.c` | Game loop, arg parsing (`--server`/`--connect`), input → PlayerInput, net wiring |
| `src/physics/movement.h` | `PlayerInput` struct, `PlayerState` (includes `health`), movement constants |
| `src/physics/movement.c` | CS-style air acceleration, ground friction, crouch, gravity — input-decoupled |
| `src/render/map.c` | Map geometry, AABB collision, `map_raycast`, `map_ray_hits_box` |
| `src/net/net.h` | Packet structs (`PktInput`=27B, `PktWorld`=185B), `NetClient`, constants |
| `src/net/net.c` | Client: connect handshake, `send_input`, `recv` drain loop, disconnect |
| `src/net/server.c` | Headless server: 64 Hz loop, recv/tick/send_world, hitscan hit detection |
| `src/game/weapons.c` | Weapon stats, hitscan, spread, procedural gunshot sound |
| `src/game/round.c` | Round timer, win conditions, restart (milestone 6) |
| `src/game/economy.c` | Money, kill rewards, buy menu (milestone 7) |

---

## CS Movement Reference Values

These are the target values for milestone 2. All movement code goes in `src/physics/movement.c`.

| Parameter | CS 1.6 value | Notes |
|-----------|-------------|-------|
| Ground speed | 250 units/s | Max run speed |
| Crouch speed | 68 units/s | While crouching |
| Ground accel | 5.6 | How fast you reach max speed |
| Air accel | 10 | Very low — intentional, gives CS its feel |
| Friction | 4 | Ground friction coefficient |
| Gravity | 800 units/s² | Standard |
| Jump velocity | 268 units/s | Upward velocity on jump |

Key behavior: in the air you have very little ability to change direction (low air accel = 10), but you can strafe jump to maintain speed. This is what makes CS feel like CS and not Quake.

---

## Milestone Tracker

### Milestone 1 — Window + Game Loop
- [x] Add raylib as git submodule in `lib/raylib/`
- [x] Write `src/main.c`: init window, game loop, close
- [x] FPS counter displayed (top-left, DrawFPS)
- [x] Makefile builds raylib static lib then links one-tap binary
- [x] Extra X11 deps needed on Fedora: `libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel libXext-devel`
- [x] **Checkpoint:** blank window opens, FPS counter visible, closes cleanly ✓

### Milestone 2 — CS-Style Movement
- [x] `src/physics/movement.c`: player state struct (position, velocity, on_ground, crouching)
- [x] Ground movement with acceleration and friction
- [x] Air movement with CS air acceleration values
- [x] Gravity
- [x] Jumping (single jump, no double jump)
- [x] Crouching (speed penalty, height change)
- [x] Mouse look (pitch + yaw, pitch clamped)
- [x] No world geometry yet — player moves in an infinite void
- [x] **Checkpoint:** movement in void feels like CS — snappy ground, deliberate air, crouching works ✓

### Milestone 3 — Walk Around a Room
- [x] `src/render/map.c`: hardcoded room geometry (800x800x160, 2 crates, pillar, low platform)
- [x] AABB collision detection: player vs walls, floor, ceiling, obstacles
- [x] **Checkpoint:** player can walk around a box room, collision works, nothing clips through walls ✓

### Milestone 4 — Shoot Something
- [x] `src/game/weapons.c`: hitscan pistol, procedural gunshot sound
- [x] `map_raycast()` in map.c: slab-method ray vs AABB, returns hit point + face normal
- [x] Spread value (random yaw/pitch offset in cone)
- [x] Bullet hole decals: thin dark slab rendered flush to hit surface
- [x] Gunshot sound: procedural white noise with exponential decay via raylib audio
- [x] **Checkpoint:** fire a weapon, hit walls, see and hear the result ✓

### Milestone 5 — Two Players (LAN/Localhost)
- [x] Dedicated server model — same binary, `--server` / `--connect` flags
- [x] `PlayerInput` struct decouples input from physics; `player_update` takes `const PlayerInput *`
- [x] `src/net/net.h`: packed packet structs (PktConnect/Accept/Input/World/Disconnect), NetClient
- [x] `src/net/net.c`: UDP client — SO_RCVTIMEO handshake (25 retries), O_NONBLOCK game loop
- [x] `src/net/server.c`: headless 64 Hz server, recv/tick/send_world, SIGINT shutdown
- [x] Client-authoritative position: x/y/z sent in PktInput, server trusts it (avoids sim divergence)
- [x] Server-authoritative hit detection: ray vs wall + ray vs player box, 34 dmg, respawn on death
- [x] Remote player rendered as 32×72×32 RED box; HP HUD shows server-reported health
- [x] Offline mode (`./one-tap` no flags) unchanged
- [x] **Checkpoint:** two clients connect, see each other move, can shoot each other (3 hits to kill), kill logged on server ✓

### Milestone 6 — Rounds
- [x] `src/game/round.c`: round timer (1:55 default)
- [x] Team assignment (CT / T) on connect
- [x] Team-aware spawn points (hardcoded per map for now)
- [x] Death: player becomes spectator until round ends, no respawn
- [x] Win condition: all enemies eliminated
- [x] Round end → scoreboard → round restart
- [x] **Checkpoint:** full round loop works — timer, death, win condition, new round ✓

### Milestone 7 — Economy
- [ ] `src/game/economy.c`: money per player (persists between rounds)
- [ ] Kill reward: +$300
- [ ] Round win: +$3250 / Round loss: +$1400 (with loss bonus streak)
- [ ] Buy phase at round start (first 15 seconds)
- [ ] Console buy command: `buy <weapon>`
- [ ] Starting pistol always available for free
- [ ] **Checkpoint:** money accumulates across rounds, can buy weapons

### Milestone 8 — Weapon Arsenal
- [ ] Pistol (free, always — Glock/USP style)
- [ ] T rifle (AK-style: high damage, one-tap to head, higher spread)
- [ ] CT rifle (M4-style: lower damage, lower spread, faster fire)
- [ ] Sniper (AWP-style: one-shot kill body, slow movement while scoped)
- [ ] Each weapon: damage, spread, fire rate, price, ammo count
- [ ] Ammo runs out, can buy more
- [ ] **First-shot accuracy:** first shot should be perfectly accurate (or near-perfect) when standing still — spread only kicks in on subsequent shots and resets after a cooldown. Developer has ideas about tuning this per-weapon.
- [ ] **Checkpoint:** weapon choice has real strategic tradeoff

### Milestone 9 — Bomb Objective
- [ ] Bomb item: spawns on a T-side player each round
- [ ] Bomb sites: two marked zones in the map
- [ ] Plant: hold USE at bomb site for 3 seconds
- [ ] Detonate: 35 seconds after plant, T wins
- [ ] Defuse: hold USE at bomb for 10 seconds (5 with kit), CT wins
- [ ] Defuse kit: purchasable item for CTs
- [ ] **Checkpoint:** full CT vs T objective loop works

### Milestone 10 — Real Map + Trenchbroom Workflow
- [ ] Design a simple map file format (or parse Quake .map brush format)
- [ ] Map loader in `src/render/map.c`
- [ ] Set up Trenchbroom with One-Tap game config
- [ ] Build a proper two-bombsite map (de_ style): two routes, two sites, distinct areas
- [ ] Spawn points and bombsite zones defined in map file
- [ ] **Checkpoint:** a round on a real map feels like Counter-Strike

---

## Design Decisions Log

| Date | Decision | Reason |
|------|----------|--------|
| 2026-02-26 | Use id Tech 2 / Yamagi as engine base | Initial choice — same lineage as GoldSrc/CS |
| 2026-02-26 | Switch to raylib + write everything | Developer wants to own every line of code above library level. Yamagi is 150k lines of black box. |
| 2026-02-26 | Vibecodeing approach, one milestone at a time | Developer preference, keeps scope manageable |
| 2026-02-26 | Minimal visuals intentional | Performance goal + aesthetic preference (picmip CS look) |
| 2026-02-26 | Hardcoded geometry for milestone 3 | Get walking first, map format is a separate problem |
| 2026-03-03 | Milestone 1 confirmed working on Fedora 43 | mesa-libGL-devel + libXrandr/Xinerama/Xcursor/Xi/Xext-devel required |
| 2026-03-03 | Milestone 2 + 3 complete | CS movement values in movement.h, AABB collision in map.c (SAT pushout, 3-pass, ground check) |
| 2026-03-03 | Milestone 4 complete | Hitscan pistol, map_raycast slab method, bullet hole decals, procedural gunshot sound |
| 2026-03-03 | Milestone 5: dedicated server model | Same binary, --server runs headless; avoids NAT issues, server is authoritative for health/kills |
| 2026-03-03 | Milestone 5: client-authoritative position | Server trusts x/y/z from PktInput — avoids parallel physics divergence. Health/hit-detection stay server-side. Acceptable for LAN prototype. |
| 2026-03-03 | Milestone 5: PlayerInput decoupling | Removed all raylib input calls from movement.c; player_update now takes const PlayerInput*. Required for server to drive player physics without a window. |
| 2026-03-11 | Milestone 6: round system | RoundState in round.c, team assignment on connect, team-aware spawns, death→spectator, win-by-elimination, 1:55 timer, 5s scoreboard, round restart. PktWorld expanded to 210B with phase/round_ticks/scores/win_team. |

---

## Conventions and Preferences

- Developer is vibecodeing — keep AI suggestions focused on the current milestone only
- Do not suggest refactors or improvements outside the current task
- Prefer editing existing files over creating new ones
- C is the primary language
- No over-engineering — the simplest implementation that passes the milestone checkpoint wins
- When in doubt about movement values, reference the CS Movement Reference table above

---

## Build

`make` — builds raylib (first time only) then links `./one-tap`. Sources picked up automatically via `find src -name '*.c'`.
Fedora deps: `sudo dnf install mesa-libGL-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel libXext-devel`

## Resources

- raylib docs: https://www.raylib.com/cheatsheet/cheatsheet.html
- raylib GitHub: https://github.com/raysan5/raylib
- CS 1.6 movement breakdown: search "CS 1.6 bhop mechanics air acceleration"
- Trenchbroom docs (milestone 10): https://trenchbroom.github.io/manual/latest
