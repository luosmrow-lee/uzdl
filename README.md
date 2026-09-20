# uZDL

A launcher for ZDoom based Doom source ports: pick a port and an IWAD, add PWADs, keep the combinations you play as presets, and browse the idgames archive without leaving the launcher. A fork of [Lcferrum's ZDL](https://github.com/lcferrum/qzdl) - which targets Qt 4.8 and no longer builds on current toolchains - ported to Qt 6 and extended.

## Features

- **Presets** - named sets of PWADs, patches and configs, each optionally with its own source port and IWAD, and its own save folder and settings file so a mod's saves never mix with the vanilla game's. Load, append or launch one in a click; edit it beside the PWAD library; import and export as `.zdl`; make a desktop shortcut that starts straight into it, which also serves as a Steam entry.
- **PWAD library** - a folder of your choosing, scanned recursively and watched for changes, with a filter and Folders / Newest / Name views. Select files or a whole folder and add them to the launch.
- **idgames browser** - the whole /idgames archive, searchable offline from a mirror's own listing, with each entry's description. Installs straight into the PWAD folder, marks what is already installed, and opens the entry's Doomworld page for reviews and screenshots.
- **Per-port parameters** - arguments a source port always gets, plus a *Run* button that starts a port on its own, which is how ports with a built-in updater get to update.
- **Portable** - opt in with an empty `uzdl_portable.ini`; anything kept inside the uZDL folder is stored relative to it, so the folder moves or copies as one.
- **Updates itself** - checks the GitHub releases on demand or once a day, shows the release notes, and on Windows, and on Linux when run as the AppImage, downloads the new version, verifies it against the checksum GitHub publishes, installs it over the old one and restarts. Your configuration and files are never touched; *Skip this version* silences a release you do not want.
- Themes (System, Light, Dark), multiplayer setup, demo playback and file associations, as in ZDL; on Linux the `.zdl` type is registered with the desktop.

## Download and install

- Releases: https://github.com/luosmrow-lee/uzdl/releases - a zip for Windows and an AppImage for Linux, each with everything needed; there is nothing to install.
- Windows: unpack the zip anywhere and run `uzdl.exe`.
- Linux: make the AppImage executable (`chmod +x uZDL-*.AppImage`) and run it. It carries its own Qt and needs a distribution with glibc 2.39 or newer, which means Ubuntu 24.04, Debian 13, Fedora 40 or anything more recent.
- Building from source: see [COMPILE](COMPILE).
- uZDL is only a launcher: you need a source port and the game's IWAD separately.

## Portable mode

- Create an empty file named `uzdl_portable.ini` beside `uzdl.exe`, or beside the AppImage on Linux, and it becomes the only configuration uZDL reads or writes. Without it the configuration is per-user, as in ZDL.
- Neither release package contains the file, so unpacking a newer version over an old folder, or swapping in a newer AppImage, never touches your configuration.
- A port, an IWAD, the PWAD folder, and the files in the launch list and in presets are stored relative to the uZDL folder whenever they sit inside it. For ports and IWADs the add and edit dialogs tick *Relative to the uZDL folder* by default and can keep the absolute path instead; `sourceports` and `iwads` folders are created for the purpose.
- The folder has to be somewhere writable, since the configuration lives beside the executable. A folder under Program Files will not work without elevation.
- File associations are the one feature that reaches outside the folder, into the Windows registry, and only when you press the button.
- To carry a per-user configuration into a portable one, copy it over `uzdl_portable.ini`: `%APPDATA%\Vectec Software\qZDL.ini` on Windows, `~/.config/Vectec Software/qZDL.ini` on Linux.

## Using it

Three tabs:

- **General settings** - the source ports you have and the IWADs you own, added by drag and drop or the buttons, each port with its own parameters. Also the parameters always added to every launch, the theme, the update check and file associations.
- **Launch config** - from left to right the PWAD library, the presets, the external files for this launch (in load order; an entry can be disabled to keep it in the list but off the command line), and the source port and IWAD with map, skill, compatibility mode, fast and respawning monsters, and multiplayer options. *Launch* starts the game; the *uZDL* menu shows the command line about to run.
- **idgames** - *Update index* once to fetch the listing, then narrow it by type, age and name. *Download and install* unpacks the selection into the PWAD folder, where the library picks it up.

Quickstart:

1. On General settings, add your source ports and IWADs.
2. On Launch config, choose a port and an IWAD and add files from the PWAD library.
3. Press *Launch*.
4. When a combination is worth keeping, press *Save as preset*. Next time, select the preset and press its *Launch*.

The port and IWAD lists, the launch configuration, pane widths and window size are all remembered between sessions.

A preset can be started from outside uZDL too: `uzdl --preset "Name"` loads it and launches the game, which is what the *Shortcut* button on the presets pane writes into a desktop shortcut. Add that shortcut to Steam as a non-Steam game and the preset appears there like any other title.
