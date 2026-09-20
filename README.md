# TaskFlow Pro

TaskFlow Pro is a compact macro recorder for Windows with two recording slots, configurable global shortcuts, profiles, loops, and adjustable playback speed.

## Features

- Record global mouse and keyboard input.
- Use one recording normally or alternate `A → B` continuously.
- Pause and resume recording or playback.
- Playback speeds from `0.25×` to `5×`; the default is `1×`.
- Finite repetitions or infinite looping.
- Configurable global shortcuts.
- Save and open `.taskflow` profiles.
- Dark and light themes.
- English and Spanish interface.
- Compact view controlled with the title-bar square button.

## Downloads

GitHub Releases can provide two portable builds:

- `TaskFlow-Windows-x64.zip` for 64-bit Windows.
- `TaskFlow-Windows-x86.zip` for 32-bit Windows.

No installation is required. Extract the ZIP and run `TaskFlow.exe`.

## Default shortcuts

- `F8`: start or finish recording.
- `F9`: play, pause, or resume.
- `F10`: stop.

The shortcuts can be reassigned from Settings.

## Building

### Zig

Install Zig and run:

```bash
./build-zig.sh
```

This creates both architectures in `build/windows-x64` and `build/windows-x86`.

### Visual Studio

Open the Developer Command Prompt for the desired architecture and run:

```bat
build.bat
```

## GitHub Releases

The included workflow builds x64 and x86 artifacts automatically. Push a tag such as `v2.3.0` to create the release build.

## Privacy and fair use

TaskFlow works locally and does not transmit recordings. Use automation only where it is permitted.

## License

MIT © Soma
