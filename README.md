# mcsm-3ds <img src="icon.png" alt="MCSM 3DS Icon" width="32"/> 


An unofficial port of **Minecraft: Story Mode** (Netflix Edition) for the Nintendo 3DS and 2DS family of systems.


Inspired by [mcsm_portable](https://github.com/entitybtw/mcsm_portable) (PSP port).

<p align="center">
  <img src="https://github.com/user-attachments/assets/69cca2c7-c9e7-40cd-b5b3-2dd8c9791de9" width="48%" alt="image 1" />
  <img src="https://github.com/user-attachments/assets/ac875009-dc15-482f-9e66-efa058b8bcf7" width="48%" alt="image 2" />
</p>

---


## 🗺️ Roadmap

### v1.0.0-ep1 (Released)
- [x] Fully playable Episode 1 with all branching storylines and choices
- [x] Title screen, Main Menu, and in-game Pause menu (`START`)
- [x] Hybrid 3-in-1 controls (ABXY, D-Pad, and Touchscreen)
- [x] Custom *Minecraft Seven* font rendering (`.bcfnt`)
- [x] Hardware-accelerated video playback (Theora/Y2R) & zero-CPU audio thread (Tremor)
- [x] Persistent settings & autosave system

### v1.0.x-ep1 (Fixes & QoL)
- [ ] English set as default interface language
- [ ] System sleep mode / lid close handling via `aptHook`
- [ ] UI sound effects (Minecraft button clicks and navigation SFX)
- [ ] Pop-up notification overlay (*"Jesse will remember that"*)
- [ ] Seamless black fade transitions between video scenes

### v1.1.x-ep1 (Subtitles & Enhanced UI)
- [ ] Subtitles engine (`.srt`) with character-specific text colors
- [ ] Action QTE sequences ("Mash A" building moments)
- [ ] Multiple save slots (Wii U / Switch style slot selector)
- [ ] Dual-screen HUD elements (Dynamic Order Amulet & active item display)
- [ ] Fast-forward option (`R` trigger) for previously watched scenes
- [ ] Stereoscopic 3D depth effect for UI layers and subtitles

### v2.0.0+ (The Complete Season)
- [ ] Modular Episode Manager
- [ ] Cross-episode decision flag persistence
- [ ] Video pipeline & script manifests for Episodes 2 through 5
- [ ] Support for the Female Jesse story branch (`epX_fem`)
- [ ] Modular localization system (partial UI, subtitles, and audio selection across languages)
- [ ] C-Stick / Circle Pad Pro navigation support


---


## Controls

| Button | Action |
| :--- | :--- |
| **Y / X / A / B** | Direct Story Choices |
| **D-Pad Up / Down** | Navigate Menu / Choice Highlight |
| **A** | Confirm Selection |
| **B** | Back / Resume from Pause |
| **Touchscreen / Stylus** | Direct tap on buttons & choices |
| **START** | In-Game Pause Menu / Title Screen Proceed |
| **L + R + X** | Toggle Secret Debug Menu (Skip scenes enabled) |


---


## Installation


1. Download the latest release from the [Releases](https://github.com/m1-q/mcsm-3ds/releases) page.
2. Copy `mcsm_3ds.3dsx` to `sdmc:/3ds/mcsm/` on your SD card.
3. Copy the `assets/` folder to the root of your SD card (`sdmc:/assets/`).
4. Launch the game via the **Homebrew Launcher** on your 3DS / 2DS.


## Credits

- **Mojang & Telltale Games** - Original Minecraft: Story Mode.

- **pox1016** - Netflix Interactive to YouTube adaptation
  
- **entitybtw** - Inspiration and research from the PSP port (mcsm_portable).

- **oreo639** - Base video playback research (3ds-theoraplayer).

- **devkitPro / libctru team** - Nintendo 3DS toolchain & libraries.
