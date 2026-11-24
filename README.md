# Apache Redux

An enhanced edition of Digital Integration's **Apache Longbow**, released as **Apache** in the US. Built on our [Apache C++ reconstruction](https://github.com/Lost-Engines/apache-cpp), part of the [Lost Engines](https://github.com/Lost-Engines) project.

Quickstart, training, single missions and campaigns are playable, with the original software graphics, flight models, combat, instruments, mission planner, music and speech. LAN multiplayer includes combat, cooperative missions and a shared pilot/gunner cockpit.

Redux adds high-resolution rendering, widescreen views, an adjustable field of view, smoother HUD lines and optional MFD overlays.

## Getting started

[Download the latest release](https://github.com/Lost-Engines/apache-redux/releases/latest) for Linux, Windows or macOS and unpack it.

On Windows, run `scripts\fetch-assets.bat`, then open `apache.exe`. On Linux or macOS, run these commands inside the unpacked directory:

```sh
./scripts/fetch-assets.sh
./apache
```

The fetch helper downloads the reference CD from Archive.org, verifies its checksum and extracts the game files. The temporary CD image is removed afterwards. On Windows, use `scripts\fetch-assets.bat`. If your executable is somewhere other than `build/` or the package directory, set `APACHE_EXECUTABLE` to its path.

Already have the game? Select an extracted CD directory or a matching Windows software installation:

```sh
./apache --data-dir /path/to/apache
```

That location is remembered for subsequent launches. The reference is the January 1998 software build, supplied on the Sold Out and October 2000 European CDs. Other engine versions are not supported yet. Original files are read without modification.

## Improvements

- **Higher resolutions.** Choose automatic window sizing or a fixed resolution, including 4K and high-DPI displays. Rendering and cockpit scaling are optimised for larger windows.
- **Widescreen cockpit views.** The world fills the space around the cockpit, including the lower corners. Side-view artwork stays against the screen edges.
- **Adjustable field of view.** Set the FOV in Preferences or on the command line, with HUD aiming cues that stay aligned with the scene.
- **Longer detail distances.** Keep aircraft, scenery and effects detailed farther away, with an adjustable model detail distance.
- **HUD with live MFDs.** H cycles through the plain HUD, HUD with the pilot’s two MFDs, and HUD with the gunner’s two MFDs. The displays update live in the lower corners and share the cockpit’s page selections.
- **Smooth HUD lines.** HUD lines render smoothly at your chosen resolution. Text keeps the original pixel lettering.
- **Sharper target views.** The F3 target preview and FLIR MFD images render at full resolution.
- **Clearer model details.** Tree trunks, aerials and other model lines thicken with proximity and resolution.
- **Camera controls while paused.** Orbit, zoom and change views to inspect the scene or compose screenshots.
- **Convenient shortcuts.** Alt+Enter toggles fullscreen, Escape opens the exit prompt, and pressing F1 again closes the manual.

## Gameplay fixes

- A-10s and F-15s lower their landing gear on approach and keep it down while taxiing or parked, then retract it after takeoff.
- Aircraft on the ground no longer release chaff or flares.

## Playing

Flight opens the training, single-mission and campaign choices. Briefing maps support route editing, payload selection and a live 3D preview. Use Preferences to choose graphics, flight model, difficulty, audio and keyboard/joystick controls.

Arrow keys control pitch and roll, Q/A change collective, Enter selects a weapon, and Space fires. F2/F3 select the pilot/gunner cockpit; F4/F5 look left/right. F1 opens the illustrated manual in single-player flight; press F1 again or F2 to return to the pilot cockpit. P or Pause pauses; Escape asks whether to end the sortie. Press Y to confirm, or N or Escape to cancel. Ctrl+Escape also works.

F6 shows the helicopter from outside. Hold Alt and use the arrow keys (or your selected joystick) to orbit it; comma and period zoom out and in. Camera movement and view switching work while paused. H selects the full-screen helmet display. Press H again to add the pilot’s two MFDs in the lower corners, again for the gunner’s pair, and again for the plain HUD. Brackets cycle the displayed MFD pages; Shift reverses direction and Ctrl switches a display off or on. F2 returns to the pilot cockpit. I switches the HUD between black and green; V toggles the pilot’s night-vision system (PNVS).

`--skip-intro` skips the opening sequence, and `--no-audio` disables playback. Run `apache --help` for other options.

Pilot profiles and preferences are saved under `$XDG_DATA_HOME/apache-redux` or `~/.local/share/apache-redux` on Linux, and `%LOCALAPPDATA%\apache-redux` on Windows. `--save-dir` selects another location. Game files are shared with Apache C++ under `$XDG_DATA_HOME/apache/game` or `~/.local/share/apache/game` on Linux, so one download serves both versions. On Windows, they live in `data` beside the executable.

## Graphics options

Press **Alt+Enter** to toggle fullscreen at any time, or launch with `--fullscreen`. Returning to a window restores its previous size and position.

No options are needed for automatic resolution. To start fullscreen, choose a fixed render size or a wider view:

```sh
./apache --fullscreen
./apache --resolution 1920x1080
./apache --resolution 3840x2160 --fov 110
./apache --resolution auto --fov 100 --lod-distance 6
```

Fullscreen uses the display’s current resolution and refresh rate. Automatic rendering follows that resolution; `--resolution` can still select a smaller render size. In a window, automatic resolution follows its actual pixel size as you resize it. Original menus and cockpit artwork keep their pixel detail and proportions.

`--fov` sets the horizontal viewing angle in degrees, from 30 to 150. The default is the original 90°. HUD symbols scale with the field of view, keeping acquisition cues and target markers aligned with the scene; cockpit artwork and MFDs retain their normal size. A fixed resolution stays unchanged when the window is resized; its image is fitted without stretching. `--scale 2` sets the initial window to 1280×960 while keeping automatic rendering, or doubles the chosen fixed resolution's window size.

**Preferences** includes **Field of view** and **LOD distance**, with a scene preview. Both choices are remembered between launches. Command-line values override saved choices for that run; selecting a new value in the menu saves it.

`--lod-distance` keeps detailed models visible farther away. The default is **6×** the original distances; **1×** restores the original thresholds. It covers aircraft and attached parts, buildings, shadows, craters, debris and effects, without extending the terrain cutoff. The command line accepts values from 0.1 to 32. Larger values draw more objects and may reduce performance.

The renderer runs on the CPU. A lower fixed resolution can improve performance on slower machines. The original cockpit and menu images retain their source detail; increasing resolution sharpens the 3D scene and target previews, including the small screen in F3. The original 320/640 preference selects cockpit artwork, independently of Redux's render resolution.

## Multiplayer

Two Player offers combat, cooperative missions and a shared pilot/gunner cockpit. Network supports combat with up to eight Apache players. Players on the same LAN are discovered automatically; a host address can also be entered in the menu. Everyone must choose the same network channel. These sessions connect Apache Redux and Apache C++ copies using UDP, rather than the original IPX or modem drivers.

Press Shift+M during flight to chat, Enter to send, and Up to recall the last message. Pause and the manual are disabled during network flight, as in the original.

Capture the Flag needs the complete `MISSDATA/CAPTURE.DTA` from the US or Japanese CD. The reference European disc is missing the flag and respawn tasks; the menu explains this before takeoff.

## Building from source

Use CMake 3.30 or later, a C++26 compiler and OpenGL development libraries. The [build guide](docs/build-release.md) lists platform dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j3
ctest --test-dir build --output-on-failure
./scripts/fetch-assets.sh
./build/apache
```

`apache_inspect` exercises the decoded resources and simulation without opening a window. See [implementation and validation](docs/implementation.md) for the original-code comparisons and release checks. Game data and generated captures are kept outside this repository.
