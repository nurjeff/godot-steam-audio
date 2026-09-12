# godot-steam-audio
This is a GDExtension that integrates the [steam-audio](https://valvesoftware.github.io/steam-audio/) library
into Godot 4.4. This adds sound effects such as occlusion and reverb into the engine.

## About this fork

A maintained fork of [stechyo/godot-steam-audio](https://github.com/stechyo/godot-steam-audio), built
against Steam Audio 4.8.0 and godot-cpp 4.4, developed and tested on Godot 4.7.2. It exists because
upstream cannot change its acoustic scene at run time without crashing, and because a game needs more
than upstream exposes.

Fixed here, among others: mutating acoustic geometry while the reflection simulation runs, freed
players left in the source list, parametric and hybrid reverb scaled to silence, Godot's 5 kHz
attenuation filter muffling every source, reverb cut off the moment a sound ended, impulse response
dimensions taken from the wrong settings, and a failed Steam Audio start handing back uninitialised
handles. The portable ray tracer is the default, because Steam Audio's Embree backend leaves a dead
scene behind the first time a level is unloaded.

Added here: an editor menu that tags a scene's geometry and validates the setup, an OBJ dump of the
scene the simulation actually traces, sound pathing and baked reverb through `SteamAudioProbeBatch`,
and reverb that rings out after a source stops. Each fix has a reproduction and a measurement in its
commit message.

Defaults differ from upstream, chosen so that a scene which just drops in geometry, a source and a
probe volume sounds right without touching a property:

- `distance_attenuation` is on. A 3D source that does not get quieter with distance is rarely the
  intent, and Steam Audio's curve replaces Godot's rather than stacking with it.
- `pathing` is on. It costs nothing until a `SteamAudioProbeBatch` with baked paths exists, because
  the simulator skips sources it has no probes for. Once probes exist it is the difference between a
  source that muffles behind cover and one that falls off a cliff: occlusion has no diffraction
  model, so on its own a 4 m wall in open air takes 59.5 dB and the level lurches by up to 21 dB as
  the listener merely turns on the spot. With pathing that wall costs 35.2 dB and holds steady.

Configuration warnings only report what a single scene can actually decide. Whether a config or
a listener exists somewhere is not that: a player usually lives in an avatar scene instanced at
run time, and the environment is often built in code. Those are now reported once at run time,
when a source is genuinely waiting on one, instead of on every sub-scene. Warnings about panning
strength and the attenuation filter are gone too, because this node overrides both itself, so
they fired on every untouched player and said nothing actionable.

- `occlusion_radius` is 2 m rather than 4 m. Volumetric occlusion models the source as a sphere
  and reports the fraction of it the listener can see, so the radius doubles as the width of the
  shadow edge. Measured stepping past a door jamb at 10 cm intervals: 1 m ramps over 40 cm with a
  worst step of 9.2 dB, 2 m over 70 cm and 7.1 dB, 3 m over 90 cm and 6.7 dB but only 0.95
  occluded with a clear sightline. At the old 4 m it never occluded past 0.50 at all, leaking half
  the direct sound through a wall for ever. 2 m is the widest that still reaches both 1.0 and 0.0.

Place sources clear of surfaces. Volumetric occlusion samples a sphere around the source and first
discards every sample the source itself cannot see, so one sitting exactly on a floor or a table
top degenerates to on/off whatever the radius is set to, and the level drops off a cliff the
instant the sightline breaks. Measured at 10 cm steps through a doorway: on the floor the fraction
went 1.00, 1.00, 1.00, 1.00, 0.00, 0.00 with the radius making no difference at all; lifted 25 cm
clear it went 0.88, 0.72, 0.48, 0.16, 0.04, 0.00. `get_occlusion_level`, `get_transmission_level`
and `get_distance_attenuation_level` report what the simulation last computed, so this is visible
rather than guessed at.

Probe spacing has to be fine enough to put a probe inside a doorway, or the path graph has no
route through it. In a house with 1 m doorways, dropping the spacing from 1.5 m to 0.75 m lifted
the level just past a jamb by 6.5 dB and, more tellingly, made it stop falling: at 1.5 m it kept
sliding from -58 to -65 dBFS as the listener walked away along the wall, where at 0.75 m it held
flat around -57, which is what a doorway acting as the source sounds like.

`SteamAudioProbeBatch` now also registers itself with the simulator when you bake it directly, not
only when `prepare_on_ready` is set. Baking an unregistered batch used to produce real probes, a real
file and a real probe count that the simulator never saw, which is indistinguishable from having no
probes at all. It warns, too, when it bakes at run time with no `data_path`, since that repeats the
whole bake on every launch.

Windows and Linux x86-64 are built and exercised; Android, macOS and iOS come from CI and are
unverified. Single listener only.

This extension has been created and maintained by me (@stechyo), but due to a lack of time/interest in game
development in the past year this is not really being maintained/developed at the rate it could be. I am, of
course, extremely thankful to all of the people who have opened issues and PRs, starred the project or
generally taken an interest in it. If anyone is interested in maintaining the project, feel free to fork it.
If you have any questions about the code/architecture, email me or DM me on Twitter at any time.

### [Demo Video](https://www.youtube.com/watch?v=vRnzfnb93Gw)
![A picture of the editor screen with some godot-steam-audio nodes.](doc/imgs/editor.png)

SteamAudio is a library made by Valve that improves aural immersion in games by adding effects such as
rotational and positional tracking and real-time sound propagation (occlusion and reflection). It is used in
[many games](https://steamdb.info/tech/SDK/Steam_Audio/), such as Counter-Strike 2 and Half-Life: Alyx, and
Valve offers [official plugins](https://valvesoftware.github.io/steam-audio/downloads.html) for Unity, Unreal,
and FMOD. This Godot extension is an unofficial plugin, but our end-goal is to offer the same features and
allow Godot devs to use them in their games. This extension is open-source and
MIT-licensed, while SteamAudio is Apache-licensed (but still open-source), as
per its [license](https://github.com/ValveSoftware/steam-audio/blob/master/LICENSE.md). Please note that
SteamAudio does use proprietary libraries, and to get a fully open-source extension you'll need to explicitly
compile everything by yourself to choose not to include them.

This extension is in an alpha phase, will have bugs and missing polish, and may crash. Don't expect to be able
to ship a game with this extension right now unless you are ready to make some fixes/optimizations to it on
your own (and if you do, PRs are accepted). Linux and Windows are currently working. Mac probably works, but I
don't have the time nor the money to support that, sorry.

### Features 
 - Spatial ambisonics audio 
 - Occlusion and transmission through geometry 
 - Distance attenuation
 - Reflections (reverb), as convolution, parametric or hybrid
 - Dynamic geometry
 - Sound pathing: audible routes around corners, baked into probe volumes
 - Baked reverb, as a cheaper alternative to tracing reflections every frame
 - Reverb tails that ring out after a source stops
 - Editor tooling: geometry tagging, scene validation, a probe volume gizmo, an OBJ scene dump

 To come: 
 - Baked reflections from a static source position, not just listener reverb
 - Multiple listeners
 - TrueAudio Next and Radeon Rays back ends

### Getting started
Check [Installation](https://github.com/stechyo/godot-steam-audio/wiki/Installation) for how to install the extension, [Project setup](https://github.com/stechyo/godot-steam-audio/wiki/Project-setup) for how to integrate it with your project, and [Contributing](https://github.com/stechyo/godot-steam-audio/wiki/Contributing) if you're interested in improving the extension.

### Acknowledgements
godot-steam-audio is developed by [stechyo](https://github.com/stechyo). [<img src="https://github.com/gauravghongde/social-icons/blob/master/SVG/Color/Twitter.svg" width=14/>](https://twitter.com/stechyo_) [<img src="https://github.com/gauravghongde/social-icons/blob/master/SVG/Color/Youtube.svg" width=14/>](https://www.youtube.com/@Stechyo/)
Check the [contributors](https://github.com/stechyo/godot-steam-audio/graphs/contributors) for other authors.

godot-steam-audio uses the Steam® Audio SDK. Steam® is a trademark or registered trademark of Valve
Corporation in the United States of America and elsewhere.
Steam® Audio, Copyright 2017 – present, Valve Corp. All rights reserved.

Vespergamedev's [GDNative module](https://github.com/vespergamedev/godot_steamaudio) was helpful in guiding the early development of this extension.

The icons for the SteamAudio nodes are from Godot, with color changes that match one of the colors in
the Steam Deck OLED page. These are MIT-licensed, so they are Copyright (c) 2014-present Godot Engine
contributors.
