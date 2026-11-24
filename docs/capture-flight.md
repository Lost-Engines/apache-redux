# Offline flight capture

With `BUILD_TESTING=ON`, the `apache_capture` target runs a live mission without a window and writes RGBA frames, optional stereo audio and telemetry. It is a development tool and is excluded from runtime packages.

```sh
cmake --build build --target apache_capture -j4
build/apache_capture DATA_DIRECTORY -3 3840 2160 90 6 --audio
```

Arguments select the game data, mission, width, height, horizontal FOV and model LOD distance multiplier. Negative mission IDs select Quickstart: `-1` Yemen, `-2` Korea, `-3` Cyprus. The tool uses realistic physics, maximum visual detail and the original 640-resolution cockpit artwork. `--audio` connects the same mixer and sound callbacks used by the game, without opening an audio device.

The sibling `apache-reverse` repository contains `tools/capture_comparison_flight.py`, which drives this executable or the original instruction harness using the same timed keyboard script. Its `tools/flights/README.md` describes the supplied 90-second flight and lossless video capture commands. Retain one lossless master for each engine and resolution. Apache video, stills and capture metadata default to `/mnt/extra2/lost-engines/video/apache/`, outside the repositories. File names include the engine and resolution. Use `--size 1920x1440` with the wrapper for a 4:3 recording matching the original aspect ratio.

## Stream protocol

Each whitespace-delimited request has this form:

```text
frame|audio|step TICKS HELD_COUNT SCAN... EVENT_COUNT SCAN...
```

`TICKS` is a number of original 200 Hz clock ticks, from 1 to 200. A zero interval draws the current state without advancing. `HELD_COUNT` and its following scan codes give the complete set of held keys; the tool applies press and release changes. `EVENT_COUNT` and its scan codes post discrete commands. For example:

```text
frame 8 0 1 64
```

This advances 40 milliseconds with no held keys and presses F6. The response is one JSON line followed by `bytes` bytes of RGBA pixels and then `audio_bytes` bytes of audio. A `frame` response has `width * height * 4` video bytes. With `--audio`, each tick produces 240 stereo sample frames at 48 kHz. Samples are interleaved 32-bit little-endian floats. The header records the audio sample count and rate alongside game clock, aircraft and camera transforms, flight readings, weapons and damage state.

Use `audio` in place of `frame` to draw and mix the complete scene while returning only the audio payload. This supports adding sound to an existing video. `step` advances without drawing. Both return zero video bytes and include PCM when `--audio` is enabled. Sending EOF finishes the process. Standard error is reserved for errors; standard output carries only the capture protocol.

Camera updates occur during drawing, as in the game. A final recording should therefore use `frame` throughout, with matching intervals and inputs for the original and Redux. The capture wrapper does this and writes the inputs and state to JSONL for later comparison. Redux recordings include game audio by default, encoded as 24-bit stereo FLAC at compression level 12. Original-engine recordings remain silent.

`audio_events` logs requested sound callbacks as four integers: kind, sample/event ID, volume/state and pan. Kinds are 0 for effects, 1 for cockpit voices, 2 for radio voices, 3 for flight events and 4 for the gun loop.

The wrapper can upgrade a silent Redux master with `--add-audio`. It replays the same flight and checks aircraft and camera transforms, view, weapon and damage state for every frame before replacing the master. It copies video packets directly and verifies their hash, audio duration and lossless PCM round trip. Temporary audio and video files are removed after verification, leaving one master per recording.
