# Third-party dependencies and credits

Everything silver imports from outside this repository, with where it comes from,
who made it and the license it is used under. The `import` lines in each module
name the exact commit or version; this file is the human ledger of the same set.
Keep it in step with the imports.

## Libraries (git imports, built from source at the pinned commit)

| dependency | pinned | used by | source | license | authors |
|---|---|---|---|---|---|
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
| sherpa-onnx | v1.13.4 | speech | https://github.com/k2-fsa/sherpa-onnx | Apache-2.0 | Next-gen Kaldi (k2-fsa) |
| FAAD2 | 2.11.2 | spectra, speech | https://github.com/knik0/faad2 | GPL-2.0-or-later (commercial license available) | Nero AG, Fabian Greffrath, Krzysztof Nikiel and contributors |
| Mbed TLS | ec4044008d2d069da38288bc76b0fee34ec78646 | tls | https://github.com/Mbed-TLS/mbedtls | Apache-2.0 OR GPL-2.0-or-later | Arm Limited and contributors |
| QEMU | e47e2d0 | qemu | https://github.com/qemu/qemu | GPL-2.0-only (mixed, see its LICENSE) | Fabrice Bellard and the QEMU project |
| virglrenderer | main | qemu | https://gitlab.freedesktop.org/virgl/virglrenderer | MIT | Dave Airlie, Collabora and contributors |
| Mesa | 26.2.2 | os-bootstrap | https://gitlab.freedesktop.org/mesa/mesa | MIT (with other permissive parts) | The Mesa 3D project |
| libuev | v2.4.1 | features | https://github.com/troglobit/libuev | MIT | Flemming Madsen, Joachim Wiberg |

## Files fetched by url

| file | used by | source | license | authors |
|---|---|---|---|---|
| Linux kernel 6.18 source | os-bootstrap | https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.18.tar.xz | GPL-2.0-only WITH Linux-syscall-note | Linus Torvalds and the kernel community |
| Kokoro English TTS model (kokoro-en-v0_19) | speech | https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-en-v0_19.tar.bz2 | Apache-2.0 | hexgrad (Kokoro-82M), packaged by k2-fsa |
| silver LICENSE | features (an import test) | https://raw.githubusercontent.com/ar-visions/silver/master/LICENSE | this repository's own | ar-visions |

## Scene assets (fetched at `silver --export scenes`, never committed)

| asset | baked as | source | credit | license |
|---|---|---|---|---|
| Gaia DR2 all-sky brightness and colour, plate carree 4000x2000 | scenes/textures/gaia.png 4096x2048 | https://sci.esa.int/web/gaia/-/60196-gaia-s-sky-in-colour-equirectangular-projection | ESA/Gaia/DPAC | CC BY-SA 3.0 IGO |
| Jupiter cylindrical map, Cassini + Juno | scenes/textures/jupiter-color.png 4096x2048 | https://planetary.s3.amazonaws.com/web/assets/pictures/20180511_jupiter_map_css_plus_juno_bj.jpg | NASA/JPL/SSI, processed by Björn Jónsson, via The Planetary Society | as published by The Planetary Society |
| Saturn colour map | scenes/textures/saturn-color.png 4096x2048 | https://www.solarsystemscope.com/textures/ | Solar System Scope | CC BY 4.0 |
| Titan Mosaic: The Surface Under the Haze (PIA22770) | scenes/textures/titan-color.png 5760x2880 | https://www.jpl.nasa.gov/images/pia22770-titan-mosaic-the-surface-under-the-haze/ | NASA/JPL-Caltech/Space Science Institute | NASA image use policy (public domain with credit) |
| MOLA MEGDR 128 ppd tiles (MGS-M-MOLA-5-MEGDR-L3-V1.0) | read from checkout/mola by the Mars scene | https://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/mgsl_300x/meg128/ | NASA/JPL/GSFC, MOLA Science Team, PDS Geosciences Node | NASA PDS, public domain |

## Notes on the copyleft entries

FAAD2 is GPL. spectra and speech link it, so a distributed build of those modules
is subject to the GPL unless a commercial FAAD2 license is obtained or the decoder
is swapped. QEMU and the Linux kernel run as separate programs in orbiter-os, which
does not put silver under their license. Mbed TLS and Vulkan-Headers are dual
licensed and are used under Apache-2.0.
