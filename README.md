# Portalis

A standalone Wi-Fi captive portal for **M5Stack Cardputer and Cardputer ADV**, with HTML templates, on-device form submissions, and customizable lights and sounds.

## Install

Download [release v1.0.0](https://github.com/ro8inmorgan/portalis/releases/tag/v1.0.0).

- **portalis.bin**: full merged firmware; flash at **0x0**. Back up existing firmware/settings first.
- **Portalis-package.zip**: firmware, manifest, instructions, and SD-card examples.
- **Portalis-SD-examples.zip**: extract its contents to the root of a FAT32 SD card.
- **Portalis-app.bin**: application only; do not flash this at 0x0.

Select an AP name and template, then choose **Start portal**. Join the network and open the captive portal notification or visit **http://172.0.0.1**. The AP is local and does not provide internet access.

## Features

- HTML/HTM templates from SD, with an alphabetical folder browser and built-in Aurora page.
- Automatic submission of named HTML form controls, including repeated fields and small file uploads.
- SD submission storage and an on-device viewer, client and confirmation counters, and event history.
- AP-name presets in `portalis.json`, plus manual naming.
- UI colors, LED colors and brightness, five LED modes, menu/event sounds, and volume controls.
- Animated startup logo and counters, connection/confirmation/disconnection feedback, and battery percentage estimate.

## Templates and submissions

```html
<form>
  <input name="comment" type="text" placeholder="Comment" required>
  <button type="submit">Save</button>
</form>
```

No form action or custom JavaScript is needed. Portalis injects a handler that posts to `/confirm`. Submissions are stored on SD at `/portalis/submissions.jsonl`; choose **Submissions** on the device to read them. Forms with fields require a writable SD card. Template JavaScript and external assets are blocked; inline CSS and data-URL images are supported.

Templates are limited to 32 KB; submissions to 16 KB and 64 entries, including at most 8 KB of uploaded files. Each page ticket allows one submission within five minutes. The submission log is capped at 256 KB; archive or remove it when full. The network is open and submission logs are unencrypted; use forms appropriate for that environment.

See [the full manual](README.txt) for controls, settings, template behavior, limits, and hardware notes.

## Build and test

From the project directory, with [uv](https://docs.astral.sh/uv/) installed:

```sh
uvx --with requests --with intelhex --from platformio pio run -e cardputer
uvx --with requests --from platformio pio test -e native
```

Dependencies are pinned in `platformio.ini`. Builds generate firmware, SD examples, ZIP packages, and a SHA-256 manifest in `dist/`. Generated files and downloaded dependencies are excluded from the source repository.

Native tests and firmware compilation do not replace testing on the physical device; consult the manual for hardware validation limitations.
