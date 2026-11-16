# Release checks

Run all of these before shipping a build. Every one must pass.

| Check | Command | What it covers |
|---|---|---|
| UI tests | `build/PedalSteelShot_artefacts/Release/PedalSteelShot.exe` | Editor interaction, keyboard, menus, screenshots |
| Host test | `build/PedalSteelHostTest_artefacts/Release/PedalSteelHostTest.exe` | Loads the built VST3 like a DAW: scan, audio, editor, state |
| pluginval | `pluginval --strictness-level 10 --validate-in-process --sample-rates 44100,48000,96000,192000 --block-sizes 1,64,441,1024 --validate "<path>/NVA Pedal Steel.vst3"` | Host-compatibility suite, parameter and state fuzzing |
| Stress | `build/PedalSteelStress_artefacts/Release/PedalSteelStress.exe 1` | Fuzzed MIDI and automation at 7 sample rates x 6 block sizes, audio-thread allocations, CPU, corrupted state, bad preset files, threads, editor open/close |
| Memory errors | `cmake -B build_asan -DPEDALSTEEL_ASAN=ON`, build `PedalSteelStress` (RelWithDebInfo), run with `--quick` | AddressSanitizer over the stress suite (needs the MSVC bin folder on PATH for the ASan DLL) |
| Dependencies | `dumpbin /DEPENDENTS` on the `.vst3` | No `MSVCP140` / `VCRUNTIME140` (static runtime) |

Notes
- The stress tool measures CPU time used by the audio thread, so its load figures hold on a busy machine.
- Hosts at 88.2 kHz and above: the engine runs at 44.1 / 48 kHz and is upsampled (16 samples of reported latency at 2x, 32 at 4x).
- Not covered by automation: a real session in each target DAW (FL Studio, Ableton, Reaper), code signing, and the installer on a clean PC.
