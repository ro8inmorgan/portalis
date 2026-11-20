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


MAIN MENU, RADAR AND APPROVED PROBE TEST
  The animated carousel contains Portal, Wi-Fi radar, Probe test and Settings.
  Use comma/slash (left/right) and Enter. Esc returns from portal setup.

  Radar passively scans nearby 2.4 GHz access points, keeping the strongest 32.
  It shows SSID, RSSI, channel and security. It does not enumerate client devices.
  Stronger signals appear nearer the center. Angle is decorative; RSSI is not
  a reliable distance in metres. Walls and transmit power affect it.
  ;/. selects an AP, Enter switches radar/list, R refreshes, Esc stops scanning.
  The scan refreshes automatically. Scanning and portal modes are exclusive.

  Probe settings share /portalis.json with the AP-name presets. A blank writable
  SD card gets this config automatically when mounted:
    {"ap_names":["Portalis Demo","Portalis Guest","Portalis Workshop"],"cooldown_seconds":120,"notice_template":""}
  Existing files are preserved. For older installs, move cooldown_seconds and
  notice_template from probe-test.json into portalis.json; the old file is no
  longer read. Missing keys default to 120 seconds and no selected template.
  The config is limited to 4 KB. Cooldown range is 60-86400 seconds.
  Without a writable SD card, automatic config creation cannot succeed.

  Probe test opens a setup menu before listening: Page, Start probe test,
  and Submissions. Use ;/. to move and Enter to select.
  Page opens the SD HTML picker. Choose Start
  probe test to begin. Stopping returns to this menu.
  R reloads SD while stopped, S starts/stops, H opens the log, and Esc exits.
  T opens the HTML picker while stopped; V opens saved form submissions.
  While running, Enter reviews the approved scope.
  The selected template is saved separately from the Portal template, persists
  across reboots, and overrides notice_template. There is no built-in probe page;
  select an SD HTML file before starting. Every start reloads the config and chosen
  HTML from SD, so edits take effect. It never starts automatically.
  The listener hops available 2.4 GHz channels and ignores unnamed probes and
  requests outside the list. For an approved request it starts one open test AP
  using that SSID. The AP stays available for at most 120 seconds from the
  attempt start, allowing time to enter feedback, then resumes listening.
  Open http://172.0.0.1 if needed. The configured notice_template is served
  with a one-use submission ticket. The supplied probe-notice.html is clearly
  labeled as a local test and requests a rating and optional feedback. It does
  not request account details or passwords. Forms use the existing /confirm
  submission handler and show a saved confirmation after success.
  Successful submissions are saved to /portalis/submissions.jsonl and can be
  read in Submissions on the device. A save failure displays an error and
  allows retry while the AP remains available. No IP or MAC is stored with feedback.

  A device's probe MAC enters cooldown after an attempt, across all approved
  SSIDs. Cooldowns occupy 32 RAM slots; when full, no new attempt starts until
  a slot expires. No cooldown entry is evicted early. Stopping/restarting the
  test resets its session, counters and cooldowns. There is no persistent MAC log.
  TESTS counts attempts; JOINED counts attempts with any observed association,
  not unique people or devices. H shows the newest 32 results in RAM (SSID,
  elapsed session time, whether a join was seen), also retained after stopping
  until the next test start or reboot. The listener pauses while its AP is active.
  The radio receive queue is bounded; busy-air traffic can be dropped.

  An association does not establish that the probing device joined. MAC
  randomization also limits per-device cooldown accuracy. A device with a saved
  WPA2/WPA3 network will generally not treat an open AP as the same saved network.
  These counters alone are not a verdict on a device's security.
  RF behavior, actual associations and display orientation need device testing.
