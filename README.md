# Glate - Translator and Text-to-Speech for Desktop

![Glate: translate and listen, online or offline](screenshots/banner.png)

Glate is a fast desktop translator with built-in text-to-speech, history, and sharing tools.
It translates online, or offline on your own computer with downloadable language models.

## Features

- Translate between 100+ languages
- Offline mode: download a language once and translate without internet; your text never
  leaves your device (Pro)
- Line-by-line translation view for structured text
- Text-to-speech playback with voice and gender options
- Export speech to audio formats with advanced controls
- Translation history with search, quick copy, and text export
- Share translations with built-in paste link support
- Global quick-translate hotkey support on X11/XWayland
- Dark and light themes

## Screenshots

![Translate between 100+ languages](screenshots/00-hero.png)
![Translate offline, privately](screenshots/01-offline.png)
![Nearly 60 languages offline](screenshots/02-languages.png)
![Check it line by line](screenshots/03-linebyline.png)
![Every translation, kept](screenshots/04-history.png)
![Share it, or listen to it](screenshots/05-share.png)
![Make it yours](screenshots/06-settings.png)

The screenshots are made with `scripts/screenshots.sh` from a dev build. An older video tour
of Glate is on [YouTube](https://www.youtube.com/watch?v=FxTDqcIgk7g).

## Install via Snap

Install from Snap Store:

```bash
snap install glate
```

[![glate](https://snapcraft.io/glate/trending.svg)](https://snapcraft.io/glate)
[![Get it from the Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)](https://snapcraft.io/glate)

## Build From Source

### Linux desktop build (Qt + CMake)

```bash
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j
```

## Packaging Instructions

### Flatpak

```bash
./build-flatpak.sh
```

This creates a versioned bundle like `glate-4.3.0.flatpak`.

### Snap

```bash
snapcraft
```

Snap version is derived from the CMake project version during build.

### Windows

```bash
cmake -S . -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j
cpack --config cmake-build-release/src/CPackConfig.cmake
```

Windows packaging output is generated through NSIS/ZIP via CPack.

## Global Hotkey Note (Wayland)

Global hotkeys are not available on native Wayland sessions due to compositor security restrictions.
If you need system-wide quick-translate hotkeys, run Glate on an X11 session (or via XWayland where supported).

## Credits

[Alan Pope](https://github.com/popey) of [Canonical](https://github.com/canonical) helped decide the app name.



## Licence

Glate is free software under the GPL-3.0-or-later. The account and licence code is
source-available under the Ktechpit Licensing Module License. See [LICENSING.md](LICENSING.md)
for which files are covered by which licence. Glate 4.3.0 and earlier were released under MIT.
