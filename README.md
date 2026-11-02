# OpenRB Distort

A JUCE VST3 / AU / standalone effect that wraps the ReBirth distortion from
[OpenRB-338](https://github.com/pingw33n/openrb-338). The DSP is a copy of `orb-engine`'s
`Distortion` in `third_party/openrb-338`; this project does not rewrite it.

The effect always runs at 44100 Hz in 64-sample blocks, the same grid the original uses. Host
audio at any other rate is resampled in and out. Distortion has no tempo, pattern, or song
clock. Shape 0 is ReBirth 1.5's hard clip; shapes 1..127 use the original waveshaper.

This plugin is GPL-3.0-only. JUCE is pulled under its AGPLv3. The engine copy is the
`orb-engine` and `rbs` crates only, under the same license. Copyright (C) 2026 the
OpenRB-338 authors. OpenRB-338 itself is not modified.

## Layout

```
third_party/openrb-338/crates/orb-engine   # copied engine
third_party/openrb-338/crates/rbs           # copied song-format crate the engine links
```

## Build

Rust 1.99.1 (see `rust-toolchain.toml`) and CMake 3.24+. macOS builds are universal
(`arm64` + `x86_64`) so native Ableton 12 on Apple Silicon can load the VST3. The first
build will ask `rustup` for `aarch64-apple-darwin` if that target is missing.

```sh
cargo test
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

VST3, AU, and the standalone app are copied into `output/` after the build. Nothing is copied
into the system plug-in folders.

```
output/VST3/OpenRB Distort.vst3
output/AU/OpenRB Distort.component
output/Standalone/OpenRB Distort.app
```

## Controls

| Control | Range  | Notes                                      |
|---------|--------|--------------------------------------------|
| Amount  | 0..127 | drive; default 70                          |
| Shape   | 0..127 | 0 = 1.5 hard clip; 1..127 = waveshaper row |

At 44100 Hz the resamplers are bypassed, so the samples that reach `Distortion` are the host samples.
