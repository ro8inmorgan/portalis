PORTALIS 1.0.0 — Cardputer / Cardputer ADV

A standalone local Wi-Fi connection demo with a readable 240x135 interface.
No internet uplink is required. Nothing starts broadcasting until you choose Start.

INSTALL
  dist/portalis.bin is the full merged image, flash address 0x0.
  dist/Portalis-app.bin is application-only, not an image to flash at 0x0.
  The full image includes a partition table; back up any firmware/settings you
  want to keep before replacing another firmware.

AP NAME PRESETS
  Put portalis.json at the SD card root, for example:
    {"ap_names":["Portalis Demo","Portalis Guest","Portalis Workshop"]}
  Choose AP on the setup screen, then select a preset or "Type a name...".
  Use ; / . to move and Enter to select. Esc returns to setup.
  The list scrolls through up to 32 names. Names follow the same rules as typed
  names: 1-32 printable ASCII characters, without leading/trailing spaces.
  Config size limit: 4 KB. Missing config/card leaves manual naming available;
  invalid config shows a notice and leaves the manual option available.
  The file reloads whenever the AP picker opens; R remounts the SD and reloads.
  The selected or typed name is saved on the device for the next boot.

SD TEMPLATES
  Copy the CONTENTS of sd-card/ to the FAT32 SD card root.
  Put .html or .htm templates anywhere on the card (case-insensitive extensions).
  The picker sorts names alphabetically (case-insensitive), with folders first
  and the built-in template at the top. Sorting applies across all pages.
  Without SD, the built-in Aurora page still works. Old JSON templates must be
  converted to HTML; a saved JSON selection falls back to the built-in page.

  Templates are UTF-8 HTML documents or fragments, up to 32 KB. The filename is
  the device label. Inline CSS and data-URL images work; external assets and
  template JavaScript are blocked. Example: sd-card/aurora.html.

  Use ordinary HTML forms with named controls, for example:
    <form>
      <label>Comments <textarea name="comments"></textarea></label>
      <button type="submit">Save</button>
    </form>
  Portalis automatically handles submission for every form on the page. No
  action, method, hidden ticket, or hand-written JavaScript is required. The
  generated handler posts to /confirm regardless of the form's action/method.
  A browser with JavaScript, fetch, and FormData's submitter argument is required.
  Native HTML validation (required, pattern, min/max, etc.) still applies.

  Named inputs, textareas, selects, multiple selections, and the clicked submit
  button are captured. Duplicate names remain separate entries in their original
  order. Controls associated using form="id" are also included. Normal browser
  rules apply: unchecked checkboxes/radios, disabled controls, unnamed controls,
  and unclicked buttons are omitted. Only the submitted form is recorded.
  File inputs include filename, MIME type, and base64 content. Files may total
  at most 8 KB. The complete JSON request is limited to 16 KB, with at most 64
  entries and 256 bytes per field name. Oversized submissions show an error.
  Each page ticket permits one submission and expires after five minutes;
  reload to submit another form. Data fields named "ticket" are supported.

CONTROLS
  ; / . (the arrow-labelled keys): move through menus or history.
  Enter: choose, open a folder, save the AP name, or confirm a stop.
  Backspace: go to parent folder in the SD picker; delete while editing a name.
  ` (Esc): cancel/back. In the live view it opens the stop confirmation.
  R in SD picker: remount the SD card and return to root.
  H in live view: history; the portal keeps running while viewing it.
  X in live view: stop confirmation; Enter then disconnects clients and turns Wi-Fi off.

USE
  1. Set the AP name (1–32 printable characters, no edge spaces).
  2. Select a page from the SD root/folders, or select the built-in Aurora page.
  3. Choose Start portal. It creates an open 2.4 GHz AP on channel 1 (up to 4 clients).
  4. Join that network with a phone. Open its captive-network prompt or visit
     http://172.0.0.1 manually if the prompt does not appear.
  5. Submit the template form. The dashboard updates its confirmation count.
  6. Use H for timestamps, or X then Enter to stop.

DEVICE SETTINGS
  Scroll to Settings in the main menu, or press C from the live/stopped dashboard.
  Use ; and . to choose a setting. Comma decreases; slash increases
  (the left/right arrow-labelled keys).
  Enter also advances the selected setting. Esc/Backspace saves and returns.
  Accent: Mint, Ocean, Violet, Amber, Rose, or White.
  Background: Navy, Charcoal, or Plum. Colors apply to device screens immediately.
  HTML template colors and the generic confirmation page are independent.
  LED brightness: 0-100% in 10% steps, default 80%; 0 turns it off.
  Highlight the LED row to preview breathing even while the portal is stopped.
  Menu sounds: On/Off. Volume: 0-100% in 10% steps, default 30%; 0 mutes.
  Navigation uses a short click; selections/adjustments use a higher tone.
  Event sounds: On/Off, independent of menu sounds (default On). New clients
  play a rising two-note chime; accepted confirmations play a three-note chime.
  Both use the Volume setting; 0 mutes all sound. Events play on any screen
  while the portal runs. Confirmation chimes take priority over connection chimes.
  Departures play a descending two-note chime. No sounds play for rejected
  submissions or session resets/stopping the portal.
  LED colors: choose Blue, Cyan, Green, Amber, Violet, Red, White, or Mint for
  breathing/idle, connections, confirmations, and disconnects independently.
  Highlight a color row to preview it on the LED, including while stopped.
  LED mode: Breathing, Steady, Blink, Rainbow, or Off. Event flashes override the
  idle animation temporarily. Off disables all LED output, including previews.
  Brightness applies to all modes. Rainbow cycles colors instead of the idle color.
  Settings persist in device flash after exiting Settings; no SD card is needed.
  The header battery icon shows an estimated 0-100% charge, refreshed every two
  seconds. At 20% or lower it turns amber; --% means the reading is unavailable.
  This is a voltage-based estimate, not a precise fuel gauge or charging indicator.

ANIMATIONS
  Startup shows a brief shimmering PORTALIS logo; press a key to skip it.
  Live counters pulse for 700 ms when their values change. Client joins glow
  mint, departures glow amber, and confirmations get a stronger mint pulse.
  While the portal runs, the RGB LED slowly breathes blue. A new client causes
  two cyan flashes; a confirmation causes two green flashes; a departure causes
  two amber flashes. Confirmations take priority over simultaneous client changes.
  Effects use elapsed time without animation delays, including while viewing
  submissions/history. The LED turns off when the portal stops.
  On ADV/v1.1, RGB power shares GPIO38 with the display backlight. The display
  uses full brightness while the portal runs to keep LED power stable, returning
  to its normal brightness when stopped. The same applies during LED preview.
  Brightness is adjustable in Settings; the new default is brighter than before.
  Hardware reference: https://docs.m5stack.com/en/core/Cardputer-Adv

COUNTS AND HISTORY
  CLIENTS is the current number of associated Wi-Fi stations, not page views.
  CONFIRMED counts accepted form submissions in the current session, not
  unique people. Reloading creates a new one-use button; resubmitting the same
  button does not count again. Buttons expire after five minutes.
  A new session resets the live count. History retains the newest 96 events in RAM.
  When SD is present, events append to /portalis/events.csv:
    session,elapsed_seconds
    1,12
    1,28
  Time is elapsed since that session started, not a calendar timestamp. No clock
  synchronization or internet connection is needed. IP/MAC addresses are not logged.
  Entered fields are stored separately from the timestamp history.
  History reloads after reboot from SD. Without SD it lasts until power-off.
  The log stops growing at 256 KB; archive/remove this app's CSV manually to resume.
  History shows RAM when no card is mounted, or ERR if a write failed/log is full.

FORM DATA
  Submissions append to /portalis/submissions.jsonl on the SD card, one JSON
  object per line, with session, elapsed_seconds, and a fields array:
    {"session":1,"elapsed_seconds":12,"fields":[{"name":"comments","value":"Hello"}]}
  File entries additionally have filename, type, and encoding="base64"; value
  contains the base64 bytes. Text values are preserved, including line breaks
  and Unicode. JSON escaping prevents line breaks from splitting log records.
  A successful write is required before accepting a submission with fields.
  Without SD, only empty-field confirmations can be accepted into RAM history.
  The submission log has its own 256 KB cap. Archive/remove it to resume when
  full. Write failures stay on the form with an error and permit retrying.
  On the device, choose Setup > Submissions, or press S in the live view.
  Use ; / . to select a submission (newest first), then Enter to open it.
  Use ; / . to scroll through all field names and values; Esc returns to the list.
  Press R in the list to refresh after new submissions arrive. The portal keeps
  running while browsing. Existing saved submissions are available after reboot.
  Long values wrap; repeated fields remain separate. File uploads show their
  filename/type; read the SD log for file contents. The device font displays
  unsupported Unicode glyphs as ?; original values on SD remain unchanged.
  Missing SD shows a notice. Malformed/incomplete log records are skipped.
  H still opens the timestamp history from setup or the live view.

BUILD AND TEST
  uvx --with requests --with intelhex --from platformio pio run -e cardputer
  uvx --with requests --from platformio pio test -e native
  The build packages merged/application binaries and SD examples into dist/ and
  writes a SHA256 manifest. Hardware/library versions are pinned in platformio.ini.
  Hardware API references: M5Stack M5Cardputer, M5Unified/M5GFX, Espressif Arduino
  ESP32 2.0.17, and ArduinoJson 7.4.2.

VALIDATION
  Firmware compilation, native model/storage tests, screen-bound checks, and
  browser rendering of the generated phone page were checked locally.
  USB flashing, on-device display, Wi-Fi association, DNS redirection and SD hardware
  operation still need verification on the physical Cardputer.


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

CUSTOM PROBE NOTICE
  Set "notice_template": "/probe-notice.html" in /portalis.json on SD.
  Change that path to choose another .html/.htm file (maximum 32 KB).
  Restart the probe test to reload the file. Missing/invalid files prevent start;
  an empty/missing notice_template requires selecting a page in the picker.
  The template is served with the portal's nonce-bound form submission script.
  Inline CSS, forms and data-URL images work; template scripts and external assets do not.
  Probe mode posts the submitted form to /confirm with the same limits as Portal:
  64 named entries, 16 KB per submission, and at most 8 KB of file data.
  Text, select/rating, radio and checkbox controls work without custom JavaScript.
  Repeated names are preserved; unchecked checkboxes and disabled/unnamed inputs
  are not sent. Each page ticket allows one successful submission. To submit
  another form, reopen the page while the test AP is still active.
  Template scripts and external resources remain blocked.
