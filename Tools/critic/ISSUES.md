# Critic issue ledger (MetaHuman player rounds)

Pass mark: overall > 8.5 with zero errors. Reports: `Tools/critic/mhN/` (shots, img, menus).

| Round | Score | Players | Crowd | Pitch | Lighting | Stadium | Surroundings | Ball | HUD | Menus | Broadcast |
|---|---|---|---|---|---|---|---|---|---|---|---|
| mh1 | 4.5 | 3.5 | 4.5 | 5.5 | 6 | 6 | 4 | 5 | 5.5 | 6 | 6 |
| mh2 | 5.0 | 4.0 | 3.5 | 5.5 | 6 | 6.5 | 4 | 6.5 | 6 | 6.5 | 6 |
| mh3 | 4.8 | 3.5 | 4.0 | 6 | 6 | 6 | 3.5 | 5.5 | 6 | 5.5 | 6.5 |
| mh4 | 5.0 | 4.0 | 4.0 | 5.5 | 5.5 | 6 | 3.5 | 5 | 5.5 | 6 | 5 |
| mh5 | 5.0 | 4.0 | 5.5 | 5.0 | 6 | 6 | 2.5 | 6.5 | 5.0 | 6 | 5.5 |
| mh6 | 5.6 | 5.0 | 4.5 | 5.5 | 6.5 | 6 | 3.5 | 6 | 6.5 | 6.5 | 6.5 |
| mh7 | 5.2 | 4.5 | 5.5 | 5.5 | 6 | 5.5 | 2.5 | 6 | 5.5 | 6 | 6 |
| mh8 | 5.5 | 5.0 | 4.0 | 6 | 5 | 5.5 | 3 | 6.5 | 5.5 | 6 | 6 |
| mh9 | 5.5 | 5.0 | 4.5 | 4.5 | 6 | 6 | 3 | 7 | 6.5 | 6 | 6 |
| mh10 | 5.5 | 4.5 | 5.0 | 6 | 5.5 | 6 | 2.5 | 6.5 | 6 | 6.5 | 5.5 |
| mh11 | 5.4 | 4.5 | 4.0 | 5.5 | 6.5 | 6 | 3 | 6.5 | 6 | 6 | 5.5 |
| mh12 | 5.4 | 4.5 | 4.5 | 5.5 | 6 | 6 | 3 | 7.5 | 5.5 | 6 | 5.5 |

## Issues by area (rounds that raised them) and status

### Players
- Hair cards look like opaque black shards, floating clumps, hard hairline (mh1-mh4). Tried: PDO 0, LOD0 cards, roughness/spec down, opacity falloff, darker painted scalp. **Open.**
- Bald scalp showing under fades / bare temples (mh2-mh4). Tried: denser painted hair map. **Open.**
- Head vs limb skin tone mismatch (mh1-mh4). Tried: shared desaturated tone, skin-tinted scatter. **Open** (limbs are the mannequin).
- Mannequin limbs: seams at elbow/wrist, smooth plastic, rubber-glove hands (mh1-mh4). Tried: limb relief. Seams = cracks from smoothing split vertices. **Open.**
- Faces waxy / dead eyes / no expressions (mh1-mh4). Tried: head normal + AO maps, wet eyes. **Open.**
- Too many bald players reading as white eggs (mh2). Fixed in r3 (cards on most short cuts).
- Kit: knit too coarse (mh1, fixed); no names / sponsor (mh1-mh4); back number font/size wraps side seam (mh3-mh4); stripes warp (mh3-mh4). **Open.**
- Shorts double hem / shirt clipping into shorts (mh2-mh3). Tried: hem lower, waistband masked. Not raised in mh4.
- GK kit ragged cuffs / shorts hem (mh4). **Open.**
- Boots are slippers, socks plain tubes (mh2, mh4). **Open.**
- Poses: limp splayed hands, pigeon toes, hunched / looking at the ground (mh2-mh4). **Open.**
- Beards floating / stipple (mh1). Fixed in r2 (cards only on their own head).

### Crowd
- Identical rigid A-pose 3D fans; flat card fans with faceless heads behind them; abrupt 3D-to-card switch (mh1-mh4). **Open.**
- Saturated single-blue crowd (mh1-mh2). Fixed in r3 (shade / second colour / casual mix).

### Pitch
- Repeating dark speckles, no blades near camera, no wear, blown-out sunlit yellow (mh1-mh4). **Open.**

### Stadium / goal
- Blank beige corner slabs, flat concrete, unreadable fascia text (mh1-mh4). **Open.**
- Rigid grid net with no sag (mh1-mh4). **Open.**
- Boards: missing behind goal (mh2, fixed r3); corner overlap (mh3, fixed r4); blurred banner behind goal (mh4). **Open.**
- Sparkle / dither noise on rear surfaces (mh4). **Open.**

### Lighting
- Flat noon light, no haze, crowd under roof lit like the pitch, no contact shadows (mh2-mh4). **Open.**

### Surroundings
- City is grid-window boxes, no haze (mh1-mh4). **Open.**

### Ball
- Faceted silhouette (mh1, fixed r2); blurry decal, no seams (mh2-mh4). **Open.**

### HUD / camera
- Kick-off banner said BLUE (mh1, fixed r2). Goal banner generic, no period tag, crude minimap (mh2-mh4). **Open.**
- Broadcast camera too high / players tiny (mh4). **Open.**

### Menus
- Two buttons highlighted (mh1-mh3; fixed r4). Generic panels, no player renders, controls table (mh1-mh4). **Open.**

## Round 5 changes (before critic mh5)
- Kit: Bahnschrift Bold Condensed SDF lettering (Tools/art/make_kit_font.py): typeset back number with outline, surname on the back, small front number, chest sponsor (sponsor atlas row per club), maker's mark.
- Limbs: the dashed "seams" at elbows/wrists were a discontinuity in the round-3 relief (per-bone-region parameter); relief now position-based. Body rebuilt with shared normals, one render section, welded seams.
- Hands: relaxed finger curl and 0.92 scale (PKAnimInstance ApplyHands). Keeper cuffs stop clear of the gloves.
- Boots: laces; socks: ribbing + shin-pad bulge.
- Ball: anti-aliased print with key line, narrow seam grooves, pebbled casing normal.
- Grass: anisotropic blade streaks replace the value-noise dots (colour + normal).
- Lighting: sky light 1.9 -> 1.45, fog 0.04 -> 0.065, AO 0.8, local exposure shadow contrast 0.72, white temp 6100.
- Crowd: M_PK_FanVAT had been failing to compile since r3 (collection ids orphaned when MPC was rebuilt) -> 3D fans rendered grey; fixed (MPC kept, nodes rebound). Resting poses mixed (16 % slow fist pump, 6 % arms up); 3D rows 5 -> 8 (Epic), 3 -> 4 (Medium).
- Camera: broadcast height 20 m -> 15.5 m, back 23 -> 26 m, FOV 40 -> 34.

## Critic mh5 (5.0) new findings
- Kits read white / mustard at broadcast distance (r5 lettering bug: sub-pixel text washed the whole shirt in the number colour). **Fixed r6.**
- Hair: floating pieces, scalp holes, cut-paper ends. Dashed white lines on the ball (atan2 derivative). Away players wearing home-squad names (32-name pool shared by 8 clubs). Bald heads glow white. GK gloves = white hands; GK cuff notched. Shorts shiny. Stripes warp at side seams. Eyes look down. Pitch-side empty (photographers hidden behind boards). City = textured boxes (2.5).

## Round 6 changes (before critic mh6)
- Lettering: coverage only inside glyph cells, fades out when sub-pixel (kit colours correct at distance).
- Hair: own card material M_PK_HairCards (dithered coverage, root-to-tip and depth shading, HairColor lifted off black); each haircut on the head it was groomed for (no floating pieces).
- Skin albedo capped (lum 0.36). Shorts roughness 0.82 (matte). Ball AA from position (no dashes).
- Surnames: pool 32 -> 190, unique across all clubs.
- GK gloves: trim wrist strap + cuff, grey latex palm, padding relief.
- Photographers in front of the goal-end boards.
- City: aerial perspective in the materials, per-building window rhythm; height fog thicker beyond the bowl.
- Broadcast camera 21.5 m back / 16.5 m high (clear of the near-stand fans).

## Critic mh6 (5.6)
Errors: hair shards / stipple buzz cuts; double / missing brows; head vs arm tone; stripes warp; GK gloves mismatched; GK shirt blotches; console ">_" bar in shots; black-box photographers; flat downlights; kick-off banner over players.

## Round 7 changes (before critic mh7)
- Console forced shut during critic capture. Banners moved to the lower third. Photographer cards removed.
- Stripes / pinstripes mapped by arc length round the torso (straight on the sides); striped kits get plain sleeves.
- Hair cards: coverage thins on edge-on cards and toward the tips. Brows 1.8x coverage. Skin scatter softer; palest albedo capped at 0.27.
- Gloves one colour (palm guess flipped between hands). Cloth fold normal at 40 % (blotches on bright kits).
- Head pitch toward the ball 0.6 -> 0.3 (clamped), so portraits are not all looking at the grass.

## Critic mh7 (5.2)
Errors: foot inside ball; 3D-to-card crowd drop; hair shards; shaved sides read bald; arm tone grey; one head on 5 players (r6 regression: head tied to haircut); shorts side-panel blob; back name over shoulder seam; elbow crease; blank corner wall; untextured outer roof.

## Round 8 changes (before critic mh8)
- Heads spread evenly over each team again. Shorts side stripe removed. Back name lower (143 cm).
- Arms warmer. 3D crowd rows 8 -> 11 (Epic), 4 -> 5 (Medium). KAIZEN boards dark with white type. Radar smaller and translucent.

## Critic mh8 (5.5)
Errors: 3D crowd faces (black eyes, loose heads); 3D-to-card crowd break; hair card shards / scalp gaps; players interpenetrate in duels; floating sliver under the roof; wrist seam + ragged GK cuff; blank corner slab walls; glossy arms vs matte faces; grass smear at low angles; Team Setup #8 node overlaps labels.
Strengths named: kit fabric (7), broadcast composition, goal nets.

## Plateau (rounds mh4-mh8: 5.0 5.0 5.6 5.2 5.5)
Recurring ceilings that material work has not moved: groom-card hair up close, mannequin limbs/hands/boots, crowd faces + card back rows, procedural box city / blank stadium exterior.

## Round 9 changes (before critic mh9)
- 3D fans: eyes/teeth shaded brown-grey instead of black voids; pale skin tones toned down.
- Limbs: skin roughness 0.48 -> 0.62 (matte like the faces); white wrist tape over the hand/forearm joint; keeper sleeves reach the glove straps (no ragged cuff).
- Painted scalp coverage 2.2x (no skin between cards). Front number 9.5 cm (was 6.5).
- Lighting: sun 5100 K (less yellow), sky light 1.75 (shadow half less flat).
- Roof girders light painted steel (the dark edge-on girder read as a floating sliver).
- Player separation 64 -> 72 cm (duel interpenetration). Team Setup: lone CM node dropped below the wide pair.
- Broadcast camera 14 m high, FOV 31.

## Critic mh9 (5.5)
Errors: hair shards; duel interpenetration; GK chest blotches; stair-stepped shirt shadows; heads tilted down ~30 deg; shorts hem flap; ball floats in shade (no contact shadow); crowd LOD cut; blank corner slabs; arm pinch seam.

## Round 10 changes (before critic mh10)
- Hair cards shaded with a head-volume normal (80 % from the head centre, 20 % card): no more per-card faceting.
- Chin up: +12 deg neck/head lift on top of the clips (portraits no longer look at the grass).
- Ball contact-shadow decal (M_PK_BallShadow), fades with height.
- Sponsors differ between the sides; keeper kits pinstriped.
- Grass mowing bands differ in colour (visible in shade). 3D fans' hair coverage 2x.
- Sky light 2.0, contact shadow length 0.02 (blotchy cloth shadows). Camera 12 m high, 24 m back.
- Sock rib fine.

## Critic mh10 (5.5)
Errors: broadcast camera inside the near stand (r10 regression: camera too low/back); empty replay framing; sock without boot pushing into ball; white ghost figure; duel interpenetration; sponsor skewed against the stripes; hair cap edges; glowing ear; box fans in the upper tier; short GK sleeve.

## Round 11 changes (before critic mh11)
- Broadcast camera 18 m back, height clamped above the near stand rake (no more frames inside the crowd).
- Sponsor mapped on the same arc as the stripes. Keeper kits hoops. Knit tile 70 at 45 %.
- Skin scatter opacity 0.05 (ears), eye whites greyer, palest albedo cap 0.22. Keeper sleeves full length.

## Critic mh11 (5.4)
Errors: hair shards / floating mop; long fringe over an eye; buzz-cut ring; hand on ball; blank corner slabs; glazing sparkle; crowd hem/hand faults; crowd LOD step; floating city; overlapping players at kick-off; grass smear; untextured outer roof. Faces read 50-60 years old.

## Round 12 changes (before critic mh12)
- Faces younger: scan wrinkle normals at 50 %, AO crease tint lighter.
- No long side-swept fringe (big cuts use pulled-back / curly fade).
- Main-stand end walls (the "blank slab" since mh1) clad like the hospitality level (gen_stadium.py + build_stadium.py).

## Critic mh12 (5.4)
Errors: hair shards / see-through tips; duel interpenetration; crowd LOD pop; roof glazing sliver; fascia notch; blockout exterior; pale heads blow out; lumpy glove-like hands; camera tilts until no stand is in frame. Strengths: kits (7), ball (7.5).

## Round 13 changes (before critic mh13)
- No painted-scalp buzz cuts (every player gets cards: bald pale heads blew out). Skin specular 0.26.
- Separation 80 cm. Hand relief removed. Number outline subtler.
- Broadcast look-at clamped so the far stand stays in frame.

## Critic mh13 (5.4)
Errors: kick boot through an opponent's face; hair shards / cap at distance on pale scalps; head vs limb tone; bright eye whites on dark skin; elbow seam; faceted roof underside; bare exterior; box city; crowd LOD cliff. Missing officials / pitch-side life.
After r13: eye whites 0.38, limb skin warmer. Packaged as v1.2.

## Critic mh14 (5.7, best so far) — first round with the camera limit, photo skyline and Sketchfab boots
Errors: socks through boot openings, wide collars, faceted heel; pale #9 face; hair chips; ball through shin; cube-seam ball; crowd hands in faces; crowd speckle; flat card crowd; faceted fascia; dark fascia text; controls with shared buttons. Skyline 3.5: hidden behind the roofs, mountain read as a cliff.

## Round 15 changes (packaged as v1.3)
- Boots: whole mannequin foot hidden under the boot (instep poked through), shared normals (no faceted heel), baked sole mask -> two-tone sole, boot colour swapped when it matches the socks. (A per-slice collar fit tore the multi-shell collar into spikes: reverted.)
- Skyline: downtown-only stretch tiled (mirrored) round the horizon, band raised (Base 12, Stretch 1.35) so the towers clear the roof from every side, brighter; transparent below the band and out of GI (an opaque haze wall streaked the pitch).
- Fascia ribbon brightness 9000. Controls labelled Attack: / Defence: for the shared buttons.

## Critic mh15 (5.3) - first round under the inside-the-stadium-only brief (CRITIC_BRIEF.md)
Sub-scores: architecture 5.5, crowd 4.5, pitch 5.5, lighting 5.5, players 4.5, ball 6.5, HUD 6.0, menus 6.0, composition 5.5.
Errors (9): hair cards shredded / floating shelf; chalk-white crowd faces, floating crowd heads; crowd LOD cards visible;
boot collar clipping and ankle gap; grey patch through the back of a sock; pinstripes kink / keeper hoops wobble;
mismatched eye colours (one eye blank); flat dark stand-corner panels; Team Setup swap possibly not saved (Menu_05 vs Menu_09).
Note: skyline / exterior are out of scope since mh15, so 5.3 is not directly comparable to mh14's 5.7.

## Round 16 changes (before critic mh16)
- Hair: solid helmet LOD under the cards (MHHairBase, 98.5 %), so gaps show hair, not a bald shelf.
- Eyes: roughness 0.3 / specular 0.2 (the mirror eye reflected the sky: one eye read blue / blank), lid shadow.
- Kit patterns anti-aliased by screen footprint (stripe kinks, wobbling hoops).
- Boots: sock collar lowered above 10 cm (the tall white heel tab read as a grey patch on the calf).
- Stand corners: the end walls use the facade material instead of the dark cladding.
- Crowd: no near-white skin tones, hair coverage x3.
- Team Setup "not saved": expected (the menu capture restores the owner's sheet); noted in CRITIC_BRIEF.md.

## Critic mh16 (5.8) - inside-the-stadium scope; up from 5.3, errors 9 -> 6
Sub-scores: architecture 5.5, crowd 5.5, pitch 6.0, lighting 5.5, players 5.0 (kits 6.5, boots/hair 4), ball 6.5, HUD 6.0, menus 6.0, composition 6.0.
Errors (6): hair still hard-edged shards; sock/boot collar sawtooth (boot_a5_front); player overlap (action_carrier);
stand-end blocks alias/shimmer (facade stripes); faceted grey corner bases below the front rows; crowd 3D-to-card switch in frame.
Top issues: hair, cloned faces / chalky skin in sun / ear glow, crowd card switch, end blocks and corner bases, crude boots,
crowd clothing and poses, empty touchline (dugouts, staff), rigid net, plain interior surfaces, hands / wristbands,
flat stand lighting, grass close up, big screen content, HUD clutter, menus ("Prototype build" footer).
Fixed since mh15 and not re-raised: floating hair shelf, mismatched eyes, stripe kinks, grey sock patch, chalk crowd faces.

## v1.4 packaged (mh16 state): Downloads\PitchKings_Windows_v1.4.zip

## Round 17 changes (before critic mh17)
- Corner bases: bowl corners 20 -> 48 segments (the curved front wall was faceted).
- Stand-end blocks: plain concrete (the facade's fine window grid shimmered on the narrow blocks).
- Boot collar: calf hidden below 9.5 cm inside the collar (sock sawtooth through the opening).
- Hair cards thinned to 85 % coverage over the solid helmet base (finer, fewer shards).
- 3D crowd rows 11 -> 15 (Epic), 5 -> 6 (Medium): the card switch leaves close-up frames.
- Not changed: duel overlap (separation already 80 cm; reach in tackle animations).

## Critic mh17 (5.8) - inside the stadium
Sub-scores: architecture 6.0, crowd 5.0, pitch 5.0, lighting 6.0, players 5.0, ball 6.5, HUD 6.0, menus 6.5, composition 6.5.
Not re-raised from mh16: stand-end shimmer, faceted corner bases, boot collar sawtooth, duel overlap.
Errors (9): head vs body skin tone; one head repeated across both teams (only 6 scanned heads exist); hair cards (stray shard,
loose cards, stepped shingle); warped parapet behind the corner boards; crowd LOD switch to "peg" figures; grass washes to beige
at low angles; dugout seats smear (ghosting); faceted self-shadow on a shirt; brightness seam along a tier fascia band.
Top issues: heads (needs more heads: MetaHuman Creator), turf detail, crowd poses/seating, boots colourway, hands, stand back
walls, roof underside / floodlights, empty dugouts, big screens, bowl lighting, ball micro-detail, HUD, menus, kit cloth.

## Round 18 changes (before critic mh18)
- Heads: 12 MetaHuman Creator presets (Omari, Mateo, Kelvin, Lorenzo, Isaiah, Jorge, Trey, Mikel, Dominic, Victor, Orlando,
  Bruce), cloud-rigged and built at Medium (Tools/editor_scripts/metahuman/build_player_heads.py) -> DA_PK_HeadSet. Every
  player on a side has his own face; the away side starts half way round the set. Each face keeps its baked skin / eye /
  teeth materials. The face's neck and shoulder chunk is trimmed so it never pokes through the shirt.
- Hair, beard, moustache and brows: each preset's own groom cards (LOD 0/2/4) in head-bone space with M_PK_HairCardsMH (reads
  the MetaHuman card atlases: coverage, root-to-tip), colour from the groom's melanin.
- Limbs take the face's sampled skin tone (head / body match).
- Grass: rougher and less specular toward grazing views (beige wash at low angles).
- Dugout canopy: satin instead of mirror (reflected seats smeared as ghosts).

## Critic mh18 (5.8) - inside the stadium
Sub-scores: architecture 6.0, crowd 5.0 (6.5 far / 3.5 close), pitch 5.0, lighting 6.0, players 5.8, ball 6.5, HUD 6.0, menus 6.5, composition 6.0.
Not re-raised from mh17: head/body skin mismatch, repeated head, warped parapet, crowd peg switch, dugout ghosting, faceted shirt shadow, fascia seam.
Errors (5): grey dithered hair halo + noisy stubble cards; sock through the boot collar; torn collar spikes at the nape;
keeper collar notch hole; crowd heads seem to float on the chests behind (rows merge in one blue), paddle-like discs at faces.
Top issues: crowd close up (poses, outfits, sea of blue), turf (smeared streaks), hair, lighting (hard sun split, flat shade),
kit folds, casting (one ~65-year-old head: Lorenzo), plain interior architecture, broadcast framing, big screens, roof
underside, boots, empty dugouts, ball, net, HUD, menus.
Note: the grass (grazing roughness) and dugout canopy fixes were NOT in this round (files locked by hung commandlets).

## Round 19 changes (before critic mh19)
- Hair material: edge-on / faint coverage drops out (smoothstep 0.2-0.65), tips no lighter than roots; beards and
  moustaches at coverage 1.3 (solid, not a pixel cloud).
- Collar: faces rebuilt fresh (Build2) with a gentler trim (front kept down into the collar notch, back to the collar,
  shoulders cut); the shirt's collar band is lifted 6 mm off the neck (WPO) so it no longer cuts through it in spikes.
  (Tried and dropped: the mannequin's own neck as filler - it flares wider and read as a flat plastic band.)
- Hair: a dense, darker copy of the hair cards just under the hair (thin cuts showed the pale scalp).
- Casting: Cameron (young, braided scalp) replaces Lorenzo (read as ~65).
- Sock hidden up to 10.5 cm inside the boot collar.
- Crowd shirts: wider value / saturation spread of the club colour, 48 % casual clothes (8 colours incl. stone, white).
- Grass grazing roughness / specular and satin dugout canopy (from round 18) applied.
- Bruce head added (12 heads); MetaHuman probe folder deleted.

## Critic mh19 (5.8) - inside the stadium
Sub-scores: architecture 5.0, crowd 5.0, pitch 5.5, lighting 6.5, players 6.5, ball 6.5, HUD 6.0, menus 6.5, composition 6.0.
Not re-raised from mh18: keeper notch hole, nape collar spikes, crowd rows merging, casting.
Errors (6): replay camera framed on empty turf (bc_1); roof fascia ring torn (slits, odd panel, cyan strip, facets:
fascia_red); hair seam down the crown + halo (face_h9); speckled hair tips, white artefact in the ear (face_a9); jagged
sock hem above the boot, faceted boot (boot_h4); crowd 3D-to-card switch inside a tier, standing cards hovering (crowd_away).
Top issues: crowd mannequins (poses, outfits), grey-box interior (divider walls, parapets, rails, tunnels), turf like carpet,
roof ring / floodlights, hair, untextured boots, bare big screens, rigid nets, plastic arms / stiff hands, repeated boards,
plain HUD / replay, empty dugouts, flat lighting off the sun, generic menus.

## Round 20 changes (before critic mh20)
- Replay camera cuts to the rewound ball when the replay starts and tracks faster (it eased over empty turf).
- Roof ribbon: the LED-board edge test dimmed all of it to 5 % except the first metre (dark band + one bright panel):
  board edges now optional (off on the ribbon), brightness 2500 nits; the ribbon sits 6 cm proud of the roof edge and
  overlaps it (flush, it tore into slits).
- Hair: underlay coverage 1.6 (3 made faint cards a grey halo), specular 0.15 / roughness 0.7 on the cards; Mateo
  (fringe over the eye, grey tips) out of the set: 11 heads.
- Boots 3 % roomier, sock hidden to 10 cm (torn hem at 10.5, poke-through at 9.5).
- Crowd: card rows within 6 rows behind the 3D fans stand (the standing 3D band seemed to hover over seated cards).
- Turf: shorter blade streaks (1.6 x 0.45 cm, the 5 cm streaks read as smeared carpet) + 25 cm clumps.
- Lighting: sun source angle 4 (softer shadow edge), sky light 2.6, local exposure shadow contrast 0.62 (bounce in the shade).

## Critic mh20 (6.2) - best so far
Sub-scores: architecture 5.5, crowd 4.5, pitch 6.5, lighting 6.0, players 6.0, ball 6.5, HUD 6.5, menus 6.8, composition 6.5.
Not re-raised from mh19: replay framing, roof ribbon slits, sock hem, crowd standing band.
Errors (5): skin patch under the home keeper's collar (keeper_home); crowd heads float over collars / thin necks / dark
eye sockets (crowd_close, crowd_extreme); black smudges at the temples + speckled hair (r_face_away4, r_hair_home7);
faceted self-shadow on the shirt (kit_extreme); beige block in the roof band where banners meet (big_screen).
Top issues: crowd up close, bare stand architecture (tier fronts, slab end walls, no rails / tunnels / signage), low-poly
kits and boots, hair, bowl lighting (stands under the roof lit like open sun), keeper kits flat, empty pitch-side, grass
close up, rigid net / corner flag, lifeless players, scoreboard, HUD, controls screen, repeated boards.

## Round 21 changes (before critic mh21)
- Keeper chest patch: the face's front neck is cut 6 cm below neck_01 (9 poked through the shirt; 3 left a hole).
- Hair: no shrunken underlay (it sank into the temples as black smudges); hair cards at coverage 1.25 instead.
- Crowd eyes lighter (dark empty sockets).
- Sun shadow slope bias 1.0 / bias 0.7 (stair-stepped self-shadow on low-poly cloth).
- LED board / roof ribbon material matte (roughness 0.65, specular 0.1): the sun glared off faceted ribbon corners as
  beige blocks.
- Crowd eyes mid grey (0.24 glowed white; 0.1 read as empty sockets).
- Architecture: a steel safety rail (posts every 2.5 m, top and mid rails) along the front row all round the bowl; a 2 m
  LED band across both faces of the main stand's end-wall slabs.

## Critic mh21 (5.8) - down from 6.2
Sub-scores: architecture 5.5, crowd 4.0, pitch 6.0, lighting 5.5, players 5.0, ball 6.5, HUD 5.5, menus 6.5, composition 6.0.
Errors (7): hair (scalp diamond patches / star bursts face_away9, floating fragments + grease patches face_home5, blocky
chunks face_away4, sparse cards face_home7/9); collar / neck shards (face_home9, face_home7, face_home5, keeper notch
jagged); upper-tier crowd boxes (roof_underside); flat end-wall slabs (wide_bowl, big_screen); players overlap
(action_carrier); boot heel spike + white noisy blob (boots_side_away6, boots_home4); grass smear near the camera.

## Round 22 changes (before critic mh22)
- Hair cards back to coverage 1.0 (1.25 hardened them into blocky chunks); Cameron wears Trey's afro fade (his braids are
  strands only: the bare baked braid pattern read as a fault); Jorge shaved (thinning groom read as grease patches).
- Neck trim: shoulder cut from 6 cm off the neck axis, up to 6 cm above neck_01 (trapezius shards).
- Grass detail nearly isotropic (0.8 x 0.6 cm grain): any streak along the mowing direction smeared near the camera.
- Two LED bands on each face of the main stand's end-wall slabs.

## Critic mh22 (5.6)
Sub-scores: architecture 5.0, crowd 4.5, pitch 6.0, lighting 5.5, players 5.5 (faces ~7, boots ~3.5, hair ~4.5), ball 7.0,
HUD 6.0, menus 6.5, composition 5.5.
Errors (8): brown blotches under the home keeper's collar / chest; curly hair cards flake; dither noise on thin hair,
beards, brows; collar clips the neck (red patch, brown tab); sock surface over the knee (boots_side_away_6); glazing
speckle + smudges under the roof beam; white wall slab at a stand corner; lighter blotch on a bald scalp.
Trend mh17-mh22: 5.8, 5.8, 5.8, 6.2, 5.8, 5.6 - a plateau around 6; the critic's blockers are now assets (crowd poses /
seats, boots, kits, hair library), not tuning.

## Reverted to the round-20 state (owner's call after mh22)
Rounds 21-22 undone: head trim back to v4 values (faces rebuilt fresh in /Game/MHHeads/Build3), hair underlay 1.6 back,
no hair overrides (Cameron / Jorge own grooms), crowd eyes 0.1, sun shadow biases at engine defaults, LED board material
glossy again, grass blades 1.6 x 0.45, no front rails, no end-wall LED bands. Kept from round 20: everything listed under
"Round 20 changes".
