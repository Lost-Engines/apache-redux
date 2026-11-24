# Offline flight capture

With `BUILD_TESTING=ON`, the `apache_capture` target runs a live mission without a window and writes RGBA frames and telemetry. It is a development tool and is excluded from runtime packages.

```sh
cmake --build build --target apache_capture -j4
build/apache_capture DATA_DIRECTORY -3 3840 2160 90 6
```

Arguments select the game data, mission, width, height, horizontal FOV and model LOD distance multiplier. Negative mission IDs select Quickstart: `-1` Yemen, `-2` Korea, `-3` Cyprus. The tool uses realistic physics, maximum visual detail and the original 640-resolution cockpit artwork.

The sibling `apache-reverse` repository contains `tools/capture_comparison_flight.py`, which drives this executable or the original instruction harness using the same timed keyboard script. Its `tools/flights/README.md` describes the supplied 90-second flight and FFV1/MP4 encoding commands. Apache video, stills and capture metadata default to `/mnt/extra2/lost-engines/video/apache/`, outside the repositories. File names include the engine and resolution. Use `--size 1920x1440` with the wrapper for a 4:3 recording matching the original aspect ratio.

## Stream protocol

Each whitespace-delimited request has this form:

```text
frame TICKS HELD_COUNT SCAN... EVENT_COUNT SCAN...
```

`TICKS` is a number of original 200 Hz clock ticks, from 1 to 200. A zero interval draws the current state without advancing. `HELD_COUNT` and its following scan codes give the complete set of held keys; the tool applies press and release changes. `EVENT_COUNT` and its scan codes post discrete commands. For example:

```text
frame 8 0 1 64
```

This advances 40 milliseconds with no held keys and presses F6. The response is one JSON line followed by exactly `width * height * 4` bytes of RGBA pixels. The header includes this byte count, game clock, aircraft and camera transforms, flight readings, selected weapon, ammunition, target and damage state.

Use `step` in place of `frame` to advance and return only the JSON state line, with a byte count of zero. This is useful for planning flight controls. Sending EOF finishes the process. Standard error is reserved for errors; standard output carries only the capture protocol.

Camera updates occur during drawing, as in the game. A final recording should therefore use `frame` throughout, with matching intervals and inputs for the original and Redux. The capture wrapper does this and writes the inputs and state to JSONL for later comparison. It records video without audio or overlays.
