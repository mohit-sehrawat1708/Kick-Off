# Kick Off (Pitch Kings): game data

All the data that defines the game, collected from `README.md`, `PitchKings.uproject`, `Config/` and the C++ source
(`Source/PitchKings`). For how the systems work in depth, see `README.md`. Source of each section is noted in *italics*.

## Overview

| Field | Value |
| --- | --- |
| Title | Kick Off (in-game working title: *Pitch Kings*) |
| Genre | 11-a-side football, single player vs AI, AI vs AI spectate, local 1v1 |
| Engine | Unreal Engine 5.8, C++ module `PitchKings` (Runtime) |
| Project version | 0.5.0 (`DefaultGame.ini`); release builds v1.5 / v1.6 |
| Platform | Windows 10/11 64-bit, DirectX 12 GPU, 8 GB RAM |
| Map | `/Game/Maps/Stadium` |
| Stadium | 30,000 seats, 3D crowd, code-built (`PKPitch`) |
| Players | MetaHuman heads, City Sample Crowds clothing, Mixamo soccer animations retargeted to the UE mannequin |
| Measured cost | ~0.9 GB RAM, 0.65 GB video memory (full match, 1600 x 900) |
| Plugins | ModelContextProtocol, AllToolsets, GeometryScripting (editor only); MetaHumanCharacter, MetaHumanCrowd, AnimToTexture |

## Pitch and ball constants

*`PKTypes.h`, all units are cm unless stated*

| Constant | Value | Real size |
| --- | --- | --- |
| Pitch (half length x half width) | 5250 x 3400 | 105 x 68 m |
| Goal half width / height / depth | 366 / 244 / 200 | 7.32 x 2.44 m goal |
| Post radius | 6 | |
| Penalty area depth / half width | 1650 / 2016 | 16.5 x 40.32 m |
| Six-yard box depth / half width | 550 / 916 | 5.5 x 18.32 m |
| Penalty spot | 1100 | 11 m |
| Centre circle radius | 915 | 9.15 m |
| Ball radius | 11 | size 5, 22 cm |
| Gravity | 980 cm/s² | |
| Air drag | 0.12 | |
| Rolling deceleration | 300 cm/s² | |
| Grass restitution | 0.62 | (README) |
| Shot speed cap | 3600 cm/s (~130 km/h); above it, only 30 % of extra speed is kept | |
| Players per team | 11 | |
| Default match length | 360 s (6 minutes) | |

## Movement speeds

*`PKTypes.h`*

| Movement | cm/s | m/s |
| --- | --- | --- |
| Walk | 300 | 3.0 |
| Jog | 560 | 5.6 |
| Controlled sprint (with ball) | 700 | 7.0 |
| Sprint | 840 | 8.4 |
| Jockey | 330 | 3.3 |
| Shield | 200 | 2.0 |

## Clubs

*`PKTeamData.cpp` → `MakeClubs()`: all clubs and players are fictional. Squads are generated from fixed seeds (`9173 + club × 101`), so every launch has the same players.*

| # | Club | Short | Quality offset | Style | Default formation | Home kit |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | Pitch Kings FC (you) | PKF | 0.00 | Balanced | 4-4-2 | royal blue pinstripes, white shorts |
| 1 | Red Rovers | RRV | -0.02 | Counter Attack | 4-4-2 | red and white hoops |
| 2 | Crimson Athletic | CRA | +0.03 | Possession | 4-3-3 | crimson and black stripes, gold trim |
| 3 | Union Rojo | URO | -0.05 | Long Ball | 5-3-2 | red, white sash |
| 4 | Scarlet City | SCC | +0.06 | High Press | 4-2-3-1 | scarlet and maroon halves |

Scarlet City is the strongest opponent and Union Rojo the weakest.

### Squad shape (18 players per club)

| Squad slot | Position | Shirt number | Depth modifier |
| --- | --- | --- | --- |
| 1-2 | GK | 1, 13 | +0.03, -0.05 |
| 3-8 | DEF | 2, 3, 4, 5, 12, 15 | +0.03, +0.02, +0.02, +0.01, -0.04, -0.05 |
| 9-14 | MID | 6, 8, 10, 14, 16, 18 | +0.03, +0.03, +0.02, +0.01, -0.04, -0.05 |
| 15-18 | FWD | 7, 9, 11, 17 | +0.03, +0.03, -0.03, -0.05 |

Names are "initial. Surname" built from 32 first names and ~200 surnames; each surname is used only once across all clubs.

## Kits

*`PKTeamData.cpp` → `MakeKits()`*

Patterns: 0 plain, 1 stripes, 2 hoops, 3 sash, 4 halves, 5 pinstripes, 6 chevron, 7 quarters, 8 chest band, 9 fade,
10 contrast shoulders, 11 centre stripe.

| Club | Kits |
| --- | --- |
| Pitch Kings FC (8) | Home: royal pinstripes · Away: white chevron · Third: black and volt · Sky blue quarters · Teal fade · Purple and gold · Sunset orange · Retro green hoops |
| Red Rovers | Home: red and white hoops · Away: navy, red band · Third: lime fade |
| Crimson Athletic | Home: crimson and black stripes · Away: white, crimson chevron · Third: grey and gold quarters |
| Union Rojo | Home: red, white sash · Away: black, red sash · Third: sky, white centre stripe |
| Scarlet City | Home: scarlet and maroon halves · Away: yellow, maroon shoulders · Third: teal pinstripes |

Every keeper wears his own colours with long sleeves and gloves. **Kit clash**: the shirt's main colour (primary blended
with secondary by pattern share) decides 80 % and the shorts 20 %; a club wears the first kit (home, away, third...) whose
clash score with the opponent's kit is under 0.45, otherwise the least clashing one.

## Formations

*`PKTeamData.cpp` → `GFormations`. Slot 0 is the keeper; slots 9 and 10 take the kick-off.*

| Formation | Outfield slots |
| --- | --- |
| 4-4-2 | RB, RCB, LCB, LB · RM, RCM, LCM, LM · RS, LS |
| 4-3-3 | RB, RCB, LCB, LB · RCM, CM, LCM · RW, LW, ST |
| 4-2-3-1 | RB, RCB, LCB, LB · RDM, LDM · RAM, LAM, CAM · ST |
| 3-5-2 | RCB, CB, LCB · RWB, RCM, CDM, LCM, LWB · RS, LS |
| 5-3-2 | RWB, RCB, CB, LCB, LWB · RCM, CM, LCM · RS, LS |

(See `GFormations` for exact slot coordinates: x runs -1 own goal to +1 opponent goal, y > 0 is the left side.)

## Player ratings

*`PKFootballer.h`, `PKTeamData.cpp`*

Six attributes, stored 0-1 and shown ×100: **Pace, Control, Passing, Shooting, Defending, Physical**.
Overall (OVR) = average of the six × 100.

### Base profiles by position (before club quality, depth and ±7 random)

| Position | PAC | CTL | PAS | SHO | DEF | PHY |
| --- | --- | --- | --- | --- | --- | --- |
| GK | 55 | 55 | 60 | 35 | 80 | 75 |
| DEF | 68 | 62 | 66 | 45 | 82 | 80 |
| MID | 72 | 78 | 82 | 66 | 64 | 70 |
| FWD | 84 | 80 | 70 | 84 | 40 | 72 |

Each rating = base + club quality + depth modifier ± 0.07, clamped to 35-97.

### Position fit (out-of-position penalty, applied to every attribute)

| Player vs slot | Multiplier | Team-sheet ring |
| --- | --- | --- |
| Natural position | 1.00 | blue |
| Neighbouring line (DEF↔MID, MID↔FWD) | 0.92 | orange |
| Two lines apart (DEF↔FWD) | 0.82 | red |
| Keeper ↔ outfield | 0.55 | red |

### Auto-pick weights (best XI)

| Slot | Score |
| --- | --- |
| GK | DEF 45 % · PHY 30 % · PAS 15 % · PAC 10 % |
| DEF | DEF 40 % · PHY 25 % · PAC 15 % · PAS 15 % · CTL 5 % |
| MID | PAS 35 % · CTL 30 % · SHO 10 % · DEF 10 % · PAC 10 % · PHY 5 % |
| FWD | SHO 40 % · PAC 25 % · CTL 20 % · PHY 10 % · PAS 5 % |

Score is multiplied by position fit; the keeper slot is filled first.

## PlayStyles (traits)

*`PKFootballer.h` → `PKTrait` (23 in all)*

| PlayStyle | Effect | PlayStyle | Effect |
| --- | --- | --- | --- |
| Technical | tighter dribbling touches | Relentless | stamina |
| Finesse Shot | curled, placed shots | Bruiser | physical duels |
| Power Shot | harder shots | Intercept | cutting out passes |
| Tiki Taka | accurate short passing | Block | blocking shots |
| Rapid | extra top speed | Aerial | headers |
| Press Proven | better at winning the ball | Jockey | quicker jockeying |
| Anticipate | reads passes, longer interception reach | Acrobatic | volleys |
| Whipped Pass | faster, more accurate crosses | Far Throw | long throw-ins |
| Incisive Pass | through balls | Dead Ball | free kicks and corners |
| Long Ball Pass | long passes | Chip Shot | chips |
| Trickster | skill moves | Slide Tackle | longer slides |
| Quick Step | acceleration off the mark | | |

- Outfield players get 1 PlayStyle, a 60 % chance of a 2nd and a 20 % chance of a 3rd, from their line's pool.
  Keepers have a 30 % chance of Far Throw.
- Players with OVR 80+ get the **PlayStyle+** version of one of theirs (shown with +, ~60 % stronger).
- **AcceleRATE** types from attributes: Explosive, Controlled, Lengthy.

| Line | PlayStyle pool |
| --- | --- |
| DEF | Press Proven, Anticipate, Intercept, Block, Bruiser, Aerial, Jockey, Slide Tackle, Long Ball Pass, Far Throw, Tiki Taka |
| MID | Tiki Taka, Technical, Incisive Pass, Long Ball Pass, Whipped Pass, Relentless, Trickster, Dead Ball, Intercept, Press Proven, Finesse |
| FWD | Finesse, Power Shot, Rapid, Quick Step, Trickster, Chip Shot, Acrobatic, Aerial, Technical, Whipped Pass |

## Team play styles and game plans

| Style | Behaviour |
| --- | --- |
| 1 Balanced | default |
| 2 Possession | patient, safe passing |
| 3 Counter Attack | direct, through balls, deep block |
| 4 Long Ball | lofted passes |
| 5 High Press | high line, two pressers |

Mentality: Ultra defensive .. Ultra attacking (moves the block, line and press). The AI attacks when losing late and
sits deeper when winning.

## Player looks

*`PKTeamData.cpp` → `MakeLook()`, fixed per player*

| Feature | Options |
| --- | --- |
| Skin tones | 7 |
| Hair colours | 6 (dark most common; light hair goes with lighter skin) |
| Hair styles | shaved/buzz 18 %, short 45 %, medium 25 %, big 12 % (MetaHuman cuts: Buzz, Afro Fade, Curly Fade, Side Swept, Pulled Back, Hair Loss, Coil) |
| Beard | none 55 %, stubble 25 %, beard 20 % |
| Boot colours | 7 (black, white, volt, orange, blue, red, yellow); 70 % have a side flash |
| Height scale | 0.95-1.04 |
| Build | 0.95-1.06, follows Physical |

## Match rules and systems

| System | Data |
| --- | --- |
| Halves | two, ends changed at half time |
| Substitutions | 5 per team (pause menu); the computer subs at stoppages |
| Stamina | sprinting drains it; tired players are slower and less accurate, no sprint when empty |
| Free kick | opponents 9.15 m away; 3-4 man wall inside 32 m; AI shoots from inside ~29 m |
| Penalty | foul in the box; best finisher takes it |
| Cards | yellow by severity, 2nd yellow = red, denying an obvious chance = straight red |
| Offside | standard law; not from goal kicks, corners or throw-ins |
| Advantage | played on, called back if the ball is lost straight away |
| Replay | 5 s recorded at 30 Hz after a goal |

### Pass and ball-flight bands

| Pass | Behaviour |
| --- | --- |
| < 9 m | rolls |
| 9-18 m | skids, small hops |
| > 18 m | knee-to-waist high, then bounces |
| Lob / cross / switch | high arc, slight curl, landing ring |
| Side-foot pass | up to ~11 m/s; ball ≈ 1.3× foot speed |
| Laces / running strike | ball ≈ 1.45× foot speed |

## Settings and defaults

*`PKSettings.h`; saved in SaveGame slot `PitchKingsSettings` (team sheet in `PitchKingsTeam`, graphics in `GameUserSettings.ini`)*

| Setting | Default | Options |
| --- | --- | --- |
| Match length | 6 min | adjustable |
| Difficulty | Normal | Easy / Normal / Hard (computer team ratings) |
| Camera distance | Normal | Near / Normal / Far |
| Stick dead zone | 0.2 | |
| Vibration | 1.0 | 0 = off |
| Invert X / Y | off | |
| Shirt numbers | off | |
| Help panel | off | |
| Mini-map | on | |
| FPS counter | off | |

Rebindable keyboard actions (22): Move up/down/left/right, Sprint, Hold, Pass, Through, Lob, Shoot, Tackle, Slide,
Switch, Barge, Knock-on, Drag-back, Feint, Flick-up, Pause, Precision, Runs, Skill. Gamepad buttons are fixed.

## Controls

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Move | WASD / arrows | Left stick |
| Sprint | Left Shift | RT |
| Jockey / shield / close control | hold Left Ctrl | hold LT |
| Pass | J / Space | A |
| Through ball | L | Y |
| Lob / cross | I | X |
| Shoot (hold to charge) | K | B |
| Standing tackle | E | B |
| Slide tackle | C | X |
| Switch player | Q | LB |
| Controlled sprint / precision / teammate contain | R | RB |
| Runs / one-two / chip | Z | LB |
| Effort touch | F + direction | R3 |
| Skill move | B + direction | right stick flick |
| Drag-back / feint / flick-up / barge | G / T / V / X | D-pad down / D-pad up / L3 / A |
| Keeper rush | hold L (no ball) | hold Y |
| Play style | 1-5 | D-pad left / right |
| Mentality | 9 / 0 | LT + D-pad left / right |
| Help · numbers | H · N | |
| Pause | Esc / P | Start |

Full list: `README.md` → *Controls*.

## Front end pages

Landing → Home (Kick Off, Spectate AI vs AI, Team Setup, Controls, Quit) → Team Setup (formation, style, opponent,
18-man squad, auto pick) · Local 1v1 (each device picks a side) · Settings (Controls, Graphics, Gameplay) · Pause.

## Code map

| System | File |
| --- | --- |
| Constants | `PKTypes.h` |
| Footballer, kits, ratings, traits | `PKFootballer.*` |
| Animation | `PKAnimInstance.*`, `PKCopyPoseAnimInstance.*` |
| Ball physics | `PKBall.*` |
| Match rules and AI | `PKGameMode.*` (largest file, ~6,100 lines) |
| Input, cameras, pause | `PKPlayerController.*`, `PKInput.*` |
| Menus | `PKMenu.*`, `PKMenuSettings.cpp`, `PKMenuSubs.cpp` |
| Clubs, squads, formations, kits | `PKTeamData.*` |
| Team sheet + saves | `PKGameInstance.*`, `PKSettings.*` |
| HUD | `PKHUD.*` |
| Stadium | `PKPitch.*` |
| Feature tests | `PKFeatureTest.cpp`, `PKCritic.cpp` |

## Credits

- Boots: "Vrfs boots" by zanman730, CC BY 4.0 (recoloured, refitted).
- Skyline: "Homecoming Center Rooftop", Poly Haven, CC0.
- Heads: MetaHuman Creator presets; clothing and crowd: City Sample Crowds (Epic Games).
- All clubs and players are fictional.
