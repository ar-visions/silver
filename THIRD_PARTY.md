# Third-party dependencies and credits

Everything silver imports from outside this repository, with where it comes from,
who made it and the license it is used under. The `import` lines in each module
name the exact commit or version; this file is the human ledger of the same set.
Keep it in step with the imports.

## Libraries (git imports, built from source at the pinned commit)

| dependency | pinned | used by | source | license | authors |
|---|---|---|---|---|---|
| Node.js | 24.18.0 | featured/vscode, quark, gemini (runs its CLI) | https://github.com/nodejs/node | MIT (bundled parts under their own licenses) | OpenJS Foundation and contributors |
| Gemini CLI (its bundle) | 0.64.0 | gemini (install/bin/gemini, on our Node) | https://github.com/google-gemini/gemini-cli | Apache-2.0 | Google LLC |
| Codex CLI (release package: codex, rg, bwrap) | 0.163.0 | codex (install/bin/codex) | https://github.com/openai/codex | Apache-2.0 (rg: MIT or Unlicense; bwrap: LGPL-2.0-or-later) | OpenAI |
| GIMP (its official AppImage, unpacked; not redistributed) | 3.2.6 | featured/gimp (install/bin/gimp) | https://download.gimp.org/gimp/v3.2/linux | GPL-3.0-or-later (its bundled libraries under their own licenses) | The GIMP Team |
| Firefox (Mozilla's release build, not redistributed) | 158.0 | featured/firefox (install/bin/firefox) | https://archive.mozilla.org/pub/firefox/releases | MPL-2.0 (the Firefox name and logo: Mozilla's trademarks) | Mozilla Foundation and contributors |
| Claude Code (native binary, not redistributed) | 2.1.304 | claude (install/bin/claude) | https://downloads.claude.ai/claude-code-releases | proprietary, Anthropic's terms | Anthropic PBC |
| Wine (Valve's tree, Proton Experimental 11.0-20261001's commit) | 6d211aab | featured/wine (install/bin/wine, Wayland driver) | https://github.com/ValveSoftware/wine | LGPL-2.1-or-later | the Wine project, Valve and CodeWeavers |
| Proton's lsteamclient and steam.exe (Steam's Windows client bridged to the Linux Steam) | experimental-11.0-20261001 (70b7e109) | featured/wine (install/lib/wine) | https://github.com/ValveSoftware/Proton | BSD 3-Clause; lsteamclient carries Valve's Steamworks SDK headers under the Steamworks SDK license | Valve Corporation |
| DXVK (its release build: Windows DLLs, not built here) | 3.1.1 | featured/wine (install/lib/wine/dxvk) | https://github.com/doitsujin/dxvk | zlib | Philip Rebohle and contributors |
| libxkbcommon (for xkbregistry) | 1.6.0 | featured/wine | https://xkbcommon.org | MIT | Daniel Stone, Ran Benita and contributors |
| NASM (build tool) | 2.16.03 | featured/wine (FFmpeg's assembly) | https://www.nasm.us | BSD-2-Clause | the NASM development team |
| FFmpeg (shared, LGPL build: no --enable-gpl) | 4.4.5 | featured/wine (winedmo, gst-libav) | https://ffmpeg.org | LGPL-2.1-or-later | the FFmpeg developers |
| GStreamer | 1.24.12 | featured/wine (winegstreamer) | https://gstreamer.freedesktop.org | LGPL-2.1-or-later | the GStreamer team |
| gst-plugins-base, -good (isomp4), -bad (videoparsers), gst-libav | 1.24.12 | featured/wine (winegstreamer) | https://gstreamer.freedesktop.org | LGPL-2.1-or-later | the GStreamer team |
| libxkbfile | 1.1.3 | featured/vscode | https://gitlab.freedesktop.org/xorg/lib/libxkbfile | MIT | X.Org Foundation |
| libsecret | 0.21.7 | featured/vscode | https://gitlab.gnome.org/GNOME/libsecret | LGPL-2.1-or-later | The libsecret authors |
| MIT Kerberos | 1.21.3 | featured/vscode | https://github.com/krb5/krb5 | MIT (krb5 notice) | Massachusetts Institute of Technology |
| Code - OSS | 1.142.0 | featured/vscode | https://github.com/microsoft/vscode | MIT | Microsoft Corporation |
| wxWidgets | 3.2.8 | featured/audacity | https://github.com/wxWidgets/wxWidgets | wxWindows Library Licence 3.1 | Julian Smart, Vadim Zeitlin and contributors |
| LAME | 3.100 | featured/audacity | https://lame.sourceforge.io | LGPL-2.0-or-later | The LAME project |
| libogg | 1.3.5 | featured/audacity | https://xiph.org/ogg | BSD-3-Clause | Xiph.Org Foundation |
| libvorbis | 1.3.7 | featured/audacity | https://xiph.org/vorbis | BSD-3-Clause | Xiph.Org Foundation |
| FLAC | 1.4.3 | featured/audacity | https://xiph.org/flac | BSD-3-Clause (libFLAC) | Josh Coalson, Xiph.Org Foundation |
| Opus | 1.5.2 | featured/audacity | https://opus-codec.org | BSD-3-Clause | Xiph.Org Foundation, Skype, Octasic and others |
| opusfile | 0.12 | featured/audacity | https://opus-codec.org | BSD-3-Clause | Xiph.Org Foundation |
| libsndfile | 1.2.2 | featured/audacity | https://github.com/libsndfile/libsndfile | LGPL-2.1-or-later | Erik de Castro Lopo and contributors |
| libid3tag | 0.16.3 | featured/audacity | https://codeberg.org/tenacityteam/libid3tag | GPL-2.0-or-later | Underbit Technologies, Tenacity team |
| WavPack | 5.7.0 | featured/audacity | https://www.wavpack.com | BSD-3-Clause | David Bryant |
| PortMidi | 2.0.4 | featured/audacity | https://github.com/PortMidi/portmidi | MIT | Roger B. Dannenberg and contributors |
| RapidJSON | 1.1.0 | featured/audacity | https://github.com/Tencent/rapidjson | MIT | THL A29 Limited (Tencent), Milo Yip |
| Audacity | 3.7.9 | featured/audacity | https://github.com/audacity/audacity | GPL-3.0 (GPL-2.0-or-later parts) | Audacity Team, Muse Group |
| libgedit-amtk | 5.11.0 | featured/gedit | https://gitlab.gnome.org/World/gedit/libgedit-amtk | LGPL-3.0-or-later | The gedit team |
| libgedit-gfls | 0.5.0 | featured/gedit | https://gitlab.gnome.org/World/gedit/libgedit-gfls | LGPL-3.0-or-later | The gedit team |
| libgedit-gtksourceview | 299.8.0 | featured/gedit | https://gitlab.gnome.org/World/gedit/libgedit-gtksourceview | LGPL-2.1-or-later | The gedit team, GtkSourceView authors |
| libgedit-tepl | 6.15.0 | featured/gedit | https://gitlab.gnome.org/World/gedit/libgedit-tepl | LGPL-3.0-or-later | The gedit team |
| libhandy | 1.8.3 | featured/gedit | https://gitlab.gnome.org/GNOME/libhandy | LGPL-2.1-or-later | Purism, GNOME |
| libpeas | libpeas-1.36.0 | featured/gedit | https://gitlab.gnome.org/GNOME/libpeas | LGPL-2.1-or-later | GNOME |
| gspell | 1.14.5 | featured/gedit | https://gitlab.gnome.org/GNOME/gspell | LGPL-2.1-or-later | GNOME |
| gedit | 51.0 | featured/gedit | https://gitlab.gnome.org/GNOME/gedit | GPL-2.0-or-later | The gedit team |
| FreeType | VER-2-13-3 | trinity | https://github.com/freetype/freetype | FreeType License (FTL); GPL-2.0 alternative | David Turner, Robert Wilhelm, Werner Lemberg and the FreeType project |
| FriBidi | v1.0.16 | features | https://github.com/fribidi/fribidi | LGPL-2.1-or-later | Behdad Esfahbod, Dov Grobgeld, Roozbeh Pournader |
| FAAC | 3aa4c6d | trinity (recording's AAC track, shared library) | https://github.com/knik0/faac | LGPL-2.1-or-later | M. Bakker, Krzysztof Nikiel and the FAAC contributors |
| libpng | 3061454d980de7d53608f594194cfac722721d2a | img | https://github.com/glennrp/libpng | PNG Reference Library License v2 | Glenn Randers-Pehrson, Cosmin Truta and contributors |
| libtiff | v4.7.2 | img | https://gitlab.com/libtiff/libtiff | libtiff License (BSD-style) | Sam Leffler, Silicon Graphics, Inc. and contributors |
| zlib | 51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf | img, features | https://github.com/madler/zlib | zlib License | Jean-loup Gailly, Mark Adler |
| Vulkan-Headers | 29184b98984f6169a5e83e97557a77cff1e5b0ca | trinity, wintest | https://github.com/KhronosGroup/Vulkan-Headers | Apache-2.0 OR MIT | The Khronos Group |
| Vulkan-Utility-Libraries | vulkan-sdk-1.4.341.0 | trinity | https://github.com/KhronosGroup/Vulkan-Utility-Libraries | Apache-2.0 | The Khronos Group, LunarG |
| Vulkan-Tools | 734638e | trinity | https://github.com/KhronosGroup/Vulkan-Tools | Apache-2.0 | The Khronos Group, LunarG |
| Vulkan-ValidationLayers | vulkan-sdk-1.4.341.0 | trinity | https://github.com/KhronosGroup/Vulkan-ValidationLayers | Apache-2.0 | The Khronos Group, LunarG |
| SPIRV-Headers | vulkan-sdk-1.4.341.0 | trinity | https://github.com/KhronosGroup/SPIRV-Headers | MIT | The Khronos Group |
| SPIRV-Tools | vulkan-sdk-1.4.341.0 | trinity | https://github.com/KhronosGroup/SPIRV-Tools | Apache-2.0 | The Khronos Group, Google |
| glslang | 715c8500e7cd67f2eba9e60e98852a1ed49d2f15 | trinity | https://github.com/KhronosGroup/glslang | BSD-3-Clause with Apache-2.0 and MIT parts (see its LICENSE.txt) | The Khronos Group, Google, LunarG, John Kessenich |
| MoltenVK | db445ff | trinity (macOS) | https://github.com/KhronosGroup/MoltenVK | Apache-2.0 | The Brenwill Workshop, The Khronos Group |
| OpenSubdiv | v3_7_0 | trinity | https://github.com/PixarAnimationStudios/OpenSubdiv | Apache-2.0 (Pixar modified) | Pixar Animation Studios |
| RNNoise | 0.2 | trinity (recording mic) | https://github.com/xiph/rnnoise | BSD-3-Clause | Xiph.Org Foundation, Mozilla, Jean-Marc Valin |
| sherpa-onnx | v1.13.4 | speech | https://github.com/k2-fsa/sherpa-onnx | Apache-2.0 | Next-gen Kaldi (k2-fsa) |
| FAAD2 | 2.11.2 | spectra, speech | https://github.com/knik0/faad2 | GPL-2.0-or-later (commercial license available) | Nero AG, Fabian Greffrath, Krzysztof Nikiel and contributors |
| Mbed TLS | ec4044008d2d069da38288bc76b0fee34ec78646 | tls, aura (https) | https://github.com/Mbed-TLS/mbedtls | Apache-2.0 OR GPL-2.0-or-later | Arm Limited and contributors |
| libyaml | 0.2.5 | aura (Ruby's build) | https://github.com/yaml/libyaml | MIT | Kirill Simonov, Ingy döt Net and contributors |
| QEMU | e47e2d0 | qemu | https://github.com/qemu/qemu | GPL-2.0-only (mixed, see its LICENSE) | Fabrice Bellard and the QEMU project |
| virglrenderer | main | qemu | https://gitlab.freedesktop.org/virgl/virglrenderer | MIT | Dave Airlie, Collabora and contributors |
| Mesa | 26.2.2 | os-bootstrap (Vulkan: venus, radv for AMD, anv for Intel; patched: os-bootstrap/mesa.diff) | https://gitlab.freedesktop.org/mesa/mesa | MIT (with other permissive parts) | The Mesa 3D project |
| libdrm | 2.4.134 | os-bootstrap (radv and anv need 2.4.133+) | https://dri.freedesktop.org/libdrm/libdrm-2.4.134.tar.xz | MIT | The DRI project |
| libusb | 1.0.27 | fastboot (a phone's bootloader over USB) | https://github.com/libusb/libusb/releases/download/v1.0.27/libusb-1.0.27.tar.bz2 | LGPL-2.1-or-later | the libusb project (Peter Stuge, Nathan Hjelm and contributors) |
| milos-mainline Linux | v7.2.0-milos | os-bootstrap (the Fairphone 6's kernel, built) | https://github.com/milos-mainline/linux | GPL-2.0-only | Linus Torvalds and the kernel's contributors; Milos (SM7635) work by Luca Weiss and contributors |
| postmarketOS pmaports (Fairphone 6 kernel config, deviceinfo, modules-initfs, kernel-cmdline.conf; read at build time) | main | os-bootstrap (how the Fairphone 6 kernel is configured and booted) | https://gitlab.postmarketos.org/postmarketOS/pmaports | MIT (packaging); the kernel config is part of the GPL-2.0-only kernel | the postmarketOS contributors (Luca Weiss, maintainer) |
| FP6-firmware (Fairphone 6 firmware blobs) | cb1e77c | os-bootstrap (placed in the phone's boot image: GPU, audio, compute DSP, IPA, modem, video, Wi-Fi, Bluetooth) | https://github.com/FairBlobs/FP6-firmware | proprietary (Qualcomm / Fairphone; redistributed as postmarketOS does) | Qualcomm Technologies, Fairphone |
| pil-squasher | 3c9f8b8 | os-bootstrap (build tool: joins split .mdt/.bNN firmware into .mbn) | https://github.com/linux-msm/pil-squasher | BSD-3-Clause | Linaro, Bjorn Andersson and contributors |
| linux-firmware ath11k WCN6750 board-2.bin | main | os-bootstrap (the Fairphone 6's Wi-Fi board file) | https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git | Qualcomm redistributable (linux-firmware's LICENSE.QualcommAtheros_ath10k) | Qualcomm Atheros |
| mkbootimg | main | os-bootstrap (build tool: the phone's Android boot image) | https://android.googlesource.com/platform/system/tools/mkbootimg | Apache-2.0 | The Android Open Source Project |
| BusyBox (Alpine busybox-static, aarch64) | 1.37.0-r20 | os-bootstrap (pid 1 and the shell on the Fairphone 6's root until orbiter runs on arm64) | https://dl-cdn.alpinelinux.org/alpine/v3.22/main/aarch64/busybox-static-1.37.0-r20.apk | GPL-2.0-only | Erik Andersen, Rob Landley, Denys Vlasenko and the BusyBox contributors; packaged by Alpine Linux |
| iwd (with its ell) | 3.12 | wifi; runs on orbiter-os, started by init | https://mirrors.edge.kernel.org/pub/linux/network/wireless/iwd-3.12.tar.xz | LGPL-2.1-or-later | Intel Corporation and contributors |
| SPIRV-LLVM-Translator | 8e82970 | os-bootstrap (build tool: anv's built-in GPU programs) | https://github.com/KhronosGroup/SPIRV-LLVM-Translator | Apache-2.0 WITH LLVM-exception | The Khronos Group |
| libclc (from LLVM bd7db75) | bd7db75 | os-bootstrap (build tool: anv's built-in GPU programs) | https://github.com/llvm/llvm-project/tree/main/libclc | Apache-2.0 WITH LLVM-exception | The LLVM project |
| libuev | v2.4.1 | features | https://github.com/troglobit/libuev | MIT | Flemming Madsen, Joachim Wiberg |
| PCSX-Redux (OpenBIOS) | 20b63161065d9198bbb6a2543cd4163548e8da6d | psx (the BIOS, built from source) | https://github.com/grumpycoders/pcsx-redux | MIT | Nicolas "Pixel" Noble and the PCSX-Redux authors |

## Code ported by hand

| source | used by | what | license | authors |
|---|---|---|---|---|
| ares (ps1/spu) | psx (Spu.ag) | the SPU: ADPCM, ADSR, Gaussian table, noise, reverb, transfers | ISC | Near and the ares team |

## Python packages (pip into install/pylib, for the reference tests)

| package | used by | source | license | authors |
|---|---|---|---|---|
| librosa | composer (rvc mel references) | https://pypi.org/project/librosa/ | ISC | Brian McFee and the librosa contributors |
| SciPy | composer (rvc high-pass references) | https://pypi.org/project/scipy/ | BSD-3-Clause | The SciPy developers |

## System libraries (linked from the OS, not fetched)

| dependency | used by | source | license | authors |
|---|---|---|---|---|
| Barlow Condensed (Regular, Bold, SemiBold, Thin, MediumItalic) | trinity/fonts (BarlowCondensed-*.ttf) | https://github.com/jpt/barlow | OFL-1.1 | Jeremy Tribby |
| Fontconfig | trinity (FontMatch, linux) | https://gitlab.freedesktop.org/fontconfig/fontconfig | MIT-style (HPND, see its COPYING) | Keith Packard and the Fontconfig contributors |
| D-Bus (libdbus-1, dbus-daemon) | wifi (talks to iwd); tray (apps' tray icons, window menus); orbiter-os's system and session buses | https://gitlab.freedesktop.org/dbus/dbus | AFL-2.1 OR GPL-2.0-or-later | Red Hat, Havoc Pennington and the D-Bus contributors |
| wlroots 0.17 | wayland (the compositor orbiter shows windows from) | https://gitlab.freedesktop.org/wlroots/wlroots | MIT | the wlroots contributors |

## Files fetched by url

| file | used by | source | license | authors |
|---|---|---|---|---|
| Linux kernel source (6.18, 6.12: the build page's choices) | os-bootstrap (fetched by its build page) | https://cdn.kernel.org/pub/linux/kernel/v6.x/ | GPL-2.0-only WITH Linux-syscall-note | Linus Torvalds and the kernel community |
| Kernel configuration (os-bootstrap/linux.config, begun from Ubuntu's 6.17 config) | os-bootstrap | /boot/config-6.17.0-40-generic (Ubuntu 24.04) | GPL-2.0-only (part of the kernel build) | Canonical and the Ubuntu kernel team |
| NVIDIA open GPU kernel modules 610.57.04 | os-bootstrap (fetched by its build page) | https://github.com/NVIDIA/open-gpu-kernel-modules | MIT OR GPL-2.0-only | NVIDIA Corporation |
| Linux firmware (the build host's /lib/firmware, copied into the drive tree) | os-bootstrap | https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git | per file (see its WHENCE) | the hardware vendors, gathered by the linux-firmware project |
| mtools 4.0.48 (build tool) | os-bootstrap (the installer stick's EFI partition) | https://ftp.gnu.org/gnu/mtools/mtools-4.0.48.tar.gz | GPL-3.0-or-later | Alain Knaff, David Niemi and contributors |
| EfiFs NTFS driver (ntfs_x64.efi) v1.13 | os-bootstrap (the boot program reads the kernel from an orbiter-os folder on a Windows partition) | https://github.com/pbatard/EfiFs/releases/download/v1.13/ntfs_x64.efi (sha256 202aeb2cc078693e9c03f3c0a64e8c6c44f2c75eded8a32ada5e1589753b3317) | GPL-3.0-or-later (built on GRUB's file system code) | Pete Batard and the GRUB contributors |
| gtk-shell.xml (GTK 4.14.5's Wayland protocol; C made from it by wayland-scanner) | wayland (a GTK app's menu bar address) | https://gitlab.gnome.org/GNOME/gtk/-/raw/4.14.5/gdk/wayland/protocol/gtk-shell.xml | LGPL-2.1-or-later | The GTK Team |
| appmenu.xml (KDE plasma-wayland-protocols 1.13.0; C made from it by wayland-scanner) | wayland (a Qt app's menu address) | https://invent.kde.org/libraries/plasma-wayland-protocols/-/raw/v1.13.0/src/protocols/appmenu.xml | LGPL-2.1-or-later | David Edmundson |
| Material Symbols: Wi-Fi bars, lock, off, power, restart (white fill added) | trinity/images (wifi_*, power, restart) | https://github.com/google/material-design-icons | Apache-2.0 | Google |
| Material Symbols: mouse, keyboard, desktop_windows as display (white fill added) | trinity/images (mouse, keyboard, display) | https://github.com/google/material-design-icons | Apache-2.0 | Google |
| Material Symbols: apps, list, grid_view (white fill added) | orbiter/images (apps_grid, view_list, grid_view) | https://github.com/google/material-design-icons | Apache-2.0 | Google |
| Material Symbols: clear_all (white fill added) | orbiter/images/clear_all.svg | https://github.com/google/material-design-icons | Apache-2.0 | Google |
| Material Symbols: delete_sweep (white fill added) | orbiter/images/delete_sweep.svg | https://github.com/google/material-design-icons | Apache-2.0 | Google |
| Material Symbols: find_replace (white fill added) | orbiter/images/find_replace.svg | https://github.com/google/material-design-icons | Apache-2.0 | Google |
| Material Symbols: edit (white fill added) | orbiter/images/edit.svg | https://github.com/google/material-design-icons | Apache-2.0 | Google |
| Material Symbols: eject (white fill added) | orbiter/images/eject.svg | https://github.com/google/material-design-icons | Apache-2.0 | Google |
| Material Symbols: person (white fill added) | orbiter/images/person.svg | https://github.com/google/material-design-icons | Apache-2.0 | Google |
| Material Symbols: manage_search (white fill added) | orbiter/images/find_in_files.svg | https://github.com/google/material-design-icons | Apache-2.0 | Google |
| Material Symbols: volume_up, volume_off (white fill added) | trinity/images/volume_up.svg, volume_off.svg | https://github.com/google/material-design-icons | Apache-2.0 | Google |
| gedit app icon | featured/gedit/images/icon.png | https://gitlab.gnome.org/GNOME/gedit/-/raw/51.0/data/icons/org.gnome.gedit-256x256.png | GPL-2.0-or-later (repo) | The gedit team |
| RVC v2 base generator and discriminator, 40 kHz with pitch (f0G40k.pth, f0D40k.pth) | composer | https://huggingface.co/lj1995/VoiceConversionWebUI | MIT | RVC-Project |
| RMVPE pitch model (rmvpe.pt) | composer | https://huggingface.co/lj1995/VoiceConversionWebUI | MIT | RVC-Project |
| Blender 4.5.14 LTS (Linux x64 release; run as a program, not linked) | aeon (blend.py reads .blend files into a project folder) | https://download.blender.org/release/Blender4.5/ | GPL-3.0-or-later | the Blender Foundation and the Blender community |
| HuBERT base content encoder (hubert_base/pytorch_model.bin + config.json) | composer | https://huggingface.co/lj1995/VoiceConversionWebUI | MIT | Meta AI (fairseq), as shipped by RVC-Project |
| Kokoro English TTS model (kokoro-en-v0_19) | speech | https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-en-v0_19.tar.bz2 | Apache-2.0 | hexgrad (Kokoro-82M), packaged by k2-fsa |
| silver LICENSE | features (an import test) | https://raw.githubusercontent.com/ar-visions/silver/master/LICENSE | this repository's own | ar-visions |
| WPE WebKit 2.54.0 | aura | https://wpewebkit.org/releases/wpewebkit-2.54.0.tar.xz | LGPL-2.1-or-later and BSD-2-Clause (per file) | Apple, Igalia and the WebKit contributors |
| Ruby 3.3.6 (build tool) | aura | https://cache.ruby-lang.org/pub/ruby/3.3/ruby-3.3.6.tar.gz | Ruby License or BSD-2-Clause | Yukihiro Matsumoto and the Ruby contributors |
| unifdef 2.12 (build tool) | aura | https://dotat.at/prog/unifdef/unifdef-2.12.tar.gz | BSD-2-Clause | Tony Finch and contributors |
| pkgconf 2.3.0 (build tool) | aura | https://distfiles.ariadne.space/pkgconf/pkgconf-2.3.0.tar.xz | ISC | Ariadne Conill and contributors |
| Meson 1.8.3 (build tool) | aura | https://github.com/mesonbuild/meson/releases/download/1.8.3/meson-1.8.3.tar.gz | Apache-2.0 | The Meson development team |
| libgpg-error 1.51 | aura | https://www.gnupg.org/ftp/gcrypt/libgpg-error/libgpg-error-1.51.tar.bz2 | LGPL-2.1-or-later | g10 Code GmbH |
| libgcrypt 1.11.0 | aura | https://www.gnupg.org/ftp/gcrypt/libgcrypt/libgcrypt-1.11.0.tar.bz2 | LGPL-2.1-or-later | g10 Code GmbH and the Free Software Foundation |
| libtasn1 4.19.0 | aura | https://ftp.gnu.org/gnu/libtasn1/libtasn1-4.19.0.tar.gz | LGPL-2.1-or-later | Free Software Foundation |
| libpsl 0.21.5 | aura | https://github.com/rockdaboot/libpsl/releases/download/0.21.5/libpsl-0.21.5.tar.gz | MIT | Tim Rühsen and contributors |
| nghttp2 1.64.0 | aura | https://github.com/nghttp2/nghttp2/releases/download/v1.64.0/nghttp2-1.64.0.tar.xz | MIT | Tatsuhiro Tsujikawa and contributors |
| GLib 2.84.4 | aura, featured/gedit (with its GIRs) | https://download.gnome.org/sources/glib/2.84/glib-2.84.4.tar.xz | LGPL-2.1-or-later | The GLib developers (GNOME) |
| libsoup 3.6.5 | aura | https://download.gnome.org/sources/libsoup/3.6/libsoup-3.6.5.tar.xz | LGPL-2.0-or-later | The libsoup developers (GNOME) |
| glib-networking 2.80.1 | aura | https://download.gnome.org/sources/glib-networking/2.80/glib-networking-2.80.1.tar.xz | LGPL-2.1-or-later | The glib-networking developers (GNOME) |
| gobject-introspection 1.84.0 | featured/gedit | https://download.gnome.org/sources/gobject-introspection/1.84/gobject-introspection-1.84.0.tar.xz | LGPL-2.0-or-later (library), GPL-2.0-or-later (tools) | The GObject Introspection developers (GNOME) |
| gsettings-desktop-schemas 47.1 | featured/gedit | https://download.gnome.org/sources/gsettings-desktop-schemas/47/gsettings-desktop-schemas-47.1.tar.xz | LGPL-2.1-or-later | GNOME |
| enchant 2.8.2 | featured/gedit | https://github.com/rrthomas/enchant/releases/download/v2.8.2/enchant-2.8.2.tar.gz | LGPL-2.1-or-later | The Enchant developers |

## Scene assets (fetched at `silver --export scenes`, never committed)

| asset | baked as | source | credit | license |
|---|---|---|---|---|
| Gaia DR2 all-sky brightness and colour, plate carree 4000x2000 | scenes/textures/gaia.png 4096x2048 | https://sci.esa.int/web/gaia/-/60196-gaia-s-sky-in-colour-equirectangular-projection | ESA/Gaia/DPAC | CC BY-SA 3.0 IGO |
| Jupiter cylindrical map, Cassini + Juno | scenes/textures/jupiter-color.png 4096x2048 | https://planetary.s3.amazonaws.com/web/assets/pictures/20180511_jupiter_map_css_plus_juno_bj.jpg | NASA/JPL/SSI, processed by Björn Jónsson, via The Planetary Society | as published by The Planetary Society |
| Saturn colour map | scenes/textures/saturn-color.png 4096x2048 | https://www.solarsystemscope.com/textures/ | Solar System Scope | CC BY 4.0 |
| Titan Mosaic: The Surface Under the Haze (PIA22770) | scenes/textures/titan-color.png 5760x2880 | https://www.jpl.nasa.gov/images/pia22770-titan-mosaic-the-surface-under-the-haze/ | NASA/JPL-Caltech/Space Science Institute | NASA image use policy (public domain with credit) |
| MOLA MEGDR 128 ppd tiles (MGS-M-MOLA-5-MEGDR-L3-V1.0) | read from checkout/mola by the Mars scene | https://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/mgsl_300x/meg128/ | NASA/JPL/GSFC, MOLA Science Team, PDS Geosciences Node | NASA PDS, public domain |
| Blue Marble Next Generation, July 2004 (world.200407), 21600x10800 | scenes/baked/earth-land.png: where Earth's sea, ice, desert and forest are (fetched into ~/.local/state/scenes) | https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/bmng-base/july/world.200407.3x21600x10800.jpg | NASA Earth Observatory, Reto Stöckli (NASA GSFC), MODIS | NASA image use policy (public domain) |
| Blue Marble clouds (cloud_combined), 8192x4096 | launch: the clouds' coverage over the Earth (fetched into ~/.local/state/scenes) | https://eoimages.gsfc.nasa.gov/images/imagerecords/57000/57747/cloud_combined_8192.tif | NASA Visible Earth, NASA Goddard Space Flight Center (MODIS) | NASA image use policy (public domain) |
| Fir Tree 01 (fir_tree_01), glTF 1k + twig alpha | scenes/baked/earth-fir_tree_01-{color,height,normal}.png | https://polyhaven.com/a/fir_tree_01 | Poly Haven | CC0 1.0 |
| Island Tree 01 (island_tree_01), glTF 1k + leaves alpha | scenes/baked/earth-island_tree_01-{color,height,normal}.png; its leaves scattered into scenes/baked/earth-foliage.png | https://polyhaven.com/a/island_tree_01 | Poly Haven | CC0 1.0 |
| Island Tree 02 (island_tree_02), glTF 1k + leaves alpha | scenes/baked/earth-island_tree_02-{color,height,normal}.png | https://polyhaven.com/a/island_tree_02 | Poly Haven | CC0 1.0 |

## Notes on the copyleft entries

FAAD2 is GPL. spectra and speech link it, so a distributed build of those modules
is subject to the GPL unless a commercial FAAD2 license is obtained or the decoder
is swapped. QEMU and the Linux kernel run as separate programs in orbiter-os, which
does not put silver under their license. Blender runs the same way for aeon's
.blend import: a separate program, never linked. Mbed TLS and Vulkan-Headers are dual
licensed and are used under Apache-2.0.

WebKit, GLib, libsoup, glib-networking and the gcrypt libraries are LGPL and
linked dynamically by the aura module. aura/ holds our changes to them as
overlays (wpewebkit/, libsoup/, glib-networking/ with its mbedtls backend); those
changes are under the same LGPL terms as the files they change.
