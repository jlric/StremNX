# StremNX for Nintendo Switch 2

An unofficial Stremio homebrew client for Nintendo Switch 2. It runs through
OunceHEN and uses libounce.

This Switch 2 port is based on [StremNX by denpingus](https://github.com/denpingus/StremNX)
at commit `151ac135c6a35499a37fb6b535fbd7be7f99a7ff`. The icon and logo
are unchanged from upstream. The port was modified in November 2026. For
Switch 1, use the upstream project.

The Switch 2 homebrew SDK is currently part of the OunceHEN private beta.
This repository contains app source and assets. It does not include the SDK,
graphics driver, or launcher. Building and running the port requires beta access.

<p align="center"><img src="icon.jpg" width="160" alt="StremNX icon"></p>

> [!IMPORTANT]
> StremNX is an independent, community project. It is **not affiliated with, endorsed by, or connected to Stremio**
> or to Nintendo. "Stremio" is a trademark of its owner and is used here only to describe compatibility.
> StremNX does not host, provide or link to any content: everything you see comes from the addons **you** install.

---

## Features

- **Streaming-service style home**: rotating hero banner, horizontal rows, *Continue Watching*, *My List* and
  *Recently Watched*
- **Recommendations** ("Because you watched…", "Top Picks for You") based on the genres you watch
- **Discover** tab with type / catalog / genre filters, plus Movies, Series and Search tabs
- **Stremio account**: log in, use the addons installed on your account, sync your watch progress and library
- **Details page** with title logo, IMDb rating, cast, seasons and episodes with thumbnails and progress
- **Continue where you left off** with the same stream you picked last time (or pick another one)
- **Up next**: the next episode starts automatically, picking the best stream for your preferences
- **Stream list** with quality badges (4K / 1080p / HDR), size, seeders, source, filters and sorting
- **Built-in BitTorrent engine** (trackers, DHT, PEX, uTP fallback) — torrents play directly on the console,
  no PC needed. Optional upload limit.
- **Save to SD card**: download while watching, keep downloading in the background, watch offline, delete when done
- **Player** built on mpv with its own interface: progress bar with buffer, audio / subtitle / playback menu
  (speed, aspect ratio, zoom, volume boost), subtitle timing and size, resumes after sleep
- **Subtitles from addons** (e.g. OpenSubtitles) in your language, with size, position, style, color and delay settings
- **Touch screen** support in handheld mode

## Requirements

- A Nintendo Switch 2 running OunceHEN and its homebrew menu.
- An internet connection. A Stremio account is optional.

## Installing

1. Build `stremnx.nro` with the beta Switch 2 homebrew SDK as described below.
2. Copy it to `sdmc:/switch/stremnx.nro` on your SD card.
3. Open it from the OunceHEN homebrew menu.

Without logging in, only the official Cinemeta catalog is available. Log in under **Settings** to use the addons
installed on your Stremio account, or add an addon by its manifest URL.

## Controls

| Where | Button | Action |
|---|---|---|
| Everywhere | L / R | Switch tabs |
| Everywhere | + | Exit |
| Rows / grids | D-pad, A | Move, open |
| Continue Watching | A / X / Y | Resume with the same stream / pick another stream / details |
| Details | ↑ ↓ | Buttons → seasons → episodes |
| Streams | Y / X / ZL·ZR | Quality filter / source filter / sort order |
| Player | A | Play / pause |
| Player | ← → / ZL·ZR | Seek 10 s / 60 s |
| Player | X | Audio, subtitles and playback options |
| Player | Y | Subtitles on / off |
| Player | L / R | Subtitle delay |
| Player | B | Back |
| Touch | tap / drag | Select and open / scroll |

## Settings worth knowing

- **Torrent uploading** (Off / Limited / Normal): see [About torrents](#about-torrents).
- **Save torrents to SD card**: files go to `sdmc:/switch/stremnx/downloads/`. Files over 4 GB need an exFAT card.
- **Preferred quality** and **Prefer streams from**: used to sort streams and to pick one automatically.
- **Subtitles**: language, size, position, style, color, default delay and the timing step.

## About torrents

BitTorrent works by sharing: while you download a torrent, other people download pieces of it from you.
StremNX uploads at a **limited** rate by default. You can turn uploading **off** (torrents may get slower or stall)
or set it to **normal** in **Settings → Torrent uploading**.

In many countries, downloading or sharing copyrighted material without permission is illegal and can lead to notices
or fines from your internet provider. **You are responsible for the addons you install and for what you watch.**
StremNX ships with no torrent or piracy addons.

## Privacy

- Your Stremio password is sent only to `api.strem.io` over HTTPS when you log in, and is never stored.
- The session key (`authKey`) is stored in plain text in `sdmc:/switch/stremnx/config.json`.
  **Do not share that file.** Logging out removes it.
- Watch history and settings stay on your SD card. When logged in, your progress and library are synced to your
  Stremio account, like the official apps do.
- `sdmc:/switch/stremnx/log.txt` contains technical logs (addon URLs, errors). Check it before sharing it.

## Building

1. Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) with Switch Development,
   then install the beta Switch 2 homebrew SDK into the same `DEVKITPRO` tree
   using the SDK's instructions.
2. Install the required devkitPro portlibs if they are missing:
   ```sh
   dkp-pacman -S switch-dev switch-sdl2 switch-sdl2_ttf switch-sdl2_image switch-curl switch-libmpv
   ```
   The beta SDK must provide `libounce/switch2_rules` and its graphics libraries.
3. From this repository, build:
   ```bash
   make -j2
   ```
   The result is `stremnx.nro`.

Tested on Switch 2: UI, account login, addon browsing, and direct HTTP(S)
playback. Rendering uses OpenGL. On the tested console, video decoding fell back
to the CPU. Torrent playback/downloads and HLS/live streams have not been broadly
tested. Compatibility with other SDK revisions is unverified.

## Project layout

| Path | What it does |
|---|---|
| `source/app.*` | Main loop, screen stack, config, account sync |
| `source/home.cpp` | Home, Discover, Movies, Series, Search, Downloads, Settings |
| `source/detail.cpp` | Details page |
| `source/screens.cpp` | Stream list (scoring, filters), torrent buffering, Up next |
| `source/player.cpp` | mpv player and its on-screen interface |
| `source/torrent.cpp` | BitTorrent engine: trackers, DHT, PEX, uTP, SD card saving |
| `source/stremio.*` | Addon protocol and Stremio account API |
| `source/history.*`, `library.*`, `downloads.*` | Watch history, saved streams, My List, downloads |
| `source/ui.*`, `input.*` | SDL2 drawing, image loading, controller and touch input |

## License

The original StremNX and this port are distributed under the **GNU General Public License v3.0** — see [LICENSE](LICENSE).
It includes or links third-party software listed in [THIRD_PARTY.md](THIRD_PARTY.md).

## Disclaimer

This software is provided "as is", without warranty of any kind. The authors are not responsible for how it is used,
for the content provided by third-party addons, or for any consequences of using it.
