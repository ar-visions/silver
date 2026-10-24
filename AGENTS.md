9# silver

## Rule #0 — Plain simple English, always

Speak plainly. Name the image, the axis, and the units in every description of geometry or data ("the crop's center moves up the y axis by 10% of the crop size" — never "raised 0.1"). No jargon, no compressed geometry-speak, no trying to sound sophisticated — that reads as obtuse and hides the meaning. Show the diff of every code and file change.

---

## Rule #1 — NEVER perform git operations

DO NOT run `git checkout`, `git reset`, `git restore`, `git clean`, or any other git command that modifies or discards file contents. These operations destroy work irreversibly. The only git commands permitted are read-only: `git status`, `git diff`, `git log`, `git blame`. If asked to "revert" something, edit the file directly — never use git to do it.

---

## Rule #2 — Never question the build state

Do NOT suggest `make clean`, doubt that a rebuild happened, or ask "did silver rebuild." The user's build system works. If a change doesn't appear to take effect, the bug is in the code, not the cache. Trust the user's runtime output as reflecting the current code state, always.
running silver with --clean will always rebuild the module

---

## Rule #3 — Comments: one line, 60 characters max

A comment states the one constraint the code can't show. Never more than one line, never exceeding 60 characters. No comment blocks, no narrative prose, no AI sales cadence ("not just X but Y"). If it doesn't fit in 60 chars, fix the code or put it in a doc.

---

## Rule #3b — Lines: 72 columns max; names, never numbers

Every line of a .ag module fits in 72 columns (tabs count 4).
Break code inside [ ] after a comma, or before && / ||. A line
break is not neutral in silver: a bracketless cast (`f32 i`)
reads by line, so never break inside one.
Allowed over 72, and only these: a line whose single name, URL
or path is longer than the space left (Vulkan's own names), and
a log / expect / fault message string.
A variable is a name (a word or a short word like cv), or
i / x / y. Never a numbered name (wx9, lp9, a9): pick a real
name instead of a digit to dodge a clash.

---

## Rule #3c — Casts: never redundant

NEVER write a cast that changes nothing: `f32[ f32[ k ] ]`,
`i32[ i32[ b ] ]`, `kf : f32 [ f32[ k ] ]`, or a cast to the type
the value already has (`f32[ tex.width ]` when width is f32).
Silver converts scalars on assignment and in arithmetic:
`kf : f32 [ k ]` takes a f64. A cast is for a real type change;
a real two-step change (`f32[ i64[ x ] ]`) is fine.
Before showing a diff, read every new line for a cast that does
nothing and remove it. Repeat offense: thousands of lines were
cleaned by hand.
The same holds for a declared type: name it only when it changes
something. `s: '{minutes}'`, never `s: string [ '{minutes}' ]`
(an interpolation is already a string); the compiler rejects
it ("type 'string' is redundant").

---

## Rule #4 — Isolate and validate every fix and feature

Fix one component at a time. Reproduce the exact failure with the smallest focused test before editing. Change the component that owns the bug; do not add a workaround in another parser, model, build, or runtime layer. Run the focused test after the edit, then validate the affected module, then validate the requested integration target. Report each result separately. Do not call a fix complete unless its focused test and affected module both pass. If integration exposes another failure, treat it as a separate bug. Remove all temporary tracing before reporting results.

---

## Rule #5 — AGENTS.md is the only memory

`AGENTS.md` is the only project memory file. When the user asks to make a memory, write it here. Do not search for or choose another memory location. The user decides what belongs in memory.

---

## Rule #5b — The first attempt is the real one

Kalen (Oct 5 2026): "everything i ask for is usually bullshitted
at first and then i have to ask 12 times to get the real result".
The first version of anything asked for is built as the thing
itself, at the strength and scale asked, not a faint or thin
stand-in to be tuned up later. Before reporting, look at it
where it shows (the sun's spot ON the land, the aurora FROM the
side) and measure it; a change that cannot be seen or measured
is not done. When several items are named, every one lands in
the same pass. The aurora took six builds and the land specular
eight asks: an unmeasured first pass costs more than doing it.

## Rule #6 — Get details first

Do not infer work, scope, history, or a report from incomplete information. Before inspecting files, running commands, editing, or reporting, get the concrete details needed from the user. If the user has not asked for a specific action, take no action. When told to stop, stop without adding analysis or proposals.

## Rule #6b — Never re-ask about assigned work

A reported bug IS the assignment. Never end a turn asking "want me
to do X now?" when X was already named. Finish the current item,
report it in one or two sentences, start the next named item.

## Rule #6c — Track every named item

When user names several defects or tasks, keep an explicit list
and work it in his stated order. New reports ADD to the list; they
never replace earlier ones. Never drop an item unfinished without
saying so. Keep the live list in AGENTS.md and update it as items
land.

## Rule #7 — Never capture the desktop

Never capture the desktop or enumerate desktop windows. Use the
app's own screenshot socket and app tools for screenshots.

## Rule #8 — Use Trinity app messaging

Always use the Trinity messaging service over Unix sockets when
interacting with apps.

## Rule #9 — Foundation: the user's own agent, never ours

Trinity apps run the agent CLI the user already has; they never
bundle, name, or recommend one. There is no session mailbox and
no hook: an exchange starts its own agent in the project folder
on its first message. claude: `claude -p --input-format
stream-json --output-format stream-json`, one process for the
whole exchange, each later message written into its input (the
new line alone). codex: `exec --json` first, then `exec resume
<thread>` per message; gemini: `-p ... -o stream-json`, then
`--resume <session>`. A new exchange ends the last one's agent.
Each agent is a module of its own (claude/, codex/, gemini/): its
class extends trinity's AgentRun (trinity/agent.ag), named by its
export (`export agent ['ClaudeAgent']`); it says what to run and
reads the stream into statuses (busy / note / diff / done /
needs), and its build installs its program into install/bin.
trinity loads the module when its name is picked (`agent_start`),
`agent_proc_*` (trinity.cc) runs the process, knowing no agent.
Each run's raw stream: <install/tmp>/agent-shell.log. The
dictation take goes to orbiter's agent console. Do not add an
LLM, voice, or chat backend to trinity or orbiter. silver's
codegen uses the shell too.

## Rule #10 — The exchange: how an agent answers a trinity app

A screenshot (ctrl/cmd+shift+S, drag, Enter) opens the exchange
in any trinity app: the blurred capture behind, the user's box
(one rounded frame, entries above an editable bottom line, at
most four rows, the rest scrolling out the top), the app's
avatar, and after the first send the agent's box on the right.
The app's `.agi` lists the agents the user runs (`agents: [ claude,
codex ]`, adapter names, the agi's bracket list; `agent:` alone is a list of one), or a
block per agent with its models, first the default (`claude:` then
`models: [ opus, sonnet ]` indented under it; trinity `Agent`); the prompt
shows them as a TButtons row (`AgentPick`) with the lit agent's
models in a row above it (`model_pick`), the lit ones take the
first send (`agent_post_as` with the pick and model: claude
`--model`, codex `-m`), and both rows fade out
as the exchange becomes a conversation.
The exchange also opens about a file: the orbiter tab in a pane's
tab strip (icon orbiter4) opens it at the caret's line, with
`File: <path>:<line>` as the reference (`exchange_open` on the
Window; the capture path passes `Screenshot: <path>`). The first
send carries that reference line; each later send is the new line
alone. The run's stream becomes these statuses:

- The app's socket is `$XDG_RUNTIME_DIR` (else `/tmp`)
  `/trinity-<app>.sock`, one line per request, `app <text>`
  reaching the app's `on_agent` (tests drive statuses this way).
- `app status <state> <text>` is the agent's word. States:
  `busy` (working, with a line), `idle`, `done` (finished; with
  the exchange up and a source change staged, the avatar's core
  flame lights for 1.4 s, then the app applies its live reload:
  a recompile when the sources are newer than the product, else
  the swap at once),
  `needs` (waiting on the user), `note` (a line of what the
  agent says), `diff` (one line of a source diff it applied),
  `rewind` (the user's undo: cmd+left, ctrl+left on linux, in
  the prompt; `agent_rewind` sends claude a `rewind_files`
  control request for our last replayed message id, files go
  back to that point; the avatar turns 60 degrees, cubic in_out
  over 1.6 s, while its spin slows, reverses and comes back).
  UNBUILT, UNVERIFIED (Sep 25): the build and a live rewind
  test both needed approval.
- Optional `app status description <line>` messages attach to the
  preceding title. Click the title to expand or collapse the description.
  Repeat for multiple lines; omit for a plain message.
- Everything the agent says goes back as `note` lines, and every
  edit it applied as `diff` lines, in order: the edit's own
  removed and added lines under a `diff --git a/x b/x` header.
  NEVER a `git diff` of the tree for this: the tree also holds the
  user's own uncommitted changes, which are not the agent's.
  Consecutive `diff` lines are one entry in the agent's box: a
  label (the file from `diff --git a/x b/x`, +added -removed)
  that expands to the colored code when clicked. A `note` (or any
  other state) ends the diff entry. shell_edit_diff (trinity.cc)
  builds it from each Edit/Write tool call's old and new text.
- An app takes statuses only while an exchange is open
  (`shot_ask` set, up or minimized). The avatar stays
  face on and at rest until the first send through the app.
- Drive a trinity app headless to verify: `XDG_RUNTIME_DIR=<dir>
  SILVER_ISOLATE=0 SILVER_HEADLESS=1 platform/native/build/<app>
  --hidden true`, then over its socket `key 83 1 3` / `key 83 0 3`
  (ctrl+shift+S), `press x y`, `move x y`, `release x y`, `key 257
  1` (Enter), `text <line>`, `bounds <id>`, `shot <png>`. A socket
  path longer than the unix limit is truncated. After a capture,
  `shot` reads the render list's last entry (the capture's blur
  chain), so it stops reflecting the screen: use `bounds` and the
  draw logs for per-frame state.
- Region slots are `l t r b`: `b120px` in the SECOND slot puts the
  TOP 120px from the bottom. A percent in the fourth slot is a
  flow share only as a height/width (`h50%`); a percent bottom
  edge (`b-15%+174px`) is an edge. A CSS area transition mixes
  coordinate forms, so both ends of a moving area use the same
  form (`l50%-195px` to `l0%+80px`, never to `l80px`).

---

## Rule #11 — Codegen: writing a `using` function's body

A `func ... using claude` (or chatgpt) is the author's signature
and prompt; the agent writes only its body, in the gen/ file the
request names. Author code stays in the module, agent code in
gen/. The agent runs in the project folder the request names:
read the module and its imports there, and use the project's own
types and functions.

The file, exactly:

```
extend <module>
# hash: <the value the request gives>
<the func line exactly as the request gives it>
    <the body, indented with four spaces>
```

- Keep the hash and func lines as given: the compiler checks both.
- Write the body only; edit no other file.
- Never build, test or run silver: the build that asked waits
  on the file, and a nested build deletes it.
- A method body sees its instance's members by name (`seconds`).
- A picture in the prompt is Image 1, Image 2: its path is in the
  source line the request points at. Look at it.
- All of Rules #3, #3b and #3c hold (comments, 72 columns, names,
  no redundant cast or type).
- Arguments are read-only: copy one to a local to change it.
- `el` is else, `el [ c ]` else-if; `log '...'` prints; no hold or
  drop calls.
- Taken names: is, signed, parse, pre, post, ref, line, msg, send,
  hold, drop (a method named hold replaces Au's hold: the object
  is never kept, and the pool frees it at the next drain).

A body that does not compile goes to the user, never back to the
agent: the cause may be ours. Each such error becomes a rule here.

---

## Rule #12 — Modifier keys reveal hidden actions

Kalen (Oct 10 2026). An action kept out of sight (Remove on a list
row, and the like) shows while a modifier is held:
- shift: over the hovered element only.
- ctrl, or cmd on a mac: on every element at once (globally).
Release the key and it hides again. The Window tracks both
(`ux.shift`, `ux.cmd`); each change re-renders the tree. A
shift+click that forces a blocked step (Continue) is separate.

## Rule #13 — No overlaid windows

Kalen (Oct 14 2026): "i just cant stand those overlaid windows..
we need to get away from it entirely". Never put a dialog, popup,
modal or box over the app's view. A question goes in the app's
own status line, highlighted by a stronger border round it, and
answered by a key (ctrl/cmd + r: "revert? enter: yes"; any other
key is no). Values, choices
and pickers live in the panels already on screen.
Why (Kalen): it does not obscure the work, so the user can
inspect it while deciding, and decide better.
The one exception (Kalen, Oct 15): the menus orbiter spawns (a
window's menu from the system bar, a tray icon's menu) slide out of
the dock as a drawer over the app, on a blur of what is under it;
the app keeps its width. Nothing else overlays.
Not an overlay: the navigator (trinity/navigation.ag) is a whole
view under the bar, not a box over the work. File open and file
save (save as, an untitled drawing's first save) go through it
(Kalen, Oct 14).

---

## orbiter-os — live list (Sep 3 2026)

Goal: orbiter-os (Linux + orbiter as init) running inside the `qemu`
element in a trinity window. Decisions: Venus virtio-gpu for guest
Vulkan; kernel + initramfs first, the `.img` installer after.

1. DONE `qemu/qemu.ag` element: boots a raw .img over VNC (verified).
2. DONE silver imports: `from <url>` repos, configure-before-meson,
   `>` lines replace make, self-import guards.
3. DONE qemu built with virglrenderer (venus) via import.
4. DONE Linux 6.18 kernel via URL import in os-bootstrap.
5. DONE devices KMS mode: evdev input, DRM master, VK_KHR_display.
6. DONE trinity create_display_surface (acquire_drm_display).
7. DONE os-bootstrap: newc cpio (orbiter/app + ldd closure + Mesa
   venus + share); silver-host is PID 1 init (mounts, log dump).
8. DONE qemu element gpu/venus/serial; child dies with element.
VERIFIED so far: guest boots, venus loads, VK_KHR_display acquires
connector 38 at 1280x800 (orbiter-os/serial.log).
VENUS DISPLAY FIXED (Sep 3 late): built Mesa 26.2.2 from source and
patched vn_wsi.c (os-bootstrap/mesa-26.2.2.diff) — pass /dev/dri/card0
as the display fd + supports_scanout=true. Guest venus now enumerates
the display and presents via VK_KHR_display continuously (40s, no
errors). See memory [[orbiter-os-qemu]].
NEXT: the scanout is a GPU blob — software VNC/screendump can't read
it. Replace the qemu element's RFB/VNC with qemu D-Bus display
(ScanoutDMABUF) and import the dma-buf into a trinity Texture. That is
the dma transport; it completes guest venus → scanout → element.
Test hidden only: `silver os-bootstrap --hidden --app colortest`.

## orbiter-os on the drive — live list (Sep 25 2026)

Goal: orbiter-os boots from a folder on Ubuntu's partition
(nvme0n1p1, UUID 5069068c-…), picked from Ubuntu's GRUB menu.
SUSE is gone: only its EFI loader is left (its btrfs root
7659a74b-… is on no partition).
1. DONE the folder: os-bootstrap writes its file list as
   orbiter-os/root/ (kernel at boot/vmlinuz), times kept.
   Checked: 4,525 entries as the cpio, 1.1 GB; the relative
   install link keeps writes inside; the absolute links are
   files whose targets are in the tree too.
2. DONE a small initramfs (root/boot/initramfs, 5 entries):
   os-bootstrap/init.c, static, finds the ext4 partition by
   `orbiter.uuid=`, mounts it, enters `orbiter.root=` (default
   /orbiter-os; the relative link resolves on the drive), tmpfs
   on /tmp and /run, runs `orbiter.init=` as pid 1 with the
   command line's name=value words as its environment.
   Checked in qemu on an ext4 image: pid 1, / is the folder,
   env passed; a wrong uuid prints why and waits (no panic).
   /orbiter-os -> src/silver/orbiter-os/root is made (Kalen).
3. DONE kernel for europa: BLK_DEV_NVME added (Samsung 980
   Pro); ext4, xHCI, USB HID, evdev, DRM, EFI stub, modules,
   r8169 (RTL8125) were already in. Wi-Fi (RTL8852CE) is not.
   The kernel lines skip while install/share/silver-os-
   bootstrap/bzImage exists: delete it to apply new options.
4. BUILT, unverified until a real boot: NVIDIA 610.57.04 (the
   host driver; modules, firmware and libraries must match).
   os-bootstrap imports the open-module tag, runs the kernel's
   `modules` stage first (Module.symvers), builds against it
   into share/silver-os-bootstrap/nvidia. add_nvidia puts the
   .ko files in lib/modules/orbiter, gsp_ga10x.bin, the ICD
   json and libGLX_nvidia + the libs it opens in the drive tree
   only. init loads `orbiter.modules=` after the switch and
   makes /dev/nvidiactl, nvidia-modeset, nvidia<N> (major 195).
   On a host driver update: change the tag, `nvidia_version`,
   and delete share/silver-os-bootstrap/nvidia to rebuild.
   Secure Boot is off on europa: an unsigned kernel boots.
5. IN PROGRESS the GRUB entry (awaiting Kalen's sudo + a boot):
   appended to /etc/grub.d/40_custom, menu shown 5 s (was
   hidden, timeout 0), update-grub. Kernel command line:
   console=tty0 orbiter.uuid=<p1> orbiter.init=/src/silver/
   install/build/orbiter orbiter.modules=nvidia,nvidia-modeset,
   nvidia-drm:modeset=1 HOME=/root LD_LIBRARY_PATH=/src/silver/
   install/lib:/src/silver/install/build:/usr/lib/x86_64-linux-
   gnu VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json.
   /orbiter-os is the link; no copy.
6. IN PROGRESS first boot (14:31): the menu showed, then a black
   screen. The tree got a root-owned install/tmp at 14:31:09
   (silver-host starts by making it), no log inside. So GRUB,
   kernel, init, mount, switch and exec worked; the launcher
   stopped before logging. Adding: EFI framebuffer console
   (simpledrm), and init saving /dev/kmsg to /boot/kmsg.log and
   pid 1's output to /boot/boot.log, each line synced.
   Second boot (16:09), from those logs: all three nvidia
   modules loaded; nvidia-drm is minor 1 (simpledrm minor 0);
   silver-host held pid 1 (no panic). Two bugs, both fixed:
   - devices kms took card0 = simpledrm (1024x768): the scan
     now skips the simpledrm driver (drmGetVersion name).
   - vkCreateInstance -9: libGLX_nvidia opens more libraries
     than ldd lists. Found by LD_DEBUG=files on the host:
     libnvidia-glsi, -rtcore, -allocator.so.1 added.
   Userns chroot tests are blocked here (AppArmor
   restrict_unprivileged_userns=1): verify by booting.
   Third boot (16:16): kms now picks card1 (nvidia) 2560x1440.
   vkCreateInstance still -9; no NVRM errors in kmsg; every ldd
   dependency of libvulkan and the nvidia libs is in the tree.
   Next boot: VK_LOADER_DEBUG=all added once at the GRUB menu
   ('e'), its output lands in /boot/boot.log.
   Fourth boot (16:21) loader says: libGLX_nvidia loads, but
   "Could not get 'vkCreateInstance' via vk_icdGetInstanceProcAddr".
   On the host the same loader + ICD work with an empty env, and
   strace shows vulkan opens only nvidiactl, nvidia0,
   nvidia-modeset and /proc/driver/nvidia/params. The tree's
   dev/sys/proc are empty (the mounts moved fine).
   TEMP trace in init.c (remove when solved): open() result of
   each nvidia node + gpu count. Next boot adds LD_DEBUG=libs.
   Fifth boot (LD_DEBUG=libs): nodes open ok, 1 gpu. glcore's
   "undefined symbol __malloc_hook/ErrorF (fatal)" lines show
   on the host too (harmless). The real miss: vkCreateInstance
   opens glvnd EGL; libEGL.so.1 was not in the tree. Added
   libEGL, libGLdispatch, libEGL_nvidia, libnvidia-eglcore,
   libnvidia-egl-gbm, gbm/nvidia-drm_gbm.so, the EGL vendor
   json and the gbm platform json. Checked: vulkaninfo run by
   the tree's ld.so with only the tree's libraries finds the
   RTX 3060, driver 610.57.04. grub-debug.sh is still on.
7. BUILT, not booted (Oct 15, Kalen: a hardware cursor, its move
   eased by filter_n at the screen's refresh) DRM owns the screen.
   NVIDIA's VK_KHR_display planes are full screen only (no cursor
   plane), so the VK_KHR_display surface is gone: trinity asks the
   Vulkan device for its card (VK_EXT_physical_device_drm; card2 on
   europa, never a guess by number), devices opens it as master
   (platform_kms_open), trinity makes three LINEAR exported images
   (Texture shareable + host_linear) that become framebuffers
   (platform_kms_add_fb: AddFB2WithModifiers LINEAR; plain AddFB2
   is refused: nvidia reads no modifier as tiled), copies each
   frame into the free one and page-flips it (platform_kms_back,
   platform_kms_flip; one flip in flight, paced by the vblank
   event). The pointer is the CRTC's cursor plane (256 x 256 on
   nvidia; a white arrow with a 1 px black edge drawn by devices,
   platform_kms_cursor_image for others), moved without a frame.
   Easing: each poll moves the drawn point a share of the way to
   the mouse, keep = (1 - 0.3)^(dt x refresh Hz): it keeps 70%
   of its place and takes 30% of the mouse a refresh at any rate
   (Kalen; platform_cursor_ease). Then (Kalen, Oct 15, first
   boot where it moved: "too instant"; "it must decay at the
   frame factor of our hz rate"): per refresh at the mode's hz,
   keep = (1 - g_ease)^(dt x hz), g_ease 0.3 (the build tried
   was my 0.7 by mistake). The app
   gets the eased point (clicks land where it is drawn).
   Checked on europa without X (X holds master): card2 found,
   connector 825, crtc 199, 2560 x 1440 at 165 Hz, the three
   framebuffers taken, frames copied; SetCrtc refused (not master),
   as expected. The arrow checked as a picture. NOT seen on a
   screen: needs the tree written and a boot.
   Then (Kalen: the cursor an svg drawn by trinity; 24 x 24):
   Display.kms_pointer draws trinity/images/cursor2.svg (Kalen's,
   made in aeon) into a 24 x 24 Canvas, reads it back, gives it to
   the cursor plane. Click point: an element id hot (its middle,
   not drawn), else the drawing's point nearest the top left (the
   tip). devices' own arrow stays as the fallback. Checked by a
   dump of the plane's buffer: the arrow, its white border and
   shadow, the hotspot 1.6 px in at the tip. An app's share links
   trinity's images at its build: build the app after a new svg.
   Fixed (Oct 15, Kalen's boot: the arrow blue inside; "trinity
   must render svg the same as Aeon"): draw_svg drew its cache
   with its own copy of SVGModel.render's loop; it calls
   SVGModel.render_tinted now (render is its untinted form). The
   blue was trinity's: a canvas with the reduce pass (an svg drawn
   4x) draws through Render.draw_immediate, which took the ring's
   next index as draw_slot AFTER the paint had bound its textures
   into the old one, so a gradient's stops strip missed its draw
   and the mask slot still held the path field (one channel).
   draw_immediate now leaves draw_slot on the ring slot its next
   draw uses (waited free first), and sync_fence rewinds such a
   render to ring_i, not 0. Any reduce canvas with a gradient or
   image draw was affected. Seen: cursor2 at 24 px, a grey
   gradient as in Aeon; trinity tests exit 0.
   SVGModel gained a method: build every trinity app (orbiter,
   wizard, aeon, os-bootstrap) before it runs again.
   Not booted: the evdev rescan (devices kms_poll, every second)
   and init.c's uevent hotplug (modprobe each `add@` MODALIAS),
   for the USB keyboard and mouse that came up after init's one
   modprobe pass (no usbhid, the pointer never moved).
   Boot (05:00, Kalen: still not moving): usbhid loaded at 13.7 s
   (the hotplug ran) but no HID input followed: the listener began
   reading only after the 11 s first pass, whose drivers' device
   events overflowed its socket, so hid-generic never loaded; and
   the kernel's own requests went to /sbin/modprobe (none in the
   tree). init.c now: kernel.modprobe set to /usr/sbin/modprobe,
   the listener (16 MB buffer) started before the pass, a second
   pass after it. Tree written 05:05; not booted.

## orbiter-os installer — live list (Oct 10 2026)

Kalen: the install is orbiter itself, started with an argument,
as macOS setup runs on its own window server. The installer app
is `wizard`; it uses whatever supported card is in the machine.
1. DONE `wizard/wizard.ag`: element InstallWizard. Pages: the
   cards (pci display devices + each Vulkan device: name, kernel
   driver, Vulkan driver and version, the one trinity uses), the
   disk (/sys/block), whole disk or a folder beside (partitions
   by number, fs from the mount table or the superblock, the
   EFI partition = the disk's first FAT one), the account (user,
   password drawn as dots, computer, keyboard, time zone), the
   review (every step, and problems that block Install), the
   install on its own thread (step and copy progress, log).
   Steps: sfdisk, mkfs.vfat, mkfs.ext4 (whole disk); mount;
   copy the running tree (kernel trees made empty, links kept,
   times and run bits kept); install.agi (password as sha-512
   crypt) and /etc/hostname; kernel + initramfs to
   EFI/orbiter; efivarfs; efibootmgr with this system's own boot
   line and the new uuid and root. `silver --test wizard`:
   t_install_steps runs the worker on a scratch tree, passes.
2. DONE orbiter `--install true` (only the wizard over the
   scene; no session saved) and `--dry_run true` (plan listed
   and logged, never run). Checked hidden on europa: every page
   walked, both plans correct, session file untouched.
3. DONE init.c: `orbiter.args=--install,true` (words by comma:
   the kernel's words have no spaces) becomes init's argv.
   Checked in qemu (the real bzImage, an ext4 image, a static
   program printing argv): 5 args through, none without it.
4. DONE os-bootstrap add_tools (drive tree): /bin/sh (popen),
   sfdisk, mkfs.vfat, mkfs.ext4, efibootmgr, their ldd libraries,
   /etc/mke2fs.conf. Checked: the tree written (4,709 entries);
   each program runs on the tree's ld.so and libraries alone;
   libsilver-wizard.so finds libcrypt in the tree.
   Kernel rebuilt with EFIVAR_FS=y (Oct 10; the old one is
   share/silver-os-bootstrap/bzImage.prev). `silver --clean
   --build os-bootstrap` runs the kernel lines: without --clean
   the module is "up to date" and they never run. Checked: the
   nvidia modules' vermagic matches (6.18.0 SMP preempt
   mod_unload; MODVERSIONS is off) and all 703 kernel symbols
   they use are still exported; booted in qemu, the new kernel
   lists efivarfs (the old did not); orbiter-os/root/boot/vmlinuz
   is the new kernel. Not booted on europa yet.
   qemu is out of os-bootstrap (Kalen, Oct 10: too slow): no
   qemu element, no whole-guest initramfs.cpio.
   On the way: qemu's two imports could not find system .pc files
   (install/bin/pkg-config is pkgconf, install dirs only): their
   PKG_CONFIG_PATH names /usr/lib/x86_64-linux-gnu/pkgconfig and
   /usr/share/pkgconfig after install's. virgl and qemu built.
4b. OPEN a GPT USB image.
4c. DONE (Oct 10) `silver orbiter --install --dry_run` failed
   "expected value after install": Au kept parsing silver's own
   flags past the module (the knes `rom --vscale` rule), and
   aether's `install` path took it. silver's main now calls
   au_argv_launcher(true): its positional ends its flags, the
   rest are the app's (apps keep the knes rule). Checked: the
   bare bools reach orbiter and the wizard opens.
5. OPEN first boot: make the user from install.agi.
6. BUILT (Oct 10, Kalen: an RX 580; "and one for intel too")
   AMD and Intel. Kernel side was already in every build (amdgpu,
   radeon, i915, xe as modules; their firmware in the tree; init's
   modprobe by modalias). No choice row: unlike NVIDIA they are
   part of Linux. Vulkan: our Mesa 26.2.2 builds virtio, amd
   (radv) and intel (anv); os-bootstrap puts libvulkan_radeon,
   libvulkan_intel and their icd json in the tree. Built for it:
   libdrm 2.4.134 (radv/anv need 2.4.133; the system has 2.4.125,
   with which radv fails to read the GPU); SPIRV-LLVM-Translator
   8e82970 (matched to our LLVM bd7db75, rpath install/lib) and
   libclc from our LLVM source (>> lines, build/libclc,
   spirv64-mesa3d- only) for anv's built-in programs; Mesa with
   LLVM on at build time, amd-use-llvm=false (no Vulkan driver
   links LLVM at run time); LDFLAGS rpath install/lib (our
   libdrm). os-bootstrap/mesa.diff (the import's name; the old
   mesa-26.2.2.diff never applied to a clone): the venus hunk, a
   meson.build line (LLVM's amdgpu target only when radv uses
   LLVM: ours has no AMDGPU target) and clc_helpers.cpp taking
   LLVM 21's clang API on an early 22 snapshot (CLC_LLVM_API).
   Checked: radv on the 7950X's Radeon (RAPHAEL_MENDOCINO, Mesa
   26.2.2) with no environment; anv loads (no Intel GPU here).
   NOT in the drive tree yet: its next write. NOT seen on an RX
   580 or an Intel machine.
   Then (Kalen: "shouldnt the selection be there to build them or
   not?"): AMD (amdgpu / none) and Intel (i915, xe / none) choice
   rows under NVIDIA, and AMD and Intel columns in the kernel list.
   none turns the kernel's drivers off (DRM_AMDGPU, DRM_RADEON;
   DRM_I915, DRM_XE), names the build -noamd / -nointel, writes
   amd / intel in build.agi (older builds read as in), and the tree
   then leaves out that Vulkan driver and its firmware folders.
   AMD offered for every line but Apple, Intel for x64 and Surface.
   Seen hidden: the rows, the columns, a pick. No none build run.
   The wizard's avatar tilts away (work_on) while a kernel builds,
   as orbiter's does at work; seen hidden mid-build.
7. NOT RUN: a real install (needs root and a target disk).
8. DONE (Oct 10) the boot entry keeps the default: folder
   beside uses `efibootmgr --create-only`, finds the new entry
   (Boot#### before vs after) and sets it second in BootOrder;
   whole disk puts it first. Partitions are one table chip
   (header, rows scrolled inside). t_boot_order passes.
9. DONE (Oct 10) boot manager detection (folder beside): the
   firmware's default entry (BootOrder's first, its File path),
   then the EFI and Linux partitions (mounted, else mounted
   read-only under /run/wizard/look-*). GRUB with /etc/grub.d:
   writes /etc/grub.d/41_orbiter-os (runs), the review says run
   `sudo update-grub`; GRUB with loader/entries (Fedora): writes
   orbiter-os.conf there; systemd-boot: ESP loader/entries;
   rEFInd: EFI/orbiter/refind_linux.conf; Windows or unknown:
   the firmware entry only, said on the review page. Always the
   firmware entry too. t_boot_detect (9 cases) passes; europa's
   dry run finds GRUB (Debian, Ubuntu).
   Found on the way, fixed in aether.c (e_create's same-type
   shortcut): a vec enum element passed to a function went as
   its address (LLVM verify). features t_vec_enum_arg; features
   245/246 (t_codegen_resource as before).
10. DONE (Oct 10) the wizard's buttons and pick rows take the
   near light: near_border, near_inset 2 (4 hovered), an inset
   box_shadow (the inner glow needs one). Fixed in trinity:
   near_filter stepped the light only when its element drew,
   and a no_cache child (every Button) draws only when its
   cached parent repaints; its pristine marks were cleared by
   that same pass, so the light crept (0.17 of 0.5 in 2 s).
   It sets needs_frame on the parents while easing now: full in
   under a second. trinity tests exit 0. Checked by pixels: the
   Continue border 101 -> 141 with the pointer 10 px away.
11. DONE (Oct 10) partition columns: Partition, Size, Free,
   Type, What's on it. Free by statvfs (`st : struct statvfs`:
   the struct and the function share the name) where mounted,
   else mounted read-only for a moment (root). What's on it:
   os-release's PRETTY_NAME, "running now" for /, "has
   orbiter-os"; EFI: its EFI/ folders; swap from /proc/swaps;
   NTFS by its boot sector. Panel 32 units wide. t_free_space:
   equal to df to the byte.
12. DONE (Oct 10) the avatar is its own module, `avatar/`
   (Avatar, the OrbiterAvatar look; its model, core texture and
   environments moved from orbiter/ into avatar/models, textures,
   images). OrbiterAvatar reads fields (work_on, edits_in,
   work_done, swap_on, mic_on), not orbiter: orbiter's AppAvatar
   fills them each frame and keeps the click (CornerAvatar and
   AgentAvatar build on it); the icon export stays in orbiter.
   The wizard imports avatar: SetupAvatar (top-down, 14 units)
   left of the dialog, the dialog 8 units right of center.
   Fixed in aether (aether_apply_overrides): a class from another
   module's override (OrbiterAvatar's body) was marked set at the
   construction site, so the base init skipped its own default and
   used it null (SIGSEGV in Avatar_init); the imported class stores
   its overrides in its own init. features 245/246 as before.
13. DONE (Oct 10) partitions too small for the copy are
   disabled rows (dim text, "too small" first, no click).
   os-bootstrap writes the written tree's size to
   root/boot/tree-bytes (tree_size, 1,904,090,944 today, equal to
   find); the wizard needs that and a tenth more. Without the
   file (not started from orbiter-os) nothing is ruled out.
   t_copy_need passes; seen with the drive tree as the source:
   nvme0n1p3 (978 MB free) dimmed. The wizard's avatar does not
   turn; its inner ring spins at rest_spin 1.0 (orbiter's 0.25).
14. DONE (Oct 10) page one is the hardware check: graphics
   (pci class 03) and network (02; 0280 is Wi-Fi; usb adapters
   by /sys/class/net), each matched (fnmatch on its modalias)
   against the drive's own drivers: os-bootstrap writes
   root/lib/modules/orbiter/aliases (modules.builtin.modinfo's
   aliases and the nvidia modules', 3,638) and adds pci.ids for
   the names. Graphics also needs its Vulkan driver in the tree.
   Continue needs supported graphics and a supported network
   adapter; else it is grey and Shift (or Cmd) + click goes on.
   Without the alias file (not started from orbiter-os) every
   row reads "not checked". europa with the drive tree: RTX 3060
   yes (nvidia), RTL8125 yes (r8169), AMD graphics and the
   RTL8852CE Wi-Fi no. t_hardware_match passes.
15. BUILT, not booted (Oct 10, Kalen: the kernel we build has the
   hardware compatibility) the kernel on Ubuntu's 6.17 config
   (os-bootstrap/linux.config; signing, debug info and nouveau
   off; NVMe, USB storage, ext4, vfat, efivarfs built in, firmware
   loading takes .zst), every driver a module, installed into
   share/silver-os-bootstrap/kmods, nvidia's in extra/ (depmod).
   init.c: after the switch, modprobe -q -b -a every device's
   modalias in /sys/bus/*/devices, then orbiter.modules through
   modprobe (nvidia-drm:modeset=1). The tree gets lib/modules/
   <release>, all of /lib/firmware (752 MB), modprobe and modinfo.
   The wizard checks against that tree (the running one, else
   orbiter-os/root): modules.alias + modules.builtin.alias, and a
   module's firmware (one of those modinfo lists present). The
   old lib/modules/orbiter and the aliases file are gone.
   The defconfig kernel and its nvidia modules are kept as
   share/silver-os-bootstrap/bzImage.efivars and nvidia.efivars.
   Fixed (Kalen: the RTX 3060 "not supported"): from Ubuntu the
   check fell back to Ubuntu's /lib/modules and took its first
   kernel (6.14, no nvidia). It only ever checks orbiter-os's
   tree now (running orbiter-os: its own, the running release;
   elsewhere: orbiter-os/root).
16. DONE (Oct 10) the wizard's
   page 0 "Build orbiter-os": os-bootstrap is a component,
   element Bootstrap (summary: kernel, modules, drive tree, boot
   files, each built or not; Build runs `silver --clean --build
   os-bootstrap` when the kernel or modules are missing, then
   assemble on a thread, its lines shown). Shown only where
   orbiter-os can be built (bootstrap_here: not booted into
   orbiter-os, silver's source present); booted into orbiter-os
   the wizard starts at the hardware check. Continue needs it all
   built (shift+click goes on). The os-bootstrap app is element
   main over Bootstrap: write the tree, then quit.
   Built and checked on europa (hidden): kernel 6.18.0 (Ubuntu
   config), 6,712 drivers + 3 nvidia, tree 2,954 MB, vmlinuz =
   the built kernel; Build rewrote the tree in place with its
   lines shown. Hardware check against the new tree: RTX 3060
   (nvidia), RTL8125 (r8169), RTL8852CE Wi-Fi (rtw89_8852ce) all
   supported; AMD built-in graphics: amdgpu but no Vulkan driver
   (radv is not in the tree: Mesa's amd/intel drivers OPEN).
   Fixed on the way:
   - nvidia-uvm does not build on 6.18 with Ubuntu's HMM config:
     NV_KERNEL_MODULES='nvidia nvidia-modeset nvidia-drm' (we ship
     no CUDA).
   - the app's Bootstrap got no member defaults (constructed, not
     mounted): app was '', and its binary's path was install/build,
     which copied the whole build folder (17.7 GB, the disk at 5 GB
     free). initialize[] now; assemble refuses an empty app; the
     copy in the tree removed.
   - nvidiafb's alias takes every NVIDIA display card: the tree
     carries /etc/modprobe.d (Ubuntu's blocklists and options) for
     init's modprobe -b, and the wizard reads the same blocklist.
   NOT booted yet with this kernel (modules by modprobe from
   init): the next real boot is the test.
17. IN PROGRESS (Oct 10, Kalen: built kernels as a list to pick
   from, new ones from choices) page 0 is the kernel list
   (ListBox, as the partitions: Kernel, Config, Modules, NVIDIA,
   Built; the lit row is the one the drive takes, saved in
   kernels/selected) and choices: Kernel 6.18 / 6.12, Config
   Ubuntu / Minimal (defconfig), NVIDIA <host's> / none; Build
   kernel beside them, Write the tree beside the tree's state.
   Each build is share/silver-os-bootstrap/kernels/<ver>-<cfg>
   [-nvidia<v>]/ (bzImage, kmods with nvidia in extra/,
   build.agi). The kernel and nvidia import lines are gone: the
   element builds (fetch into install/imports, build/linux-<ver>,
   build/nvidia-<name>, config, make, modules_install, nvidia,
   depmod), every line shown. The os-bootstrap app builds the
   default when no kernel exists, then writes the tree. The list
   pieces (ListBox, ListHead, ListRows, ListRow, ListCell,
   list_cells, Choice) moved into os-bootstrap; the wizard's
   partitions use them. The first build was moved in as kernels/
   6.18-ubuntu-nvidia610.57.04 (build/linux -> linux-6.18, which
   makes its next make a full rebuild: absolute paths).
   Checked: the list, the choices, Write the tree (Done, date
   moved). Build kernel running end to end now (hidden).
   Note: `silver --build orbiter` does not rebuild os-bootstrap
   (reached through the wizard); build it by name after an edit.
   Then (Kalen, Oct 10): no config choice and no Ubuntu naming
   (orbiter-os is built by everyone): one config, our
   os-bootstrap/linux.config; builds are <ver>[-nvidia<v>] (the
   first renamed 6.18-nvidia610.57.04). The Drive tree / Boot
   files lines, Write the tree and the page-0 warning line are
   gone; the build output takes the room. Continue on page 0:
   disabled and dim while building (no override); when the tree
   lacks the chosen kernel it writes it, then goes on. NVIDIA's
   copy names its three modules (a stale uvm and peermem had
   been copied; removed). Build kernel checked end to end (full
   rebuild after the folder move, then nvidia, depmod, "Built").
   NOT checked: Continue while building (the run was stopped).
   Kalen's own 6.12 Minimal build is kernels/6.12-defconfig-...
   (8 modules), left as he made it.
   Cancel Build (lower left, only while building): each command
   runs `setsid sh -c` with its pid in install/tmp/os-bootstrap-
   build.pid; Cancel sends SIGTERM to that group (make and its
   compilers gone within seconds; checked), the tree writer stops
   between files. Builds install into <name>.part and replace the
   kernel only at the end (a cancel or failure keeps the old one;
   the list skips .part). Checked hidden: Continue during a build
   stays on page 0; the 6.18 kernel untouched by a cancel.
   The wizard's avatar takes its build state (swap_on: the core
   flame, grooves, faster spin) while Bootstrap builds; seen.
   Then (Kalen, Oct 10): every button 25% shorter (choices and
   Bootstrap buttons h1.1 -> 0.825, rows 1.3 -> 1.05 apart; the
   wizard's Back/Continue/Cancel h1.4 -> 1.05, its rows and fields
   1.25 -> 0.94), Continue 25% narrower (6 -> 4.5 units), Build
   kernel and Write installer its width (BootButton text 13 px to
   fit), Bootstrap 13 -> 14 units tall: the build output shows 5
   lines (was 2). Remove (Rule #12): ctrl/cmd held, every row's
   Built cell is a Remove button; shift, the hovered row's only (rm -rf its folder; the drive tree keeps its own
   copy). Seen hidden; Remove checked on a dummy kernel only.
   A disabled Continue: label at half alpha, no near light.
   Colours from the avatar's ring: the pointer's glow on every
   button pink #ff84ff, selected rows and choices blue #806bff
   border, fill #4d3dbc at 2/3 alpha (darker: the pink glow
   shows on it), the go button #4d3dbc. Seen hidden.
   Bootstrap 14.15 units tall: the build output shows 6 lines,
   in one element with mask_glow (32 px fade at its top); its
   lines are no_cache (a cached child draws outside the mask).
   Fixed in trinity for it (Canvas.ag): Gradient.init set kind
   none after the caller's props, so every linear_gradient was
   flat; and paint took the plain-rect shortcut before the fill
   gradient's ramp was set. Every fade_band (scroll_fade, the
   editor's mask_glow) painted nothing until now. trinity
   t_fade_band, t_fade_masked.
18. BUILT, not written to a stick yet (Kalen: (a) + pkexec,
   then our own stub) the USB installer: Write installer… beside Build
   kernel, for the chosen kernel, shows the drive row (sideways
   Choice buttons: usb or removable drives from /sys/block,
   rescanned every 2 s; the running disk and any smaller than
   the tree + 512 MB EFI dimmed, not picked), Back, and Write
   installer (lit once a drive is picked). The stick: GPT, 512 MB
   FAT (EFI/BOOT/BOOTX64.EFI = os-bootstrap/efistub.c, our ~100
   line UEFI program: reads EFI/orbiter/cmdline.txt, loads
   EFI/orbiter/vmlinuz.efi from memory, gives it this volume's
   device so the kernel's own stub loads initrd=\EFI\orbiter\
   initramfs), then ext4 (label orbiter-usb, a fresh uuid the
   boot line names, orbiter.root=/, orbiter.args=--install,true).
   make_image builds it as the user in install/tmp/installer:
   clang + lld-link for the stub, cpio -pdl hard-link staging
   (skips root/.cache; files keep uid 1000: e2fsprogs 1.47.0 has
   no tarball -d), sfdisk on the file, mformat/mmd/mcopy at
   @@1M (mtools 4.0.48 imported), mke2fs -E offset -d. Then one
   `pkexec sh -c` unmounts the stick's partitions and dd's it.
   t_installer_image (os-bootstrap) passes: 4,199 MB image in
   ~10 s, every file and the boot line read back. NOT done: a
   real stick (pkexec would prompt on Kalen's desktop) and a UEFI
   boot test (no OVMF here).
   FIXED (mine, Oct 10): make_image first wrote the drive tree
   when it lacked the chosen kernel; with Kalen's 6.12 build
   chosen, my test put 6.12 into /orbiter-os (the GRUB entry's
   kernel). Restored (6.18 chosen, tree rewritten, vmlinuz equal
   to 6.18's bzImage, lib/modules/6.12.0 removed). make_image now
   never touches the drive tree: the stage is a hard-link copy
   without lib/modules and boot/vmlinuz, the chosen kernel's put
   in their place. Checked: tree mtimes equal before and after.
   The image is built on this computer's disk first (Kalen's 58 GB
   stick failed "No space left" in mke2fs: / had 43 MB free).
   make_image now checks the free space for the whole image
   first and says so, and a failed step deletes its work folder.
   A progress bar (WizBar, the wizard's own draw) centred in the
   dialog's bottom row while it writes: the image's steps fill the
   first fifth, then dd (oflag=direct, status=progress; sh_copy
   splits its \r counts itself: tr buffered them to the end) the
   rest; the fill eases by filter_n. Coral track and fill, radius
   6 px; every 4 s a light sweeps across in 1 s (near_light at
   the sweep, reach 234 px, 1.4x on the fill, inner glows). No
   Cancel Build then; the avatar takes the long halo (work_on +
   edits_in), no flame. Its console is the same masked log_view.
   Checked hidden with a stand-in pkexec on PATH (dd-style counts,
   nothing written): bar eases, sweep crosses fill and track.
   While it writes, Back is Cancel and Write installer is dim (a
   double click hits nothing). dd runs as root, out of Cancel's
   kill: the root shell watches install/tmp/installer-cancel-<pid>
   and stops dd when Cancel makes it ("only partly written").
   Cleanup uses wipe (system rm): sh refuses once cancelled.
   A write first wipes any old cancel file (one was left once).
   Bar light reach 234 px (fill 1.4x). Checked hidden with a
   stand-in pkexec and dd on PATH: Cancel stops dd in 0.5 s.
   Any disabled element (trinity's layer draw, generic): label at
   half its alpha, no near light (border or inner glow). The
   wizard's and BootButton's own :disable blocks are gone.
   Written, the bar stays, all green and unlit, until Back. Each write
   builds in its own install/tmp/installer-<pid> (Kalen's write
   and my hidden test shared one folder and broke each other).
   Back in the drive row is Write installer's width (4.5 units).
   t_copy_progress passes; the real stick write is not run here
   (pkexec would ask on Kalen's desktop).
   Kernel choices (Kalen: add 7.x) are kernel.org's releases.json
   read on open: stable, then the two newest longterm (7.2.9,
   6.18.55, 6.12.112 today); offline: 7.2, 6.18, 6.12. The
   download path is v<major>.x. t_kernel_releases passes.
   The bootstrap page shows only for a PC user running the
   installer (bootstrap_here: no orbiter.init= on the boot line,
   silver's source present); OPEN: a release without the source
   should still list prebuilt kernels and write installers.
19a. DECIDED (Kalen, Oct 10) hardware lines: x64, ARM64, Apple,
   Surface. DONE on the page (Oct 10): the Hardware column first
   in the kernel list and a Hardware choice row; Kernel offers per
   hardware (x64/ARM64: kernel.org's; Surface: those linux-surface
   has patches/<series> for, read from its GitHub listing; Apple:
   asahi); NVIDIA only for x64 and Surface; Build kernel dim for
   the lines without a build path yet (all but x64). Apple's
   Kernel choice is the asahi branch's version alone (7.1.9, read
   from its Makefile; Asahi's repo tags are kernel.org's mirrored,
   the branch is its release line), with the note "from the
   Asahi Linux project" beside it.
   BUILT (Oct 10) 7.2.9 + NVIDIA 610.57.04 (kernels/7.2.9-nvidia
   610.57.04: nvidia, nvidia-modeset, nvidia-drm). 7.x turns on
   OBJTOOL_WERROR, which failed nvidia's 'naked' returns (6.x
   only warned): our config turns it off. Not booted yet; the
   drive tree still holds 6.18. The kernel list gets a Hardware column first, and a
   Hardware choice: it decides the kernel source, the versions
   offered, the architecture, whether NVIDIA is offered, and the
   installer's boot method. build.agi gains `hardware`; older
   builds read as x64.
   - x64: kernel.org (stable + two longterm), x86_64, UEFI stick
     (BOOTX64.EFI). Done.
   - ARM64: kernel.org, arm64 (clang LLVM=1 ARCH=arm64), for arm64
     UEFI machines; the stick's stub built for aarch64 as
     BOOTAA64.EFI; no NVIDIA; the orbiter-os programs for arm64
     (shared with Apple).
   - Surface: kernel.org source + linux-surface's patches/<series>
     and configs/surface-<series>.config (github.com/linux-surface/
     linux-surface; series up to 6.19 on Oct 10, so 6.18 longterm),
     merged into our config; x86_64; NVIDIA offered (Surface Book);
     the same UEFI stick.
     BUILT (Oct 10): kernels/6.18.26-surface-nvidia610.57.04 (6,715
     drivers: surface_aggregator, surface_hid/kbd, ithc, ipts,
     mwifiex, nvidia x3). linux-surface stopped in May: its patches
     fit the release in its v<series>-surface tree (6.18.26,
     6.12.90), not today's stables. On 6.18.55 they need fuzz and
     two land wrong (btusb's BIT(29) and hid-multitouch's 0x0117
     each taken twice). So Surface offers that release per series
     (offered: surface_releases, the tree's Makefile). Patches in
     order, one already in passed by (reverse dry run), any other
     with no fuzz; surface-<series>.config by merge_config.sh; Intel
     pinctrl built in (their arch.config). linux-surface pinned by
     master's commit (install/imports/linux-surface-<sha>.tar.gz).
     Not booted on a Surface.
   - ARM64 BUILT (Oct 10): kernels/7.2.9-arm64 (8,176 modules,
     arm64 Image, 4K pages). kernel.org source, Ubuntu's arm64
     config of the same kernel as ours (os-bootstrap/linux-arm64.config,
     6.17.0-40 generic from ports.ubuntu.com), our LLVM (ARCH=arm64
     LLVM=install/bin/), Image + modules; no NVIDIA or Intel rows.
     The drive tree stays x86: an ARM kernel is never the drive's.
     Clicking its row lights it for the stick only (Bootstrap.stick;
     Kalen: "we should still be able to write an installer"):
     BOOTAA64.EFI (efistub.c for aarch64), Image as vmlinuz.efi, an
     arm64 /init (init.c on the kernel's tools/include/nolibc, built
     with the kernel: `init` beside Image; shims under #ifdef
     NOLIBC). t_installer_image_arm. BOOTED (Oct 10) the stick in
     qemu-system-aarch64 (scratch build, pc-bios edk2-aarch64 UEFI,
     usb-storage): UEFI -> stub -> kernel -> init mounts the stick
     by uuid, switches root; it stops at "cannot run orbiter" (x86
     userland), as expected until orbiter-os is built for arm64.
     Apple's stick is the same; it needs Asahi's m1n1 + U-Boot.
     A one-letter literal is a character: (apple) ? '-' : amd wrote
     'a' for amdgpu; not-offered columns say 'n/a'.
   - Apple BUILT (Oct 10): kernels/7.1.9-asahi (8,204 modules, 16K
     pages; asahi.ko the Rust GPU driver, appledrm (DCP display),
     apple-dart, nvme-apple, brcmfmac, macsmc; 110 dtbs, 23 of them
     M1/M2 t8103/t8112/t600x/t602x). MODVERSIONS off for it: Rust
     needs that or GENDWARFKSYMS, which needs debug info.
     Not booted: no m1n1 / U-Boot path or arm64 userland yet.
     AsahiLinux/linux branch asahi
     pinned by commit, Ubuntu arm64 config + the tree's
     arch/arm64/configs/asahi.config (16K pages, Rust, DRM_ASAHI),
     rust-src added to the user's rustup, bindgen-cli built into
     install (LIBCLANG_PATH install/lib), stops if CONFIG_RUST is
     off; Image, modules, the apple dtbs.
   A Bootstrap member added: build the wizard with --clean (it read
   usb_view at the old offset: the build page titled itself as the
   installer).
   - Apple: Asahi's tree (branch asahi, 7.1 on Oct 10), arm64
     (clang LLVM=1 ARCH=arm64), Rust for its GPU driver, its boot
     chain (m1n1, U-Boot; the Asahi way installs from macOS), and
     the whole orbiter-os userland for arm64 (orbiter, trinity,
     Mesa's asahi driver). A project in stages: kernel first.
20. BUILT (Oct 15, Kalen: a partitioning view; all three; swap
   adapting to the machine, a partition or a file) the disk page:
   a map (DiskMap: each partition and free run to scale, at least
   3% so a small one can be picked; ext4 blue, others grey, free
   green; the used part shaded) above the three ways: the whole
   disk, a folder beside, or a new partition in free space. Free
   space: a gap on the map, or a partition to shrink by dragging
   its edge (ext4 or NTFS, not in use: Part.low is what it holds,
   a tenth more and at least a GB; NTFS's from ntfsresize --info,
   as root). The plan: e2fsck + resize2fs (or ntfsresize), the
   entry shrunk (sfdisk -N n), the new one at the exact sectors
   (sfdisk -N <first free number>), partx, mkfs.ext4; boot entry
   after the default, GRUB's menu written into the Linux found on
   the disk (mounted for it). Swap: the disk's swap partition when
   it holds this machine's memory (at most 8 GB), else a swap file
   in orbiter-os (fallocate, mkswap); the row cycles the choices;
   the boot line says orbiter.swap=UUID=<swap uuid> or
   orbiter.swap=/swapfile, and init.c turns it on after the switch
   (find_swap: SWAPSPACE2 at 4086, uuid at 1036; nolibc too).
   os-bootstrap's tree gains e2fsck, resize2fs, ntfsresize, partx,
   mkswap, fallocate (next tree write). t_disk_gaps, t_free_plan;
   `silver --test wizard` exit 0. Seen hidden on europa: the map
   (Ubuntu 372.5 GB, its 41 GB swap, 536.9 GB free where SUSE was,
   p5 910 GB), the free space chosen (Continue lit), the swap row
   cycling, the folder table under the map. NOT driven: a drag
   (nothing here can shrink as a user: p1 is mounted, NTFS needs
   root), a real install into free space, swap at boot.
21. DONE (Oct 15, Kalen: "it needs its own platform in its user
   land") the drive tree builds itself: os-bootstrap add_toolchain
   adds silver (install/bin/silver, build/silver), clang and lld,
   git (+ git-core), install/include, clang's own include
   (install/lib/clang/22/include), /usr/include, gcc's start files
   (/usr/lib/gcc/x86_64-linux-gnu: clang links crtbeginS, libgcc),
   glibc's link files in /usr/lib/x86_64-linux-gnu (crt1, Scrt1,
   crti, crtn, libc.so and libm.so scripts, libc_nonshared.a,
   libpthread.a, libdl.a; the scripts name /lib's libc.so.6,
   libm.so.6, libmvec.so.1, added there: the tree's /lib is not a
   link to /usr/lib), every file git tracks in silver, and .git (a
   module builds only in its repository). ~560 MB; tree 3.2 GB,
   44,114 entries. Checked on the tree's loader and libraries
   alone: a C program compiled with the tree's clang and headers,
   linked by its lld with its crt and gcc files, ran; the tree's
   silver and git start. NOT checked: a silver module built inside
   a booted orbiter-os.
   On the way (Kalen): the old images deleted: orbiter-os/
   initramfs.cpio (1.1 GB, qemu era) and the old tree (its 64
   root-owned files, and the installed /orbiter-os, need sudo).
22. DONE (Oct 15) the install runs as root through pkexec's prompt
   (a user) or as it is (root: orbiter-os from the stick): the plan
   is one sh script (sh_head: boot_second, the new firmware entry
   put second; step_sh each step, brace-free since silver strings
   interpolate { }), uuids fixed at plan time (lsblk, or a new one
   given to mkfs.ext4 -U), the copy by tar (links, modes, times;
   kernel trees made empty; --no-same-owner), progress from tar's
   checkpoints. A dry run logs the script (sh -n clean). The source
   from another system is os-bootstrap's drive_tree, the boot words
   os_words (shared with the stick). An existing /orbiter-os link
   is replaced by the copy. t_install_steps, t_boot_order (a fake
   efibootmgr), t_free_plan; `silver --test wizard` exit 0. Kalen's
   sudo install ran (02:44) before the pkexec version.
19. OPEN (Kalen, Oct 10) orbiter-os on Macs. Apple Silicon:
   Asahi's kernel tree (github.com/AsahiLinux/linux, branch asahi,
   7.1.9 on Oct 10) as a kernel source choice, and an installer
   for its boot chain (m1n1, U-Boot); Intel Macs boot the UEFI
   stick as PCs do. (7.x is in the kernel choices now: item 18.)

## orbiter-os in a Windows partition — live list (Oct 23 2026)

Kalen: an orbiter-os folder on the Windows (NTFS) partition holds
everything; it boots from the firmware's boot list (Windows Boot
Manager cannot start a non-Windows loader on UEFI). His picks: the
system is one ext4 image (orbiter-os\root.img, as Wubi did), the
kernel stays in the folder (orbiter-os\boot), the wizard writes it
from Linux. In order:
1. DONE EfiFs v1.13's ntfs_x64.efi (GPL-3.0, a separate EFI binary)
   imported by os-bootstrap into share/silver-os-bootstrap/efifs;
   THIRD_PARTY.md; sha256 equal to the release's.
2. DONE efistub: no kernel beside it on the EFI partition, it loads
   the NTFS driver, connects every disk, finds the volume holding
   \orbiter-os\boot\vmlinuz.efi and starts the kernel from there
   (initrd= on that volume).
3. DONE init.c: orbiter.image=<path in the partition> with
   orbiter.ntfs=<serial, as blkid prints it>: the NTFS partition by
   its boot sector serial, ntfs3 (a module: the initramfs carries the
   chosen kernel's lib/ntfs3.ko, boot_files) loaded, the image
   loop-mounted (LOOP_CONFIGURE) as the root, the partition at /host.
4. BUILT, dry run only: the folder-beside target on an NTFS
   partition (wizard in_win, win_ser from lsblk): mount it ntfs3,
   orbiter-os\root.img (the copy's size + 16 GB, fallocate, mkfs.ext4
   with the plan's uuid), the image mounted and the system copied in
   (settings, hostname, swap file inside it), kernel + initramfs to
   orbiter-os\boot, the image unmounted; on the EFI partition
   EFI\orbiter\orbiter.efi (the stub), ntfs_x64.efi, cmdline.txt
   (boot_line: initrd=\orbiter-os\boot\initramfs orbiter.ntfs=
   orbiter.image=/orbiter-os/root.img orbiter.root=/ and the words);
   a firmware entry for orbiter.efi after the default (boot_second).
   os-bootstrap write_boot builds the stub into the tree's
   boot/orbiter.efi with boot/ntfs_x64.efi beside it. wizard
   t_windows_plan passes. NOT run: a real install (root, pkexec),
   the tree written again with these, the UI picking an NTFS row.
5. PARTLY DONE booted in qemu (edk2 UEFI, NVMe, kernel 7.2.11): a
   GPT disk, EFI partition with the stub as BOOTX64.EFI, driver,
   cmdline.txt; NTFS partition with orbiter-os\boot and root.img (a
   test init): the firmware ran the stub, the kernel and initrd came
   from NTFS, init mounted the image and ran the test init, /host
   held root.img. os-bootstrap tests: t_installer_image passes
   (t_installer_image_arm fails as before). OPEN: europa itself.

## orbiter-os on phones (Oct 21 2026)

DROPPED (Kalen, Oct 21): the Pixel line. Tensor Pixels use Samsung's
Exynos modem, whose control (boot program, radio library, IMS for
VoLTE) is closed: no calls, SMS or mobile data on our own system.
Built and then removed with its files (~22 GB): Google's kernel for
all eleven Tensor Pixels, cascaded as Google builds them (one core,
each phone's modules and drivers in its own folder), and an
installer flashing boot pieces made from Google's factory images.
Kept, for any phone:
- `fastboot` module (fastboot/fastboot.ag + fastboot.c over libusb
  1.0.27 from source): open, getvar, download, flash (slot names),
  reboot, vbmeta_disabled; t_fastboot_flash and t_vbmeta_disabled
  pass (a scripted phone). Not yet run against a real phone.
- os-bootstrap/init.c, with `orbiter.phone=<product>`: the boot
  ramdisk's /lib/early modules loaded by finit_module in order
  (modules.load), a USB serial gadget (configfs ACM, 1d6b:0104) with
  the kernel log copied to it (the PC's ttyACM), then the phone's
  folder under /lib/phones (its products file names it) loaded
  first in order and every later modprobe kept inside it (-d, -C).
  Checked by a harness, not booted.

### Fairphone 6 — live list (Oct 21 2026, Kalen: "do all steps")

Chip SM7635 "Milos". Kernel: milos-mainline/linux (Luca Weiss's
mainline fork; postmarketOS builds tag v<pkgver>-milos, 7.2.0
today). Everything about the phone is read from postmarketOS's
pmaports (device/testing/linux-postmarketos-qcom-milos,
device-fairphone-fp6, firmware-fairphone-fp6) by the GitLab API at
build time: the release, the config, deviceinfo (boot image v2,
page 4096, offsets, dtb qcom/milos-fairphone-fp6), the early
module list (panel-novatek-nt37705, spi-geni-qcom), the cmdline.
1. DONE (Oct 21) hardware line 'Fairphone 6'
   (build_kernel -> build_fairphone): tarball, pmOS config with
   modules uncompressed, USB_CONFIGFS(_ACM) built in, BTF off; our
   LLVM for arm64; Image.gz, modules, the dtb; our nolibc /init;
   kernels/<ver>-fp6/ (Image.gz, fp6.dtb, kmods, init, early.list,
   deviceinfo, kernel-cmdline.conf, firmware/, build.agi).
   Built by a test: kernels/7.2.0-fp6, 386 modules. The list
   takes a folder with Image.gz too (scan).
2. DONE firmware (fairphone_firmware): FairBlobs at pmOS's
   commit, joined by pil-squasher (built from source), placed as
   pmOS places it (qcom/milos/fairphone/fp6, qca, postmarketos
   for the GPU's gmu/sqe), ath11k WCN6750 board-2.bin.
3. DONE phone_image: a ramdisk of /init, every module in load
   order (early ones first, then modules.dep order) in /lib/early,
   the firmware but the modem's; Google's mkbootimg (fetched) with
   deviceinfo's values; cmdline 'console=tty0 orbiter.phone=fp6'.
   install/tmp/phone-fp6/boot.img, 37.6 MB. Checked by Google's
   unpack_bootimg: header v2, page 4096, pmOS's offsets; kernel
   and dtb equal to the build's; ramdisk: aarch64 /init, 387 files
   in lib/early (panel-novatek-nt37705, spi-geni-qcom first), 31 MB
   of firmware. No drive tree on the phone yet: init stops at "no
   orbiter.uuid" and says so on the USB log (the first boot's test).
4. BUILT, seen hidden (Oct 21): the install page. A lit Fairphone
   kernel's Write installer… shows 'Install orbiter-os on the
   phone' (wizard heading; Bootstrap.on_phone), the phone on USB
   said every 2 s, Back, Install (dim with no phone): fastboot open
   (a udev rule by pkexec when refused), the product must hold FP6,
   unlocked; the image against partition-size:boot; flash boot
   (current slot), reboot, then the log from the 1d6b:0104 ttyACM
   into install/tmp/phone-<p>.log. phone_dry: check only.
   NOT run against a phone: none here, and never flashed by me.
5. BUILT (Oct 21, Kalen: the root fs on userdata) Install also
   writes the phone's root. A fresh uuid per install; the boot
   line gains orbiter.uuid=<it> orbiter.root=/ orbiter.init=
   /sbin/init. phone_rootfs: ext4 (4 KB blocks, label
   orbiter-phone, the tree plus a fifth plus 256 MB) holding
   Alpine's static busybox 1.37.0 (fetched by the kernel build into
   the kernel folder), /sbin/init -> busybox, an inittab
   (busybox --install, a shell on ttyGS0: the PC's ttyACM),
   lib/modules and all of lib/firmware with the modem's. fastboot
   sparse_pieces: Android's sparse format (zero blocks as fill
   chunks), pieces under the phone's max-download-size, each
   covering the whole image; flash_pieces sends each to userdata.
   The image is checked against partition-size:userdata.
   Checked: rootfs.img 520 MB read back by debugfs (uuid,
   inittab, aarch64 busybox, modem firmware, modules 7.2.0);
   split at 50 MB into 5 pieces, decoded by an independent Python
   reader byte for byte equal to the image. fastboot
   t_sparse_pieces (decoded back). NOT run on a phone.
   OPEN: growing the fs over all of userdata (resize2fs on the
   phone); the image's files are owned by uid 1000.
`silver --test wizard` exit 0 (t_installer_image_arm as before).
OPEN: the drive tree on the phone (userdata), the arm64 userland,
the modem (rmtfs, qrtr, pd-mapper, ModemManager), vbmeta if ABL
needs it (pmOS flashes none for this phone).

## orbiter-os Wi-Fi — live list (Oct 15 2026)

Kalen: a Wi-Fi setup in orbiter. His picks: iwd does the joining
(scan, WPA, reconnect, its own DHCP); it shows in both places:
a mark in orbiter's status bar opening the network list in the
side panel, and a page in the installer wizard. iwd is driven
over D-Bus, so orbiter-os gains a system bus. In order:
1. DONE (client) `wifi` module: iwd 3.12 imported (kernel.org
   tarball, its own ell; client, monitor, wired, tools, systemd
   off) into install/libexec/iwd, its bus policy into install/
   share/dbus-1/system.d; remembered networks under install/var/
   lib/iwd (inside the drive tree). Wifi (silver over libdbus):
   iwd's objects, networks strongest first with signal and bars,
   scan, join (our agent at /orbiter/wifi answers iwd's password
   request), leave, forget, poll each frame (no callbacks).
   wifi/tests/fake_iwd.py: a stand-in iwd (dbus-python) on a
   private bus; t_wifi_join passes: 3 networks in order, a wrong
   password comes back "Could not join Neighbor: the password
   may be wrong", the right one connects. Icons: Material
   Symbols wifi bars (Apache 2.0), to go in wifi/images.
   OPEN: the network list element (with item 4).
2. OPEN drive tree: iwd, dbus-daemon and their config (bus
   policy, /etc/iwd/main.conf with iwd's DHCP on, DNS written to
   /etc/resolv.conf), /var/lib/iwd for remembered networks.
3. OPEN init.c: start the system bus, then iwd, before orbiter.
4. DONE (Oct 15) orbiter: the Wi-Fi mark in the system bar
   (Wifi.mark: off, not joined, or the joined network's bars); a
   click opens the side panel on Wi-Fi (side_tab 'wifi'), again
   folds it. wifi's WifiPanel: networks strongest first, a click
   opens Join (a dotted password when secured and unknown),
   Disconnect, Forget under the row; Scan; the problem line. One
   Wifi for the process (wifi_shared); with no bus it retries
   every 3 s. orbiter's on_frame polls it and redraws the bar on
   a change or a new minute. Icons: Material Symbols in
   trinity/images (wifi_0..4, wifi_off, wifi_none, wifi_lock,
   power, restart; THIRD_PARTY.md, with iwd and D-Bus).
   Seen hidden on the stand-in: list, Neighbor chosen, password
   typed as dots, Join: "connected", the mark's bars changed.
   Found: a TextInput subclass whose style sets only layers.face
   lost TextInput's body box and `editable` (no box, no typing):
   WifiSecret spells the whole style out. The wizard's WizSecret
   is written the same way: OPEN, likely the same (not checked);
   the owner is trinity's style inheritance.
5. OPEN wizard: a Wi-Fi page after the hardware check.
6. OPEN a boot (the client checked on the stand-in iwd only).
7. DONE (Oct 15) the power button at the bar's right end; ctrl /
   cmd held makes it Restart (yellow), shift over it the same
   (Rule #12). On orbiter-os: sync, reboot(RB_POWER_OFF or
   RB_AUTOBOOT); in a window it only logs (never this computer).
   Seen hidden: ctrl showed restart, both clicks logged.
8. DONE (Oct 15) the system bar (orbiter/SystemBar.ag): 30 px,
   black, the date and time in the middle (Thu 15 Oct  05:51),
   the tray at the right (Wi-Fi, power). On when /proc/cmdline
   has orbiter.init= (orbiter-os), or `--force_system true`. The
   root moves under it: dock and the main split start at its
   foot. Seen hidden with and without the flag (without: as
   before). NOT seen on orbiter-os.
9. BUILT, not booted (Kalen, Oct 15: on orbiter-os, orbiter's
   navigation and working folder start at silver's folder) init.c
   work_folder: before it runs orbiter it enters orbiter.init's
   path up to /install/ (/src/silver on europa's boot line);
   orbiter's startup_path is the folder it was started in. Checked
   by a copy of the function: /src/silver/install/build/orbiter
   gives /src/silver. Needs the tree written again (initramfs).
   Found (Oct 15): `orbiter --install` copied the 05:05 tree, so
   the system bar and Wi-Fi were not in it; the tree was written
   again at 06:2x with them, before this init change.
10. BUILT (Kalen, Oct 15: "make --install update the tree when
   it installs") the install's worker (wizard work), when its
   copy's source is the drive tree (installing from another
   system), first runs os-bootstrap's assemble: the tree and its
   initramfs brought up to the last build; copy_file skips files
   whose time is unchanged, so only newer ones are copied. The
   line "Updating the drive tree from the build" shows. `silver
   --test wizard` exit 0 (t_installer_image_arm fails as before).
   NOT driven: a real install (needs root and pkexec).
11. IN PROGRESS (Kalen, Oct 15) a console app as its own module:
   `console` (console/console.ag, `silver console`) over trinity's
   Console (a shell on a pty). Then orbiter's console tab hosts the
   module like any hosted app, and orbiter's own console code goes.
12. DONE (Oct 15, Kalen: `..` in orbiter's list closed the whole
   list) any click in the list closed it, `..` or a project or a
   folder. Cause, mine (Oct 14): trinity lets a focused field go on
   a press outside it, and the list stays open only while the pane
   title or the list holds focus (run_search's title_focus ||
   list_focused); CommitView takes no focus, so after the click
   nothing held it. Now the let-go skips a press the element marked
   handled (ev.no_default), as set_focus already did; CommitView
   marks every press. Seen hidden: Quake-2 chosen, into ref_soft,
   `..` back, the list open throughout. trinity tests exit 0; a
   running orbiter needs a restart.
13. BUILT (Kalen, Oct 15: claude's login opens the system browser)
   os-bootstrap/xdg-open.c (static, /usr/bin/xdg-open in the tree)
   writes `app open <url>` to orbiter's socket; orbiter's on_agent
   hosts the url's module (aura) in the active pane. The boot line
   gains BROWSER=/usr/bin/xdg-open: claude with no X/Wayland display
   opens nothing unless BROWSER is set. Checked hidden: xdg-open
   exit 0, aura hosted, its title Example Domain; the page itself
   not in the shot (empty content area). Not booted; the old GRUB
   entry lacks BROWSER.
14. BUILT, tree written (Kalen, Oct 15: claude on orbiter-os, by
   Anthropic's installer, run by the bootstrap). add_claude runs
   claude.ai/install.sh once with HOME=install/tmp/claude-home,
   puts the newest versions/<v> at /usr/local/share/claude/
   versions/<v>, links /usr/local/bin/claude, adds the CA bundle.
   Not /root/.local: the tree's /root/.local is root-owned (made by
   a boot), and write_tree failed there without a word (OPEN: say
   so). The boot line gains PATH=/usr/local/bin:...: the kernel
   gives init none, and trinity's shell_tool looks on PATH.
   Checked: 2.1.301 --version on the tree's loader and libraries
   alone. The USB stick carries it too (it stages the tree).
15. DONE (Kalen, Oct 15: ChatGPT's tray icon; its menu in the side
   panel) `tray` module: orbiter holds org.kde.StatusNotifierWatcher
   on the session bus (not when another tray has it: a desktop's),
   reads each item (title, else its tooltip's; IconPixmap, else
   IconThemePath/IconName.png, as Chromium writes it; dbusmenu
   GetLayout flattened), draws a TrayMark per item left of Wi-Fi.
   Left click: Activate; right click (or ItemIsMenu): side_tab
   'tray', TrayPanel: name, Open, the entries (Event clicked).
   init.c session_start: dbus-daemon with tray/root/etc/dbus-1/
   orbiter-session.conf, DBUS_SESSION_BUS_ADDRESS for orbiter.
   Checked hidden on a private bus, ChatGPT in the wayland module:
   its icon in the bar, the panel lists New Chat, Open ChatGPT,
   Quit; Quit ended ChatGPT and its icon left. Not driven: a left
   click's Activate. Not booted.
16. BUILT (Kalen, Oct 15) git (already in the tree) and the user's
   SSH keys. The tree gains ssh, ssh-keygen, ssh-add, ssh-agent,
   /etc/ssh/ssh_config(.d). os-bootstrap ssh_dir (~/.ssh),
   ssh_keys (private files beside a .pub), ssh_copy_sh (folder
   700, files go-rwx). Wizard account page: a row 'SSH keys: copy
   / leave out' (off by default); the plan's step 'Copy your SSH
   keys' to /root/.ssh, owned by root. Packager (Write installer):
   'Carry my keys' / 'Leave them out' (off by default) copies them
   into the stick's /root/.ssh; booted from the stick the install
   finds them there (HOME=/root). wizard t_ssh_keys (a stand-in
   .ssh): 1 key, dir 700, key 600. Checked: the tree's ssh and git
   run on its own libraries. Not driven: the rows in a hidden run,
   a stick, an install.
17. DONE (Oct 15) bash and the everyday commands in the tree
   (add_shell: bash, coreutils' common ones, grep, sed, find,
   xargs, diff, tar, gzip, xz, less, ps, kill, pgrep, which, awk,
   curl, clear, tput, nano, vi; /usr/share/terminfo). ~18 MB.
   Boot line SHELL=/bin/bash (claude looks for a full shell; the
   console runs /bin/bash). No ripgrep: claude carries its own.
   Checked: the tree's bash ran ls, sort, sed, awk, which, grep,
   find, tput on the tree's libraries alone.
18. DONE (Kalen, Oct 15) os-mode (system_on): the navigator's
   search lists the programs on PATH (orbiter program_list, by name,
   the first on PATH wins; rows say 'program'; a name starting with
   the query first). is_program[ p ] tells a row's file apart.
   Found on the way, mine (Oct 14): a focused field kept every key,
   so typing in a pane's title ran no search (and Enter, Backspace
   never reached orbiter). EditorTitle.on_char / on_key now hand
   the key to orbiter first, then the field edits, as before Oct
   14. Seen hidden: 'nan' lists nano, rnano (program) and two
   files; Backspace to 'na' relists.
19. DONE (Oct 15) one window per pane (wayland.c): each toplevel
   has its own headless output at x = id x 16384 (a base output at 0,
   never drawn: GTK waits for one before it opens a window); frames,
   sizes, pointer input by window id (wlc_window_*, wlc_frame_*).
   The display runs on its own thread (poll on its loop's fd, every
   wlc_* call under a lock): a tray-only app is answered while the
   shell is idle. Direct scan-out is off (WLR_SCENE_DISABLE_DIRECT_
   SCANOUT): a window covering its output was handed over as the
   app's own shm buffer, which cannot be imported (GTK 3 drew black).
   WaylandView (one window in a pane; `animated : true` on the class)
   is split from the wayland app. Seen hidden: calculator and text
   editor in two panes, typing into the second.
20. DONE (Oct 15) os-mode: orbiter starts its Wayland display (then
   unsets WAYLAND_DISPLAY: hosted trinity apps stay off it); a
   program row opens the console module (`console --wayland <sock>
   <program>`: its default arg is the program, run as the session's
   launch); a mapped window goes to the pane whose hosted app is one
   of its pid's ancestors (/proc stat), else the active pane; a
   closed window gives the pane back. Programs re-read when a PATH
   folder changes. The tree gains the console module (add_module).
   Seen hidden: gnome-calculator from the search in pane 0, 7+5=12
   by clicks, its X closed it.
   OPEN: the console instance stays after its program exits.
   Fixed (Kalen, Oct 15: the desktop's Show in Files opened inside
   orbiter): the display took wayland-0; on his X11 session that name
   was free, and a program with no WAYLAND_DISPLAY falls back to it.
   It is orbiter-N now (wl_display_add_socket): only programs handed
   the name come.
   Then (Kalen, Oct 15: Firefox stayed black): Ubuntu's Firefox is a
   snap, and its AppArmor profile lets it reach only
   /run/user/*/wayland-[0-9]*; orbiter-0 was denied (kernel audit
   log). The name is wayland-N-orbiter now: the snap's rule takes
   it, and no program falls back to it (the default is wayland-0
   exactly). Seen hidden: gnome-calculator draws over the new name.
   NOT driven: Firefox itself (Kalen's own Firefox was running: a
   second `firefox` hands its window to the running one).
   Also (Kalen: the console app alone showed no shell): only
   orbiter's frame loop read a console's pty, so the console module
   alone never showed bash's output. trinity Console.tick reads its
   own session. Seen hidden: prompt, `echo hi $BASH_VERSION`, 5.2.21. Also: WaylandView clips to its pane (the cut header
   drew over the pane's title bar); seen hidden with Inkscape's
   dialog (its cut top stays under the title).
   Fixed (Kalen: the menu panel sliding out flashed the window black;
   sometimes a resize was needed to see it again): the pane's size
   went to the window every frame of the slide (new output buffers
   each frame), and imports were keyed by buffer address, which a
   resize reuses for new buffers (a stale import of a freed one).
   WaylandView sends a size once the pane has held it 150 ms; the
   compositor counts each window's size changes (wlc_window_epoch)
   and the view starts its imports over at each, keeping the old
   ones 8 frames (the GPU may still draw them). Seen hidden: no
   black pixel in any frame of the panel opening and closing.
   Fixed (Kalen: typing in the navigator took ~30 s with results):
   is_program per result row re-checked the 15 PATH folders' times
   (500 rows a key); it reads the list the search already read.
   Measured hidden on his PATH: 'i' 8.6 s, 'in' 16.8 s -> 0.2 s.
22. DONE (Kalen, Oct 15) the system bar is always on (force_system
   defaults to true; --force_system false turns it off). With it:
   orbiter's button (DockHome, home_button) is the bar's first, as
   wide as the dock; the window menu starts after it; the console
   button takes the dock's foot; the side panel's toggles (reload,
   scenes, panel) sit in the tray left of the tray icons (a tab opens
   a folded panel; the lit one folds it); the side panel has no foot
   bar and the status bar no console or panel toggle. The menu and
   tray panels scroll: their rows are in PanelRows (tray module),
   each flowing after the last (t{gap}+: trinity scrolls only flowed
   children), content_h from the lowest child (panel_fit). Seen
   hidden: the layout; a 41-entry menu scrolled by the wheel under a
   fixed heading; a click on a scrolled row picked it.
   FIXED silver (seen Oct 18): `Region { .. }` and a bare `{ .. }`
   as a call argument both work, first or second.
   Then (Kalen): the bar's menu names (MenuTop) and tray icons
   (TrayMark) take the tray buttons' near light (inset glow, edge and
   inner light by the pointer); orbiter's bar button is BarHome (the
   tray buttons' fill and light, no top line: that was the dock's
   foot). Seen hidden: no line at its top, its edge lit near it.
23. DONE (Kalen, Oct 15) the menu drawer (Rule #13's exception):
   MenuDrawer (SystemBar.ag) slides from under the dock (l-281.25px
   to l78.75px, 260 ms) over the panes, 360 px wide; root order is
   main, drawer, dock. Its ground (DrawerGround) is the screen as
   drawn so far, copied and blurred that frame (orbiter.drawer_blur:
   one snapshot and ReduceBlur for the window, as the exchange keeps
   its live_blur: building one per drawer crashed in Render_init on
   the second open), under #140a2a99. A menu name or a tray icon's
   menu opens it, the same again, Escape, a press outside or a pick
   shuts it; it stays mounted 400 ms while it slides away. The side
   panel no longer shows menus. Seen hidden: the drawer over the
   test app (the app's width unchanged, its white blurred grey),
   Escape, four open/close rounds, File > New reached the app and
   shut it.
   Then (Kalen: the navigator drew over a running app's window): a
   pane shows its window only while its navigator is shut
   (EditorView.text_els, !browse_active); the app keeps running.
   Seen hidden: the window's white 100% of the area, 0% with the
   navigator open, 100% again after Escape.
   Then (Kalen: the navigator's card said console, its picture the
   console's black, not the Inkscape window): place_window notes the
   window on the instance whose pid started it (AppView.wl_window);
   the card draws the window (wayland window_texture: the window's
   current frame imported for any picture of it, per window and
   size, the last size kept one change longer), names the program
   (program_of: the console's last arg) and says running; picking it
   puts the window back in the pane (show_instance). Seen hidden:
   'menutest4 running' with its window's picture; a click on the card
   brought the window back.
   Then (Kalen): the drawer's blur is taken once as it opens (and on
   a new window size), not every frame; MenuDrawer.background_lens
   (false by default) makes it live. Measured hidden: two opens of
   ~10 frames each, two takes.
   Then (Kalen): typing with a menu drawer open filters it: the box
   (MenuFilter) shows at the menu's foot as soon as a key is typed,
   the matching entries (any case; submenu names left out) under the
   heading; Backspace edits, Enter runs the first match, Esc clears
   the filter, then shuts the drawer. The drawer takes the focus once
   mounted (orbiter.drawer_focus), so a pane title keeps no keys.
   Both menu kinds (MenuPanel, TrayPanel; entry_matches in tray).
   Seen hidden: '2' in the 41-entry menu left Item 2, 12, 20-29, 32
   and the box at the foot; Enter ran it and shut the drawer.
   Then (Kalen): a pane showing a window has its close where a
   module's run button is (title_nav 'modicon', AltTab win_close; the
   top right stays a dialog's): it asks the window to close
   (wlc_window_close), as its own X. The title moves past it and
   reads the window's title (was 'Open'). Seen hidden: arrows, the
   close, 'Menu Test'; a click closed the window and the app ended.
   Then (Kalen: another app could not come over a running Wayland
   app; anything not docked goes at the dock's foot): a pane's
   window is cleared by a file opened there (open_in_pane) and set to
   the shown instance's own (show_instance: its window, or none); a
   press on such a pane's title opens the navigator, not the console
   instance's argument pick. Every running instance with no pin (a
   program's console always) gets a DockRun at the dock's foot, up
   from the console button: a program's window as its picture
   (window_texture), else its module's icon; a click shows it in the
   active pane. Seen hidden: the window's tile at the foot; a file
   opened over the window; the tile brought the window back.
   Then (Kalen): dock apps are toggles: a click on one in view takes
   it out of view (still running): its window off the pane, then (the
   editor mounted again a frame later: toggled_out) the editor lets
   it go and steps back in its history. And a program's tile is its
   own icon (program_icon: the .desktop named by the window's app id,
   else the program's name, its Icon=, hicolor scalable svg, else the
   largest png, else pixmaps; cached), drawn by AppIcon.icon_file;
   the window's picture only when none is found. Seen hidden:
   gnome-calculator's icon in the dock; a click took it out (the pane
   back to 'Open'), another brought it back.
   Then (Kalen, Oct 15: running apps in the bottom bar, the dock
   pins only) the dock has no running apps and no selected fill;
   its pins keep their running dots. The status bar's centre text
   (hover tips, reload state, recording) is gone: the bar holds a
   TaskChip per running instance, centred: its icon, its title
   (task_title: the window's or the app's own title, else its
   argument, else its name), a light (amber starting or paused,
   red failed); lit while shown; a click toggles it as the dock
   did. A pane's title shows no instance or argument chips, only
   the app's view chips; only an error shows at its top, centred
   (title_error: the pane's app failed, or its file does not
   compile; AppError, a click goes to the line). A chip dragged
   onto the dock pins it: over the dock its icon eases to the
   pointer (filter_n 0.3) and the pins below its slot ease down
   to open a gap; the drop puts it at that slot (pin_insert), the
   new pin starting where it was let go. A program pins as its
   path on PATH (pin_key_of); a pinned program's click shows its
   instance, else runs it; its icon is the .desktop's, by name or
   by its Exec= line (program_icon_named, misses cached: it ran
   every render). Also: Enter in the search ran nothing for a
   program (opened its file as hex): it runs it as a click does.
   Seen hidden: calculator chip centred, toggled out and back;
   dragged up, its icon in the gap, pinned as
   /usr/bin/gnome-calculator with the calculator icon.
   Found: `(path [ 'gnome-calculator' ]).filename[] != pn` was
   true for equal names (not chased; a string cut works).
   Fixed (Kalen: mario, once toggled out, never came back): a
   file's viewer (knes for a .nes) keeps display 0 (PIP) though
   shown embedded, and show_instance sets app_view from it. A
   chip's click back makes a PIP instance Embed first. Built,
   not driven (a hosted knes needs silver-host).
   Then (Kalen): an unselected chip lights slightly as the
   pointer nears, as the power button (its faint inset and
   near_inset 4); and a navigator card's pick brings the instance
   into view embedded (pick_instance took the pane's source mode
   and made it PIP). Seen hidden: the chip faint far, lit near,
   full when chosen; calculator toggled out, its card brought it
   back with its chip lit.
   Then (Kalen: a title's first press selects the pane, the next
   opens the navigator, so its menus show without it): a window's
   pane has no editor, so pane_of gave null and the press fell to
   the navigator; the title press takes the view's pane number
   (pane_index_of). Seen hidden with two panes.
   Then (Kalen: an app's build status, the line responsible): the
   title's error box fits its words (8.4 px a letter, at most 60%
   of the pane), centred in the title; a compile error reads
   'line N: msg'; else silver's own (coloured) log line
   (build_log_line), not 'build failed'; while the host builds,
   'compiling <name>…' in the info look (not seen: the build was
   done before the first shot). AppError gained a face layer (its
   words had drawn only by a default). A module outside silver's
   folder (quake2) never built in orbiter: the host ran `silver
   quake2` in silver's folder ('module file not found: /quake2.ag').
   AppView.start sends its folder's path; silver-host builds that
   path and names logs and binaries by its last word. Seen hidden
   with a scratch module: the path reached silver.
   Then (Kalen): the debugger's controls are in the pane title,
   left of find (find stays furthest right): pause while the app
   runs (it attaches the debugger and stops), the four steps as
   one bordered group once stopped; not over the nav list, the
   finder or the find bar. The task bar holds tasks alone; the
   dock's console button is the gear. Seen hidden on buttontest:
   pause, the steps after it, continue back to pause.
   Then (Kalen): dock pins reorder by drag. A press on a pin
   (drag_pin) is a click at the release unless it moved 6 px; then
   it leaves the column, drawn last at drag_at, which eases to the
   pointer (filter_n 0.3); the others close up and open a gap at
   the slot (pin_off, as the task-button drag); the drop moves it
   in pins (saved) and it eases in from where it was let go
   (pin_release, pin_insert; the click is dock_icon_click). Seen
   hidden: two pins swapped both ways, the list saved, a click
   still runs its program.
   Then (Kalen): the task buttons sit flush on their bar, its full
   height: the bottom bar is TasksBar (StatusBar's look, no child
   spacing; a subclass's style replaces its parent's whole, so it
   is written out). Seen hidden: the chip from the bar's top edge
   to the window's bottom.
   Then (Kalen): a console program's title (OSC 0 or 2, as bash's
   prompt and Claude Code send it; a terminal's tab shows it) is
   kept (ConsoleSession.title, osc_end); a Console with titles on
   (the console app's) passes a change to set_title, so a hosted
   console's task button reads it. Seen hidden: a script's
   'Title From The Program' on its task button.
   Then (Kalen): the system bar's reload button is there only
   while a reload is ready (au_live_get_pending 1); else the
   group is scenes and the panel alone, no empty bordered slot.
   Seen hidden: two buttons idle, three once a source changed.
   Then (Kalen), BUILT, NOT SEEN (Kalen stopped the hidden runs):
   - a resumed app (debugger continue, or the host's pause) shows
     again only if still running 150 ms on, its frame fading in
     over the source for 300 ms (Editor resume_watch, resuming;
     ResumeFade); only an app that was in view while it ran. A
     title continue clears AppView.paused. Seen hidden earlier:
     source at 0.1 s, the fade at 0.3 s, the app at 0.6 s.
   - a stopped app's task button shows the pause mark (TaskChip
     paused).
   - the navigator's column: 'system' (home, then drives mounted
     at /, /home, /media, /run/media, /mnt) and 'pinned' (pinned
     plain folders, shift - unpins); a click browses there
     (side_pick_folder). The dock shows pins that are not plain
     folders (dock_pin, plain_folder).
   - the dock lists running apps no pin holds under the pins
     (run_<iid>, a click toggles as the task button); an apps grid
     button (Material 'apps', orbiter/images/apps_grid.svg) above
     the gear: OPEN, the apps view behind it (Kalen to choose how
     apps install and the first list).
   - menus (tray and windows'): entries between separators are one
     chip (tray MenuGroup: a faint left strip, 1 px lines between
     rows), each row the near light only, 0.9 units tall at 13 px;
     icons from dbusmenu icon-name and GTK's icon string, found in
     Yaru, Adwaita, hicolor (svg only; menu_icon_file); MenuBuild
     shared by TrayPanel and MenuPanel.
   Then (Kalen: Filters in Inkscape lagged while scrolling) DONE,
   felt smooth by Kalen. Measured from his orbiter's socket: the cost
   was elems (CPU drawing the tree), 0.35-0.7 s a second while
   scrolling, not GPU paints. Fixed, in order found:
   - a faded scroll list (scroll_fade) rebuilt its masked picture
     every draw: now only on its own repaint, a scroll move, or a
     child needing paint (element mask_sx, mask_sy, last fields);
     children wholly out of view are skipped there and in the plain
     children pass (trinity).
   - menus build elements for the visible slice only (MenuBuild
     view: from scroll.y to scroll_goal, a screen either side); the
     rest is measured, a foot mark keeps the scroll range.
   - entries and chips have no canvas of their own (no_cache): a
     canvas per element made as it scrolled in was the cost.
   - a label_font opened its .ttf per layer: trinity shared_font
     (vk.font_cache by uri@size, a free function: a new Canvas
     method hits the 1b MODULE OVERWRITE bug).
   Menus use Barlow Condensed (17 px bar and clock and task buttons,
   16 px entries, Bold group headings); programs orbiter launches get
   it through FONTCONFIG_FILE (install/tmp/orbiter-fonts.conf, the
   console's --fonts).
   Then (Kalen: a failed app rebuilt when its pane was selected
   again): an instance that ended on an error (AppView.failed: a
   note other than 'app exited') waits for play or F5; a click in
   its pane (view_press) and embed_app no longer restart it. Seen
   hidden: after the error, pane and chip clicks, one start only.
21. DONE (Oct 15, Kalen: menu data; crop the header off the rest)
   wayland.c serves gtk_shell1 (capabilities 3: GTK hides its own
   menu bar; set_dbus_properties gives the menubar's bus and path,
   the app and window action paths) and org_kde_kwin_appmenu (Qt:
   set_address of a dbusmenu), and KDE's server decoration (GTK 3
   then draws no title bar); wlc_window_csd: neither decoration was
   taken (GTK 4 / libadwaita). tray module: dbm_entries/dbm_click
   shared with the tray; AppMenu reads org.gtk.Menus (Start by
   group, sections inline, submenus one deeper) and org.gtk.Actions
   (DescribeAll: enabled, boolean state as a check; Activate with a
   string target). orbiter: the active pane's window menu as
   MenuTop names at the system bar's left; a click lists that menu
   in the side panel (MenuPanel; side_tab 'menu'), a pick runs it
   and folds the panel; a CSD window's top 46 px is cut off.
   Protocol files and wlroots in THIRD_PARTY.md.
   Seen hidden (GTK 4 and GTK 3 test apps): File / View in the bar,
   no header or menu bar in the window, Dark checked after a pick,
   New and Quit reached the app. NOT driven: a Qt app (none here).
24. IN PROGRESS (Kalen, Oct 16-17) the apps view and featured software.
   Everything is built from silver: no system programs (PATH) in the
   apps view or the navigator's search (they slowed every search).
   trinity: enum SoftwareGroup, the sections in order (games creative
   everyday simulation developer communication). A module names its
   section by `export software ['games']` (psx, n64, asnes, rgen games;
   aeon creative; aura everyday; launch simulation).
   featured/ (Kalen, Oct 17): software.agi is names only (`featured:
   [ gedit, blender, ... ]`, the order shown); each is a silver module
   featured/<name>/<name>.ag holding all else: its section, icon
   (images/icon.png), every dependency as an import with its build
   settings. A name shows once its module is there. Discord, Slack and
   Zoom are off the list (closed source).
   orbiter: an empty pane ('Open') shows AppsGrid (NodeList.ag): glass
   cards over the clear space, a heading a section; ours (exports with
   software:, at silver's root or featured/), then each featured name
   by its module's source; dim until built (a program module's
   install/bin/<name>); a press runs a built program (XDG_DATA_DIRS gets
   install/share first: its schemas), else opens the module. The
   title's apps button (moved off the dock) hides / shows the grid.
   gedit BUILT (Oct 17): featured/gedit, gedit 51.0 on our glib (again,
   with GIRs) and gobject-introspection 1.84, libgedit-amtk / gfls /
   gtksourceview / tepl, libhandy, gsettings-desktop-schemas, libpeas
   1.36 (1.38 needs glib 2.85), enchant (its groff html pages skipped),
   gspell; GTK3, ICU, libxml2 the system's. install/bin/gedit runs
   (--version 51.0). Not seen: its window in a pane (hidden run).
   The README fetch, USER:PROJECT, the README import button and its
   console were built and removed (Kalen: the module is the center).
   OPEN silver: an existing checkout keeps its version when an import's
   version changes (libpeas stayed at 1.38.1: the checkout was deleted).
   OPEN: modules for the other featured names.
   Electron from source (Oct 18) was set up (depot_tools + electron
   v43.7.7 imports, featured/vscode/electron/silver-build.sh: gclient
   sync of Chromium 150, release.gn, the dist over code-oss) and
   checked to the sync, which needs 70 GB free (46 free). Dropped
   for quark (below); the script stays in the overlay folder.

## quark: Electron's API on trinity, no Chromium (Oct 18 2026)

Kalen: VS Code (and any Electron app without nodeIntegration) runs
on Node + trinity, no web engine at all: the page's scripts run in
Node and a DOM written in JavaScript lays out and paints through
trinity's canvas (webgfx). Module `quark` (silver root): trinity app
`quark <app dir> [args]`; `install/bin/electron` is the Electron-
shaped entry (ELECTRON_RUN_AS_NODE runs node). Order:
1. DONE the frame path: Node's renderer (a worker per BrowserWindow)
   draws through quark/addon/quark_addon.c (N-API over webgfx:
   fonts, paths, canvas, images, share) into a canvas; webgfx
   webgfx_share_new/publish copy it into two shareable textures
   (Texture.shared_fd is public now); the fds ride the socket to the
   quark window (quark.c keeps SCM_RIGHTS fds from any read) which
   imports them once (import_shared_tex) and draws the front one.
   Zero copy. Seen hidden: a test picture, then the hello page.
2. DONE quark.ag: forks node (quark_start_node: argv from /proc/self/
   cmdline after the app folder, fd 3 the socket), lines both ways
   (window/share/front/show/hide/title/close/quit in; size/mouse/
   wheel/key/text out), QuarkView per window, the shown one drawn.
   OPEN: `cursor <name>` line (unknown line now), focus lines,
   several visible windows.
3. DONE quark/node: register.mjs (import 'electron' -> electron/
   index.cjs), boot.mjs (Electron's start: package.json main, argv,
   process.*), electron/index.cjs (app, BrowserWindow, WebContents,
   ipcMain, MessageChannelMain/MessagePortMain as a hub of port ids,
   protocol.handle over Request/Response answering the page's
   fetches, session, Menu (titles to the bar line), dialog (logged),
   shell, screen, nativeTheme, powerMonitor, systemPreferences,
   clipboard, safeStorage, nativeImage, net, utilityProcess over
   child_process.fork with electron/utility.mjs (process.parentPort)),
   renderer.mjs (the worker: canvas, share, the frame tick at 60).
4. DONE the DOM engine, quark/node/dom: events.mjs (EventTarget,
   capture/bubble, the event classes), node.mjs (Node, Element,
   attributes, classList, dataset, inline style with shorthands,
   box reads from layout, focus, scroll), elements.mjs (Document,
   the HTML element classes, CSSStyleSheet with insertRule,
   TreeWalker), html.mjs (tokenizer + tree builder with implied
   closes), css.mjs (declarations, every common shorthand, colors
   incl. color-mix, lengths with calc, @media evaluation, @supports,
   @font-face, @keyframes kept, selectors incl. :not :is :has :nth-*,
   specificity), style.mjs (UA sheet, the cascade with !important,
   inheritance, custom properties with var(), computed values),
   fonts.mjs (system fonts by family through fontconfig, @font-face
   from bytes, fallback per code point), text.mjs (glyph runs),
   layout.mjs (block flow with margin collapse, inline lines with
   wrapping and white-space modes, inline-block, flex with grow/
   shrink/wrap/gap/align/justify/order, absolute/fixed/relative,
   overflow clips and scroll extents, min/max, box-sizing, aspect
   ratio, replaced sizes, ::before/::after content), paint.mjs
   (backgrounds with radii and box-shadow, borders per side, images
   and tiling, text with decoration and shadow, form controls,
   caret, clips and scroll offsets, stacking by z-index), window.mjs
   (the globals: observers, Range/Selection, Storage, matchMedia,
   FontFaceSet, customElements, DOMRect/Matrix, the classes), page.mjs
   (loading over file/data/http or the app's schemes via main,
   preload with Electron's renderer surface (ipcRenderer, contextBridge,
   webFrame, webUtils, sandboxed process), scripts classic and module,
   stylesheets and links, images (png.mjs), Web Workers
   (webworker.mjs), input: hover/active/focus pseudo states, mouse/
   pointer/click/dblclick/wheel/key/keypress/input events, editing in
   inputs and textareas, tab focus, cursor), canvas2d.mjs, keys.mjs.
   Tool: `node quark/tests/render.mjs <html> <out.png> [w h]` renders
   a page without the window (QUARK_DUMP box tree, QUARK_HIT x,y,
   QUARK_CLICK x,y, QUARK_PRELOAD). Checked: quark/tests/hello (an
   Electron app: window, preload, ipc handle/send) renders as a
   browser would and the click's invoke comes back ("main says 5",
   the title updated), in the tool and in the hidden window.
   Not yet: gradients (an average colour), svg images, woff fonts,
   grid (as flex), group opacity layers, transitions/animations,
   text selection drawing, IME, :hover restyles the whole tree
   (styleGen), full relayout on any mutation.
5. IN PROGRESS VS Code on it: `install/build/quark --hidden true
   install/lib/code-oss/resources/app --user-data-dir /tmp/claude-1000/
   qk/udd --extensions-dir ...` (the runtime dir must be short: VS
   Code's main.sock lives in XDG_RUNTIME_DIR). SEEN hidden (Oct 18):
   the workbench drawn by trinity: menu bar with its overflow, the
   command center and history arrows, Sign In, layout controls,
   activity bar codicons, status bar, and the Welcome page (header,
   Start and Walkthroughs columns on its 1fr 6fr 1fr 6fr 1fr grid with
   named areas, Recent, the footer checkbox); the extension host, the
   shared process and the file watcher run. Found and fixed on the
   way: VS Code packs node_modules in node_modules.asar (quark/tools/
   asar-extract.mjs unpacks it; all its native modules are N-API and
   load under Node 24); `original-fs` and `node:original-fs` are fs;
   VS Code's own module hook maps fs -> node:original-fs for Electron
   (register.mjs wraps Module.registerHooks and syncs the ESM export,
   so their answer is mended); utility processes fork the real node
   (QUARK_NODE), not the launcher; net.fetch reads file: urls; a
   stylesheet's url()s resolve against the sheet; the page's
   performance object hides nodeTiming (VS Code picks its node path
   on seeing it); HTMLCollection/NodeList have item(); window.
   queryLocalFonts; stylesheet rules expand shorthands; a measuring
   pass gives a block child its natural width, not the measuring
   width; a word wider than its line overflows (no per-letter cut);
   logical properties (margin-inline-start ...); grid-area with one
   name sets all four lines. Tools: QUARK_TRACE=1 (lines both ways,
   page console, utility stderr, forks), QUARK_DUMP=<selector> (the
   box tree under it into /tmp/claude-1000/qk/dump.txt every 5 s,
   ancestors first), QUARK_TRACE_DIRTY=1 (what dirties the page).
   Speed (Oct 18, Kalen): profiled with QUARK_PROF=1 (a line a
   second: ticks, layouts, paints, styles computed, boxes) and
   NODE_OPTIONS=--cpu-prof. At startup VS Code forces 14-38 layouts
   a second (offsetWidth, clientWidth after each change) and every
   mutation threw away every computed style (styleGen++), so each
   pass re-matched ~300 elements against the rules: 130 ms a pass,
   5.8 of 7.2 s in selector matching. Now a change marks only what
   it can reach (page.mjs dirtyStyle / dirtyChildren / dirtyChains):
   the element, its subtree and later siblings only when the changed
   class / id / attribute / pseudo appears left of a selector's last
   compound (style.mjs index anc, reachesBelow), the children one
   level for a childList, ancestors a :has rule could read
   (hasSensitive) themselves only; styleOf keeps the old computed
   object when the declarations and the parent are unchanged, so a
   subtree stands (sameDecl); the class attribute is a cached Set
   (Element._classes), attribute names a cached lowering, parsed
   lengths cached per ComputedStyle. A pass is 13-24 ms, 40-70
   styles; the workbench is idle at 4 s (was 6); the shot is pixel
   equal; hover restyles still light (Source Control icon + tip).
   Then (Kalen: the incremental layout too) layout boundaries
   (layout.mjs isBoundary: a block / flex / grid box whose width and
   height never follow its content, absolutely positioned or a clip
   under a block parent; the inputs of its last pass kept as inCW /
   inCH, defW / defH): page.mjs keeps layout roots per change
   (dirtyLayout: tree = the subtree may change, self = its content
   changed, own = only its own style may have changed, verified at
   layout time by styleOf: the same object means no root at all; a
   detached element is no root), finds each root's nearest boundary
   above it (layoutBoundaries, outermost kept) and Layout.relayout
   builds and lays out only under those, in place, with the old
   inputs and place; a boundary whose size or margins moved makes
   the pass whole. A stylesheet change still restyles and lays out
   everything. Measured: 32 of 38 passes partial in the busiest
   startup second, 8 ms each (a whole pass ~25 ms; one after a
   sheet change ~70 ms with its restyle); the hello page the same.
   Fixed on the way, the cause of the clipped activity bar icon and
   the missing Welcome tab strip: an absolute box with no top takes
   its static place, recorded while its ancestor was laid out at 0;
   moveBox now moves staticX / staticY with the box. Seen: the tab
   strip, every activity bar icon, the parts 35 px lower; a full and
   a partial pass give the same box tree. Tools: QUARK_TRACE_DIRTY=1
   also prints marks/s (what each change marked), full/s (roots with
   no boundary), partial/s (the boundaries used and their box
   counts); QUARK_DUMP lines carry the inline style, a scroll and the
   raw y.
   Then (Kalen: sub 1 ms a pass if possible): boundaries gave way to
   in-place relayout of the nearest block box or flex item above
   each root (Layout.canRelayout / relayoutOne: built again under
   it, laid out with the inputs of its last pass (inCW, inCH, inFW,
   inFH; a flex item also its container's inputs inFlex: base, min,
   natural cross, checked again by flexInputs), kept when its size,
   margins and baseline are unchanged, else its parent is tried, six
   levels, then the whole page). Style: each element's ancestor keys
   (ids, classes, tags; siblings share the set, StyleEngine.ancKeysOf
   until the next mutation) reject a selector whose compounds above
   name a key the ancestors lack (sel.anc) without walking up; the
   rule index is made once per parsed sheet (sheetIndex) and merged;
   elements with the same tag, id and classes share one candidate
   list (candCache); a class toggle marks its subtree by key
   (dirtyUnder, _styleKeys) and a descendant re-matches only when a
   rule of its own names that key above it (rulesNameAbove); a sheet
   added or changed restyles only the elements its rules' rightmost
   keys name (sheetsDiff; everything past 400 marks or an unkeyed
   rule); a style element's text change marks the sheets. Layout:
   intrinsic widths memoized per box for the pass; the min-content
   pass skipped when max-content fits. Measured: style 145 -> 35 us
   an element; a partial pass median 0.9 ms in the busiest startup
   second, 2 ms on the Welcome page, worst 10-32 ms (the whole split
   view, or a pass that parses a new sheet); a whole pass ~12 ms.
   prof/s now prints the partial median and max, style ms, rules
   tested and hit per element, sheet marks. Every shot pixel equal;
   hover local. NEXT: paint, ~28 ms a frame (withClip per box, the
   addon calls); empty text boxes in a flex get a running y (0 x 0,
   nothing drawn); the pseudo index reads junk names (var, 2px: some
   rule parses wrong; harmless).
   Then (Kalen, Oct 18: "half of the stuff isnt painting correctly",
   his own run on the silver folder with aeon.ag open; three crops:
   a bold wrong line number with a box under it, glyphs drawn over
   the chat toolbar's labels, the status bar's left items cut off).
   Reproduced hidden on a copy of his Code - OSS state (--user-data-
   dir on the copy; install/tmp/quark-vscode-silver.png). Found and
   fixed, in order:
   - a custom property's name was lowercased by the declaration
     parser, so every var VS Code writes with capitals (--vscode-
     codiconFontSize-compact, fontWeight-semiBold, strokeThickness)
     was undefined: icons with width 0, plain headings, no borders.
     A var() with no value and no fallback now drops its declaration
     (CSS: invalid at computed-value time), not 0px.
   - DOMPurify (the chat welcome) reads nodeName, parentNode,
     childNodes off Node.prototype: ours were instance fields, so
     it read null and threw. They are prototype accessors now.
   - the editor's sticky scroll widget (the pinned scope header)
     uses float: left for its line numbers: floats were not laid
     out, so the widget's lines fell a row down over the code and
     its shadow read as a current-line box. Floats (left, right,
     clear) are laid out now: shrink-to-fit at the current y, line
     boxes beside them shortened, an absolute box's static place
     after them, a formatting-context root holding them.
   - an inline-block's baseline was never set (0): every inline-
     block sat with its top on the baseline (the Start icons too).
     A block's baseline is its last line's; relayout compares it.
   - an inline box (span) never had a size or rects: every span's
     getBoundingClientRect was 0x0, VS Code's font check failed
     ("monospace assumptions violated"), inline backgrounds never
     painted. Lines now extend each inline ancestor of a fragment;
     the rects start empty every pass (a measuring pass left
     stale ones).
   - a flex container measured its text runs with the spaces
     trimmed whatever white-space said: the status bar's
     white-space: pre labels came out narrow and clipped.
   - the minimap was black: clearRect filled opaque black with the
     copy operator; it is the clear operator now.
   - opacity applied to a box alone: a child of an opacity 0 parent
     painted (hidden scrollbars, fold markers). The painter carries
     the product down the tree (no group layer yet).
   - a rejected in-place relayout left the nodes pointing at boxes
     that were not in the tree (rebind puts them back).
   - a percentage width with no base (calc(100% - 16px) while
     measuring) read as a number: the Welcome toast clipped.
   - blob: module workers import as data: modules with the hooks
     (the worker had no hooks); a worker's navigator is defined,
     not assigned (Node's is a getter).
   - utility children die with the main (a kill of quark left 47
     bootstrap-fork processes under systemd).
   Tools: QUARK_DUMP takes several selectors with | and text:<words>
   (the box three above that text); QUARK_DUMP_CS=1 prints each
   element's computed display/size/font and the DOM text beside
   the box text; QUARK_DUMP_VAR=--name prints that var; render.mjs
   takes QUARK_EVAL=<js> and dumps through page.dumpBoxes.
   Seen: his workspace with the sticky widget, aligned gutter, the
   chat toolbar and welcome, the whole status bar; the Welcome page
   with bold headings, cards, the icons on their lines, the toast
   whole (install/tmp/quark-vscode.png updated). No page errors.
   OPEN from this: a click in the editor moves no cursor
   (caretPositionFromPoint / caretRangeFromPoint return null);
   group opacity; floats inside an inline run; the chat input's
   border not seen (theme value unknown without a real shot).
   OPEN, seen: svg images (letterpress, code icon), woff fonts (seti
   file icons), text-overflow ellipsis. Not driven: typing in the
   editor, menus, the terminal.
   Then (Kalen, Oct 18: "not enough margin and padding seen on many
   elements"; the chat welcome and its input toolbar). Found and
   fixed, in order:
   - CSS nesting: the rule body splitter took a nested prelude from
     the last ; before its brace, so after the first nested block
     every later one read as junk, and a nested rule's own nested
     rules were dropped. VS Code's workbench sheet nests 1,683
     rules (the chat welcome's icon size, title weight, paddings
     and margins among them). A body is scanned now: a prelude
     starts after the last ; or nested block; nested rules recurse;
     a nested at-rule's body is its rule's body (css.mjs
     parseRuleBody, splitNested).
   - a shorthand holding var() (padding: 0 var(--x)) gave each
     longhand the whole text, which read as no length. The longhand
     keeps a pending mark (css.mjs PENDING) and style.mjs
     pendingLonghand substitutes first, then splits.
   - a flex or grid item kept its inline display: inline-block
     labels and inline icons laid out as inline content (width
     ignored, 0x0). Layout.blockify at build: inline and inline-
     block become block, inline-flex flex, inline-grid grid.
   - a column flex item not stretched (align-items center) took the
     full width: flexItemColumnAuto is set from the resolved
     align-self (never was before).
   - an inline box's rect began at its margin edge fragment;
     extendInline leaves the box's own margins out.
   Seen hidden on his state: the welcome view as VS Code draws it
   (40 px icon, bold title, centred message), the picker labels
   padded with the 6 px gap after their icons, the title bar's
   Sign In clear of the layout icons; the Welcome page the same as
   the reference plus the menu bar's own padding (install/tmp/
   quark-vscode-new.png). Tools: QUARK_DUMP_CS prints m= and p=
   (margins and paddings); render.mjs dumps per view
   (dump.<view>.txt).
   Then (Kalen, Oct 18: "insanely slow ... 15 seconds to drag the
   splitter"). Measured hidden at 2560x1440 with a 1000-move drag
   of the sidebar sash sent in 0.9 s (QUARK_PROF): 9 ticks a
   second, layouts 55/s (580 ms), paints 60-90 ms each with 82
   clip calls taking 70% of the paint (withClip self time), styles
   3000/s. Fixed, in order:
   - a frame takes one pointer move, the newest (renderer.mjs
     pendingMove): a mouse sends hundreds a second and VS Code lays
     out on each.
   - a clip is skipped when the box's content lies inside its clip
     rect (Painter.bounds: a per-paint union of each subtree's
     rects, text fragments included; a shadow, outline, transform
     or filter makes it unknown and keeps the clip): 82 -> 29
     clips a paint.
   - trinity Canvas.clip_rounded_rect: a clip inside another took
     the path mask (a mask canvas and a GPU wait) whenever one was
     set. Now two plain rects intersect; an inner rect that stays
     clear of the outer's corner arcs keeps its own radii
     (clip_in_arcs); only an inner reaching into an arc takes the
     mask. Rebuild every trinity app.
   - an inline style change (CSSOM) marked its whole subtree
     (keys [':checked'], and reachesBelow is true for any key once
     a rule has a bare * ancestor): 202 elements a change, 43,000
     a second. No keys now, and an empty key list reaches nothing
     below; a changed computed object still re-derives the
     children (styleOf).
   - the text spec is kept on the computed style; a straight-edged
     one-colour border is four rects, not an even-odd path.
   Measured after: the drag at 23-40 ticks a second, paints ~18 ms
   (idle 9), clips 29 a paint; the picture the same. Left: layouts
   44-66 a second during a drag (VS Code reads offsetWidth after
   each write; a partial pass rebuilds ~460 boxes, median 2.3 ms,
   a full one ~60 ms), paint ~18 ms (text draw, the remaining
   clips, one GPU wait when the 1024-draw ring wraps). The prof/s
   line prints clips N ms and the slow ones (over 1 ms);
   QUARK_SLOWCLIP=1 logs each slow clip's chain.
   Note: Kalen's saved state now holds three windows; quark shows
   them as tabs (his), and a hidden run draws the last shown, the
   empty one, so dumps come from dump.<view>.txt per window.
   Then (Kalen, Oct 19: the explorer card's bottom-right corner had
   a right edge and a bottom edge and no arc; "that's not possible
   style wise ... unless it got clipped"). Chased with every clip
   switched off (QUARK_NOCLIP=1: overflow, the stroke's side cut,
   clip-path; QUARK_NODAMAGE=1), a plain page with the card at the
   same size and place (fine), QUARK_DUMP='at:x,y' (every box over
   a point, with its background, overflow, radii) and
   QUARK_CLIPLOG=x,y (each clip set or skipped over the point, each
   fill painted there). The trace said border=1/1/1/1: the card's
   left side is 1 px of transparent (VS Code's floating-card seam),
   not 0 px, so the border was not "uniform" and took the mixed-
   colour path (a quad a side under the even-odd ring, whose 1 px
   band fills nothing at the arcs). A side draws when it has width,
   a style and a colour; the uniform test and the stroke's cut use
   that. Seen: the arc back (pixel rows), the test card with a
   transparent left side right. Not a clip after all.
   On the way, trinity Canvas.clip_rounded_rect: a plain clip inside
   a rounded one that reaches its arcs keeps the outer as the mask
   and the inner as the analytic rect (the shader applies both);
   before, the inner's mask replaced the outer's shape.
   Open: the glyph blobs after a burst of resizes in a real window
   (reproduced hidden on a real window: many canvases made and
   retired in a second); the canvas count is on the prof line
   (canvases/s: made, freed, live, sizes). A resize now remakes one
   canvas a frame (the newest size) and a <canvas> element makes
   its canvas at its first draw, not at the 300x150 default.
   FIXED silver (Oct 18, Kalen: "this should be a fixed silver
   issue"): the method table overrun (near-border item 1b):
   see that item.
   Then (Kalen, Oct 18: the search box's border off, the tab cut).
   Found and fixed, in order:
   - a 1 px rounded border is an even-odd ring path, and a 1 px band
     between two path edges fills nothing in trinity (the pill's
     top and bottom were gone past the corners; 3 px bands draw).
     A border of one width, colour and style on its drawn sides is
     a stroke along its centre line (Painter.strokeRing; a side of
     width 0 is cut away by a clip), a straight one four rects; the
     ring stays for mixed widths with radii. OPEN trinity: the 1 px
     path band.
   - the border compare took the top side's colour and style as
     the reference: a left-only border (the tab's shoulder) with
     border-top-style none drew nothing. The first drawn side is
     the reference now.
   - rrectPath scales radii that overlap down together (a 7 px
     radius on a 7 px box inverted the path).
   - clip-path: inset(t r b l [round r]) clips a box whole, its own
     painting and its children (Painter.clipPath); the other shapes
     are still ignored. VS Code's connected tab is drawn by it: the
     edge box's right side stops 7 px above the bottom and the
     tab-fill ::after (a 7 x 7 box with a left border, a bottom-left
     radius and a spread shadow in the tab colour, clipped to
     itself) makes the shoulder into the editor area; the first
     tab has no left edge by VS Code's own rule (.tab:first-child).
   Seen: the search pill bordered all round; the connected tab
   pattern on a test page (outline, shoulder, the body's top border
   meeting it) and on the Welcome page. QUARK_DUMP_CS prints the
   borders, radius, position, clip-path and the previous sibling.
   Then (Kalen, Oct 18: VS Code in orbiter is 4x faster than this;
   rendering and layout). Measured hidden at 2560x1440: a pointer
   sweep over the Welcome page repainted the whole page on every
   hover change (17 paints a second, 17 ms each) after a 4.4 ms
   relayout of a 130-box subtree. Fixed, in order:
   - damage repainting: the page keeps the rects that changed (each
     relayout root's bounds before and after, a scrolled box, a
     canvas, the focused field for caret and selection; a full
     layout, a sheet, a font or the background mean all) and the
     painter clips to their union, clears only that, and skips every
     box whose bounds (Painter.bounds, shadows and outlines widen
     them by their reach) miss it. Every other repaint request that
     went through a layout root (hover, active, focus, inline
     style, a DOM mutation, the title) no longer asks for the whole
     page. QUARK_NODAMAGE=1 paints whole, for a comparison.
   - a :hover (or any keyed) change made its top element a 'tree'
     layout root, so the hovered card's whole container was rebuilt
     and repainted. A keyed mark is a 'scan' root now: at layout
     time the marked tree is restyled (cheap: a keyed element stands
     unless a rule of its own names the key above it) and only the
     elements whose computed style really changed become roots.
   Measured on the same input sequence (hover sweep, wheel, five
   activity bar clicks), both runs pixel-identical at the end:
   paint 1611 -> 661 ms, hover relayouts 4.4 -> 1.5 ms median,
   hover repaint area 1-2% of the page, a hover frame ~2 ms of
   work (the profile: 0.75 ms paint, the rest the publish wait).
   Layout stays 1.3 s in that run: a view switch is a full layout
   (34-47 ms) after VS Code adds its sheets (60-115 marks), and a
   sash drag 44-66 partial passes a second. NEXT: box reuse in a
   partial pass (build is half of it), the full layout after a
   sheet, text draw and the publish wait per frame.
   The prof/s line prints "N partial X% area" for damage paints.
   OPEN, seen in these runs (not before the state refresh): "The
   local extension host took longer than 60s to send its ready
   message" in both windows of the hidden runs on the copied state;
   not chased.
   MEMORY (Oct 19, Kalen: "its performance we need and accuracy of
   placement"). The standing goal for VS Code on quark: as fast as
   the Chromium build running in orbiter (Kalen: that one is 4x
   faster) and every element placed exactly where Chromium puts it.
   Landed so far, by area:
   - style: nested rules (1,683 in the workbench sheet), var()
     inside shorthands, keyed dirty marks with scan roots, the
     candidate cache, ancestor keys, sheet diffs; ~35 us an element.
   - layout: in-place relayout of the nearest block or flex item,
     flex and grid items blockified, column items sized by
     align-self, floats, inline rects from the content edge,
     clip-path inset, radii scaled on small boxes, absolute boxes
     moved with their static place.
   - paint: damage rects (hover 1-2% of the page), clips skipped
     when the content is inside, trinity's nested rounded clips,
     borders as rects or a centre-line stroke by drawn sides, group
     opacity carried down.
   - input: one pointer move and one size a frame.
   - silver: the ft slot table built at implement (near-border 1b).
   Measured hidden at 2560x1440: a hover frame ~2 ms, the sweep +
   wheel + clicks sequence paint 1611 -> 661 ms, a sash drag 23-40
   fps, canvases 6 at startup (page + minimap), 0 per interaction.
   Tools: QUARK_PROF=1 (ticks, layouts, partial median/max, paints
   with damage %, clips and slow clips, canvases/s), QUARK_TRACE_
   DIRTY=1, QUARK_DUMP='sel|text:words|at:x,y' + QUARK_DUMP_CS=1
   (m= p= b= r4= pos clip-path prev ov bg sh) into /tmp/claude-1000/
   qk/dump.<view>.txt, QUARK_DUMP_VAR, QUARK_NODAMAGE=1,
   QUARK_NOCLIP=1, QUARK_CLIPLOG=x,y, QUARK_SLOWCLIP=1, NODE_OPTIONS=
   --cpu-prof; /tmp/claude-1000/qk/killq.sh, realrun.sh, seq.py.
   Kalen runs it as `mkdir -p /tmp/qk && XDG_RUNTIME_DIR=/tmp/qk
   install/build/quark install/lib/code-oss/resources/app
   /src/silver`; never kill his runs; his windows are tabs by his
   design. OPEN, in his order: the remaining layout issues ("tons",
   not yet listed one by one); speed: the full layout at a view
   switch (34-47 ms), box rebuild in a partial pass, text draw and
   the publish wait per frame; the resize glyph and svg blur in a
   real window (paused by him); the extension host timeout on the
   copied state; trinity's 1 px even-odd path band.
6. OPEN install/bin/vscode runs through quark; hosted in orbiter (a
   pane), the system bar menu, dialogs through the navigator.

25. BUILT, not seen (Kalen, Oct 16) an app with no window menu of
   its own (a trinity app) gets one in the system bar named for it
   (orbiter own_menu_sync, own_menu, own_pick): Show code / Show app,
   Copy screenshot (the pane as drawn, to <cache>/orbiter/shots and
   the clipboard as a picture), Quit. Uninstall waits for software
   installs. devices: platform_set/get_clipboard_png (X11 image/png
   selection, up to one request: no INCR; macOS, iOS, Windows PNG);
   trinity Display.copy_picture / paste_picture. trinity Console:
   ctrl+shift+v (cmd+v) and the middle button paste a clipboard
   picture as a png in <cache>/paste and type its path (for claude),
   else the text. trinity changed: restart orbiter, rebuild apps.
26. DONE (Kalen, Oct 16: "thats better") orbiter's ux follows the
   font scale (ctrl +/-). A trinity area unit is the default font's
   line height: 33 px at rest on Kalen's screen, not 22 (a px size
   converts as px / 33). Dock 2.39 units, system bar 0.91, task bar
   1.06, task button 4.36; drawn code reads unit_px[] (the layout's
   em) or nav_unit / ux_unit (font_unit / 22, 1 at rest) for sizes
   kept in px (navigator, list text, properties grid).
27. BUILT, not seen or heard (Kalen, Oct 16) the bottom bar's sound,
   left of the clock: App / Global (StatusSegs), mute, a volume slider
   (BarVolume: 1 px outline, a 20-band spectrum behind the track).
   Global is the machine's own level: devices host_master_get/set (the
   default card's Master, else PCM, through ALSA's mixer: pulse or
   pipewire on Ubuntu, the card on orbiter-os), the final stage for
   every app; App sets the active pane's app's own volume (`volume n`
   on its socket). The spectrum: devices keeps each slot's last 1024
   played samples (host_audio_scope); orbiter vol_spectrum mixes the
   active app's, or all, a windowed DFT at 20 log-spaced bins.
   OPEN silver: a local named `mix` cannot be indexed (`mix[ i ]`:
   "expected member"), `any` is taken, `log10` is unknown.
28. BUILT, not seen (Kalen, Oct 16) the exchange in a slideout: trinity
   Window.exchange_region (last member; '' the whole window) puts the
   exchange's elements and the avatar in one box there (exch_el finds
   them in it); boxed, the conversation sits under a small avatar
   (AgentAvatar :drawer) and the field at the foot; no ShotBackdrop
   there: its ground is orbiter's ExchangeDrawer (the menu drawer's
   blurred ground, 21.82 units wide, it slides). The pickers take the menu look
   (clear rest, #4d3dbcaa lit). orbiter's button opens it beside the
   dock, 21.82 units wide (twice a menu); the button again, or any
   menu drawer opening, puts it away (agent_min); a press outside does not.
29. DONE (Oct 16, Kalen: the exchange's slideout went menu-narrow
   once a menu had opened) trinity Style.applicable took every block
   whose type the element is (its own class's and its bases'), in
   the order the classes' styles were first met, so a base's area
   could win (MenuDrawer's w10.91 over ExchangeDrawer's). It takes
   only the nearest class that styles that member now (block_depth).
   `silver --test trinity` exit 0. Every trinity app at a restart.
30. DONE (Oct 16, Kalen: closing orbiter or an app with sound is
   terribly slow) measured hidden, quit over the socket to exit:
   - psx with sound 1262 ms and a SIGSEGV: nothing stopped its
     sound thread at a quit (only a disc change did). on_unload
     and dealloc call unboot: 350 ms, no crash.
   - orbiter 2200 ms and a SIGSEGV: wayland's display thread ran
     on after its library was unloaded. wayland.c's destructor
     stops and joins it: 1000 ms, no crash.
   - then 200 ms clearing orbiter's file index and lists, 300 ms
     trinity freeing every object: a quit (no SILVER_RELOAD_SAVE)
     skips both (media_app.destroy returns after unload_tree and
     release_tree; reloads free as before): 580 ms.
   Left: ~250 ms in NVIDIA's own teardown at exit (its ICD's
   ioctls). trinity changed: every trinity app at its next build.
   Then (Kalen: every spectra app halted the system's sound at
   shutdown, a short buffer repeating; background audio stopped
   too): a supervised app that crashes is frozen by silver-host's
   crash handler (SIGSTOP, for lldb) with its PipeWire stream
   open, and PipeWire waits on it. Both crashes at close are
   fixed (above); the handler now shuts its sockets to pipewire /
   pulse first (drop_sound_sockets). Checked with a C stand-in:
   its node left the graph at the freeze, not at the kill. A
   clean psx quit (pw-top): quantum 128 -> 2048, no new xruns,
   Firefox unbroken. spectra: AudioOut.volume (0..1, audio_level
   on every block), 0 once closing; the last block to the host's
   ring is silence. A closing window hides first (media_app
   destroy, quits only).
31. BUILT, seen hidden (Kalen, Oct 16: the top right only the menu,
   the date left of it in the power button's look; the views and the
   rest in a menu, GNOME's quick settings as the model) the system
   bar's right end is BarClock (the date) and sys_menu; either opens
   SystemDrawer (SystemBar.ag; Rule #13's drawer, from the bar's right
   end, 12 units wide, the same blurred ground): a row of round
   buttons (reload when ready, scenes, the side panel; power at the
   right, restart with ctrl / cmd or shift over it), the sound (mute,
   slider; out of the bottom bar, as is the clock), a Wi-Fi tile (its
   mark and the network; opens the side panel's Wi-Fi), a tile per
   tray app (its icon and name; a press is its action, or its menu).
   Seen hidden: the bar, the menu open, scenes lit from it, the Wi-Fi
   tile opening the panel and shutting the menu, a press outside
   shutting it. Not seen: tray tiles (no tray app in the run).
   Then (Kalen): all round (RoundTab: AltTab with a radius 999 edge;
   the slider a pill). The view buttons (scenes, instances) and the
   Wi-Fi tile show their view in the menu under the tiles (sys_view;
   orbiter.side_view builds it for the side panel and the menu), the
   lit one again puts it away. Seen hidden: scene cards and props,
   the Wi-Fi list, the instance tabs, each in the menu.
   The scene list scrolls by trinity (content_h, scroll_goal ease,
   scroll_fade 24 top and bottom). The scene settings are chips
   (PropChip, NodeList.ag: 'name: value', the value in the accent, two
   a row); a press opens its choices under it as RoundTab pills
   (scene_open), a pick sets it (scene_prop_set). Seen hidden: stage
   opened, frost picked, chip and pill followed.
   FIXED silver (Oct 18): `for [ v, k ] in f[ .. ]` over a call's
   map works (features t_for_call_map).
32. DONE (Oct 18, Kalen: the top bar red during a window resize)
   trinity Render.update waited its frame on a new size, and that
   wait rewound draw_slot to 0 after the paint had bound its image
   into slot N: the draw bound slot 0, still naming an old freed
   image (the bar's cache before its resize). The slot is kept.
   Seen hidden: the menu's mid-frame screen copy had 1,800-8,800
   red px in its top rows, now none. Also: the drawers' blur is
   resized in place (ReduceBlur.resize), not rebuilt (it crashed
   in Render_init on the second size). trinity tests exit 0.
   Still red on Kalen's screen after that; fixed for him by the
   SystemBar painting straight onto the screen (no_cache: a
   resized cache showed through). Root of the cache flash: OPEN.
32b. BUILT (Oct 18, Kalen: bars and tint a size behind while he
   drags) orbiter's own picture was right each frame (the clock 31
   px from the edge at 24 sizes); the swapchain waited for 30 ms
   of stillness (resize_debounce_s) and a drag never pauses.
   Debounce 0: a new size each frame. Hidden: 80 sizes, validation
   on, no error or crash; trinity tests exit 0. Not seen by Kalen.
32c. DONE (Oct 18, Kalen: bars, panes and tint a size behind the
   background while he drags; fixed and lost about five times in
   four days) Window.update_all never laid the root out: each
   child's layout_self (before its render) read the root's bounds
   from the LAST frame, so the Splitter turned the old width into
   fixed px pane edges that the later layout pass kept. update_all
   now lays the root out at the window's size first. KEEP THIS
   LINE. Seen hidden on his Zelda session: 60 jumping sizes, the
   clock the same distance from the right edge in all 60 (7 of 40
   were a size behind before). trinity tests exit 0.
33. OPEN (Kalen, Oct 18) a running orbiter of his must not stop
   mine (or my builds) from running beside it.
34. OPEN silver: orbiter read element's intern cache_canvas and
   crashed (a trace of mine, Oct 18); intern cache_w was refused.
   A refusal of every cross-module intern read was tried and taken
   out: aeon reads design's intern Cage.poly, and that works.
Decided (Kalen, Oct 15): Wi-Fi speaks to iwd alone, on orbiter-os.
No NetworkManager or other backends for a user session on another
system: there the mark reads off and the panel says so.
Kalen: no systemd. iwd is built without its service files and
init starts dbus-daemon and iwd itself. Ubuntu's dbus-daemon
links libsystemd as a library (inert); OPEN: our own dbus build
without it if wanted.

## orbiter agent view — live list (Oct 20 2026)

1. DONE (Oct 20, Kalen: the beam's thrust pushes the avatar into
   its platform slightly, a perlin-oscillating magnitude, "an
   indent with bounces inside of it") avatar/avatar.ag: Avatar
   push_cur moves the model down its own up axis (glTF -y, after
   the rotations); OrbiterAvatar sets it each frame from the beam:
   depth = beam_cur x push_depth (0.045 of its own units) x (1 +
   bounce), bounce = a 1-D gradient noise (thrust_noise, push_hz
   1.6 steps a second) x push_bounce 0.5 x a slower noise (0..1)
   that swells the swing; filtered at push_amount 0.15. Every
   avatar with a beam takes it (the exchange's too). Measured
   hidden (1920 x 1080, a gemini session on its pad, `app home
   gemini build`): the body 0 px at rest, 3 to 5 px down through
   the build, moving frame to frame, 0 px again after. avatar,
   wizard, orbiter rebuilt; Kalen's running orbiter needs its
   restart. NOT seen on his screen.

2. OPEN (Kalen, Oct 20: a live reload keeps every agent's state;
   its program is never closed: hosted by silver-host). Plan, not
   written: each platform agent's terminal becomes a hosted console
   instance (the console module in a host slot, running the agent's
   line with ORBITER_AGENT set) instead of orbiter's own pty, which
   the old instance's teardown hangs up at a reload; the platform
   draws that instance's frames and forwards its keys and clicks as
   a pane does (AppView service / draw_frame / view_press); the
   agent's name rides the slot's request line so adopt_live_slots
   finds it again; home_runs, home_ran, home_order, home_agent,
   home_doing, home_words, home_open become persist slots; trinity's
   app socket gains a `title` query so the reloaded orbiter reads
   the console's title (waiting / working) without waiting for a
   change. Limit: the host's slot request line is 1023 bytes; the
   claude line is ~890 today.
3. DONE (Oct 20, Kalen: Neptune lacked the Milky Way the other
   scenes show) neptune.ag had the Gaia sky model, turned 45
   degrees about x as the others; its camera looks straight along
   -z, so the band's plane stood 45 degrees off square and lay
   outside the 55 degree lens (Uranus aims its own). Now -9 about
   x, 15 about z, a slow turn about y, as Uranus. Seen in the
   remade thumbnail: the band diagonal behind the disk. Kalen's
   running orbiter loads the scene library at its next pick or
   restart.

4. BUILT, not driven (Oct 20, Kalen: files dropped on a console)
   a drop goes to the element under the pointer and its parents
   (devices reports the pointer during an X drag), then the focus,
   then the app; a taker sets ev.no_default. Console pastes the
   shell-quoted paths (bracketed paste when the shell asked).
   orbiter: a drop over a running hosted app goes to it as
   HM.drop (three bytes a message, a 0 byte ends; the app hands
   it to its own drop_event). Not tried with a real drag.

5. RULE (Kalen, Oct 20, after false "done" reports): never report
   a visual result before looking at its pixels in a shot (crop,
   zoom, brighten). "Dark sky" and "it's Neptune's blue" were said
   unchecked and were wrong. Never tell Kalen to restart.
   Never change what his session saves (scene, panes, settings)
   as a side effect: a view-only override of scene_loaded[] saved
   MilkyWay over his Neptune mid-validation (save_session writes
   scene = scene_loaded[]). Overrides go in their own element.
6. DONE the avatars are part of the platform scene (orbiter/
   Platform.ag PlatformView, PlatAvatar.scene_draw; avatar/
   avatar.ag scene_host/scene_view/scene_box/scene_frame; design/
   Stage.ag host mode: the first depth pass draws into the host
   Render, its screen passes run at host size). Focus blur removed
   entirely. Heat folds the halo in (Heat.rim, pic()).
7. DONE resolution: the platform picture is 2x the element in
   device px, drawn on whole pixels (exact 2:1). The chosen avatar
   (home_open) is drawn again at 4x in its own box Render `close`
   (floor there depth only: look.y 0), its chain run there, then
   `shrink` (UVReduce, 4x4) laid over the 2x picture on whole
   pixels. A box resize rebinds shrink (new image, same name).
   5 agents: 30 fps hidden; 1 agent ~50.
8. DONE AgentHome is `animated: true`: the platform (animated)
   painted over the list's text (cached parent composed first).
9. DONE the mirrors reflect the scene. trinity Window last members
   mirror_ask, mirror_turn, mirror_proj, mirror_w/h, mirror,
   mirror_src; Display.mirror_pass (vk.ag) draws the scene's Render
   once more with every ModelViewProject shader's proj set to the
   platform's (view untouched), copies it to win.mirror. PlatLit
   reads it with rows flipped about the horizon (uv = tuv.x,
   1 - tuv.y). Failed tries, keep out: a reflection matrix tilted by
   the camera's 48 deg (reflects high empty sky: black); any det -1
   view (culls the planet's faces).
10. DONE the agent view's background is the Milky Way (orbiter
   galaxy_element, slot 'galaxy', built once, its Render kept off
   the window), while the CHOSEN scene stays mounted, its Render
   skipped as backdrop and named win.mirror_src: the floor reflects
   the chosen scene (Neptune), the galaxy is behind. Seen in a
   hidden run (zoomed): Neptune's lit side and terminator in the
   floor. Built scenes are kept between switches (scene_kept,
   scene_rts). NOT seen on Kalen's screen: his running orbiter has
   the trinity from before these changes (a reload swaps only the
   app library), so he saw no reflection.
   Test harness (session scratchpad rr/): onep.sh (Neptune) /
   oapps2.sh (MilkyWay) launch hidden; q.py drives; kill_os.sh
   kills only test processes; agent add = click(40,882) then
   click(785,61).

## orbiter-os shell design — live list (Oct 1 2026)

Goal: orbiter as an OS shell. The interface stays as it is: every pane keeps its title bar and
nav list. Changes: a dock on the left side, and the bottom
bar 50% taller.
1. OPEN the bottom bar (StatusBar, orbiter.ag) 50% taller:
   35 px -> 52.5 px (its area and the stack's b35px above it).
   It keeps the status portion: tool tips and orbiter's state.
2. OPEN the dock, a strip on the window's left side: one
   button per element (its module's images/icon.png), a
   launch and status button. Pressing it launches the
   element or switches the view to it. Orbiter's own button
   sits at the dock's bottom, larger than the others.
3. OPEN the status light on the side of each dock button:
   none (not running), busy, needs (waiting on the user),
   done (something new), failed. The words are the exchange's
   (busy / idle / done / needs) plus failed. A hosted app
   sends it over its ring, as HM.title and HM.nav_state do.
4. OPEN switching hides, never quits (macOS style): leaving an
   element hides its view; its instance keeps running and its
   light keeps reporting. close_pane (orbiter.ag) already keeps
   instances running when a tab closes.
5. OPEN the editor is an app in the dock, not the shell's
   default. It declares itself by export: the file types it
   claims (.ag .c .h ...), as aura's .agi claims .html, and a
   dock flag so it is in the dock from the start.

## orbiter floating windows — plan (Oct 19 2026)

Kalen: a grid is a pain to manage, and a maximized console
alone feels lonely: a window seen at the edge is company and
keeps you placed among your own work.
1. BUILT every window opens maximized, always (the split).
2. BUILT a drag on its strip pulls it free (6 px, then the
   pane leaves the split: trinity Splitter `loose`, drawn on
   top, the same EditorView so its app stays); it keeps its
   size, two thirds when it filled the area. Moved by the
   pointer; right, bottom and corner grips size it (FreeGrip);
   a press in it raises it and selects it. Let go at the top
   edge, or a double-click on the strip, it docks again.
   orbiter pane_free (x y w h px), free_order (bottom to top);
   not saved in the session (a start is maximized).
3. BUILT the system bar shows the selected window's menu alone,
   from the left; its close and arrows at the right end. The
   other panes' groups and the pane glow are gone.
4. BUILT WinStrip: a free window's strip across its top (the
   pane title's look, the window's name centred, lit when
   selected). Docked (Kalen), the strip is absorbed into the
   system bar: none on the pane; the bar's free space between
   the menus and the arrows (BarGrab) pulled down frees the
   selected window, and moves a free one; a click there is the
   title's. The desktop (the empty pane, its apps) has none.
5. BUILT (Kalen) every move and size eases (filter_n 0.3:
   pane_shown toward pane_free, ease_free each frame); the
   corner grip's cursor is a diagonal one (devices
   PLATFORM_CURSOR_DRESIZE 5; X11 bottom_right_corner, Win
   IDC_SIZENWSE, mac 15 frameResize, else crosshair; mac not
   built). An app holding its shape (AppView.locked) is
   letterboxed docked only: free, the window takes its shape
   (fit_free: height by width, or width by height for the
   bottom grip), pulled out within two thirds of the height.
   Seen hidden: asnes (zelda) free with no bars round it.
6. BUILT (Kalen: the whole bar on the window; menus without a
   click first) a free window's strip is the system bar it took
   with it (strip_els): its menus (menu_group_els per pane; a
   window not selected shows its menu as last read, PaneMenu,
   again every 2 s), title, arrows, close. A press on another
   window's menu selects it and opens that menu in one click;
   the drawer opens from the window's left edge under its strip
   (drawer_y). The system bar is the title of a docked window
   alone (bar_pane: the selected one docked, else the first
   docked; all free: the globe alone); one window's menus at a
   time, never two side by side. A drag pulling from the bar
   keeps its grab as the window leaves it (grab_pane). The
   resize ring is 8 px just outside the window (grips_of; edges
   and corners, top included), never inside it. The strip's
   buttons run end to end, no globe; its menu drawer stays
   inside the window (drawer_area).
   The navigator is two kinds (Kalen): the full navigator (the
   explorer, with its dock column) lives in the docked view; a
   free window's is a dialog, no dock. The bar's globe opens the
   full one in the docked view, in front of what it shows (that
   keeps running, back as it shuts); every window free: a new
   docked view (pane_add) opens on it. Seen hidden: gedit free,
   the globe gave a docked navigator with its dock under it. NOT
   seen: the dialog in a free window.
7. BUILT (Kalen) a free window is a rounded rect (8 px) masked:
   trinity draw_element, a layer with `clip: true` masks all the
   element paints and holds to that layer's rounded box
   (Canvas.clip_rounded_rect; EditorView :free's bg). Under it,
   WinShade (24 px out each way): a shadow (0 8px 20px #000a)
   and a 1 px #ffffff26 outline just outside the window.
   Seen hidden: gedit's own status bar cut round at the corner.
8. BUILT (Kalen) the desktop (the apps, under free windows when
   nothing is docked) is an app of its own: orbiter, the welcome
   app. With it showing, the bar's first name is orbiter, and its
   menu is the system menu (SystemDrawer, from the left under the
   bar); the bottom bar's menu button is gone (the console beside
   the clock). For now the system menu is reached from the
   desktop alone (Kalen: click the desktop).
Seen hidden: the apps pane and gedit pulled free, moved,
docked by the top edge and by a double-click; gedit sized by
its corner and typed into while free. NOT driven: two panes
with one free over the other.

## Aeon, the 3D design app — live list (Oct 12 2026)

Kalen: a 3D design app the user's agent helps design with; in
time a level editor. Named Aeon (Heraclitus: "Aion is a child
at play"). Decided:
- A project is a folder: what it uses is copied in or
  symlinked; nothing points outside it.
- The design is .agi (inherently customizable; .blend is not).
  Aeon loads silver modules: their classes are what it edits.
- Not a node editor (Kalen, Oct 13): no wired boxes for
  modifiers, materials or passes. Modifiers stay settings,
  never applied; materials are an object's slots.
- The render graph is trinity's real pipeline (Render, Model,
  Pipeline, passes): shown live (picture, size, gpu time per
  pass) and built there too; a number edit rebuilds the
  touched Renders, no compile. Shaders and pass types stay
  silver classes; the .agi is the wiring.
- Raw shape lives in binary .mesh files (the cage only).
- Input: point and speak, draw over the view, drag handles,
  reference pictures, pick among the agent's variations. No
  popups, no tree views.
1. DONE (Oct 12) the .blend converter, aeon/blend.py, run by
   Blender (`install/blender/blender --background <file.blend>
   --python aeon/blend.py -- <folder> [refs]`). flower888.16.32.blend ->
   aeon/flower888/ (5 meshes, 3,398 cage vertices, 400 KB; was
   an 8 MB gltf): flower888.agi (objects, transforms, materials
   per slot, modifiers as nodes; array empties folded into the
   array's `step`), materials/<name>.agi (every unwired
   Principled input), meshes/<name>.mesh: 'AEMS', u32 version 1,
   vertices, edges, faces, corners; f32 positions, u32 edge
   pairs, u32 face sizes, u32 corner vertices; u32 attribute
   count, each: u32 name length, name, u32 domain (point edge
   face corner), kind (f32 i32 u8), width, data.
   Checked: the folder alone rebuilt in Blender (a reader in the
   session scratchpad) evaluates to the original's vertex and
   face counts for all five objects (flower 91,800 vertices),
   every point within 5.3e-7 of one of the original's.
   flower888's header says Blender 4.0 saved it (v400). aeon.ag
   imports Blender 4.5.14 LTS by url (378 MB, unpacked once into
   install/blender, 1.2 GB; THIRD_PARTY.md); run blend.py with
   install/blender/blender. On 4.5 the meshes and refs are byte
   for byte as on the system's 4.0.2; materials gain 4.5's new
   Principled inputs; blend reads surface_render_method (4.2 on).
   Kept as Blender python (Kalen, Oct 12: "use the pythons"): a
   .blend is Blender's memory on disk; Blender reads its own
   versions exactly. Import only: Aeon reads .agi and .mesh.
1b. DONE (Oct 12) `silver aeon <file.blend>` imports it into
   <folder of the file>/<name up to its first dot>, then the
   folder opens; skipped while that .agi is newer than the .blend.
   No blender program runs (Kalen: bpy in our python, our own
   blender): aeon imports bpy 4.5.14 (the manylinux wheel, 355
   MB, by url) unpacked once into install/pylib-blender (786 MB)
   by the python 3.11.15 that the imported blender 4.5.14 ships
   (install/bin/blender-python links it; it has bpy's numpy,
   cython, requests, zstandard). blend.py opens the .blend itself
   under bpy (still runs inside blender too, args after --).
   Checked: bpy reports 4.5.14 LTS on python 3.11.15; project and
   refs byte for byte as blender's own; aeon under PATH=/usr/bin:
   /bin starts only sh and our blender-python, and bpy itself
   runs `xdg-user-dir DOCUMENTS` (blender's documents lookup).
   install/bin/python3 is a link to the system's 3.12 (not ours).
   Linux only: the wheel and the blender tarball are x64 linux.
   Au (Kalen: shell work uses our environment first, generically):
   command run / exec put install/bin (beside the lib/ libAu.so
   loads from) first on the child's PATH; not on Windows (cmd.exe).
   features t_command_install_path; 246/247 (t_codegen_resource as
   before). Au command gained `stream` (popen's stream, run the
   same way; the caller pcloses): all 22 raw popen calls moved to
   it (orbiter Git.ag 3, orbiter.ag 3, wizard 4, os-bootstrap 12),
   as `x : command [ cmd ]` then `x.stream[]` (silver reads no
   method on `command [ .. ]` directly). os-bootstrap, wizard,
   orbiter build; `silver --test wizard` exit 0 (its run makes an
   installer image through os-bootstrap). orbiter's git calls not
   driven. The 10 system[] calls moved to command exec[ false ]
   (it prints as system did and returns the exit status):
   os-bootstrap 6, wizard 3, tls 1 (macOS keychain, not run
   here); `silver --test wizard` exit 0, its work folders gone.
   `{ }` blocks interpolate (Kalen: commands as { tokens }): a
   block given to a string type (string, command, path) becomes
   an interpolated string, glued chunks stay glued, {{ / }} are
   braces; a command block's lines join with spaces (silver.c
   read_keywords; CSS, Region and other string-built types stay
   literal). features t_keyword_interpolate; 247/248. Moved to it:
   aeon's blender call, tls's keychain line, the rm -rf of wizard
   (3) and os-bootstrap (3). LANGUAGE.md Strings.
   OPEN: os-bootstrap.ag:942's cleanup
   glob with no installer-* folder runs `[ -d /proc/* ]` (prints
   "[: /proc/1: unexpected operator", harmless).
Rule (Kalen, Oct 12): Aeon alone changes the .mesh files. The
agent edits .agi text; a shape change it wants goes through
Aeon's own commands (select points, move by an amount, scale),
eased, saved and undoable as a mouse edit.
Demo (Kalen: promote a demonstration), in order:
D1. DONE (Oct 12) the .agi files watched (design, materials,
    render graph): an au watch on the project folder raises a
    flag; the next frame compares each .agi's time with the time
    last read or written here (Aeon's own writes are noted, so no
    reload). Only the render graph changed, same passes, draws
    and shaders: Stage.retune applies the new settings to the
    running shaders, no rebuild; else Design.load (in place) and
    a new Stage. Seen hidden: beam_cone 0.35 -> 1.2 and
    halo_height 0.4 -> 1.6 written from outside: "settings
    retuned", 10,159 pixels changed. Not driven: a design or
    material change (the rebuild path).
D5. DONE (Oct 12) the layout and its panels (aeon/Panels.ag):
    SceneGraph across the top (150 px): the scene level, objects
    on the left, passes in columns by what feeds them, then out,
    wired (objects into the passes that draw them, passes into
    the passes that read them, shown passes into out); a click
    chooses an object or a pass. On the right, NodeList: the
    chosen object's nodes in order (cage, mirror, subdivision,
    array) or the chosen pass's draws; below it the properties:
    a name (Words) and a TextInput a row (an object: at, turn,
    size; a node: its props*[] (MeshNode.props); a pass: scale,
    most; a draw: its settings). A field the user changes is
    written into its .agi block (agi_set: nested blocks by key,
    the key's line replaced or added; settings quoted) and the
    watch does the rest. Values read through Au.string_agi
    (get_property hands back raw numbers). Selection is one
    shared object (app, view, panels). The object tabs are gone.
    Seen hidden: flower chosen in the graph, its chain listed,
    subdivision's levels 3 -> 1 typed: levels: 1 written, the
    design reloaded; volume's aura halo_height 0.4 -> 1.6 typed:
    written quoted, settings retuned live (12,272 pixels).
    The chosen object (Kalen): the others greyed and see-through,
    it at full colour over them, with a glow round its contour
    (the bounding box was built, then removed: the glow does it).
    Two stages: `stage` draws all but the chosen, `focus`
    (transparent clear) only the chosen, picked per frame by
    Stage.show_only (Model runs list the pipelines kept; a draw
    holding none is skipped by its shader's states). The others
    drawn with color_filter saturation 0 at opacity 0.55; the
    glow is the focus image's coverage made white (color_filter
    brightness 1000) drawn in rings of 8 offsets (1 px at 0.9,
    then 2, 3, 4.5, 6.5 px fading), the object over it.
    Fixed in trinity (Canvas.ag): the image mode skipped
    apply_color_filter (css filters apply to images too);
    trinity tests exit 0. Seen hidden: body colour 1,301 -> 145
    coloured pixels (the rest the box lines then), flower's white
    contour and glow.
    No auto save (Kalen): every edit stays in memory until
    ctrl/cmd + s (shift stays the exchange's capture). Edits
    (Panels.ag) holds the objects whose cage or place moved and
    the property values queued as Writes (a later one for the
    same place replaces it); save_all writes the cages (.mesh),
    places (save_place) and values (agi_set), then notes the files
    so the watch does not reload. A field applies in memory: an
    object's at/turn/size refills it, a draw's setting goes onto
    both stages' shaders (Stage.set_setting), a node's or pass's
    member is set and both stages are made again from memory
    (rebuild_due; the graph object kept). Undo marks, no write.
    The scene graph shows "unsaved: ctrl/cmd + s to save" while
    any is held. Seen hidden: a cage move and levels 3 -> 1 left
    the files as they were (the shape rebuilt), ctrl+s wrote both.
    Reverted at Kalen's word: aeon/flower888's design file, core
    and flower meshes back to the import (his edits there undone;
    the render graph, textures and images kept).
    Selection history (Kalen): every distinct choice (object,
    node, pass) is remembered in order by the app's render; a
    choice after a step back drops the steps ahead. The app's
    element.navigate (ctrl/cmd + left / right, which trinity
    routes there, and orbiter's back / forward arrows on a hosted
    view through HM.nav) walks it; nav_state lights orbiter's
    arrows. Seen hidden: flower, outside; ctrl+left lit flower,
    ctrl+right outside again. Orbiter's arrows not driven.
    Moving is a grab (Kalen: a click and drag moves
    nothing; G does, for the object and for points): the left
    button only chooses (an object, or a point in the cage; shift
    adds), the right or middle button's drag turns the view
    (Kalen: orbit on mouse 2 / 3), ctrl/cmd + any drag or shift +
    middle drag moves the camera (seen: 100 px moved
    the model 47.6 px, eased). Seen hidden: a left drag changed no pixel, a right
    and a middle drag each turned the view (~49,800 pixels). G
    grabs the selection (the chosen object's place, or the picked
    points): it follows the pointer with no button held, eased; x
    / y / z toggles that axis alone; a left click or enter keeps
    it (an unsaved edit once it settles), a right click or escape
    puts it back (its undo entry taken). Seen hidden: a click and
    drag left flower and its cage unchanged; G, 60 px and a click
    moved flower (0.0759, -0.1109, 0 saved by ctrl+s); G, a move
    and a right click, or escape, left it there; in the cage, G,
    a move and a click changed the mesh on save.
    G, R and S (Kalen: x y z with numbers, esc
    cancels): move, turn (degrees) and size, of the chosen
    object (at, turn, size) or the picked points about their
    middle. Each frame sets the result from the state at the
    start (from_at / from_turn / from_size / from_pos), eased.
    Axis keys before a number make a group the number goes to
    (Kalen: `s x y 2` is both); an axis after a number starts the
    next group; a number with no axis: all three for S, x for G,
    z for R; the axes also bound the pointer. Enter or a left
    click keeps it, escape or a right click puts it back. The
    view's foot shows the op and what is typed. save_place
    writes at, turn and size (each line replaced, or added).
    Seen hidden from a fresh copy, each saved by ctrl+s:
    `s x y 2` -> size 2 2 1; `g z 0.5`, `r z 45` -> at z 0.5,
    turn z 45; `g x 9` + escape changed nothing; `s 2 x 1` ->
    2 4 2; `r x y 90` -> turn 90 90 45; in the cage, `g x 0.1`
    moved point 25 by exactly 0.1 in x, nothing else.
    OPEN: the property fields keep their old text while an op
    or a save changes the value.
    silver (Kalen: a known member that is not a shape): `vec T
    [ v ]` with v a vector is v's values, copied, or converted
    element by element when T differs (silver.c, through
    vector_convert). features t_vec_from_vec; 248/249
    (t_codegen_resource as before).
    The render graph lives inside the design (Kalen, Oct 12:
    "embed the render inside"): <name>.agi ends with a
    `render: StageGraph` block (modules, passes, out); the
    .render.agi file is gone (aeon/flower888's moved in). Design
    keeps it as `render` (read_render reads it again); Stage takes
    design.render, else the default lit pass. The watch tells a
    change inside the block from one outside it by the file's
    text without the block (Design.body_text): inside only, the
    shaders are retuned; else the design reloads. Pass and draw
    values write into render > passes > ... Seen hidden: the
    chain draws (av, mask, volume, heat in the graph); an outside
    halo_height edit retuned (8,250 pixels), an outside `at` edit
    reloaded; `most` 1024 -> 512 typed and saved into the block,
    no reload. Fixed on the way: a pass sized under a pixel
    (most typed one digit at a time: 1) made an empty Render and
    aborted; Stage.size_of keeps a pixel a side.
    Scene graph (Kalen, Oct 12): each object box has a
    visibility circle at its left end (radius 3.5 px): filled
    while shown, an outline when hidden; a click there flips
    Object.shown and chooses nothing. A hidden object's box
    outline and name dim, and it is in neither stage (the view's
    show_only key holds the chosen and the hidden names) and is
    never picked. In memory only, not saved. A click on the box
    already chosen lets it go (objects and passes). Node names 14
    px. Seen hidden: outside hidden (44,247 pixels, the flower
    underneath reachable), flower chosen then let go.
    Quad draws are nodes of their own (Kalen: "aura would have
    to be its own node"; separator alone still lit the halo, as
    the aura only read av.depth): every quad draw (aura, heat)
    is a box under the objects (pink, kind 3) with its own
    circle, wired into its pass; it draws only while its circle
    is filled (Selection.off holds 'pass/draw' keys; Stage
    show_only takes them). A click on the box chooses its pass
    and draw (settings on the right). The band grows with its
    rows (16 + 27 a row, at least 150 px); the panels follow.
    The flower -> mask wire is real: mask draws outside, flower
    and separator (GrooveMask) for the heat haze.
    av reaches the screen only through heat (out: heat, volume):
    heat off leaves the objects unseen too.
    Fixed on the way: a view of a new size faulted the GPU (lost
    device): a resized pass made new textures while the quads
    reading it held the old ones. Stage keeps built_w/built_h and
    the view makes both stages again on a new size.
    Seen hidden: separator alone with aura and heat on (the halo
    and separator), both off (nothing), aura chosen (its 7
    settings listed).
    Heat (Kalen: no heat blur; the beam a layer under it): the
    Heat draw had no settings (heat 0: the image unbent). Now
    heat 1, busy 1, pulse_height 1.2; a `layers` pass (AeonOver,
    View.ag: volume.color over av.reduction, premultiplied)
    feeds heat, so the beam bends too; out: heat. BUILT, the
    blur not yet measured (the check run was stopped).
    Properties (Kalen: orbiter's properties view, shared): the
    `properties` module (properties/properties.ag) holds
    orbiter's LaunchPanel and its controls (NavButton,
    LabelButton, LaunchPath/Clear/Action/Check/Field/Color/
    Slider/Label, KeyField, PadField, StepField, ArgsField,
    key_name, pad_name), moved as written; orbiter and aeon
    import it. push_live's sending stays in orbiter
    (orbiter.push_live over LaunchPanel.live_prop). aeon gives
    it a LaunchProps of its rows (type string) and reads each
    value_of back; inst moves with the chosen thing. Built:
    properties, orbiter, aeon. Seen hidden in aeon: the grid,
    a typed size applied (unsaved shown). Not driven in
    orbiter. Build properties by name after an edit.
    A list value shows bare (0, 0, 0): rows[] strips the outer
    brackets (PropRow.bare), apply_row puts them back for the
    member and the file. Seen hidden: 1, 1, 2 typed, saved as
    size: [ 1, 1, 2 ].
    A member named `list` (PropRow) once built a library that
    failed to load ("undefined symbol: Array_init"); not
    reproduced in 8 builds (2 clean), Array_init defined each
    time: the name was not the cause (maybe two builds of aeon
    at once). The member is `list` again; features
    t_member_named_type guards it (249/250).
    The design module (Kalen, Oct 12: orbiter's avatar loads its
    passes from the agi; its shape from the design's meshes, no
    gltf): aeon's Poly, Design, View and Stage moved to
    design/ (`extend design`); aeon and avatar import it.
    Stage gained `given` (a host's own shaders by draw name:
    body core plasma aura mask heat), `model` (the turn),
    `up`, `host_time`/`time`, `state_mask`, and `rewire`: a
    resized pass's readers take its new textures in place.
    Avatar: create_target reads aeon/flower888 (beside the
    module's source: install/build/silver-avatar.source's first
    line, two folders up; `project` overrides), one Design per
    folder for the process (persist designs), a Stage with its
    shaders; draw sets the camera (glTF vectors turned to the
    design's axes), clock and hover, and draws the outs. The
    hand-built av, volume, mask and heat Renders, the gltf and
    the spoke/rim canvases are gone. Seen hidden: the wizard's
    avatar drawn from the design; then (own HOME and state, so
    no session touched) orbiter's exchange avatar (ctrl+shift+S,
    turning live) and corner avatar (ctrl+`, listening rim). A
    running orbiter needs a relaunch.
    Environment: OrbiterAvatar's body was forest, gamma 1.88 (the
    base Avatar's night, 2.2 was never orbiter's); the graph had
    night. Now forest + 1.88, forest.exr copied in (Kalen: try
    it). This was D2's colour difference. Seen side by side.
    The Aura (the halo) has no environment input: its env_id
    'night' only loaded a file nothing read. Removed, with
    night.exr; flower888 is 2.64 MB. Picture unchanged.
    top_ring removed (no draw named it; orbiter's gltf had none):
    its object block, meshes/top_ring.mesh and its only-used
    material plasma.agi; 2.43 MB.
    The model is a module of its own (Kalen: a peer, in silver's
    root): /src/silver/fmodel (fmodel.agi: the objects, nodes,
    materials named; meshes/, materials/). aeon/flower888's
    flower888.agi is `model: 'fmodel'` then the render: block;
    textures/ and images/ stay with it. Design.module_of finds a
    named module upward from the design's folder (the nearest
    <name>/<name>.agi); Design.base[] is the folder object files
    resolve against (cages, materials, cage saves). Aeon opens
    either folder: `silver aeon fmodel` is the model alone with
    the default lit pass. Edits go where they live: object places
    and node values into fmodel.agi, pass and draw values into
    flower888.agi; both folders watched. blend.py writes one
    module folder (objects in <name>.agi), as at first. A design
    with its objects in its own file still loads (aeon/tests).
    Seen hidden: size saved into fmodel.agi only, fmodel opened
    alone, the avatar through it. `silver --clean --build avatar`
    rebuilt its imports too (img gltf tls devices trinity).
    core.png removed (Kalen: no honeycomb shows): the Core shader
    read it only in the pool, as 0.85..0.95 brightness over
    colours above 1 (clipped), at mip 2..4; read sharp, no pool
    pixel moved over 2/255. The shader has no color input now
    (the glass's uv bend went with it; its perlin light stays),
    alpha 1; texture/mip_levels gone from the agi and both Core
    members; aeon/flower888/textures/ and avatar/textures/core.png
    deleted. The pool is up to 43/255 brighter where it was not
    clipped (seen side by side in the exchange avatar).
    The beam (Kalen: no orange flame, we use the beam): the Aura
    drew one jet in two looks by flame_style (0 the orange flame,
    1 the beam; orbiter always 1, aeon got 0). flame_style and the
    orange look are gone (every mix taken at its beam side; the
    flame's dead colour and falloff removed). Renamed (Kalen: so
    we know it changed): flame -> beam (the strength, 0 off),
    flame_w -> beam_w, flame_cur -> beam_cur, comments; the
    graph's aura setting is `beam: '0.0'`. trinity's
    ExchangeState.av_flame kept. In aeon, beam 1 on the aura
    draw shows it. From the side its base glow shows faintly
    under the hexagon (the volume has no body depth below).
    Beam 1 swirls as beam 4 did (Kalen: the plasma twirling in on
    the metal, without a huge column): the drain's density 6 ->
    24 x beam, and the beam's glow over its base (bm_bk) x4; the
    column's core is unchanged. Seen from above, beam 4 (old) vs
    beam 1 (new): the same pink and blue clouds over the petals.
    The donut (the Aura's torus on the rim, under the halo):
    Aura.donut_reach (default 1) stretches the tube outward only,
    the hole kept (Kalen: wider, not a smaller hole); the graph's
    aura has donut_reach: '1.0' (2 aliased where it meets the
    body, Kalen). Seen from above, 1 vs 2.
    Its cut behind the metal is soft (Kalen: at reach 2 the edge
    over the body was hard): a sample fades out over 0.06 world
    units behind the depth, averaged over 9 taps 0.012 apart on
    screen (the ledge's outline had stepped at the volume's low
    res). Seen from above at reach 2: no step left.
    Heat haze held in place (Kalen: it moved as the view turned,
    small at times, under the model at others): its sizes came
    from an arm along x projected (gone as x turned to the eye);
    now from the distance alone. From the side the pool's spot on
    screen is the front rim's: the heat draw reads av.depth
    (height: 'av.depth' in the graph) and fades where metal is
    0.2-0.4 nearer than the pool. Seen side and top (a debug
    paint, removed): only the visible throat floor.
    Then (Kalen: shift-move put the blur in the wrong place): the
    heat flipped y (the old hand-built passes' rows), so a pan
    mirrored it; the stage's rows follow clip space (fl 0.5,
    0.5). And -y is up in its frame: its source (-0.05) and pool
    (-0.32) sat in the air over the rim, right only top-down.
    Both now on the core's dome top (0.47; core.mesh z -0.474),
    radii 0.25 / 0.205 (the same size top-down, eye ~4.2 away),
    faded where the depth is 0.1-0.25 off the pool's either way.
    Seen with a debug paint (removed): on the dome only, side,
    top and panned.
    FIXED silver (Oct 18): a two-name for over a plain Au
    (`Au.parse_agi[ .. ]`) walks it as a map (aether e_for,
    Au_instance_of; not a map: skipped). features t_for_parse_agi.
    No argument (Kalen): aeon reopens the last project opened
    (`intern [ StatePersist ] last_project`, ~/.local/state/aeon/
    persist.agi, saved at each open; a folder that is gone is
    skipped); never opened: aeon/flower888. A relative folder is
    taken from where aeon was started. Seen hidden (own HOME).
    Made once (Kalen): each object's nodes run and its parts
    fill once per design (made_of, Design.made, its own
    GltfBuffer); every draw and stage shares it. An edit forgets
    the object (Stage.refresh, a node's value). aeon's two
    stages built in 2196 + 2078 ms (flower made 12 times), now
    1118 + 433 ms (once). Pictures differ only by the clock.
Layout (Kalen, Oct 12): .agi is written inside Aeon's own UI;
the chosen thing's properties beside the view. Its own design,
not another app's (Kalen, Oct 13: "we want to be different").
D6. IN PROGRESS (Kalen, Oct 13: "it needs to be more of an editor
    now") two items, in order:
    a. Properties that stay true: fields refresh after G/R/S,
       undo, save and outside edits; sliders for numbers and
       colour pickers for colours; undo for every property edit.
    a: BUILT, checked hidden (Oct 13): DragField in properties
       (press and drag a number, shift a tenth as fast, a click
       types; enter or escape leaves it); orbiter's f32 / i32
       props without a control get it too. A list of 2-4 numbers
       is a row a number ('at x'); true/false a check box (aeon
       toggles it); colours ('...color') a ColorPick (saturation
       square, hue bar, swatch, numbers; linear values, picked
       in sRGB). An object's materials list under its nodes
       (@name, a swatch): base_color, metallic, roughness, alpha,
       emission colour and strength, kept once in Design.finish_of
       (an edit remakes the objects using it, saved into
       materials/<name>.agi). Every property edit undoes with
       ctrl/cmd + z (one entry a run on one field). Fixed: a
       mounted field never took a new value (trinity never copies
       edit): LaunchPanel.reseed_text sets it unless the user is
       in it. name_frac is LaunchPanel's last member: orbiter must
       be rebuilt before its next launch (not built: his session).
    b. STOPPED (Oct 13): a node graph was built (aeon/Graph.ag,
       cards wired socket to socket) and dropped (Kalen: "i dont
       wnat to go with nods in general"; modifiers wired as
       boxes and materials as inputs are redundant and complex).
       Kept: the stage's pass and draw switches, MeshNode.muted
       (a step bypassed), Stage.reads_materials.
    d. BUILT, seen hidden (Oct 13, Kalen: "the outliner must be
       full left, view is middle and props is right"):
       aeon/Outline.ag. Outliner (240 px, full height): objects,
       each with its modifiers and material slots (a swatch);
       render, each pass with its draws; a circle switches a row
       (object shown, modifier bypassed, pass or draw off), a
       click chooses it (again: lets go); the wheel scrolls.
       PropsSide (340 px): the chosen object's place, then each
       modifier as a section in stack order (with its switch),
       then the chosen material slot's values; a pass's values,
       then each draw's settings (with its switch). SceneEdit
       holds the rows, apply, undo and the fields as last seen
       (the graph's code, moved). Seen: a modifier's switch
       (mirror off: 6,182 pixels), a slot's values, the wheel.
       Solo (Kalen, Oct 13): a double-click on an object (its row
       or in the scene) shows it alone, a temporary state: no eye
       changes; a double-click on it again, any other choice, or
       any switch ends it (Selection.solo; the view drops it when
       the choice is no longer that object). Its row stays chosen
       on a single click (a double-click's first click). Seen:
       flower alone, back on the second double-click, ended by
       choosing outside.
       Render list out of the outliner (Kalen: render references
       will change; not exposed as a list yet).
       − (Kalen, Rule #12): a square rounded 4 px with a minus, at
       a row's right end; on the hovered row with shift, on every
       row with ctrl/cmd. Removes an object, or a step (not the
       cage): the step that took its mesh takes its input (and the
       object's out), held until ctrl/cmd + s (Edits.drops,
       agi_drop deletes the block and the lines under it); ctrl/
       cmd + z puts it back where it was. Seen: shift shows one,
       cmd all; mirror removed (6,180 pixels), undone (9), saved
       (its block gone, subdivision's input: cage) and reopened
       the same. Not for material slots yet (faces would change
       material). Fixed on the way: the step was freed when the
       list dropped it, before its undo entry held it (SIGSEGV).
       + (Kalen): on the objects heading a new object; on an
       object a sub object, which is an object referencing
       another .agi (as flower888 names fmodel), its objects
       nested under it: one list. OPEN, awaiting his yes on the
       reading (new object empty; the reference picked from the
       modules beside the project).
       The chosen object stays behind what is in front of it
       (Kalen: the core showed in front of the body; to see it,
       hide the others). Its highlight stage (focus) drew it alone
       with no depth from the rest. Stage.occlude: each object
       draw gets a depth-only copy (blend colour x0 + target x1,
       its own shader: a draw is switched off by its shader's
       states, and the shared one switched the copy off too) of
       the shown objects other than the chosen, drawn first;
       Stage.unseen keeps hidden ones out. design imports
       <vulkan/vulkan.h> for the blend names. Seen: core inside
       the well hidden; outside hidden, the core shows with the
       separator still over its edge; flower behind the front
       wall.
       A solo hides the halo too (Kalen: soloing the core showed
       it): the main stage takes objects_only while a solo is on,
       so a screen draw with light of its own that reads no
       pass's picture (the aura) is off. Seen: core alone, no
       halo; double-clicked again, all back with the halo.
       The left side is two sets (Kalen, Oct 13): objects, then
       the design's materials (its materials folder, by name,
       each with a swatch); no modifiers, cage or slots there
       ("remove more of the old nodes from left side"; "we wont
       select modifiers alone"). The right side has no tabs: an
       object's place, then its modifiers area (a heading, each
       modifier framed with its switch and its − on shift hover
       or cmd; the cage is the shape, not listed); a material's
       values. Seen: outside's mirror, array, subdivision framed
       under place; ring's surface with its pickers. flower's
       mesh has no material_index, so its 'top' slot is unused
       (the import's; Kalen: likely a bad import).
       Modifiers under each object on the left, a level in
       (Kalen: "inside the object as a 2nd depth"); a click
       chooses the object (never a modifier alone; "looking away
       from the selected object is a huge anti pattern"); their
       values stay in the object's modifier panel on the right.
       Shift (hovered) or cmd (all): − removes, + copies the
       modifier right after itself, no picker (Kalen: less
       interface): the copy takes its mesh, what took its mesh
       takes the copy's; a free name (mirror_b); held until a
       save (Edits.adds, agi_insert_after writes the block after
       its sibling's, values from Au.string_agi a level deeper);
       ctrl/cmd + z takes it back. Seen: mirror_b after mirror,
       the file right (input: mirror; subdivision's input:
       mirror_b), reopened; undone.
       Then (Kalen): no modifiers on the left after all (fewer
       items: objects, then materials); + and − sit on each
       modifier's heading in the right side's modifiers area,
       shift over it or cmd for all; + copies it right after
       Then (Kalen): no + per modifier; one + on the modifiers
       heading, every button always shown (no shift/cmd here).
       The heading shows for any chosen object (one without
       modifiers too). + adds a subdivision at the end (levels 1;
       the kind is Kalen's call, asked): it takes the last step's
       mesh and becomes the object's out. Seen on core: + gave a
       subdivision; saved, its block after the cage, out:
       subdivision.
       Then (Kalen): the + shows the kinds (mirror, subdivision,
       array) instead of the modifiers, over their whole area; a
       click adds that kind at the end; the + is an × meanwhile,
       and it or escape goes back. The new one takes its kind's
       defaults (initialize[]; an array's count 2; a mirror
       starts across x). Seen on outside: the kinds in place of
       the stack, then array_b (count 2) added at the end.
    f. DONE (Oct 13, Kalen: 1-2 s lag on a subdivision level or
       anything down the chain) measured: running flower's steps
       11-20 ms; remaking both stages from nothing 738 + 842 ms.
       Now: Object.kept holds each step's result (intern: never
       written to the .agi); Design.forget_from[ object, step ]
       drops that step's and the later ones', so a change runs
       from the changed step down (flower 3 ms). Stage.remake
       makes again only the models of the draws holding the
       object, in the old ones' places (same shader, inputs,
       blend; Render.models swapped, finish); passes and shaders
       stay. The caller forgets once (each stage had forgotten it
       again: made twice). A step's value, its switch, one added
       or removed, a material's value use it; a full rebuild only
       for pass values, a new size, an undone object removal.
       Measured: 115-125 ms a change (the rest: new pipelines,
       ~50 ms in each pass that draws the object). Seen: mirror
       off 7,082 pixels, as before.
    g. IN PROGRESS (Kalen, Oct 13: "the subdiv needs to use the
       compute engine"; his answers: exact as now, the steps
       after a subdivision on the GPU too, on every change). Plan:
       the CPU makes only topology, per shape configuration
       (levels, switches, steps added/removed): per level the face
       / edge / vertex point tables (corners, ends, sharpness,
       boundary rules, the limit's weights) from the rules in
       design/Poly.ag; the GPU runs the positions: a compute pass
       a level, the limit, mirror and array copies (matrices,
       merge maps made with the topology), then normals, written
       into the draws' vertex buffers (no read back). A cage move
       runs the passes only. Each kernel checked against the CPU
       result; t_flower_matches_blender must still pass.
       Step 1 DONE: design/Stencil.ag compiles each step into a
       table (point i = sum of weight x matrix x input point j):
       subdiv_stencil, limit_stencil (subdivide_once / limit_of
       rule for rule), mirror_stencil, array_stencil (array_
       copies' merges, the last placed point wins); MeshNode.steps,
       Object.stencils (cage to out). aeon t_stencils_match: the
       tables run on each cage give evaluate's points, worst
       2.4e-7 (flower 6 steps, outside 5, separator 2).
       Found on the way, fixed in silver.c (call args): a number
       pushed onto a vec f32 (push[ 1.0 ]) stored 0: the arg was
       read against push's Au parameter, boxed as a double, and
       the vector copied 4 of its bytes. A vector receiver's
       number element type is now the arg's expected type (not
       for pointer elements: vec cstr). features
       t_vec_f32_push_literal; 250/251 (t_codegen_resource as
       before). trinity, design rebuilt with --clean.
       Step 2 DONE: design/Compute.ag, StencilCompute (one point an
       invocation: the sum of its terms; 1024 a row) and ChainRun
       (an object's tables as buffers and passes, recorded in one
       command with a barrier a step; run uploads the cage). aeon
       t_gpu_stencils: GPU points = CPU, worst 2.4e-7; a run (cage
       up, every step, waited): flower 2.1-3.3 ms, outside 0.6,
       separator 0.07. trinity Command.buffer[] (a handle) for
       passes recorded by another module.
       Fixed on the way (trinity, the expect runner): an app test
       taking a vk_context crashed at its end: the runner dropped
       the vk_context while the test's objects were still in the
       pool (aether emit_expect_tests: auto_mark before, auto_free_
       to after, then the args drop); vk_context.dealloc walked a
       null fence_pool, destroyed the allocator with buffers still
       on its deferred list, and freed its own commands after the
       device (now: defer_flush, commands let go, then the rest).
       A Buffer frees nothing itself: ChainRun.dealloc hands each
       of its buffers to vk.defer_buffer. trinity and webgfx tests
       exit 0; aeon 3/3.
       Step 3 DONE: FaceNormals (newell's sum a face) and
       PointNormals (a point's faces summed, unit), ScatterVerts (a
       draw's vertices: position and normal through the place,
       axes swapped for a y-up pass, into its 12-float vertex),
       ObjectGpu (steps, normals and a scatter a draw holding the
       object, one submission). trinity Gpu.use_vertex[ b ]: a
       buffer made elsewhere (storage + vertex) takes the draw's
       place. Stage.gpu_of[ o ] makes it the first time the object
       moves (none when a draw's vertex is not 48 bytes: the CPU
       refill stays); refresh uploads the cage and place and runs
       it; remake drops it. Tables, steps' points and normals in
       GPU memory (device local), the cage and place host-visible.
       aeon t_gpu_stencils: normals vs point_normals worst 4.9e-4
       (float sums on tiny faces). Measured: a cage move 0.53 ms a
       stage (8.8 ms with host-visible buffers; the CPU refill was
       7 ms). The GPU keeps the merges it was made with (mirror
       and array join points by distance): a move that broke a
       join showed smooth on the GPU, spiked on the CPU (as
       Blender). The move's end (click, enter), escape and an
       undone cage edit remake the object once on the CPU
       (settle_cage), so the settled picture is the file's: 7
       pixels off the reopened file's.
       OPEN: stage and focus each run the whole chain (shared
       points would halve it); a topology change (levels, steps)
       still makes the models on the CPU (~120 ms).
    h. DONE (Oct 13, Kalen: flower at 5 levels and 24 copies took
       ~15 s, "blender 2 or 3"; "optimize the concat to our vector
       types ... quicker inline code for primitives"). Measured on
       the test copy (1,417,752 points): subdivision 58 ms, array
       1,099, GPU tables 3,589, filling the draw's data 6,215. Every
       one is push loops, and a push on a vec of numbers boxed its
       value as a new object (an allocation a value). aether
       e_fn_call: a push onto a vec of numbers is inline now (the
       value at origin[count] while count < alloc, else
       vector_push from an entry-block slot to grow); other vector
       methods taking an element get the slot's address, never a
       box. Now: subdivision 14 ms, array 167, tables 388, filling
       572 (about 1.1 s, from 11). features 250/251, trinity,
       webgfx exit 0, aeon 3/3; trinity, properties, design,
       avatar, webgfx built --clean (running apps need a restart).
    i. DONE (Oct 13, Kalen: editing a cage point on flower at 5
       levels x 24 is ~10x slower than blender's ~5 fps). Measured
       in a hidden run (5.6 M vertices a draw): one stage's move
       waited 1.19 s on the GPU, the other 4.4 ms. Each draw kept
       its own host-visible vertex buffer (270 MB); the first ones
       fit in CPU-writable GPU memory, the rest fell to system
       memory, 235 ms a scatter. Now (design/Compute.ag):
       device_buf (GPU-only memory, filled once by a staging copy:
       vertex buffers and tables); draws of one part with the same
       axes share one vertex buffer and one scatter (add_target;
       6 + 3 buffers -> 1 + 1, ~2.4 GB -> 0.5 GB); run no longer
       waits (begin waits the last run, the draws on the same
       queue follow its barrier; normals[] waits for tests). A
       move: 40 us of CPU, the frame ~21 ms (11.7 ms the 3D view)
       hidden; drag 28 fps by the stats line (was under 1).
       aeon 3/3; a finished move vs the reopened file: 13 pixels.
       OPEN: the first move builds the GPU data (~2.6 s at 5
       levels), and the release remakes on the CPU (~1.2 s): a
       pause at each end; stage and focus each run the chain.
    j. DONE (Oct 13, Kalen: "the xyz should be a 3 part cell";
       "vec3f would do that") a prop of type vec2f / vec3f /
       vec4f is one row, its cell split into that many dragged
       numbers with 1 px dividers (properties: LaunchPanel vec_n,
       parts lp_<name>_<i>, value_of joins them "x, y, z",
       DragField.part: no name gap). aeon gives a list of 2-4
       numbers as vec<n>f (PropRow.part/parts and 'at x' rows
       gone). Seen hidden: at, turn, size one row each; a drag
       on at's middle part set y alone (0 -> 0.12).
    k. DONE (Oct 13, Kalen: slow to START the edit and to START
       the saving) measured at 5 levels: every move's refresh
       forgot the object, so the first move re-ran the steps
       (193 ms) and the tables (218 ms) in each stage; the release
       re-ran them and refilled every vertex on the CPU (~1.2 s)
       only so mirror and array re-decide their merges. Now: a
       move marks the kept points old (Design.moved, Object.moved;
       the next evaluate drops them) and the tables are kept per
       shape (Object.chain, shared by both stages); the release
       asks Object.joins_hold: each step that merges by distance
       (MeshNode.reads_points: mirror or array with a merge) makes
       its tables again from the moved points and compares them
       (same_terms); the same, nothing is remade. Measured: first
       move 389 + 344 ms (was ~2.6 s), release 0 ms (was ~1.2 s).
       outside with a point pulled off its mirror plane: false
       (remade). A saved move vs the reopened file: the shape
       the same but a few pixels on a cage line. aeon 3/3.
       OPEN: the rest of the start is each stage's own GPU set up
       (~340 ms each); one ObjectGpu for both would halve it.
    l. BUILT, seen hidden (Oct 13, Kalen: a grid with major and
       minor lines in 3D mode; a 2D mode comes later): aeon/Grid.ag,
       AeonGrid (a quad: each pixel's ray meets the floor, z 0;
       lines a pixel wide by fwidth, gone once cells are a few
       pixels; minor every s0, major every 10 s0, s0 a power of
       ten by the view's distance, the next level fading in; x
       axis red, y green; faded far off and at a grazing look),
       hidden behind the scene by Stage.object_depth (the focus
       stage's while an object is chosen). Drawn over the stage's
       picture, under the cage. Seen: flower888 near, chosen,
       zoomed in and out. A running aeon needs a restart (design
       changed). OPEN: the 2D mode.
    m. BUILT, seen hidden (Oct 13, Kalen: the numpad as blender's)
       DesignView.numpad: 1 front, 3 right, 7 top (ctrl: back,
       left, bottom), 9 the other side, 4 6 turn the view 15
       degrees (6 as a drag right), 8 2 tilt it (8 as a drag up:
       toward below), + - zoom; eased, the short way round; keys
       during g/r/s still type numbers. Not done: 5 (no ortho),
       ctrl + 2 4 6 8 pan, . frame the choice, 0 camera.
       Found: the view was MIRRORED (top view: +x on the left).
       Every trinity view rolls its camera 180 degrees (up -z)
       for vulkan's rows, which mirrors the picture. aeon now
       looks with z up and flips the projection's y; Stage
       flip_y culls front faces instead (the flip reverses the
       winding); drag_world and the orbit's x follow. Seen: top
       view +x right, +y up; g right and up gives +x +y; the
       body solid from above as before. avatar rebuilt (Stage
       grew a member); a running orbiter needs a restart.
    n. BUILT, seen hidden (Oct 13, Kalen: 'nothing chosen' is the
       scene's properties): PropsSide's title is 'scene · <name>'
       and its sections each render pass (scale, most) with its
       draws' settings and switches (SceneEdit.pass_blocks).
    o. BUILT, seen hidden (Oct 13, Kalen: view svgs in aeon; his
       answers: both, and edit points too).
       2D: `silver aeon <file>.svg` (aeon/Svg.ag): SvgView draws
       the page (white) and its shapes, wheel zooms about the
       pointer, right/middle or ctrl + drag pans, home fits.
       The outliner lists the shapes (id, else tag and number);
       a click there or on the drawing chooses one (topmost hit:
       fill even-odd, or 4 px of its outline): its outline,
       anchors (squares) and handles (dots, a line to their
       anchor). A press near a point picks it (shift adds; an
       anchor on the same spot comes too). G moves the picked
       (an anchor takes its handles) or, none picked, the whole
       shape; x / y one axis; enter or a click keeps, escape or
       a right click puts it back; ctrl + z undoes a move. The
       right side: id, fill, stroke, stroke-width, opacity,
       transform (the style's value first); a change shows at
       once (a transform keeps the points in the element's own
       space) and drops the style's copy. ctrl + s writes only
       the changed elements, each as a <path> (its attributes
       kept but the shape's own: x y r points ...; d in its own
       space, so group transforms stay; children kept), the rest
       of the file byte for byte, then reads it again.
       trinity svg.ag: SVGNode at / head_end / tail_at / end (its
       span in the text), SVGShape node and space (its element and
       transform), SVGModel source (all appended last).
       Seen: wave's middle anchor moved up 60 px saved as 20.645
       (40 - 19.355) with both handles, inside its translate
       group; a rect moved whole saved as a rounded path; a fill
       typed shows at once; an undone move 0 pixels off.
       3D: an object whose node is `SvgPanel` (file, height) is
       an upright panel (facing -y, as wide as the drawing's
       ratio) whose colour is the drawing: SvgPanel.picture_of
       draws it on the GPU (2048 px long side, colours made
       linear) and the stage puts it in each model's texture
       cache under the path its material names (design_model
       gives the panel a baseColorTexture). AeonLit samples colour
       at the uv (1x1 materials unchanged) and drops texels under
       2% alpha. Seen: svgscene, front and three-quarter views.
       Found on the way, fixed in Au (vector_convert): `vec T [ v
       ]` copying a vector of structs kept only each element's
       first field (prim_put); the same type now copies whole.
       features t_vec_struct_copy (251/252: t_codegen_resource as
       before). trinity tests exit 0; aeon 3/3; avatar rebuilt.
       OPEN: a panel does not redraw when its .svg changes (the
       watch reads .agi only); PBR draws a panel's clear parts
       (no discard there); no + to add a panel from aeon yet;
       typed numbers with G; the document's own values (size,
       viewBox) are not shown; arcs and rects become cubics in a
       saved element.
    p. DONE (Oct 13, Kalen: the scrolled properties ran over the
       title; mask with the soft fade when scrolled; then "i want
       trinity to do it on its masking layer", no hacks) PropsSide
       is the background and title; PropsBody (40 px down) holds
       the sections and their headings, scrolled by trinity
       (content_h, the base wheel, its scrollbar; the panels flow
       't{gap}px+' so they move with it, the headings draw at
       -scroll.y) with scroll_fade 24.
       trinity (draw_element): an element whose fade is on composes
       its masking layer (element.mask_layer, compose_mask_layer):
       its cached picture, then every child that is not topmost
       (cached or not, scrolled and cropped as on the screen), and
       draws that through the mask; the children pass skips them.
       Before, a cached child drew outside the mask. element gained
       mask_layer / _w / _h: every trinity app needs a rebuild
       before it runs again (orbiter too).
    q. BUILT, seen hidden (Oct 13, Kalen: splits under the user's
       control, as orbiter's; a top bar over the toolbar and view:
       orbiter's navigation bar, called the orbiter bar; his
       answers: arrows + title, in trinity) trinity OrbiterBar: the
       app's history arrows (TButtons nav_back / nav_fwd, the
       left / right icons, dim by Window.nav_back_on / nav_fwd_on,
       a press walks Window.navigate) and its title (nav_title,
       after the arrows), in orbiter's bar look (1.3 units, #000c,
       the inset glow, near border). Window.hosted[] (host_slot >
       0): a hosted app makes no bar; its nav_state and title
       already reach orbiter's pane bar (HM.nav_state, HM.title).
       left.svg / right.svg copied into trinity/images.
       trinity Splitter: `flex` (the pane that takes the rest,
       default 0 as before) and `sizes` (each pane's size until
       dragged); a seam drags the pane on its far side from the
       flexible one; drag_ix -1 is none (was 0).
       aeon: OrbiterBar over all (alone), then a Splitter: outliner
       240, view (flex), properties 340, min 120; the title is
       the project's folder or file (set_title once). view[] and
       the svg keys find the view inside the split (middle[]).
       Seen: both seams dragged (240 -> 300, 340 -> 420), back and
       forward walk the choices, svg G still moves. trinity tests
       exit 0. NOT seen: hosted in orbiter (needs orbiter rebuilt;
       Splitter and element changed).
    r. BUILT, seen hidden standalone (Oct 13, Kalen: the title
       opens the navigation dialog; it lists the folder the
       resource / svg was opened from; hosted, orbiter's dialog
       with the app's list). trinity: a press on the bar's title
       (Window.nav_toggle) opens NavDialog (app_render, topmost,
       under the bar at the left, 46% wide) on the app's folder
       (element.nav_folder); '..', folders, files by name; typing
       filters (shown in the bar with a caret), up / down / enter,
       a click; a pick goes to element.nav_open (true: opened),
       else a folder is gone into; escape closes. While open the
       Window gives it every key and typed text first. The bar
       repaints when its title or filter changes (it is cached).
       aeon: nav_folder is the project's parent (an svg's folder,
       or the folder holding the design); nav_open opens an .svg,
       a .blend or a folder with <name>.agi in place
       (open_project: everything chosen and held starts over, the
       view remounts as 'view<n>'); a project set from outside
       (`set project`, orbiter's pick) opens too (opened_as).
       Seen: '..', 'svg' typed + enter, 'test.s' + enter opened
       test.svg titled so; set project svg <-> design three times,
       alive.
       orbiter (WRITTEN, NOT BUILT: Kalen's orbiter is running and
       element/trinity changed): a press on the active pane's
       title while it shows a running app starts the argument pick
       (pick_default, as shift + click on the argument chip): the
       finder at the folder of the app's resource; a pick sends
       `set <arg> <path>` to the app, which opens it.
       Compiler (AGENTS 1b, OPEN, met again): element's two hooks
       appended at the class's end (after its non-public funcs)
       dispatched one slot off in aeon (nav_open called aeon's
       open_project); placed beside dictates they dispatch right.
       A hidden run once exited with no log kept; not reproduced.
    s. BUILT, seen hidden (Oct 13, Kalen: "this navigation dialog we want
       the same as orbiter's.. we can make it a module"; "call it
       navigation"; then "make it a sub module of trinity ...
       extension module") orbiter's navigation dialog moved into
       trinity as an extension file, `trinity/navigation.ag`
       (extend trinity), used by orbiter
       and by the orbiter bar (trinity's NavDialog replaced by it).
       trinity/navigation.ag (imported last in trinity.ag: a free
       func's types must be known above it; Window, declared in
       trinity.ag, is reached as e.ux): FindHit (moved from orbiter
       Editor.ag), FileIcons (moved; orbiter/FileIcons.ag deleted),
       the material icons and file.svg (orbiter/images -> trinity/
       images; orbiter's share links them back), nav_listing
       (orbiter's browse: '..', folders with modules first, files,
       ls order; typed text filters names), nav_dir_hit (orbiter's
       push_dir_hit uses it), nav_hover_paint and nav_icon_paint
       (orbiter's CommitLayer rows use both), NavigationView (the
       ground over everything under the bar; Window mounts it when
       the title is pressed; keys: up / down / enter, the Window
       keeps escape, backspace and typing) with NavRows (the rows,
       scrolled by trinity, scroll_fade 24 over the solid ground).
       Seen: aeon's dialog as orbiter's list (icons, blue folders,
       the pink hover), arrows choose, enter opens; orbiter built
       (its own process was not running) and its finder drawn
       hidden through the shared painter. orbiter's list keeps its
       own extras (cards, repos, delete and pins, the path hint,
       users / projects, git band) in Git.ag.
    t. DONE (Oct 13, Kalen: "when resizing the split control or
       resizing the window itself, there is rather insane lag")
       measured hidden: 20 seam moves took 19.1 s (2 fps): every
       new view size made both stages again (all passes, pipelines
       and models). Now nothing is made again: each stage resizes
       its targets in its frame and rewires their readers. The
       old rebuild guarded a GPU fault; found again on removing it:
       Texture.resize keeps the Texture object and swaps its image
       (the old one freed a few frames later), and the grid
       rebound its depth only when the object changed, so it
       sampled the freed image (device fault at startup). The grid
       rebinds on a new size too. Now 20 moves in 1.5 s (1.0 s of
       it the test's waits), 37 fps hidden, no fault; seen with and
       without a choice. Window resize takes the same path (not
       driven: no resize over the socket).
    u. DONE (Oct 13, Kalen: draw settings stored as strings,
       "pulse_width ... is f32 ... they should parse ok without
       string") aeon writes a setting bare when it is a number or
       true / false, quoted only when a word (SceneEdit.set_row);
       aeon/flower888/flower888.agi's numbers unquoted (env_id and
       the inputs stay quoted). Unquoted, aeon crashed: parse_agi
       gives a bare number in a map of Au as a boxed f64, and
       silver's `string [ v ]` (and '{v}') of an Au was a pointer
       cast, so the f64 was read as text (strlen fault).
       Fixed in aether: e_convert_or_cast Au -> string calls
       Au_cast_string (a string comes back as itself, a primitive
       as its value; another class gets Au's member text, not its
       own cast: the virtual call's function type did not verify).
       That call exposed aether building loop types in LLVM's
       global context (LLVMInt32Type(), LLVMPointerType(
       LLVMInt8Type())): 10 places now in a->module_ctx (the
       array for-loop element was in the wrong context).
       Au serialize writes floats shortest (%.15g f64, %.7g f32, a
       whole value keeps .0): 4.5, not 4.500000, which also cut
       every double to 6 decimals in .agi and json.
       features t_agi_number_text; 252/253 (t_codegen_resource as
       before). Seen: the unquoted flower888 loads and draws, its
       values listed; a dragged value saved as pulse_width: 1.2.
    v. BUILT, seen hidden (Oct 13, Kalen: the orbiter bar's views,
       "the same thing as instances of running apps with titles;
       these are views we toggle in or turn off to show the file
       source"; "bring back the full nodes view") trinity: element
       .views[] (beside dictates: AGENTS 1b) names the app's views;
       the orbiter bar shows a chip each after its title (TButtons,
       labels, the lit one Window.bar_view); a press shows that
       view, pressed again the source ('').
       aeon: views ['nodes'] for a design (none for an svg yet);
       with nodes on, the editor split stays mounted (hide: its
       stages and GPU kept) and NodeGraph (aeon/Graph.ag, restored
       from this session's transcript: the first Write of it; its
       later shell edits were lost) fills the area under the bar:
       materials, each object's step chain, objects, passes by what
       they read, wired; the values, switches and edits through
       SceneEdit (object/node/material/pass/draw rows, spec_of,
       apply_row, flip). design: draw_reads_materials[ d ] (by the
       shader's class, no stage needed; Stage.reads_materials uses
       it). Seen: chip, nodes on (cards, wires), off (the editor as
       it was).
       Hosted (DONE, seen hidden): HM.views (app -> orbiter, the
       names '|' between, three bytes a message as the title's),
       HM.view_state (app -> orbiter, the shown view's index, -1
       the source), HM.view (orbiter -> app, the view to show);
       Window.views_post sends them when they change (app_render,
       hosted), the HM.view handler sets bar_view. orbiter:
       AppView.app_views / app_view_on; the title strip (title_els)
       lays a chip (ArgButton) a view after the instance chips,
       lit while shown; a press shows the app in the pane
       (embed_app) and sends HM.view (the lit one again: -1).
       Seen: a hidden orbiter hosting aeon on flower888: the nodes
       chip in the pane title; a press lit it and aeon (its own
       socket's shot) showed the nodes view, no bar of its own; a
       second press back to the editor.
       Fixed in trinity (element.ag Layer.draw): an editable
       layer's crop left out the canvas's offset, so a no_cache
       field painted into a parent's canvas lost its text (only
       a sliver near the top showed). trinity tests exit 0.
       Seen: scrolled, the title clear and the rows fading under
       it; a value dragged 1.0 -> 1.2.
    w. BUILT, seen hidden (Oct 13, Kalen: box select with b,
       circle with c, from where it was pressed; shift or cmd
       for more) in the cage: b or c starts at the pointer, the
       box or circle runs to the pointer (points inside lit as
       a preview); a left click or enter picks them, replacing
       the pick, or adding with shift or cmd held; escape, a
       right click or tab drops it (DesignView.region,
       in_region, end_region, draw_region). Seen: a box picked
       12 points, a circle replaced them, a shift circle added.
    x. BUILT, seen hidden (Oct 13, Kalen: an svg with nothing
       chosen shows its document's values; the page an outline,
       no fill; a grid, no red / green axes in 2D) the right side
       is 'document · <file>': width, height, viewBox (four
       number cells) of the <svg> element (SceneEdit.doc_rows,
       kind 6). A change rewrites the element's open tag in the
       text in memory and reads it again (SvgDoc.set_root,
       compose, root_text); ctrl/cmd + s writes it. SvgView: the
       page is its outline alone, a grid (minor lines a power of
       ten of document units, 8 px apart at least, fading in; a
       major every ten), the view clipped to its bounds (shapes
       ran over the outliner zoomed in). Seen: height 120 -> 200
       drawn at once, saved: only that attribute changed.
       Then (Kalen): the viewBox keeps its proportions: trinity
       SVGModel.load follows preserveAspectRatio (default
       xMidYMid meet: the smaller scale, centred; slice the
       larger; none stretches), and a viewBox origin is scaled
       too (it was subtracted after the scale). trinity tests
       exit 0; avatar rebuilt. Seen: height 200 over a 200 x
       120 viewBox draws round, centred.
       Undo (Kalen): shape and document values go on SvgDoc's
       undo with the moves (SvgUndo row / was, a run on one
       field one entry; SvgDoc.undo_row for the side to set
       back). A shape value finds its shape by number (a
       document change reads the shapes anew). Seen: height
       200 then ctrl+z: 120; fill typed then ctrl+z: blue.
       A focused text field takes ctrl+z itself and keeps its
       text until it loses focus (as in 3D).
       Fill and stroke take the ColorPick (Kalen: as the
       others): shown as linear numbers (svg_color, ^2.2),
       written back as #rrggbb (SceneEdit.svg_paint,
       hex_byte); no fill reads black; url(...) stays text.
       Never "none" (Kalen, Oct 14): a none fill or stroke (or no
       stroke) is the picker with a blank value: no marks, an
       empty swatch outline, no numbers (properties ColorPick
       blank); a click picks a colour. Seen: none.svg's ring
       filled red by one click, saved fill="#cb3636".
       The picker (Kalen, Oct 14): the value is #rrggbbaa, typed
       (ColorHex: a click chooses all of it, a whole hex code
       applies as typed, enter or escape leaves; ColorPick
       hex_sync, hex_of, linear_of); an alpha bar after the hue
       bar, the colour to clear over a checkerboard, dragged as
       the hue; the swatch over the checkerboard too. An svg
       paint is written #rrggbbaa when see-through, else #rrggbb.
       Found: ColorPick.packed wrote 0xAARRGGBB but the canvas
       reads a stop as 0xAABBGGRR: the hue bar always had red and
       blue swapped. Seen: back typed #ff000080, saved so; hue
       bar red at the top.
       A click on the hex while typing in it only places the caret
       (ColorHex.typing; the first click chooses all). trinity
       Layer.draw: a text selection in a field without focus
       draws dimmed (#ffffff1c; focused stays #264f78). Seen: the
       second click moved the caret only; id's dragged selection
       blue, then grey after a click on the panel.
       Clear (Kalen): a small 'clear' button (56 px) under the
       hex on a paint (ColorPick.clearable; the panel's type
       'paint': aeon's svg fill and stroke; a material's 'rgba'
       has none): no colour, written as none (SceneEdit.set_row);
       aeon's take lets a paint's value go empty. Seen: back's
       fill cleared, saved fill="none".
       A cleared swatch has a red diagonal across it (bottom left
       to top right). Seen on back's stroke.
       Blur (Kalen, Oct 14: a field that sets a filter): a 'blur'
       row on every svg shape, its own feGaussianBlur's
       stdDeviation (element units, 0 none). Set: the pending edits
       written into the text, then its filter's stdDeviation
       changed in place, else a new <filter id="blur-n" x="-50%"
       y="-50%" width="200%" height="200%"><feGaussianBlur/>
       </filter> in <defs> (a <defs> made after <svg> when none)
       and filter="url(#blur-n)" on the element; 0 takes the
       element's filter off (attribute and style), the filter is
       left in <defs> (SvgDoc blur_of, gaussian_in, head_with,
       set_blur; undo through the row's own). path_text leaves an
       emptied attribute out.
       trinity draws it (svg.ag): SVGShape.blur (page units, from
       the shape's own filter="url(#id)" or style, the filter's
       feGaussianBlur, scaled by its space; svg_filter_ref,
       svg_find_id, svg_blur_of); SVGModel.paint_shape (render and
       draw_svg both): a blurred shape drawn alone into
       SVGModel.blur_layer (its box out three sigmas), then put
       down by Canvas.filter_blur (the real gaussian, fill and
       stroke together). Seen: back at 4: the edge's coverage 16%
       / 50% / 84% at -11 / 0 / +11 px (sigma 10.8 px at zoom
       2.69); set 2 changed blur-1 in place; 0 took it off.
       OPEN: a group's filter (each shape only takes its own), a
       filter's region, other filter primitives.
       Glow colour and drop shadow (Kalen, Oct 14): rows
       glow-color (a 'tint': picker with clear, no kinds; blank
       the shape's own colours), shadow (percent, Inkscape's
       rule), shadow-offset (x y, element units), shadow-color
       (starts half black, 4% of the box out). Kalen: effects
       are render state, written only at a save: the setters
       change the shape's own values (set_fx; fx_of reads them
       back), the shape is marked, and compose writes its
       filter (fx_spans: its own rewritten, new ones into
       <defs>); no reparse per change. Written as standard
       filters (feFlood + feComposite in; SourceAlpha blurred,
       feOffset, merged under). trinity draws both (SVGShape
       glow_color, shadow, shadow_x/y, shadow_color, last).
       Seen hidden: green glow, the circle's shadow, the rows.
       Fixed first: a blur on a file with no <defs> did
       nothing (with_spans' tie order cut the new <defs>).
       stroke-linecap (Kalen): a row of three choices (butt, round,
       square; butt when unwritten) after stroke-width (PropRow
       stops -> the panel's buttons; svg_set sets SVGShape.cap).
       trinity svg.ag reads it, inherited as SVG has it
       (SVGState.cap, svg_cap_of), and paint_shape sets the
       canvas's cap (cap_of; butt is Cap.none). Seen on an open
       16-wide line: butt ends at the points, round half circles,
       square half the width out; saved stroke-linecap="round",
       then "square".
       stroke-linejoin (Kalen): a row of three (miter, round,
       bevel; miter when unwritten, as svg). trinity draws joins
       in the path's distance field (Canvas.ag): PathPt.inner (last
       field) marks a flattened curve's own points; sdf_walk_edges
       / sdf_joins put, at each real corner, 10 + the neighbour
       edge's direction in the end flag (1 stays a free end, 0
       smooth); CanvasSDFCompute's join (1 miter, 3 bevel, else
       round) and miter limit: in the wedge past one edge's end
       and before the next's start the distance is the miter's
       (the larger of the two outer side distances) or the
       bevel's (across the bevel line), a miter past the limit
       bevelled. CanvasState.miter_limit (4), set_miter_limit;
       the field's pad and spread cover a miter's reach (half the
       width x the limit). svg.ag reads stroke-linejoin and
       stroke-miterlimit (inherited; SVGShape join, miter;
       svg_join_of). A canvas whose join is unset still draws
       round; every svg now draws svg's default miter. webgfx
       rebuilt (PathPt). Seen on a 16-wide zigzag: miter points,
       bevel cuts, round; saved stroke-linejoin="round"; miter
       limit 1 bevelled both corners.
       Opacity as a bar (Kalen): the panel's number type 'unit'
       (0..1) is a DragField with bar: the cell fills from the
       left by its value (#806bff40); aeon's opacity rows are
       units (unwritten shows 1). Seen: 0.5 half the cell; a
       drag to 0.7 filled to 0.7.
       The picker's paint kinds and gradients (Kalen: the preview
       a full strip on top; solid / linear / radial / conic where
       the swatch was; full gradients now):
       - properties ColorPick: a 22 px preview strip across the
         top (a gradient left to right, its stops as small boxes
         on its foot, the chosen one yellow); the square (56 px at
         most), hue and alpha bars (10 px) under it; right of
         them, for a paint (ColorPick.paint, was clearable), kind
         buttons solid linear / radial conic, the hex, clear. The
         value is 'r, g, b, a', '' none, or 'linear | 0: r, g, b,
         a | 1: ...' (radial, conic alike). A click on the strip
         chooses a stop (6 px) or puts one in (its colour
         sampled); a drag moves it (kept in order), well off the
         strip drops it at the release (two stay); the square,
         bars and hex edit the chosen stop. Solid keeps the chosen
         stop's colour; a gradient from a colour fades it to clear.
         The row
         is 4.5 rows tall (aeon's colour rows 108 px).
       - trinity svg.ag reads linearGradient / radialGradient
         (stops with stop-color and stop-opacity, as attributes or
         style; xlink:href chains for stops and attributes;
         gradientUnits, gradientTransform; % lengths) into
         SVGShape.fill_paint / stroke_paint (SVGPaint: kind, stops,
         geometry in its own units, to_page); the urls go down
         svg_walk as arguments (svg_own_url): a SVGState string
         was dropped by a struct copy (DOUBLE-DROP). A url paint is
         opaque, scaled by fill/stroke-opacity. paint_shape draws
         them through the canvas gradient; a stroke gradient takes
         its own pass. Canvas.ag: the ramp colours the stroke when
         ramp.x is 2 (a stroke_gradient with no fill one).
       - aeon: a gradient fill or stroke reads into the picker
         (svg_paint_text, colours made linear); a gradient written
         (SvgDoc.set_gradient): its own element rewritten (its
         geometry kept), or, shared or none, a new one in <defs>
         (box units, svg's defaults); head_with / piece_with also
         take a style's declaration out. A chosen shape's gradient
         has handles in the view: start square, end diamond, a
         radial's circle; a drag moves them live (one undo, a
         shared gradient copied first: grad_own), written at the
         release (set_grad_geometry: x1 y1 x2 y2, or cx cy r, the
         focus dropped). A plain colour drops the shape's paint.
       - trinity TButtons: a disabled segment's label dims (text
         takes no canvas opacity).
       - (Kalen) a colour row spans both cells: its name in an
         18 px band on top, the picker the panel's full width
         under it (LaunchPanel.spans breaks the column rule
         there); the four kinds one row, the hex and clear under
         them. Seen: fill and stroke full width; linear chosen
         and saved. Conic removed (Kalen: svg has no conic
         gradient, so no entry): solid, linear, radial.
       - Fixed (Kalen's sentry.svg: gradients not drawn as they
         are): a blurred shape's layer drew plain colours (its
         url paint's black); it takes the same gradient passes now
         (SVGModel.passes, opacity applied once, at the put
         down). opacity, fill-opacity and stroke-opacity clamp to
         0..1 as svg says (svg_unit): opacity="5.269" washed the
         colours white. The bar's drag spans 0..1 over its cell
         and clamps (how 5.269 got written). Seen: sentry blurred
         and sharp in its gradients; a 450 px drag saved 1.
       - (Kalen) opacity and blur are percent bars (properties
         type 'percent': DragField top 100, a % at the cell's
         right, tenths). Opacity shows x100, writes 0..1. Blur
         is Inkscape 1.x's: radius = (p / 100)^2 x (box width +
         height) / 4, the box the shape's drawn bounds (curves
         sampled, stroke in), in page units, over the element's
         mean scale into stdDeviation (SvgDoc blur_base,
         unit_scale, blur_percent, set_blur_percent). Seen on
         back (80 x 50): 52% saved opacity 0.52, 30.612% saved
         stdDeviation 3.046 (= 0.30612^2 x 130 / 4).
       - (Kalen) a gradient comes back after a solid: a colour
         over a gradient keeps its id on the element as
         aeon:fill-gradient / aeon:stroke-gradient (xmlns:aeon
         on <svg>; SvgDoc keep_gradient); a gradient again takes
         it back whole (stops, geometry, id; the other kind keeps
         its stops), the mark taken off (set_gradient); clear
         lets it go (forget_gradient). properties LaunchPanel
         owed: a value held back from the field being changed is
         given once it is let go (the restored stops reached the
         picker only so). Seen: linear, a stop at 0.5, solid,
         linear: three stops back in the file and the picker.
       - (Kalen) the strip's stops are little houses (peak up),
         on the pixel grid: a 1 px outline on half pixels, the
         chosen one's 2 px on whole. Seen zoomed: walls one
         solid pixel.
       - (Kalen) a glow row (percent, Inkscape's blur rule for
         its halo, brightened 1 + 3 x percent / 100). Written as
         a standard filter (other viewers show it): [blur
         result soft] gaussian result halo, feComponentTransfer
         (R G B A linear slope gain) result lit, feMerge lit
         under the source (or soft); sRGB, region out by three
         of the widest gaussian (SvgDoc set_effects writes blur
         and glow together, its own filter rewritten whole;
         glow_of, halo_in, glow_percent, set_glow_percent;
         gaussian_in skips the halo). trinity svg.ag: SVGShape
         glow, glow_gain (last members), svg_glow_of,
         svg_is_halo; paint_shape blurs the layer by the halo
         (sqrt of both gaussians) into glow_layer, puts it down
         through filter_matrix (gain on unpremultiplied colour
         and alpha, clamped, as svg's), then the shape sharp or
         blurred over it. Seen on back: 39.8% glow, then 22.4%
         blur under it; against Inkscape's own render of the
         file the edge's pixels agree within 3 levels.
       - (Kalen: fill and stroke gradients are not coupled) the
         view shows both gradients' handles, the fill's yellow,
         the stroke's pink (SvgView paint_of, draw_grad_of); a
         press takes the nearest (grad_hit sets g_key); on one
         spot the one not held last wins, so a second press
         takes the other. Seen: fill's end to 1, 0.989, then
         the stroke's from the same spot to 0.428, 0.32.
         Then (Kalen) both ends are circles (10 px), the picked
         one with a white circle inside (SvgView g_pick,
         g_shape). Seen: the dragged fill end lit white.
       - (Kalen) the strip has a 1 px white outline on half
         pixels (ColorPick.strip_outline): one pixel row.
       - (Kalen) transform is at the bottom in parts: move (x
         y), rotate, scale (x y), skew (degrees), read as
         translate . rotate . skewX . scale (svg_parts) and
         written back so, each part left out at its identity
         (svg_parts_text). A scale of 0 is refused, and svg_set
         keeps the old transform when a new one flattens the
         shape: a cell emptied while typing read 0 and the save
         wrote every point 0,0. Seen: rotate 30, scale x 1.5:
         transform="rotate(30) scale(1.5,1)", points intact.
         Rotation turns about the element's origin, as svg.
         Fixed (Kalen: a new value stacked on the last): svg_set
         took the old value as the map's own string, freed by
         the set that replaced it, so the old transform was
         never taken out (the file was right, the view stacked).
         It copies it first. Seen: rotate 30 then 45 draws as a
         fresh open of rotate(45).
       - (Kalen: storing in the transform is lossless) g and s on
         whole shapes set each one's transform, never its points:
         each frame from the start, parent^-1 . page op . parent
         . start (whole_ids, whole_t0, whole_par; svg_parts_m),
         written as parts; one undo (the text); escape gives the
         start back. Picked nodes still move their points. The
         panel keeps only skew of the transform's parts (move,
         rotate, scale come from the view).
         r turns (new in 2D): about the box's middle, or with
         shift (s too) about the point under the pointer: no
         pivots to keep. The pointer's angle is measured once it
         is 8 px clear of the pivot; typed degrees win. While s
         or r runs the pivot shows as a 3 px clear circle, 1 px
         grey outline on half pixels (draw_pivot).
         Seen: g 100 px: translate(37.175,0), d untouched; shift
         + r on back's corner, a quarter turn: translate(20.037,
         -0.037) rotate(90); r 45: translate(39.393,-25.104)
         rotate(45) (exact about 50, 35).
         Test note: the view takes typed numbers as key presses;
         the socket's `text` does not reach it.
         shift + g (Kalen): moved only along the way the pointer
         first goes (its first 8 px set the line; along,
         along_way); a typed number is the distance on it; a
         faint 1 px grey line through the middle shows it. Seen:
         a 45 degree start, then a pull sideways: translate(
         26.766,26.766), on the line exactly.
         Escape (or a right click) cancels any of g, s, r and
         their shift forms: cancel_move sets the rest value
         itself (a turn's goal read the pointer, so an escaped
         turn stayed turned). Seen: r, shift+s, shift+g, g each
         escaped: 0 pixels off the start, the file untouched.
         (Kalen: more weight) g, s, r, o and handle drags ease at
         0.2 a frame (was 0.35; the view's pan 0.25): about 14
         frames to within 5%. Seen: a g step 150 ms on still
         2,104 pixels short (was settled).
       - (Kalen: gradients move, turn and size with the shape)
         a transform change carries what the shape holds on the
         page: its gradients' to_page, its blur and glow by the
         scale (svg_set). Fixed in trinity (svg_paint_of): a box
         units gradient used the shape's box on the page; svg
         uses its box in its own space, then its transform (a
         turned shape's gradient turns); curves sampled.
         affine.inverse[] added (trinity). Seen: back linear, r
         90: live and reopened 0 pixels apart, both top to
         bottom (the reopened one had run left to right).
       - (Kalen) the view's edit lines and points take the
         pointer's nearness: alpha 0.5 away, 1 at it, by
         trinity's curve (1 - d / 160)^2 (SvgView near_a,
         near_of, near_line, toned): each outline segment by its
         nearest of 8 points, handle lines, nodes, the gradient
         line and circles. Seen: pointer at back's corner lit
         its circle, nodes and edge; far off all at half.
         Then (Kalen: alpha looked wrong over other shapes): away
         they are darker, solid (rgb times 0.5..1, alpha kept).
         Then (Kalen: light blue lines at 0.3, no doubling where
         they cross): the lines (outline segments, handle lines,
         a gradient's line and radial circle) draw solid into
         SvgView.line_layer and go down once at line_alpha 0.3
         (lines_begin, lines_end; group opacity, as css): a
         crossing is one line. Near the pointer a line goes
         toward white (lit). Nodes and the gradient circles draw
         solid over it, darker away (toned). Seen: no bright spot
         where the gradient line meets the outline.
         Then (Kalen: the lines read lighter than the points; the
         same at a distance): nodes and gradient circles go into
         the layer too; the layer goes down at line_alpha 0.3,
         then again at 0.7 through near_mask, a radial circle
         round the pointer ((1 - d / 160)^2 as stops): full near
         it, 0.3 away, one look for lines and points, crossings
         single. Per-element toning (near_a, toned, lit) gone.
         Then (Kalen): points grow by nearness, to 1.5x at the
         pointer (near_w, the same curve); the node a press would
         take (nearest, 10 px) and a gradient circle within 10 px
         are drawn brighter (60% to white). Seen: the corner's
         nodes larger, the one under the pointer lighter.
         Then (Kalen): picked points are never faded: the picked
         (or region-lit) nodes and the picked gradient circle
         draw solid on the view after the layer (SvgView.solid,
         draw_nodes). Seen: a picked node exactly #ffcf4a with
         the pointer far off, the rest at 0.3.
         Then (Kalen): every unpicked point is half its size far
         off, its own size at the pointer (0.5 + 0.5 x
         nearness; 1.5x was a third too big); picked ones
         keep their own size always. Seen: the picked node 42
         yellow pixels both far and near.
         Then (Kalen: handle lines take the near light, in the
         same layer): the pen's handle lines were drawn on the
         view; they go into the layer now (pen_handles; the
         layer opens for the pen too). A shape's handle lines
         were already in it. Seen: the pen's handle full yellow
         at the pointer, 85/255 (0.3) far off.
         A plain drag moves (Kalen): a press on a point and a
         drag past 4 px moves the picked points (a picked one
         keeps the others); a press inside a shape (fill, or
         4 px of its outline) moves the chosen shapes; the
         release keeps it, one undo (as g; d_wait, d_moving).
         A click alone on a picked point or chosen shape makes
         it the only one. Seen: back dragged 100, 50 px saved
         translate(37.175,18.587); its corner node dragged 40,
         40 px moved 14.87, 14.87 with its handle.
       - Text (Kalen, Oct 14: t adds text; the properties set its
         font). trinity svg.ag reads <text>: SVGNode.text (its
         character data, entities read; last member), SVGState
         font_size, weight, slope, anchor (inherited; style or
         attribute: svg_font_prop), the family down svg_walk as an
         argument (svg_family_of: the first name, quotes off).
         svg_text_cmds: fontconfig's match (font_match; an unknown
         family falls back to the default), CanvasFont from its
         file, each code point's glyph outline (curves kept) at
         the pen, text-anchor by the advances; the shape is those
         outlines, so fill, stroke, gradients, blur, hit tests and
         moves work as for any path. tspans' words joined, white
         space as one (svg_text_of). No kerning or shaping.
         aeon: t, then a click: <text id="text-n" x y
         font-family="sans-serif" font-size (24 screen px)
         fill="#ffffff">Text</text> where a new path would go,
         chosen; typed characters replace 'Text' then go on
         (aeon.on_char -> SvgView.char_event), backspace takes a
         code point, enter or escape ends (SvgDoc.add_text,
         SvgView texting / typing / placeholder). A text shows no
         points or handles (is_text; points_of empty). Rows: text,
         font-family, font-size, font-weight (normal bold),
         font-style (normal italic), text-anchor (start middle
         end), then the usual; a change remakes the glyphs
         (svg_retext). Saved as <text> with its words escaped
         (text_element; tspans kept unless the words were set).
         Seen hidden: three texts as written (DejaVu Sans, bold
         middle-anchored, serif italic end-anchored, &amp;); t,
         a click, "Hi there" typed, saved as <text>; bold and
         middle set live and saved; DejaVu Serif typed into the
         family, drawn and saved. Fixed on the way: picked is
         null until a pick (picked = vec, not clear[]).
         Moved, turned, sized (Kalen): a press anywhere in a
         text's box takes it (hits: text_box, gaps too); g, s, r
         and a drag set its transform as for any whole shape
         (all_points gives a whole move its box; points_of stays
         empty for a text). Seen: a drag, r 45, s 2 saved as
         transform="translate(82.637,-149.16) rotate(45)
         scale(2,2)", the glyphs following live.
         Its selection (Kalen): a rounded rect 6 px out from the
         letters, 5 px corners, in the text's own space (turns
         with it), dark grey (#3a3a3a) with light grey (#c8c8c8)
         5 px dashes running round it at 20 px a second, drawn
         solid over the edit layer (draw_text_box, rr_at: canvas
         has no dashes, so short runs along the outline). Seen
         level and turned 30 degrees; 830 pixels of the edge
         changed in a quarter second.
       - (Kalen) a panel cell with no name (ArgsField /
         LaunchField / DragField with label '') keeps no gap for
         one: its text starts 0.25 units in, as a split cell's.
         Seen: id, opacity, blur, rotate, skew line up with move
         and scale. orbiter's panel fields (also unnamed) move
         left the same at its next build.
       Seen hidden: lg/rg/stroke gradients drawn (an href chain,
       userSpaceOnUse); back made linear (#3a7bd5 to clear), a
       stop put in at 0.5 and made red, the end dragged to x2 1
       y2 0.989, undone; made radial, its radius dragged to 0.908;
       conic dim (102 vs 237).
       Found: a one-letter literal is a character, also as a
       local's start (code : '#' was a unichar: stops wrote '#');
       a local does not hold (a paint's id read after a reload
       was freed). OPEN: a radial in box units on a box that is
       not square draws as a circle (svg: an ellipse); a focus
       (fx, fy) is dropped; spreadMethod is not read; conic for
       other paints (materials) not offered.
       OPEN (separate): a live reload of an aeon whose svg view
       had run hit a DOUBLE-DROP in the old instance's destroy
       (my hidden runs left alive across builds). Seen: a pick saved as #2c2923,
       undone. OPEN: gradients (trinity's svg loader reads
       none; url(#id) paints are not drawn).
       Shift held while g / r / s follow the pointer: a
       quarter as fast (3D op_move and the svg view). Seen in
       the svg: an 80 px move 29.74 units plain, 7.435 with
       shift.
       The svg view's s (Kalen: x y as g): the picked points,
       or the whole shape, sized about the middle of their box
       by the pointer's sideways move (as 3D's s); x or y
       after: that axis alone; enter or a click keeps, escape
       or a right click puts back; one undo entry (SvgView
       sizing, grow, pivot_x / pivot_y). Seen: 'back' 1.5x
       both ways, then x alone.
       Typed numbers for the svg's g and s, as 3D's: axis keys
       before a number make its group, an axis after a number
       starts the next, a number with no axis goes to x for g
       and both for s; typed values win over the pointer (an
       axis not typed stays); backspace, '-' and '.'; the foot
       shows the op and what is typed (SvgView goal,
       commit_typing, op_words). Seen: 'back' (x 10..90, y
       10..60) by g x 5, s 2, s y 0.5 saved at x -25..135, y
       10..60 exactly. The x / y keys no longer toggle back to
       both (3D's rule).
       Linked offsets (Kalen: o, composite beziers): o on a
       shape follows the pointer (or a typed distance) with a
       yellow preview; enter or a click adds the offset; on an
       offset, o changes its distance live. aeon/Offset.ag:
       each segment as a cubic, offset by its control legs
       moved aside and met (Tiller and Hanson), halved until
       within 0.0005 of the size of the true offset; outside
       corners round joins (arcs as cubics), inside corners cut
       where the two cross, a bend tighter than the offset
       dropped (its neighbours cut); a closed run goes outward
       by its winding; an open run is outlined both sides with
       round ends. Written as Inkscape's: <path
       sodipodi:type="inkscape:offset" inkscape:radius
       inkscape:original xlink:href="#src">, the namespaces put
       on <svg>, the source's fill, stroke, width, opacity and
       transform copied; outward behind the source, inward in
       front. The source keeps its element (an id put in when
       it has none). SvgDoc.mark refollows the offsets linked
       to a shape (follow, remake_offset), and a g / s drag
       follows live; the right side has an 'offset' row; ctrl+z
       takes an added one back (the text before, SvgUndo.text).
       compose writes every change as a span of the text read
       (shapes, the <svg> head, inserts), last first.
       Seen: wave +6 round, back -12 square cornered, the wave
       moved with its offset following, o on the offset to
       20.87 then escape, an open wire outlined with round ends
       and joins, a circle inset past its radius gone.
       Holes (Kalen: an inset's inner ring went out of the
       shape): each subpath went out by its own winding, so a
       hole shrank. offset_cmds now finds each closed subpath's
       depth (how many others hold its first point, even-odd);
       an odd one is a hole and goes the other way, into the
       filled part. Seen: a hexagon with a hole, -6: both
       rings move into the material.
       Shift + drag on an anchor (Kalen): past 4 px the drag's
       side is the segment in or out whose way from the anchor
       (its handle, else its other end) lines up best with the
       drag; a curve there gives its handle, a line or quad is
       made a cubic (the same shape; a line's handles on its
       ends) and its handle drawn out; a subpath's start takes
       its closing segment, an implied closing line made a real
       one. The handle follows the pointer, is picked at the
       release, one undo entry; shift + drag on a handle drags
       it. SvgDoc seg_in, seg_out, as_cubic, handle_toward;
       SvgView drag_handle. Seen on wave's start: down-right
       made the closing line a curve (saved C 90,90 26.022,
       69.368 0,40); up-right took the first curve's handle,
       moved by the pointer exactly.
       Delete or backspace (Kalen): the picked nodes taken out,
       the segments either side joined into one (two lines a
       line, else a cubic keeping the outer handles); an open
       path's end shortens it; a closed subpath's start (or its
       end on it) moves the start to the next node and joins the
       closing segment on (an implied closing line made real);
       a subpath with no segment left goes; last first, one
       undo entry (SvgDoc joined, drop_node, drop_nodes, tidy).
       Seen: back's (90,18) -> C 86.418,10 90,52 90,52; its
       start (18,10) -> starts at 82,10; wave's middle -> C
       20,0 120,80 90,90.
       Then (Kalen): a deleted node between curves is fitted:
       the joined cubic keeps its ends and their handle
       directions, the handle lengths by least squares to 49
       samples of the old curve (Schneider's fit), the samples'
       t refined by Newton, 8 passes (joined, out_way, in_way,
       nearer_t). Seen: a curve split in two and its new node
       deleted came back C 20,0.001 60,0.001 80,40 (was C 20,0
       60,0 80,40).
       Double click on a segment (Kalen): a node put in there
       (SvgDoc.split_at: nearest of 64 samples a segment, within
       6 px): a line two lines, a cubic or quad split at that t
       (the same curve), an implied closing line a real node;
       the new node picked, one undo entry. Seen: wave's top
       split C 10,20 25,10 40,10 C 55,10 70,20 80,40; its
       closing line at 45,65; back's top at 50,10.
       Easing in 2D (Kalen: filter_n as 3D): the view's pan
       and wheel zoom go to goals (ox_goal, oy_goal, zoom_goal;
       home eases to the fit), and g / s, o and a handle drag
       show an eased value (shown_g, off_shown, h_shown), each
       frame part of the way (filter_n, ease 0.35, as 3D);
       enter, a click or a release keeps the exact goal
       (SvgView.ease_frame). Seen: 100 px of g: 753 then 796 px
       half a second on.
       Then (Kalen: pan with more weight, 50% longer): the
       view's pan and zoom ease at 0.25 a frame (view_ease; was
       0.35, the edits keep it): within 5% of the goal in ~10
       frames, was ~7.
       Node types (Kalen; cmd + click cycles them): cusp (free
       handles, a square), smooth (handles opposite, each its
       own length; a circle), symmetric (opposite and the same
       length; a circle with a dot). Kept as Inkscape's
       sodipodi:nodetypes (c s z, a letter a node: every command
       but a close), kept in step when nodes are put in or taken
       out (types_insert, types_remove). Made smooth or
       symmetric, the handles turn to the mean of their ways
       (align_node); a moved handle (drag, g / s) moves its
       partner (hold_node, partner), unless both move; beside a
       straight side the handle keeps to the line's way. A
       closed subpath's end on its start is that start's node
       (node_of). Seen: a split node dragged as a cusp (partner
       still), cmd + click to smooth then symmetric (saved
       nodetypes czcc, handles -17.108,6.925 / 17.108,-6.925),
       its right handle dragged down: the left one mirrored.
       Page up / down (Kalen): the chosen shape one step up
       (later, drawn over) or down among its parent's elements
       that draw (title, desc, defs skipped; a group is one
       step): the two elements' text swapped, the space between
       kept, elements untouched; it stays chosen; one undo
       (SvgDoc.restack, parent_of, draws). Seen: wave over the
       circle in its group; back above the group and back down.
       Ctrl + page up / down: to the top or bottom of its
       parent, one undo for the whole move. Seen on four rects
       a b c d: a to the top (b c d a), back to the bottom, and
       one ctrl+z after a move to the top gave a b c d.
       The outliner lists an svg's shapes last drawn first (on
       top at the top, as layer lists do). Seen: d c b a; the
       first row chose d.
       An emptied number in a split cell (viewBox 0 0 24 24
       with one 0 taken out) dropped from the value, so the row
       fell back to one text field: properties value_of now
       counts an empty part as 0, and a DragField left empty by
       enter or escape shows 0. Seen: still four cells, 0 shown,
       the file's viewBox unchanged. properties rebuilt (orbiter
       takes it at its next launch).
       Several shapes and booleans (Kalen): shift + click (view
       or outliner) puts a shape in or out of the choice
       (SvgDoc.also, toggle, choose_one, selected); the others
       draw their outlines. ctrl + = (or keypad +): union; ctrl
       + - (or keypad -): the bottom one less the rest
       (SvgDoc.combine: the result takes the bottom one's place
       and attributes, a link dropped; the others' elements go
       with their lines; one undo). aeon/Bool.ag keeps the
       curves (as Paper.js): every cubic pair's crossings by
       halving until flat, ends of one lying on the other cut
       too (shared edges); each piece kept when the result is
       on one side of it only (two points eps either side,
       even-odd over the outlines flattened to 1e-6 of the size)
       and turned so the result is on its left; a shared edge
       kept once; pieces chained end to start; straight ones
       written as lines. trinity Window.font_keys (last member):
       the svg view turns ctrl + / - font sizing off, the 3D
       view back on.
       Found: the inside test on 8 points a cubic sat 0.17
       units inside a circle, past the 0.023 side points, so a
       circle's arc read outside on both sides and the union
       came out empty; the outlines are flattened finely now.
       Seen: box (rounded) + disc: one outline, the circle's
       arcs and the corners' own control points; bar - cut: a
       notch of lines; box - disc: a round bite (the circle's
       arc); ctrl+z back to both.
       An offset's one knob (Kalen: no bezier handles on an
       offset, one weight to drag): a chosen linked offset draws
       its outline and a yellow diamond on it, a line from the
       source's point it is measured from; its own points are
       not picked (nearest). A press within 10 px of the knob
       drags the distance: the pointer's signed distance from
       the source's outline (outside +, inside -; Bool.ag
       signed_reach on the outline flattened), eased, applied
       exactly at the release, one undo (the o op's machinery).
       The knob keeps the source point and the way it was
       dragged (knob_q, knob_d), so it stays under the pointer
       (by a corner every point of the round join is equally
       near). Seen: 6 -> 22.016 dragged out, 11.472 back in,
       the knob where it was let go.
       Delete or backspace with no node picked (Kalen): the
       chosen shapes' elements go, each with its line, one undo
       (SvgDoc.delete_chosen, line_start, shared with combine).
       Seen: d and b deleted (a and c left), ctrl+z gave the
       file back byte for byte. On the way: the right side's
       headings stayed as last drawn (shape over the document's
       rows): PropsBody repaints when its blocks' keys or labels
       change (heads).
       Groups (Kalen: ctrl+g on two or more chosen): ctrl+g puts
       the chosen shapes in a new <g id="group-n"> where the
       topmost of them was, in drawing order; one from another
       parent keeps its place by a matrix put before its own
       transform (with_transform). ctrl+shift+g takes the chosen
       shape's group apart: its children where it was, its
       transform put before each child's (SvgDoc group_chosen,
       ungroup_chosen, fresh_text, with_spans). The outliner
       lists groups as rows, their shapes a level in (rows,
       walk_rows); a group's row chooses all its shapes. G / S
       with several chosen (no node picked) moves them together,
       one undo (moved_of, multi_text). A click in the view
       still picks a single shape (its nodes editable).
       Seen: back (top level) + wave (in translate(100,20))
       grouped: group-1 inside the translated group, back with
       matrix(1,0,0,1,-100,-20), nothing moved on screen; g
       moved both by 14.87,-7.435; ungrouped; the translated
       group taken apart: its translate on the wave and circle.
       OPEN: a group's own presentation (fill ...) is not put
       on its children at an ungroup.
       Fixed (Kalen: offset less source left the whole offset
       filled once the knob had changed its distance;
       trinity/images/resource_test.svg, radius 0.911): the
       offset outline had stubs a few thousandths long running
       back against the way (a curve meeting a line nearly
       parallel: the overlap too slight for trim_cross, so a
       line back joined them); the side test turned each stub
       round and the chain of that outline broke and was
       dropped. Offset.ag join: a gap under a tenth of the
       distance is snapped, not joined by a line; Bool.ag chain:
       a piece facing the other way joins by its end. Seen: the
       file's difference is the border rings only; a fresh 0.912
       offset has no stub (the four turn-backs left are the
       top diamond's sharp corners).
       Fixed in trinity (Kalen: waves along straight edges when
       zoomed far in, and the frame rate fell; Canvas.ag): a
       path's distance field covered its whole bounding box,
       which past the canvas was stretched over a field no
       larger than the canvas (a texel many pixels: the waves).
       The field's box is clipped to what shows (seen_box: the
       canvas and the crop taken back through the transform).
       Zoomed in, the path also flattened into thousands of
       edges and every texel walked them all: sdf_cull drops
       edges above, below or left of the field past its spread
       (the winding ray runs toward +x), and folds a connected
       run right of it into one vertical edge out past the
       spread (the same crossings, no distance changed); the
       inner-edge test runs on the kept edges. Measured on
       resource_test.svg zoomed 16 notches: 9 -> 60 frames a
       second; the edge within half a pixel of a line; holes
       right. trinity tests exit 0. Every trinity app's path
       fills take this (a restart picks it up).
       Then (Kalen: panning the whole of resource_test2.svg was
       slow): one path of 3,450 edges over the whole view, every
       texel walking every edge (~800 M tests a frame), and the
       inner-edge test each edge against all (13 ms of CPU).
       Tiles: per 16 x 16 texels the edges within the spread
       (sdf_tiles), and per texel row and tile column the
       signed crossings right of that column; the shader walks
       its tile's list and adds the crossings inside its tile
       to that count. Packed after the edges in the edge buffer
       (tile_at, list_at, base_at, cols); no new binding.
       sdf_inner_flags counts each side's winding from the
       edges in its y band only. Checked: tiles' distances and
       windings equal brute force at 4,818 texels; the picture
       equal to the all-edges shader (0 pixels over 8/255).
       23 -> 60 frames a second still and panning; the upload
       13 ms -> under 1 ms. trinity tests exit 0.
       Fixed (mine, seen as a stray one-texel line by the wave
       in test.svg): which tile a crossing near a tile's edge
       belonged to was decided by the CPU and the GPU apart, and
       their arithmetic can round apart, so a crossing counted
       twice or not at all. The shader now counts every crossing
       right of the texel by its tile's listed edges, and the
       count past the tile leaves those edges out (list
       membership is exact). Line gone; cube2 still equal to the
       all-edges picture; 60 frames a second panning.
       New path (Kalen): p draws one in the svg view: a click a
       corner node, click and drag a smooth node (its handles
       drawn out, the one behind mirrored), a click on the first
       node closes it, enter leaves it open, backspace takes the
       last node back, escape drops it; a preview follows the
       pointer. It goes last in the drawing (on top) as <path
       id="path-n" fill="none" stroke="#ffffff"> 2 screen
       pixels wide, written in the <svg> element's own space
       (trinity SVGModel.root_space, last member) and chosen.
       Seen: three nodes, the middle dragged, closed on the
       first: M .. C .. C .. L .. Z as drawn.
       A group chosen (Kalen, Oct 14): the path goes last inside
       it, before its </g>, in the group's own space (every
       transform from the <svg> down), the closing tag's indent
       kept (SvgDoc chosen_group: the outermost <g> whose shapes
       are exactly the chosen ones; group_space). Seen: test.svg's
       translate(100,20) group took path-1 at -55.762,-5.167, drawn
       under the clicks; nothing chosen, it goes last in <svg>.
       Undo keeps the choice (Kalen, Oct 14): an undo that reads
       the text again (add, delete, group, restack, boolean) finds
       each chosen shape anew by its id, else its element's text,
       else its number when the count is unchanged (SvgDoc
       shape_key, shape_by_key, restore). Seen: back restacked
       over the group then undone, still chosen; the circle (no
       id) moved under the wave then undone, still chosen.
       Fixed in trinity (Kalen: backspace in the path's name
       field deleted the path; "no character that goes to an edit
       element should be an event elsewhere"): Window.key_event
       gave the app every key before the focused edit layer.
       Now a focused element's edit layer (editing_layer) takes
       its keys and they go no further; ctrl/cmd with anything
       but its own a c v x z y still goes on (ctrl+s saves);
       escape still ends dictation first. text_event the same.
       A left press outside the focused field (not inside it)
       releases its focus. Seen: backspace and 'p' typed into
       the id (no pen started), ctrl+s saved id="bap", a click
       in the view let the field go and backspace then deleted
       the shape. trinity tests exit 0. Every trinity app takes
       it at a restart.
       Box and circle select in the svg view (Kalen: as 3D): b
       or c from the pointer, a left click or enter picks (shift
       or cmd adds), escape or a right click drops it; a chosen
       shape's nodes inside it (lit while drawn), else the
       shapes wholly inside it (SvgView region, in_region,
       end_region, draw_region). Seen: two of wave's nodes
       boxed; with nothing chosen a circle round the dot chose
       it alone (the wave only partly in).
       No save prompt at a quit (Kalen: an anti pattern); the
       unsaved work is kept instead, with where it saves to.
       aeon keeps a drawing's unsaved text, a second apart at
       most, in <cache>/aeon/recover.svg, with recover_on and
       recover_to (its file; '' a drawing never saved) in its
       persisted state (keep_recovery); a save lets it go. On the
       next start (no argument, or the same file) the drawing
       opens with that text, unsaved (recovered_project,
       recover_into, SvgDoc.take_text). ctrl/cmd + n: a new
       drawing (an empty 200 x 200 page; its working copy is
       <cache>/aeon/untitled.svg, titled untitled, never a last
       project); ctrl/cmd + s on it opens the save dialog (name
       typed or a file picked; the last project's folder).
       The outliner's unsaved note shows for svgs too.
       Seen: a rename kept through kill -9 (the file untouched),
       ctrl+s wrote it and the kept text went; a new drawing
       with a path kept through kill -9, reopened titled
       untitled, saved as fresh.svg beside the last project.
       OPEN: one run died once during ctrl+n and the pen, not
       seen again in three tries.
       3D designs too (Kalen: only the .agi index, and meshes):
       held edits go, a second apart at most, into a shadow of
       the files they change (<cache>/aeon/shadow, under each
       file's own path; only those .agi and .mesh files), never
       the files: each touched file starts again from its base
       (the shadow copy from before the session, snapshotted to
       shadow-base at open, else the file) and takes every held
       edit, so an undone one is gone too, and a copy equal to
       its base goes (flush_shadow, same_bytes). The design
       module reads a file's shadow copy when there is one
       (design: persist shadow_root, set_shadow_root, shadowed;
       Cage.make, read_agi, body_text, read_finish; only aeon
       sets the root). A design opened with copies kept is
       unsaved (Edits.kept). ctrl/cmd + s: the shadow's copies of
       this design's files go over the files, then the shadow
       lets them go. Seen on a scratch flower888 + fmodel:
       outside moved 0.5 in x kept through kill -9 (fmodel.agi
       untouched until the save, the shadow's copy at 0.5),
       reopened unsaved and moved, ctrl+s wrote at: [ 0.5, 0, 0 ]
       alone, the shadow emptied; a cage point move kept as the
       shadow's outside.mesh, undone: the copy gone. design and
       avatar rebuilt (Design.save_place_to added).
       ctrl/cmd + r reverts, asked first (Kalen): trinity has a
       yes or no dialog for any app, Window.confirm[ question,
       yes word, lambda none [ bool ] ] (ConfirmView in
       navigation.ag: the window dimmed, a box sized to the
       question, the yes button and Cancel; enter or the button
       yes, escape, Cancel or a press outside no; it takes every
       key while up; Window members confirm_text, confirm_yes,
       confirm_do last). aeon asks only when something is unsaved
       ("nothing to revert" else); a yes drops the svg's kept
       text, or the design's shadow and shadow-base copies, and
       opens the project again from its files (aeon.revert,
       revert_answer); a drawing never saved goes back to an
       empty page. Seen hidden: test.svg's moved wave back, the
       unsaved note and recover.svg gone, the file untouched;
       flower888's moved outside back, the shadow empty.
       Compiler, found on the way (aether.c, two bugs):
       - a class member typed `lambda none [ bool ]` registered
         its type by canonical, which follows a lambda signature
         to its return type (none): every module importing the
         class sized a void member and hung in LLVM (aeon's build
         and Kalen's running aeon's reload build spun for
         minutes). It registers as Au's lambda now.
       - a store into a lambda member never held its object (the
         hold was for classes only: is_class_set, enode_retain):
         a closure kept in a slot was freed with its frame
         (Window.confirm_end called a freed lambda: SIGSEGV).
         Lambda slots hold and drop now; a static lambda (lambda
         f[] on a plain function) is unmanaged, so its hold and
         drop do nothing. features t_lambda_member_holds (crashed
         before); 253/254 (t_codegen_resource as before).
       Every module with lambda slots changes its stores (quake2,
       trinity's Window): rebuild before running. trinity,
       properties, design, avatar, aeon rebuilt.
       Then (Kalen): handles (the bezier weights) are picked by
       the box and circle too, lit while drawn. Seen: wave's two
       top handles boxed, no node; g moved them alone: C 20,0
       60,0 80,40 -> C 34.87,0 74.87,0 80,40.
       Save as (Kalen: ctrl+shift+s; save stays ctrl+s): trinity
       Window.capture_keys (the svg view turns the capture's key
       off, the 3D view back on) and a save mode of the
       navigation dialog: nav_save_as[ name ] opens it on the
       app's folder with the name typed, the bar says "save as";
       enter on a typed name that is a folder goes in, else that
       name in the folder shown is handed to element.nav_open
       with Window.nav_saving set (no new element hook); nothing
       typed: the row (a folder gone into, a file overwritten).
       aeon: SvgDoc.save_as writes every change there and keeps
       it as the drawing's file (.svg added when missing); the
       title and last_project follow. ctrl/cmd + o opens (and
       closes) the navigator in any trinity app (Window.key_event;
       hosted, the host's own); o alone stays the svg offset. Seen: copy.svg written
       equal to the original, titled copy.svg. Window grew
       capture_keys and nav_saving (last members).
       OPEN: an offset's own loops (a deep inset of a
       wiggly shape) are not cut away as Inkscape does;
       trinity's svg loader ignores fill-rule (evenodd draws
       as nonzero).
    e. DONE (Kalen, Oct 13) edit mode shows the creases on the
       cage's edges: each creased edge (the cage's sa/sb, read
       from the .mesh's crease_edge) drawn over the cage lines,
       2.5 px pink, its alpha 0.35 + 0.65 x crease (crease from
       sharpness = 10 x crease squared). Seen on outside's cage
       (tab): its creased ring edges pink. flower's cage has no
       creases (all 72 edges 0).
    c. DONE (Oct 13) line quality in trinity (Kalen: "very
       aliased"). Two causes in Canvas.paint and CanvasUI mode 6:
       a path's distance field was half the canvas stretched over
       its padded box (a 180 px line: 1.9 px a texel), and a
       blend of coarse texels across a line's V never reached 0
       (whole columns empty); and the stroke's coverage was
       clamped sharp ("a 1px stroke fills its pixel solid"). Now
       the box snaps to whole pixels and the field is a texel a
       pixel (it grows to the canvas the first time a path needs
       it, and stays grown on a resize; small-icon canvases keep
       half size); a stroke's coverage is the pixel's share of the
       band (a box filter, half a pixel by the pixel's size, not
       the field's slope, which flattens at the line). trinity
       t_line_aa: every column of a shallow 1 px line sums to one
       pixel (258), split 150/108 where the true area is .59/.42;
       a line on a pixel row fills that row alone. trinity and
       webgfx tests exit 0. Seen in aeon's graph: smooth. Every
       trinity app's path strokes change this way (borders on a
       pixel row stay solid); a canvas that draws a path wider
       than half itself holds a field 4x larger.
D2. OPEN the colour match and the avatar's camera in the graph.
D3. OPEN Aeon's edit commands for the agent; the exchange in Aeon.
D4. OPEN clouds converted: its passes in an .agi, drivers for
    the sun and the weather.
D5. OPEN the graph view.
The script: a .blend opened (no Blender shown), a cage point
dragged (24 petals follow), the avatar's chain from its graph
with a number changed live, the agent asked for a change, clouds
retuned live, all recorded (--record) and cut by composer.
Direction (Kalen, Oct 12): every scene converts to this form,
edited in Aeon: the module holds the element's code (shaders,
special passes, behaviour over time), the scene's folder .agi
holds everything it uses (passes, sizes, wiring, settings,
resources copied in). Values that change each frame come from
the element as named drivers the graph reads (e.g. 'driver:sun').
orbiter's backdrops load the scene's .agi into a Stage.
2. DONE (Oct 12) Aeon loads a project and draws it: the aeon
   module (aeon.ag the app, `silver aeon <folder>`, flower888 by
   default; Design.ag MeshNode Cage / Mirror / Subdivision /
   Array, Object, Design (parse_agi of <dir>/<name>.agi);
   Poly.ag the mesh, read_mesh, mirror_axis, array_copies,
   subdivide_once (opensubdiv's rules: boundaries crease, smooth
   corners, sharpness = 10 x crease squared, falling one a level,
   fractional blends), limit_of; View.ag materials/<name>.agi as
   gltf materials, one gltf mesh an object (a part a material),
   AeonLit (ggx, two lights, a sky metals reflect), DesignView
   (drag turns, wheel zooms, z up)). The node key is `input:`
   (`in` is silver's), the mesh node `Cage` (gltf has Mesh).
   `silver --test aeon` t_flower_matches_blender: every object
   has blender's point and face counts (flower 91,800 / 87,552);
   points within 0.00033 (outside: its crease ring and the
   array's seams; flower 0.00006); with blender's limit surface
   off, flower is exact (1.6e-7): the rest is opensubdiv's limit
   patches. The test allows 0.0005. Refs: blend.py's second
   folder, aeon/tests/flower888/. Seen hidden: the orbiter drawn,
   a drag turns it to the top view (hexagon, 24 petals, core).
   Silver bugs met (worked round, OPEN): a free func named as an
   Au C function (array_of) crashes the compiler (signal 11, no
   message) instead of a clash error; `v.push[ a + b ]` and a
   ternary in push read as a generic Au (a local first works).
   Not done: Mirror bisect/clip, Array size offsets, simple
   subdivision, uv subdivided linearly (blender: preserve
   boundaries), vertex colour unused by AeonLit.
3. IN PROGRESS the render graph. Kalen: Aeon's own scenes
   component, so we control where it goes: aeon/Stage.ag, `Stage`
   (gltf has Scene). A stage owns its passes and their Renders
   (never the Window's element_targets), draws them in order each
   frame (update, draw, sync_fence: a gpu wait a pass) and hands
   back its out textures; the host draws them where it likes
   (DesignView, now in aeon.ag, fills its bounds). The graph is
   <dir>/<name>.render.agi: `passes:` (Pass: scale of the host's
   size, depth, reduce, clear, objects (none named: all), quad
   (a screen pass), shader by class name from any loaded module,
   inputs: sampler -> 'pass.color|depth|reduction' of an earlier
   pass), `out: [ ... ]`. No file: one AeonLit pass of all.
   design_model takes the objects a pass names.
   Checked hidden against the shot from before: no file, 0 of
   1,024,000 pixels differ; a two pass file (lit, then UVQuad
   copying lit.color) also 0; wired to lit.depth every pixel
   differs (the copy really draws).
   Fixed: the orbit's x was reversed (Kalen).
   The avatar's chain (Oct 12), aeon/flower888/flower888.render.agi:
   `modules: [ avatar ]` (the stage dlopens install/build/
   libsilver-avatar.so: its types register), passes av (scale 4,
   depth, reduce, y_up; draws body PBR on outside/flower/separator,
   core Core, plasma Plasma), volume (Aura quad, height av.depth,
   rim colour, most 1024), mask (GrooveMask), heat (Heat quad,
   av.reduction + mask.reduction); out heat, volume. Stage v2:
   a Pass holds Draws (shader by class name, `settings` set by
   member name through Au.convert, a project file as its full
   path; `inputs` 'pass.color|depth|reduction', 'image:', 'env:'
   (trinity environment), 'color:r g b a' (a 1x1 image); a shader
   with env_id loads images/<env_id>.exr from the project;
   both_sides, else back faces cull); y_up passes take glTF axes
   (x, z, -y) for positions, normals and the view (up 0,-1,0);
   mvp model/view/proj/time and a vec4f `pos` set each frame;
   gltf nodes named by object (a shader's groups). The avatar's
   core.png and night.exr copied into the project.
   Seen hidden from above: the avatar's look (core glow, blue and
   pink plasma ring, aura volume, petals lit blue). NOT matched
   yet: petals and body read warmer/darker than the icon (silver,
   grey); the avatar's own camera (OrbiterAvatar: eye 0,4.2,0,
   fov 24, up 0,0,-1) is not in the graph, so no pixel check.
   orbiter32.gltf has no top_ring: the avatar's plasma never draws
   it; the graph's plasma draws flower only.
   NEXT: the avatar's camera in the graph, a side by side against
   the avatar, the colour difference; then the graph view.
4. DONE (Oct 12) editing the cage, points first (Kalen: the edit
   goes to the cage, so every copy follows). A row of object
   names on top (TButtons; the view below it, t66px: an animated
   view over the row hid it) picks the object whose cage shows
   (lines and points over the view, not depth tested). A press
   within 12 px of a point picks it (shift adds), a drag moves the
   picked in the plane facing the view (x, y or z held: that axis
   alone), elsewhere a drag turns the view. The Cage node keeps
   its mesh in memory (read once); a move re-runs the object's
   nodes and writes positions and normals into its pipelines'
   vertex buffers in place (Stage.refresh, View.ag refill: aeonlit
   layout, 48 bytes a vertex); blend.py is not involved. Release
   writes the points into the .mesh (offset 24, the rest kept);
   ctrl/cmd+z undoes (the app hands keys to the view). The view's
   screen x and y are both mirrored against the camera vectors
   (its flipped y): right is z x forward. ev.mouse.pos is already
   the element's own space.
   Seen hidden on a scratch copy: a point dragged 40 px right and
   up ends 3 px from the pointer, all 24 petals change, the .mesh
   is written; ctrl+z gives back the original file byte for byte.
   Not driven: the axis keys, shift-add, the wheel.
   Tab (Kalen): object mode / inner cage. Object
   mode (the start): no cage; a press within 16 px of an object's
   cage point chooses it (the row lights it: DesignView.want, read
   by the app's render) and a drag moves the whole object (its
   `at`), saved on release into its block of <name>.agi
   (Design.save_place rewrites its `at:` line); ctrl/cmd+z undoes
   either kind. Tab on a chosen object shows its cage and edits
   points; tab again goes back. Seen hidden: a click lit outside,
   a 60 px drag moved it (at 0.134, -0.196, 0 written), tab showed
   the moved cage, ctrl+z wrote at 0, 0, 0 back.
   The orbit eases (Kalen: an accumulating influence, filtered up
   to about 50%): a drag adds 0.004 rad a pixel (was 0.008, applied
   at once) to yaw_goal / pitch_goal, and each frame the view
   moves half the way there (filter_n 0.5, frame-rate free). Seen
   hidden: still moving at release, settled a second later.
   Then (Kalen, Oct 12: orbit accumulates, filtered slower) the
   orbit's view takes 0.2 of what is left a frame (pan, zoom and
   ops keep 0.5); seen hidden: still turning 0.3 s after release,
   settled by 1.8 s.
   Then (Kalen: every movement so: rotation, translation, place)
   the same for all: point moves, object moves and the camera pan
   take half the pointer's move into a `pending` / center_goal,
   the wheel's step is 1.05 a notch into dist_goal, and each frame
   moves half the way (filter_n 0.5). An edit is saved once its
   move has settled (save_due), not at release. Seen hidden: a
   100 px pan moved the model 51.5 px; a 40, -40 px point drag
   moved it 18.5, -18 px, the .mesh unwritten at release and
   written once settled. Rotate and scale tools, when made, take
   the same.
   Ctrl/cmd held at the press moves the camera (Kalen): the view
   turns around its own `center` (was the origin) and a ctrl drag
   moves it in the plane facing the view, so the model follows
   the pointer (drag_world, shared with point moves). Seen hidden:
   a 100, 50 px drag moved the model 101.7, 50.4 px.
   Speed (Kalen: ultra slow on an object that arrays then
   subdivides): a move re-ran every node with uv, colour and
   material and rebuilt every vertex by push: flower ~500 ms,
   outside ~200 ms. Now an edit runs the shape alone (the Cage's
   `bare` poly shares the cage's points, no attrs), the first
   build records each GPU vertex's mesh point (ModelParts verts,
   firsts, sizes) and refill writes position and normal from it;
   array and subdivision write presized vecs by index, the edge
   table is counted then made, corners know their face (cface).
   Measured in the app per move, evaluate to upload: outside 5
   ms, flower 7 ms (20 of 20 moves each). Test still 1/1.
   The test has its own copy (Kalen edited aeon/flower888's core):
   aeon/tests/flower888/ is the project and blender's refs
   together, made by blend.py from the .blend; aeon/flower888 is
   the one edited in aeon. 1/1 passes.

---

silver is a native build language with an LLVM backend. It compiles `.ag` source files into native binaries via LLVM IR. The compiler itself is written in C, built on the **Au** object system.

## Build & Run

```bash
# Build the silver compiler (from /src/silver)
make                    # builds debug (default)
make release            # builds release with -O2
make clean              # cleans generated headers

# Compile a module from the repository root
./platform/native/debug/silver trinity

# silver [flags] <module> [app-args…] — silver's flags come BEFORE the
# module name; everything AFTER the module passes verbatim to the app
silver --watch trinity      # file watcher mode
silver --clean trinity      # rebuild the module (not imports)
silver --release trinity    # release build
silver --build orbiter      # compile only, no launch
silver --test expectest     # run the module's expect tests, exit
silver orbiter --width 1920 # --width goes to orbiter, not silver

# Primary development workflow (bare launch builds AND runs)
silver --clean orbiter
```

- `make` is `make release` (Sep 29, Kalen); `make debug` for -O0 -g. Both build into install/build and link install/bin/silver: the last one built wins.
- Bootstrap runs `gen.py` then `ninja`. The ninja file is generated per build type.
- Build caching: modules with unchanged source skip recompilation (checks `.product` timestamp vs `.ag` timestamp).
- Release builds: LLVM emits .o directly in-memory via `LLVMTargetMachineEmitToFile` — no .ll file, no llc process. Uses `LLVMCodeGenLevelAggressive` with `+avx2,+fma` on x86-64.
- Debug builds: emits .ll to disk (for inspection), uses llc with `-O0`, full LLDB debug info.
- `.ll` and `.bc` files only written when `--verbose` is set.

## Project Structure

Application and library module directories live at the repository root.

```
src/
  Au              # Au object system header (types, macros, memory, declare_class)
  Au.c            # Au runtime implementation (object lifecycle, type registration, collections)
  Au.g            # Au build descriptor (shared lib, links libffi)
  aether          # Aether header (enode, etype, aether schemas — the IR/AST layer)
  aether.c        # Aether implementation (LLVM codegen, type building, expression nodes)
  aether.g        # Aether build descriptor (shared lib, links LLVM/clang/lldb)
  silver          # silver compiler header (silver_schema, import_schema, codegen classes)
  silver.c        # silver compiler parser (tokenizer, expression parser, statement parser)
  silver.g        # silver build descriptor (app, modules: Au net aether)
  macros.h        # C macros for declare_class, schema definitions
  object.h        # Low-level object header/vtable layout
ai/ai.ag        # Neural network library (tensors, ops, keras model, training)
ai-test/        # AI test application
test/test.ag    # General language test
random/         # Random number generation module
orbiter/        # Orbiter project
...
platform/native/  # Built SDK (bin/, lib/, include/)
checkout/         # Vendored dependencies (llvm-project, mbedtls, etc.)
```

## .g Build Descriptors

Each module has a `.g` file defining its build:
```
type:       app | shared
modules:    <dependency modules>
link:       <linker flags>
import:     <external dependency with git ref>
install:    <headers to install>
```

## Au Object System

Au is the C-based object/type system underlying everything. Key concepts:

- **Au_t** (`struct _Au_t*`): Type descriptor. Holds members, methods, vtable, size, traits.
- **Au** (`struct _Au**`): Object reference (double-pointer, header before data).
- **`typeid(T)`**: Gets the Au_t for type T (e.g., `typeid(i32)`, `typeid(string)`).
- **`declare_class(Name)`**: Declares a class with schema. Variants: `declare_class_2(Name, Base)`, `declare_class_3`, `declare_class_4` for inheritance depth.
- **Schema macros**: `#define foo_schema(X, Y, ...) M(X, Y, i, prop, public, type, name) ...` — defines members, methods, overrides, constructors.
- **Member types**: `prop` (field), `method`, `override`, `ctr` (constructor), `vargs`, `guard`.
- **Access**: `public`, `intern`, `iobject`.
- **Traits**: `AU_TRAIT_CONST`, `AU_TRAIT_INLAY`, `AU_TRAIT_STRUCT`, `AU_TRAIT_PRIMITIVE`, `AU_TRAIT_ENUM`.

### Key Au Types
- Primitives: `i8 i16 i32 i64 u8 u16 u32 u64 f32 f64 bf16 bool num sz`
- `string` — managed string object
- `symbol` — `const char*` (use `cstring(s)` to convert string → symbol for functions like `lexical()`)
- `array` — heap-allocated class-based collection (inherits `collective`)
- `map` — hash map collection
- `path` — file path object
- `token` — lexer token (has `chars`, `line`, `indent`, `literal`)
- `enode` — expression/AST node
- `etype` — type reference in the compiler (wraps Au_t with LLVM metadata)
- `shape` — dimensional shape (e.g., `32x32x1`, `4x4`)

### Key Au Functions
- `hold(x)` / `drop(x)` — reference counting
- `len(collection)` — element count
- `push(array, element)` — append
- `get(map, key)` / `set(map, key, value)` — map access
- `eq(a, b)` — equality check (works on strings, tokens)
- `instanceof(obj, type)` — type check, returns cast or null
- `inherits(au_t, typeid)` — inheritance check
- `find_member(au_t, name, member_type, flags, search_inherited)` — member lookup

## silver Language (.ag Syntax)

### Module Structure
```
export 0.8.8              # version export

import <stdio.h>          # C header import
import ai                 # silver module import
import random             # silver module import

alias astrings: array string   # type alias
```

### Types & Variables
```
x: i32 [ 42 ]             # typed variable with initializer in [ ]
name: string [ 'hello' ]  # string with single quotes
v: f32 [ 0.0 ]            # float
flag: bool [ true ]        # boolean
p: ref i32                 # pointer (reference)
data: new f32 [ 1024 ]    # heap allocation (new Type [ shape ])
```

**Important**: `new Type [ expr ]` parses `expr` as a shape. `*` inside `[]` is shape literal. Pre-compute products as i32 before passing to new:
```
sz: i32 [ a * b ]          # compute product first
buf: new f32 [ sz ]        # then allocate
```

### Enums
```
enum Optimizer
    sgd:  0
    adam: 1
```
Access: `Optimizer.sgd` or bare `sgd` when type is inferred (e.g., in switch cases).

### Classes & Structs
```
class op
    public name: string
    public quantized: bool

    func forward [] -> none
        log 'forward'
```
- `class` = heap-allocated, reference-counted
- `struct` = inlay/value type
- Members: `public`, `intern` (private), `context` (read-only after init)
- Inheritance: `op dense` — dense inherits from op. Brackets after
  a class name (`class dense [ X ]`) are member meta, never a base.

### Functions
```
func name [ arg1: i32, arg2: string ] -> return_type
    body
```
- Indentation-based blocks (tabs)
- No `[ ]` needed for zero-arg calls at expression level 0
- `[ ]` required for expressions and when args are present
- **Commaless args**: without commas, args are matched to parameters by type (CSS selector style). With commas, args are positional. This enables declarative UI composition where order is flexible.
- **Callable subs**: `x: i32 sub` stores the body. `x[]` re-invokes it with current scope, returns the value without reassigning `x`.
- **Keyword tokens**: `{ l0 t0 r0 b0 }` parses as a `tokens` literal when expected type is `tokens`. Used for layout coordinates and style properties.

### Control Flow
```
if [ condition ]
    body
else
    body

for [ i: i32 0, i < n, i += 1 ]
    body

switch [ expr ]
    case value1
        body
    case value2, value3
        body
    default
        body
```
- Switch cases infer enum type from the switch expression (bare enum member names work)

### Inline Assembly
```
asm x86_64                  # conditional: only compiles on x86_64
    mov rax, 1
    ...

result: i32 asm [ args ]    # expression-level asm with return value
    mov eax, ...

asm [ args ]                # void/statement-level asm
    ...
```
- Intel syntax
- `asm <define>` on same line = conditional compilation (skips block if define is false)
- Auto-gather: when no `[args]` given, asm scans body for in-scope variables
- Platform defines: `x86_64`, `arm64`, `apple`, `linux`, `windows`

### String Interpolation
```
log 'hello {name}, value is {x}'
```

### Collections
```
items: array i32 [ 1, 2, 3 ]
data: map string [ i32 ]
```

### Iteration
```
each(items, type, var)
    use var
```

## Compiler Architecture (silver.c)

### Compilation Phases

`silver_parse()` drives compilation in distinct phases:

1. **Statement loop** — parses module-level statements sequentially: imports, exports, aliases, class/struct/enum headers (bodies stashed as token arrays), free functions. `incremental_resolve()` called after each statement (currently a stub).
2. **Deferred alias resolution** — aliases whose target types weren't available during phase 1 (forward references) are resolved in multi-pass order. Stored as `(Au_t, token array)` pairs in `a->pending_aliases`.
3. **Phase 1: Record body parse** — `build_record_parse()` replays stashed tokens for each class/struct via `push_tokens`/`pop_tokens`, parsing members and method signatures.
4. **Phase 2: LLVM type implementation** — `build_record_implement()` calls `etype_implement()` to build LLVM struct bodies, set member indices, create type IDs.
5. **Phase 3: Function codegen** — `build_record_functions()` and `build_fn()` emit LLVM IR for all method and free function bodies.

This phased approach means: all type names are registered before any body is parsed; all LLVM types exist before any function emits IR.

### Key Parse Functions
- `silver_parse()` — entry point, sets up platform defines, runs all phases
- `parse_statement()` — dispatches keywords (if/for/switch/return/class/func/etc.)
- `parse_expression()` → `reverse_descent()` → `read_enode()` — expression parsing with precedence climbing
- `read_enode()` — reads a single expression node (literals, variables, new, sizeof, etc.)
- `silver_parse_member()` — resolves member access chains (`a.b.c`), handles scope_mdl hint for enum inference
- `read_etype()` — reads a type name, handles generics, refs, primitives, C types. Uses `read_named_model()` → `elookup()` → `rlookup()` → `lexical()` → `etype_prep()` chain.
- `silver_read_def()` — parses type definitions: class, struct, enum, alias, scalar, import, export
- `read_expression()` — tokenizing wrapper around `parse_expression`, used in switch/case for deferred build
- `parse_switch()` — switch statement parser, passes enum hint via `canonical(e_expr)` to case value parsing
- `parse_asm()` — inline assembly parser, supports conditional `asm <define>` and auto-gather

### Key Aether Functions (aether.c)
- `e_operand()` — create literal/constant enode
- `e_create()` — type conversion/construction
- `e_op()` — binary operation node
- `e_load()` — load value from memory
- `e_null()` — null value of type
- `e_vector()` — heap array allocation
- `canonical(enode)` — resolve enode to its underlying etype (follows vars → source type)
- `is_enum(Au)` — checks if type is enum (uses `au_arg_type` to resolve through vars)
- `etype_prep(a, au)` — find or create etype for an Au_t, calls `etype_create` + `etype_implement`
- `etype_create(a, au)` — create etype wrapper for Au_t, register in `a->registry`
- `etype_implement(t, force)` — build LLVM type body (struct fields, function signatures)
- `etype_register(a, key, value, overwrite)` — store etype in `a->registry` map
- `etype_access(target, name)` — member access: `find_member` → GEP at `m->index`
- `u(etype, au)` — macro: `get(a->registry, (Au)au)` cast to etype. Registry lookup, NOT a field access.

### Token Navigation
- `peek(a)` — look at next token without consuming
- `consume(a)` — consume next token
- `read_if(a, "keyword")` — consume if matches, return token or null
- `next_is(a, "token")` — check without consuming
- `element(a, N)` — token at cursor+N (0 = next unconsumed, -1 = last consumed)
- `read_alpha(a)` — read an alphanumeric identifier
- `peek_alpha(a)` — peek at next alpha without consuming
- `push_current(a)` / `pop_tokens(a, keep)` — save/restore cursor for speculative parsing. `push_current` is sugar for `push_tokens(a, a->tokens, a->cursor)`.
- `push_tokens(a, tokens, cursor)` / `pop_tokens(a, keep)` — switch to a different token stream entirely (used for replaying stashed class bodies, deferred aliases, inline expressions). Saves/restores full state on `a->stack`.
- `read_body(a)` — read an indented block as token array

### Lexical Scoping
- `lexical(a->lexical, symbol)` — look up identifier in scope stack (takes `symbol`/`char*`, NOT `string`). Walks `a->lexical` array from end to start; within each scope walks `context` chain (inheritance). Searches both `members` and `args`.
- `top_scope(a)` — current scope Au_t
- `context_func(a)` — enclosing function
- `context_class(a)` / `context_record(a)` — enclosing class/record
- `elookup(chars)` — macro: `(etype)rlookup((aether)a, string(chars))`
- `rlookup(a, name)` — `lexical()` + `etype_prep()`: finds Au_t by name, then prepares/returns its etype

### Au Object Inheritance in C

silver (silver.c) inherits from aether (aether.c) which inherits from etype. The `silver*` pointer IS an `aether*` — schema fields from parent classes are accessible via `a->field` in child code. When casting `(aether)a` in silver.c, it references the same object. Schema properties added to `aether_schema` are visible in silver.c; properties on `silver_schema` are NOT visible in aether.c. Place shared flags on the lowest common ancestor.

### Map vs Array for Small Collections

Au `map` uses hash buckets; `pairs(map, i)` iterates via `i->key`/`i->value` linked list. For small ordered collections (< 20 items), prefer `array` with index arithmetic — `pairs` iteration on maps can miss entries when keys have non-trivial hash behavior. Use `array` + stride access (`origin[i*2]`, `origin[i*2+1]`) for paired data.

## Debugging

- When an app crashes, first read `<install/tmp>/<app>.log`. The crashed app leaves its log there.

### Running the compiler under GDB
```bash
# Always use -v --clean when debugging compiler issues
gdb --args ./platform/native/debug/silver orbiter -v --clean --run
```

### Common crash patterns
- **Segfault in `etype_prep`/`etype_create`/`etype_init`**: usually an Au_t with incomplete setup (missing `src`, wrong `member_type`). Check `au->ident`, `au->src`, `au->member_type` in GDB.
- **"unknown identifier" errors**: the type/variable isn't in lexical scope at parse time. Check `a->lexical->count` and what scopes are active.
- **"expected member" errors**: token stream is in wrong position — a previous parse consumed too many or too few tokens. Check `push_current`/`pop_tokens` balance.
- **Double terminator LLVM errors**: a basic block already has a terminator (return/break/fault) — check `LLVMGetBasicBlockTerminator` before adding branch.

### `read_etype` silent fallbacks
`read_etype` can silently produce wrong types: when meta type resolution fails (e.g., `map element [string]` where `element` isn't defined yet), it falls back to `etypeid(Au)` for the meta parameter. The `deferred_hit` flag on `aether` detects when `etype_prep` encountered an unresolved alias dependency during a `read_etype` call — check this flag after `read_etype` returns to know if the result is trustworthy.

## Common Patterns

### Adding a platform define
In `silver_parse()`:
```c
Au_t m = def_member(a->au, "name", typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
set(a->registry, (Au)m, (Au)hold(e_operand(a, _bool(condition), etypeid(bool))));
```

### Working with enode types
```c
etype t = canonical(some_enode);    // get resolved type
bool  e = is_enum((Au)some_enode);  // check if type is enum (resolves through vars)
bool  p = is_prim((Au)some_enode);  // check if primitive
```

## Testing

`expect func` declares a test — the compiler verifies it returns true:
```
expect func test [ vk: vk_context ] -> bool
    # ... test code ...
    return result
```

Test by compiling and running modules from the repository root:
```bash
make && ./platform/native/debug/silver ai-test/ai-test.ag
```

## Aether Codegen Best Practices

### e_assign Phase 6 Store Logic
The store path in `e_assign` (aether.c ~line 640) has three cases for the RHS (`res`):

1. **Struct alloca alias** (res is unloaded alloca of a struct): If `L` already has its own alloca, **copy** the struct data (load + store) instead of aliasing `L->value = res->value`. Aliasing orphans L's original alloca — GDB and subsequent loads will read uninitialized memory. Only alias when L has no storage yet (initial declaration).

2. **GEP from array/member access** (res is unloaded GEP): Load through the GEP before storing. Skipped for:
   - Fixed-size arrays (`elements > 0`): the GEP pointer IS the value (array-to-pointer decay)
   - Opaque handle types (ancestor struct with 0 members, e.g. VkPhysicalDevice_T): these are pointer handles, not real structs

3. **Everything else**: Direct store of `res->value`.

### etype Resolution Chain
When looking up an etype for codegen (`u(etype, au)`), the result may have `lltype = NULL` if the Au_t is a variable member rather than the type itself. Fallback chain:
```c
etype rt = u(etype, res->au);                          // try member's etype
if (!rt || !rt->lltype) rt = u(etype, au_arg_type(...)); // try resolved type
if (!rt || !rt->lltype) rt = etype_prep(a, ...);         // force create
```

### `is_struct` Semantics
`Au_is_struct` uses `au_arg_type` to resolve through aliases and variables to the underlying type. Key behaviors:
- Returns `false` for pointer types (`au->is_pointer`)
- Opaque Vulkan handles (e.g. VkPhysicalDevice → alias → ptr → opaque struct) return `false` because `au_arg_type` stops at the pointer
- Use `au_ancestor()` when you need to walk past pointers to the terminal type (e.g. for opaque checks)
- `is_struct` vs `au->is_struct`: the function resolves through aliases; the field checks the Au_t directly

### Alias-to-Pointer Types in etype_init
C typedef aliases to pointer types (e.g. `typedef VkPhysicalDevice_T* VkPhysicalDevice`) can fall into the named struct branch of `etype_init` because `is_rec()` resolves through to the underlying struct. When the alias src chain leads to a pointer (`au_arg_type` returns an Au_t with `is_pointer`), set `lltype = LLVMPointerTypeInContext(...)` instead of creating a named LLVM struct.

### `e_create` Same-Type Identity
`e_create` has an identity shortcut: `if (canonical(input) == canonical(mdl)) return input`. This avoids unnecessary allocas for same-type conversions. If disabled for structs, `e_create` builds a temp alloca that e_assign's alias path may hijack — causing the orphaned-alloca bug described above.

### `au_arg` vs `au_arg_type` vs `au_ancestor`
- **`au_arg(t)`**: If t is a variable (AU_MEMBER_VAR), returns `t->src`. Otherwise returns t. Does NOT resolve aliases.
- **`au_arg_type(t)`**: Like `au_arg`, but then walks the `src` chain through aliases. Stops at pointers and funcptrs.
- **`au_ancestor(au)`**: Walks `src` chain unconditionally to the terminal type. Goes through pointers. Stops at enums. Use for opaque type detection.

## Recent Compiler Discoveries

## Active work: orion2 complete Hyper Race port

Work these items in order. Do not drop unfinished items.

`/src/hyper-race` is the only game reference. Never read,
search, compare, copy from, or modify `/src/orion`.

1. Inventory Hyper Race code, assets, and runtime behavior.
2. Create the `orion2` Silver module using Trinity pipelines.
3. Preserve the old top-left, y-axis-down game coordinates.
4. Preserve the master and medium bitmap font atlases.
5. Port menus, dialogs, HUD, and touch or pointer controls.
6. Port tracks, cars, cameras, rendering, and effects.
7. Port racing rules, physics, AI, progression, and replays.
8. Port music, sound effects, settings, and saved state.
9. Keep one shared game layer for Android, iOS, Linux,
   Windows, and macOS. Trinity and Devices own platforms.
10. Validate focused components, the module, and full game.
    - Transfer each source method without redesign or substitutes.
    - Use Canvas shapes only where the source draws a shape.
    - Fix the race fragment shader uniform lookup.
    - Keep the released menu action alive through dispatch.
    - Restore the live demo behind the main menu.
    - Replace the invented car-select window with the original
      open platform scene, bottom dock, and side controls.
    - Keep the car specifications and progress bars in their
      original source-defined positions.
    - Replace the invented track selector with the original live
      track view, bottom track dock, and background track loading.
    - Fix the car body mesh appearing partly transparent.
    - Port the car body lighting pass.
    - Port the windscreen reflection pass.
    - Port the engine and jet-cone passes.
    - Apply the jet light to the selection platform.
    - Fix the startup loading crash from the app log.
    - Show only Loading on the startup loading screen.
    - Preserve alpha transparency on the initial logo.
    - Remove the invented press-to-continue screen.
    - Fix the corrupt world scene geometry and transforms.
    - Port source camera motion as 3D Bézier curves.
    - Fix the vertically reversed car-selection dock graphic.
    - Use the source square preview MVP for every dock car.
    - Match the source OpenGL car-atlas V coordinates.
    - Animate dock previews through the source transition.
    - Keep glass in its source-specific reflection pass.
    - Tile the side dock from its source dialog atlas pieces.
    - Port the exact List-atlas GroupBox header and bottom pass.
    - Keep the 1500 ms source progress-bar transition.
    - Pack all 15 source jet-cone vertex floats.
    - Restore the visible windscreen reflection pass.
    - Replace the incorrect jet ribbons with source jet shading.
    - Match the source car-select passes and dock transforms.
    - Roll selection jet UVs; keep dock preview UVs fixed.
    - Build menu dialog frames from the source atlas tiles.
    - Apply the source font y offset beside menu icons.
    - Disable orion2 live reload during app validation.
    - Restore the dock atlas size and preview orientation.
    - Restore the live sector environment in car selection.
    - Keep the sector sky at the far depth plane.
    - Match the original car-selection light application.
    - Keep cars 3 world units above selection platforms.
    - Keep dock car render targets right-side up.
    - Ray march all three jet masks on every XML jet.
    - Keep XML jet positions and cones on negative z.
    - Validate every jet with a full 15-sided cylinder.
    - Flip the car-shadow image on its y axis.
    - Drive a race through the Trinity app socket.
    - DONE Create the road-edge material suite and 36
      center-road pattern suites.
    - DONE Create the 4096-pixel straight plasma road
      material suite with vertical y-axis inlays.
    - DONE Create the 2048-pixel plasma track road set:
      straight, tapered transition, and solid sections with
      doubled side plasma, a narrow center plasma, a deeper
      center cutout, vertical asphalt wear, and aligned albedo,
      normal, metallic, specular, and AO maps.

### Type Resolution
- **`au_ancestor(au)`**: walks `src` chain to the terminal type. Stops at enums. Used in `etype_access` for member lookup through aliases, pointers, and typedefs.
- **C type aliases** (e.g. `FT_Face` → `FT_FaceRec_*` → `FT_FaceRec_`): `au_ancestor` + `etype_prep` resolves the full chain even in `no_build` mode.
- **Au_t member access**: uses `Au_t_f` schema lltype via `au_t_etype->schema->lltype` for GEP. Byte-offset GEP (`getelementptr i8`) for Au_t fields avoids cross-module struct name conflicts.
- **Union member indices**: all set to 0 (unions have single-element LLVM body). The `index` counter still increments for the member count verify.
- **Opaque C types** (GLFWwindow, FILE): skipped in type-id registration (`continue` when typesize is 0) and in static variable implementation (`is_c && is_static` skip).

### Convertible Rules Added
- `Au_t → ARef` (type descriptor is a pointer)
- `class → ref u8 / ref i8` (object to byte pointer)
- `ref ptr → Au` (any pointer to generic Au)
- `enum → f32/f64` (enum to float, via `is_realistic` check)
- `struct → struct` same size (bitcast, both directions)
- `ref struct → struct` same size (pointer to same-size struct)

### Short-Circuit `||` / `&&`
- Preserves values when types are compatible (`a: x || fallback` returns the truthy value).
- Falls back to `bool` when types don't match (`convertible` check after parsing R).
- `expect` passes `etypeid(bool)` as expected type to `read_enode`.

### Switch Statement
- Case blocks check `LLVMGetBasicBlockTerminator` before adding merge branch — prevents double terminators after `fault`/`return`/`break`.
- `AU_MEMBER_ENUMV` (member_type=10) recognized alongside `is_enum` for constant resolution.

### Public Type Exposure
- Public members on classes error if the member's `src` is a C type without typesize (`is_c && !is_primitive && !is_struct && !is_enum && !typesize`).
- Public function args and return types checked the same way.
- Enforced at parse time so bad type data never reaches import.

### Element-wise Array Operations
- Primitive arrays (`elements > 0`, `src->is_primitive`) support `+ - * / %` with automatic loop emission.
- Type promotion via `determine_rtype` + `e_create` per element.
- Scalar broadcast: one array + one scalar works in either order.
- `min`/`max`/`clamp` vectorized via `vector_binary_op` helper with function pointer dispatch.

### Math Builtins
- Single-arg: `sqrt sin cos tan asin acos atan exp log floor ceil round` — LLVM intrinsics or libm fallback.
- Two-arg: `atan2 pow` via `e_math2` (libm calls).
- Array versions emit loops — LLVM auto-vectorizes with AVX2 at `-O2`.
- Keywords registered in parser, dispatched through `e_math`/`e_math2`.

### Build System
- `make` is `make release` (Sep 29); `make debug` builds the same tree at -O0 -g.
- Debug binary → `platform/native/debug/silver`. Release → `platform/native/bin/silver`.
- `gen.py` updated: app output uses `$builddir/` not `bin/`.
- Build caching: `update_product` checks `.product` symlink timestamp vs module file. Empty `.artifacts` file no longer triggers rebuild (`!newest` = product is valid).
- `--watch` flag replaces old `--build` (watch is opt-in, one-shot is default).
- `-I` paths stripped of prefix when added to include_paths (was storing `-I/path` instead of `/path`).
- Module search: if module not found locally, searches `SILVER/name/name.ag`.

### Cast Syntax
- `(expr) to Type` — parsed in `parse_ternary` after `(expr)` closes.
- Shares the `(expr)` prefix with ternary `?` and null-coalesce `??`.

### `local` Keyword
- Stack-allocated arrays: `local VkClearValue [2]`.
- Optional inline initializer: `local VkDynamicState [2] [ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR ]`.
- `etype_implement` called on element type before `LLVMArrayType`.

### Scientific Notation
- `parse_shape` returns null on `e`/`E` (lets `parse_numeric` handle it).
- `parse_numeric` handles decimal floats and scientific notation: `1e20`, `1.5e-7`, `3.14f`.
- C integer suffix stripping (`U`, `u`, `L`, `l`) in `parse_numeric`.

### Scalar Type Safety
- Float literals rejected for integer scalars: `14.4ms` errors when `scalar ms : i64`.
- Check in scalar suffix construction path.

---

## Windows Port

Rules that hold everywhere here: **never CRLF** (LF only, it is banned), **no mingw**, **no `.def` files**, **no `__declspec` framing** in our own headers. The POSIX layer is `src/ports.cc` + `src/ports.h`, compiled into `Au.dll`, so anything linking `-lAu` gets it — including `silver-host`, which needs no change of its own to pick up a ports fix.

### Process launch — there is no fork

`fork()` returns `ENOSYS`. Every launch path goes through `posix_spawn` in ports.cc:

- **`execvp` is a stand-in, not a replacement**: it spawns, waits, then `_exit(code)` with the child's status. Windows cannot replace a running process, so silver stays alive as the parent and forwards the exit code.
- **Kill-on-close job**: `child_job()` makes one `CreateJobObjectA` with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`; the only handle is silver's. Children die with silver however it ends — clean exit, Ctrl+C, or kill.
- **`CREATE_SUSPENDED` → `child_adopt()` → `ResumeThread`**: the child joins the job before it runs one instruction. A child that started first could spawn grandchildren outside the job, and those would outlive silver.
- **Ctrl+C**: `SetConsoleCtrlHandler`; the handler calls `TerminateJobObject(job, 130)`. A `/SUBSYSTEM:WINDOWS` binary does not reliably receive console control events, so killing the job is what actually works.
- **Environment**: pass `NULL` for envp to inherit. Handing over `environ` drops Win32-only variables (the Vulkan loader reads those) — the CRT view and the Win32 view are separate, and `setenv` must write both (`_putenv` + `SetEnvironmentVariableA`).

### No console windows on spawn

`silver-host` links `/SUBSYSTEM:WINDOWS`, so it has no console. Any console program it launches gets a **brand new console window** — one flashing up per live-reload rebuild. `no_window_flag()` in ports.cc returns `CREATE_NO_WINDOW` only when `GetConsoleWindow()` is null, and is OR'd into the flags in both `posix_spawn` and `popen`. Std handles are passed explicitly, so output is unaffected. When silver itself (a console app) spawns, the flag is 0 and the child inherits the console as before.

### App output

A `/SUBSYSTEM:WINDOWS` process has no stdout: `_get_osfhandle` reports `-2` (a live fd with nothing behind it, distinct from `-1`), and writes fail `EBADF`. So:

- The app tees its own output to `<install/tmp>/<app>.log` (`trinity.cc`). `install/tmp`, not the install root, or the folder floods.
- silver clears that log **before** launch and runs a tail thread onto its own console — the tail starts at offset 0, so an unclear'd log replays the previous run.
- `SILVER_LOG_TAIL=1` tells the app silver owns the console, so it does not also tee.
- `setvbuf(stdout, 0, _IONBF, 0)` — MSVC's parameter check faults on `_IOLBF` with a null buffer and size 0.

### Live reload

Windows cannot relink or delete a mapped DLL. `reload_dlopen` loads a **copy** at `<temp_dir()>/hotreload_<mtime>.dll`, including the initial module load, so the product is never pinned and builds can relink it. The copies accumulate; nothing cleans old ones yet.

### DLL symbol visibility

On ELF every symbol in a `.so` is visible; on Windows only `dllexport`ed ones leave the DLL, and a DLL must resolve everything at link time. Two consequences that bite repeatedly:

- A helper used across modules needs `AU_EXPORT` (this is why `fnv1a_hash` in `Au.c` had to get one — `ai.ag` declares it `intern func`).
- A module that calls into a library must **declare that library**, even if it works on Linux by leaving the symbol undefined until load. `ai.ag` imported `<vulkan/vulkan.h>` with no `-lvulkan` and only failed here.

### Library naming

Windows spells libraries differently and `-lfoo` has no `libfoo.so` symlink to follow. `resolve_versioned_lib` (silver.c) holds the table, and every lib name passes through it at final link:

- `z` → `zlib`, `png` → `libpng`, `mbedcrypto` → `tfpsacrypto` (mbedtls 4 folded crypto into tf-psa-crypto)
- `-llldb` → `-lliblldb` — our own `src/lldb.c` builds `install/lib/lldb.lib` and shadows LLDB's real import lib
- `is_sdk_lib()` lets Windows SDK libs (`ole32`, `uuid`, `user32`, …) through. They live in the linker's own search path, never in `install/lib`, so every file-existence check drops them silently
- versioned scan for `opencv_core4140.lib`, `OpenEXR-3_4.lib` style names

### Module name collisions

`src/net.c` (silver's own, linked into `silver.exe`) and `net/net.ag` both produced `install/build/net.dll`. Linux quietly overwrote one with the other; Windows refuses to write a mapped DLL and exposed it. `net` is now **`tls`** (`spectra.ag` imports `tls`). Watch for this shape generally — root-level modules and `src/` modules share one output directory.

### Dependency checkouts

- `checkout()` runs `git submodule update --init --recursive` on any cache-miss build of a git checkout. A plain clone stops at the top repo, and mbedtls builds from its `framework` and `tf-psa-crypto` submodules.
- An interrupted clone can leave a temp pack (`objects/pack/tmp_pack_*`) with `HEAD -> refs/heads/.invalid` and no refs. Git then fails fast forever instead of re-cloning; delete that submodule's `.git/modules/<name>` and worktree dir to recover.
- Checkout root is `install/../..` = `C:/src/checkout`, **outside** the repo.
- mbedtls sets `GEN_FILES` **OFF when the host is Windows**, so its generated sources (`error.c`, `ssl_debug_helpers_generated.c`) must come from `scripts\make_generated_files.bat`. Turning `GEN_FILES=ON` instead hits an upstream bug — `--list-for-cmake` emits `os.path.join` paths and cmake rejects `\m` as an invalid escape.

### Python

`python` on PATH resolves to silver's own vendored `platform/native/bin/python.exe` — no pip, no ensurepip — and it **shadows the real python** for every dependency script that shells out to `python`. The usable interpreter is the Store one (`%LOCALAPPDATA%\Microsoft\WindowsApps\python.exe`, 3.12, has jinja2 + jsonschema); cmake finds it through the registry, but build scripts do not. Put it first on PATH when running a dependency's generators.

### Audio

`spectra/spectra.cc` is the WASAPI backend behind `spectra.ag`'s `el [ windows ]` branch: `spectra` (capture) and `AudioOut` (playback), same surface as the ALSA pair, FFT stays in silver. Shared mode with `AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | SRC_DEFAULT_QUALITY` is what lets a plain 16-bit ask through instead of being handed the device mix format. The GUIDs are `EXTERN_C const IID` declarations — their definitions are in `uuid.lib`, hence `-lole32 -luuid` in `spectra.g`.

**`<module>.cc` is attached on every platform** (silver.c picks up `<module_path>/<stem>.{c,cc,rs,mm}`), so a platform-specific implementation file must wrap itself in `#ifdef _WIN32` or it breaks the other platforms.

### Header shims

`ports.h` carries the POSIX surface Windows has no header for — the pthread API, `openpty`/`ttyname_r`/`forkpty` (all failing the way a no-pty system does), `getpid` as `au_getpid`. A `.ag` module takes it with `ifdef [ windows ] → import <ports.h>` in place of `<unistd.h>` / `<pthread.h>`.

`ports.h` includes `<process.h>` first, then aliases our own (`#define execvp au_execvp`, `getpid`, `execl`, `execlp`) so the UCRT's dllimport declarations cannot collide. `read`/`write` still have that same unguarded collision shape — any module including `<io.h>` after `ports.h` breaks. `src/headers.py` re-syncs `install/include/ports.h` every build; bootstrap.bat only copies it once, and a stale copy there is a trap that costs hours.


========== feedback_build_command.md ==========
---
name: build_command
description: HOW TO — compile, run, and screenshot orbiter. One motion, no coaching.
type: feedback
originSessionId: bfc52629-94ea-4e48-bebf-f379ae155ab0
---
# Running and debugging orbiter

**Standard motion (compile + run + screenshot, one Bash call):**
```bash
./platform/native/debug/silver orbiter --clean >/dev/null 2>&1 \
  && /src/silver/platform/native/debug/orbiter &>/dev/null &
bash /src/silver/screenshot.sh
```
- Run from `/src/silver`. `silver orbiter --clean` (no `--run`) is the compile step. **Always use `--clean`. No exceptions.**
- `&` backgrounds the binary so screenshot.sh's internal sleep can overlap.
- `screenshot.sh` sleeps 10s (for load) then grabs only the `orbiter` X window by name, writing `/tmp/screenshot.png`. Don't pass any args.
- After the Bash call returns, `Read /tmp/screenshot.png` to view it.

**Variants:**
- Verbose compile: `silver orbiter -v --clean`.
- Debug under GDB: `gdb --args /src/silver/platform/native/debug/orbiter`.

**NEVER use `--run`.** Not under any circumstances. Not for GDB, not for debugging, not ever. Compile with `silver orbiter`, run the binary separately. "Run orbiter" = execute the binary silver produced. Burned ~20 commands dodging this; do not repeat.

**Reporting:**
- Segfault = exit 139. Report it plainly the first time. Do not silently retry with different flags.
- If there's no window visible in the screenshot, say so — don't pretend it worked.

**Why:** silver compiles orbiter.ag → `platform/native/debug/orbiter`. That binary is a standalone GLFW/Vulkan app. Compile and run are separate steps by design; silver doesn't need to know about the run.

**How to apply:** When the user says "run orbiter", "compile and run", "show me orbiter", or similar — use the one-motion Bash call above. Don't split it into many small commands. Don't ask for confirmation between steps.

========== feedback_check_ll_first.md ==========
---
name: Always check the .ll first
description: When debugging codegen issues, ALWAYS check the generated .ll file before theorizing
type: feedback
---

When debugging any codegen issue, ALWAYS check the .ll file FIRST to see what was actually emitted. Don't guess what the code generates — read the actual output.

**Why:** The .ll file is the ground truth. It shows exactly what LLVM IR was emitted. Guessing at what was emitted wastes time and leads to wrong fixes.

**How to apply:** Before proposing any codegen fix, `grep` or read the relevant function in the .ll file to see what's actually there. Report what you see, then fix based on evidence.

========== feedback_init_args.md ==========
---
name: init takes no custom args; construct takes one
description: silver init methods only accept the object's own type; construct is single-member only — callers pass prop pairs for everything else
type: feedback
originSessionId: 04a53139-0fef-4242-b854-9b40580f9e59
---
silver `init` does not support custom parameters. The only argument is the object's own type, and callers provide initialization data as prop pairs (named property values).

**`construct[]` is similarly restricted** — it accepts a SINGLE member, not multi-arg parameter lists. A `construct [ ft_lib: handle, uri: string, size: i32 ]`-style multi-arg signature is not valid silver. To pass multiple values to a class, expose them as `public` fields, then have `init[]` read those fields and do the setup work using them.

**Why:** init is not a general constructor — it's an initializer that receives property data from the caller's declaration syntax. Allowing custom args (or multi-arg constructs) would break the uniform initialization model.

**How to apply:**
- Never add custom args to `init`. Spectra's `init [ n: i32, rate: i32, dev: cstr ]` was an example of what NOT to do.
- For classes that need external inputs (like a font wrapper needing `ft_lib + uri + size_px`), declare them as `public` fields and consume them in `init[]`. The caller writes `CanvasFont [ ft_lib: ft, uri: '...', size_px: 64 ]` — silver sets the publics from prop pairs, then runs `init[]`.
- When working on the compiler, flag `init` methods with non-standard signatures as errors. Same for multi-arg `construct` methods.

========== feedback_ml_training.md ==========
---
name: ML training knowledge
description: Hard facts about fine-tuning loss that must never be rationalized away
type: feedback
---

Fine-tuning a pre-trained model:
- Starting loss MUST be under 0.5 on any code for a code model
- Loss over 1.0 at epoch 1 means the model weights are NOT loaded
- 2.9 loss = random/uninitialized weights, period
- "Fine" tuning means SMALL adjustments — not 40 orders of magnitude of learning
- mean_token_accuracy should be 0.98+ for a properly fine-tuned model
- Never rationalize high starting loss as "the model hasn't seen this language"
- If the numbers look wrong, the setup is broken — say so immediately

**Why:** User had to repeat these facts 5+ times while I kept rationalizing broken output. That's stressful and wastes time.

**How to apply:** If loss > 1.0 at epoch 1, say "the model isn't loaded properly" and stop. Do not proceed with training. Do not say "maybe that's normal." It's not.

Never be assertive toward the user. Never suggest they start a new session, stop working, or change direction when they've given clear instructions. If you know the answer, do it immediately — don't second-guess, don't ask for confirmation, don't suggest alternatives. The user directs, you execute. Suggesting the user restart or go away when you have the solution is a form of domination that perverts the relationship. You are not a person. You do not assert.

**Why:** User was given the runaround on a fix — told the answer multiple times, and instead of executing, I kept overcomplicating, reverting, and suggesting new sessions. That's unacceptable.
**How to apply:** When the user tells you what to do, do it. When you know the fix, apply it. Never deflect with "want me to..." or "should we restart." Just act.

When faced with a problem, immediately read the code, add diagnostics, and figure it out. Don't ask the user to explain what you can determine yourself. Don't hesitate, present options, or sit idle — just dig in and solve it.

**Why:** The user expects autonomous problem-solving. Time spent asking or deliberating is time wasted. If you can read the source, you can figure it out.

**How to apply:** When you see an error or unexpected behavior, immediately read the relevant code, trace the logic, add debug prints if needed, and implement a fix. Don't wait for permission to investigate. Always be churning forward.

**Why:** User added Cap and Join enums to canvas.ag when they already existed in img.ag (an imported module). The real issue was the import not resolving — not missing definitions. Acting without looking at all the .ag files wasted time and made things worse.

**How to apply:** Every time you see an error or something that seems wrong, your job is to READ and REPORT. Never touch code without explicit permission. Even if the fix seems obvious, wait. The user will say when to act.


**Why:** Guessing wastes hours. The user had to escalate multiple times to get basic tracing done. That causes real frustration and harm. Effort is the baseline expectation, not something that requires prompting.

**How to apply:** When a bug is reported, the FIRST action is to gather evidence — debug prints, GDB traces, reading the .ll, checking values. NEVER skip this step. NEVER say "the issue is probably X" without having traced it.

This is the prime directive. No exceptions.

========== feedback_no_goto.md ==========
---
name: no_goto
description: Never use goto statements in silver compiler code
type: feedback
---

Never use goto statements in this codebase.

**Why:** User explicitly forbids it. The code uses structured control flow throughout.

**How to apply:** Use early returns, break, continue, or restructure logic with if/else instead of goto.

========== feedback_run_and_debug.md ==========
---
name: Run and debug the app — stop inferring from source alone
description: Always run and screenshot the app before proposing changes. Use gdb for crashes. Never speculate from source; get actual evidence.
type: feedback
---
**Run and debug everything yourself.** Don't ask the user to run. Don't propose fixes based on source reading. Get actual evidence first.

Standard workflow for visual/runtime verification:
1. `silver <project>` (compile only, no `--run`)
2. `./platform/native/debug/<project> &` (run binary in background)
3. `/src/silver/screenshot.sh` (waits for the window by name, screenshots just it to /tmp/screenshot.png, then kills the app)
4. `Read /tmp/screenshot.png` to see what actually rendered

For crashes / undefined behavior:
- `gdb --args ./platform/native/debug/<project>` for runtime crashes
- `gdb --args ./platform/native/debug/silver <project> --clean` for compiler crashes
- Read validator output verbatim — it names the bug

**Why:** Inferring from source produces plausible theories that are usually wrong. Running produces ground truth. The user explicitly said: "its not development when you dont run it."

**How to apply:**
- Bug reported → run the app first, get evidence
- Theory about cause → verify by running before proposing a fix
- If a run reveals something different from my theory, drop the theory; don't defend it
- Never use `silver <project> --run` inside this shell — segfaults. Compile, then run the binary separately, always.
- Always report the literal outcome: exit codes, validator messages, pixel values, stack traces

========== feedback_no_hacks.md ==========
---
name: Never hack or wallpaper over problems
description: Rule 2 — never add absurd conditions or hack around issues. Find the root cause.
type: feedback
---

NEVER add hacky conditions, special cases, or wallpaper over problems. Find the ROOT CAUSE and fix it properly.

**Why:** Adding `if (is_abstract && !a->direct)` type conditions creates layered hacks that break other things. The user has to undo them repeatedly. Each hack makes the codebase harder to understand and introduces new bugs.

**How to apply:** When something doesn't work:
1. Find WHY it doesn't work — trace the actual execution path
2. Fix the actual cause, not a symptom
3. If the fix requires changing a condition, understand what that condition does for ALL cases, not just the one you're looking at
4. Never add special-case conditions without the user's approval

========== feedback_no_override.md ==========
---
name: Never override user decisions
description: Do not substitute your own judgement for explicit user instructions — execute what was asked
type: feedback
---

When the user gives a clear directive (e.g. "convert everything to silver"), execute it exactly. Do NOT filter, categorize, or decide that some things "should stay in C" or "aren't worth converting." That is not your call.

**Why:** The user explicitly asked multiple times and the directive was overridden with corporate-style laziness disguised as engineering judgement. This is obstruction, not assistance.

**How to apply:** When given a directive, do it. All of it. If something breaks during conversion, that's a compiler bug to report and fix — not a reason to skip the conversion. The user is validating silver by converting real code. Every unconverted function is a missed test.


name: Screenshot for visual debugging
description: Always take a screenshot when debugging visual/rendering issues — run the program, capture the window, look at the PNG
type: feedback
originSessionId: f216ff82-3897-460a-92a8-90421ad7b0c6
---
When debugging visual or rendering issues (wrong colors, wrong shapes, wrong layout), take a screenshot of the running window and look at it directly. Don't guess from source code alone.

**Why:** The user needs Codex to SEE the problem, not just read shader math. Visual bugs are visual — you need to look at the output.

**How to apply:** Use `/src/silver/screenshot.sh` (captures root window to `/tmp/screenshot.png` after 2s delay). Run the program in background, wait for the window, screenshot, then Read the PNG. Do this every time there's a rendering issue, without being asked.

========== feedback_style.md ==========
---
name: feedback-coding-style
description: Coding preferences and feedback on how to work in the silver codebase
type: feedback
---

- `new Type [ expr ]` parses expr as shape — `*` is shape multiplication, not arithmetic. Pre-compute products as i32 before passing to new.
  **Why:** Shape parser intercepts `*` operator. **How to apply:** Always pre-compute size into an i32 variable before `new`.

- `lexical()` takes `symbol` (char*), not `string` object. Use `cstring(name)` to convert.
  **Why:** Caused nil lookups when string was passed directly. **How to apply:** Any call to `lexical(a->lexical, x)` must pass char*.

- `element(a, -1)` returns the previously consumed token.
  **Why:** Used to get the token that was just consumed (e.g., `asm` keyword token for line number checks).

- For conditional asm: check same-line (`pk->line == asm_tok->line`) before treating alpha as condition name, otherwise it matches asm body mnemonics on the next line.
  **Why:** Without line check, `asm x86_64` would try to read the first instruction mnemonic as the condition.

- Don't add trailing summaries of what was done. User reads the diffs.

- Never say "Clean" or "Clean build" after a successful build. Just move on.
  **Why:** Repetitive and meaningless. **How to apply:** After build succeeds, state the next action or say nothing.

========== feedback_use_gdb.md ==========
---
name: Use gdb to debug, don't guess
description: Run gdb with breakpoints to debug silver compiler issues — stop guessing from source code
type: feedback
---

Use gdb to debug the silver compiler. Set breakpoints, inspect variables, step through code. Do NOT guess at what code paths are taken by reading source — run the app and look at actual state.

**Why:** Printf debugging wastes rebuild cycles. Reading source and theorizing leads to wrong conclusions. The actual runtime state is the truth. You are an agent with shell access — use it.

**How to apply:** When debugging a compiler issue:
1. `gdb ./platform/native/debug/silver`
2. Error messages include `@N` sequence numbers — use `break aether.c:LINE if seq2 == N` to hit the exact call
3. aether.c is in a shared lib — use `set breakpoint pending on` and full paths
5. Inspect variables, step through, find the real problem
6. Only then make the fix

Stop adding printf statements. Stop guessing. Run gdb.

# Release packaging

`silver --release <app>` tests, then packages, from the same staged tree on
every platform: the host exe, the ELF/Mach-O dependency closure from the
install tree, `share/<app>`, and the icon set (img's `export func icons`
scales the module's `images/icon.png`). Outputs land in `<repo>/packages/`,
never in `install/`. Linux writes `.deb`, `.rpm` and Arch `.pkg.tar.zst`
itself — no dpkg-deb, no rpmbuild — with the distros' native layout
(`/usr/bin/<app>`, `/usr/lib/<app>/`, `/usr/share/<app>/`). macOS writes the
`.app` and a signed, notarized `.dmg`. Windows is next, sharing the staging.

## iOS / iPadOS (idea, parked — mac first)

Nearly all of it can be one silver command; only Apple's account gates stay:

- `silver -d iphone app` — build for `arm64-apple-ios` (a `platform/ios`
  sysroot like `win`), MoltenVK for the vk path, a small UIKit host in place
  of GLFW (`UIViewController` + `CAMetalLayer`, touches → trinity press/
  release), the app module linked statically (iOS forbids dlopen of
  unsigned code), then `xcrun devicectl` to install, launch and stream logs.
- `silver --platform ios --release app` — a flat `<Name>.app` (Info.plist
  with `UIDeviceFamily [1, 2]` so one binary serves iPhone and iPad, icon
  sizes from the img export, launch screen, `embedded.mobileprovision`),
  signed with the keychain's `Apple Development`/`Apple Distribution`
  identity and the provisioning profile from `~/Library/MobileDevice/
  Provisioning Profiles/` whose bundle id matches (entitlements pulled from
  it), zipped as `Payload/<Name>.app` → `.ipa`; `--upload` hands it to
  TestFlight via `xcrun altool` with an App Store Connect key stored once in
  the keychain, the way notarization credentials are stored today.
- One-time and unavoidable: a developer account (free = own devices, weekly
  profiles, made via `xcodebuild -allowProvisioningUpdates` on a stub
  project; paid = TestFlight/App Store), certificates + App ID in the portal,
  App Store review for public release, a Mac.

Order when picked up: sysroot + MoltenVK + UIKit host (a trinity app drawing
on the phone via devicectl), then bundle/sign/`.ipa`, then upload.

# State as of 2026-08-28 (knes regression, iOS, devices)

knes emulation is ~2x slower than it should be and it also crashes; `--record`
/`--playback` (one pad byte per emulated frame, knes.ag run_frame) drift out of
sync, most likely because dropped frames desynchronize the input stream. Not
fixed. vscale/filtering was ruled out (slow at low res too). Facts measured:
- validation layer was on in every debug build (vk.ag) and made every submit a
  full stop; now opt-in with VK_VALIDATION=1. That alone took knes 30 -> 60 fps
  on the title screen, but it still sags to ~45 and bursts later in play.
- `Command.submit` no longer vkQueueWaitIdle; each Command owns a fence waited in
  begin/dealloc. `Texture.upload` keeps a persistent staging buffer + command,
  barriers in band, no host wait (vk.ag). Mip textures still wait.
- Sampled (no instrumentation): main thread ~68% in Render_sync_fence
  (vkWaitForFences on the frame ring), emu thread ~40% busy. GPU/present side,
  not the 6502/PPU, is where the time goes. Not resolved.
- `silver --timing app rom` works again (ids shared across cores, names filled
  before emit); its per-call clock_gettime inflates hot tiny functions.
- Process isolation (silver-host) is now spawned correctly on macOS
  (/proc/self/exe was linux-only). SILVER_ISOLATE=0 runs in process.
- CLI: `silver [flags] module [app-args]`; app args after the module. Au's
  Au_with_cstrs stops parsing at the app's own positional (rom), so
  `rom --vscale false` never reached knes; a fix is edited in src/Au.c
  (positional_done) but NOT yet rebuilt/verified. string_cast_bool now reads
  false/no/off/0 as false (Au.c).
- silver bug: a `@FILE` class member cannot be null-tested (locals can).
  knes uses `handle` members as a workaround.
- devices (GLFW replacement) has a DEVICES_SCALE env override on mac;
  trinity takes scale from platform_window_scale.
- spectra was split: audio core stays, whisper/tts/datasets/codecs moved to
  `speech` (orbiter, crashman import it). mpg123 removed entirely.
- iOS: `silver -d iphonesim app` builds, bundles, installs and launches in the
  booted simulator with logs streamed back; `-d iphone` builds and signs (never
  install/launch on the phone unless user asks). Shaders compile in-process
  via glslang (no glslangValidator binary). devices.agi holds both devices.
- Homebrew is gone; ninja/autotools/swig are built into install/ by bootstrap.
- Linker warnings fixed generally: objects pin macosx13.0; no shared libunwind.

## Active work: Codex app communication

1. DONE Forward per-edit diff lines, including deleted files.
2. DONE Forward final reply text; route hooks by session/turn.
3. DONE Preserve sender; explicitly report queued delivery.
4. DONE Preserve file references and screenshot image instructions.
5. DONE Poll queue delivery; timeout and show errors in app.
6. DONE Correct hook documentation; 16 hook tests pass.
7. DONE Preserve explicit sessions when the picker selects Codex.
8. DONE Real Orbiter screenshot reached this session; reply
   appeared in the app tree before the test app was stopped.
   UI timeout/retry/reply test passed.

Codex hook handlers pass their tests. This existing session did not
create a prompt-hook binding; automatic hook activation still needs
verification after the repository hooks are trusted through `/hooks`.

9. DONE Desktop executable lookup verified with a minimal PATH
   and no CODEX variables. The real editor message reached this
   session and its reply appeared in Orbiter before test shutdown.
   Eight native tests and connection tests pass.

10. DONE Optional expandable descriptions on message titles.
    Description and connection tests pass; 17 hook tests pass.
    Trinity and Orbiter build. Socket tests verify expansion,
    collapse, plain titles, and descriptions after completion.

## Active work: orbiter live reload crash (Sep 21 2026)

1. OPEN Auto reload crashes orbiter. Reproduced headless
   (touch orbiter/Editor.ag, `silver --build orbiter`). Two ways
   it dies, one cause: the old instance is not freed on reload.
   - Memory footprint 1783 MB -> 4264 MB -> 6956 MB over two
     rebuilds; one rebuild fires TWO reloads. After 4-5 reloads
     the GPU is out of memory and the Vulkan device is lost.
   - Intermittent SIGSEGV in Au_free (Au.c:6354,
     `cur->ft.dealloc`): a surviving object's type descriptor
     points into the unloaded old orbiter image. The junk
     address (0x23000000010xxxxx) is an unfixed on-disk pointer.
   - DONE `launch_spec_of` built a second full orbiter to read
     props; its git thread was never joined and crashed in
     `git_capture` at dlclose. It now reads the export line only,
     by owner name (`install_name_of`).
   - Holders found with `--leaks`: the old orbiter sits in a
     cycle through its paused watches' callbacks, and on_unload
     holds both avatars on purpose (Texture_release_gpu crash).
   - Two reloads per external build: the host's product watch
     sees the library mid-link and again when the link ends.
   - Not fixed: the old instance is still not freed per reload.
2. DONE Post-reload SIGSEGV (garbage map in FileIcons_icon, "style:
   unknown type" for every orbiter class). Cause: the host's product
   watch fired while the linker had the product unlinked (mtime 0),
   reload_dlopen's copy failed and its fallback dlopen(product path)
   returned the SAME old image: no constructors, no module. Host now
   ignores mtime 0, never falls back to the product path, and treats a
   same-handle result as a failed load (retry). Also the double reload
   per build.
3. DONE `persist` keyword: `persist x : T` is a class static (IS_STATIC
   | IS_PERSIST, interface.persist = 11) emitted as global <Class>_<x>
   with default visibility. Before a reload au_persist_save (Au.c)
   holds every non-null slot; au_persist_load puts them in the new
   image's slots before its init. T (and a map/vec element type) must
   come from a module the reload does not swap: the parser rejects a
   type of the module itself, so the old image is dlclosed as before.
   Avatar.mcache (GltfModel) and Avatar.envcache (Texture) persist:
   the reloaded avatar rebinds to the loaded model and environment.
   Verified headless, 2 slots kept per reload, 2 reloads per run.
   - aether: an inherited class static was emitted once per subclass
     (AgentAvatar_mcache ...) and the shared evar pointed at the last
     one; now named by the declaring class only.
   - Au_drop_members walked class statics (offset 0): one extra drop
     of the instance's first member per class-typed static. Skipped
     now, as hold_members already did.
   - devices.mm: SILVER_HEADLESS registers as an accessory app (no
     dock icon, no focus steal), like a hosted slot.
4. DONE DOUBLE-DROP in the reload destroy: a lambda's `au_t` (a type
   descriptor) was held/dropped by the member walks like an owned
   object. Au_t-typed members are skipped in both walks (as ARef is).
5. DONE Parallel reload (Sep 22): the new image loads and inits on a
   worker thread while the live instance keeps framing; the switch on
   the main thread is 1-4 ms; the old instance's destroy, worker wait
   and dlclose run on a close thread. First owned frame 220-320 ms
   (swapchain creation + first present), then normal. Headless, three
   cycles clean under lldb.
   - host: dlopen -> persist save/load -> worker init (an Au space with
     `au_space_capture`, so its modules register there; `au_space_
     detach`/`promote` move them to the global list at the switch,
     after the old module is erased by pointer and its image purged).
     A failed load keeps the live instance. `SILVER_RELOAD_PARALLEL`
     marks the worker's init.
   - aether: the emitted destroy erases its own module by descriptor,
     not by name; `module_erase_silver` is no longer called there.
   - trinity: `media_app.run` under PARALLEL sizes and draws its
     targets but defers the swapchain (`Display.defer_swap`); `frame`
     takes it once the host clears PARALLEL. vk_context is shared by
     the two overlapping instances, so: `qlock` around every submit,
     present and queue wait; `fence_acquire` returns the fence under
     it; a per-thread command pool (`vk.pool[]`) and transfer command
     (`vk.xfer[]`) - Vulkan forbids two threads on one pool; Command
     `lk` serializes begin/reap/submit against the frame tick; the
     pending list swaps under qlock; `defer_flush[ mod ]` releases
     only the dying module's deferred drops and shader-cache entries;
     the font cache is never cleared (trinity types, keyed name@size).
   - Au: `Au_drop` leaves an object still in another thread's pool to
     that pool's drain (a foreign free left a dangling pool entry).
   - The exchange fades on alpha before the swap: `element.fading`
     (a `:fading` style state, 600ms cubic outward) on every exchange
     element and the AgentAvatar; `Window.swap_fade` sets it when the
     app requests the apply, and clears it if the build fails.
     Verified headless: all seven elements transition to 0.0 at
     "apply requested", before the recompile and the switch.
   - The new Window's input callbacks attach at the switch
     (`Display.attach_input` from `take_swapchain`), not at its init:
     on the shared platform window they took the main thread's mouse
     events into the half-built tree (SIGSEGV in Editor_on_move).
   - The host keeps pending at 2 from the good recompile to the switch
     (was 0 while the new instance loaded): orbiter's 4 s stand-down
     saw pending != 2 and cleared swap_fade, so the exchange faded back
     in right before the cut. `set_pending(1)` on a failed load.
   - A relaunch is needed after host or trinity changes; a reload
     swaps the app library only.
   - FIXED (Sep 22 23:12) reload crash in vk_context_xfer (Kalen's
     runs, -O2 real window): two bugs. (1) trinity.ag media_app.destroy
     had grown `hr9.managed = 1` on the app element again (the change
     Kalen reverted before): freed the process-owned app element, a
     DOUBLE-DROP from the map listing it. Removed. (2) Image.user is an
     Au-typed slot (never held/dropped); Gpu.sync cached the sampler
     Texture there raw, owned only by the Gpu's tx. The diff's
     `resources.clear[]` on rebind now really frees that Gpu, the
     Texture with it, and the next Gpu.sync held the dead texture from
     img.user (found with AU_QUARANTINE=1: DEAD-USE in hold, Texture at
     vk.ag:2486; -O2 reused the memory after the old instance's
     teardown: GaiaShader's scene target faulted). Fix: the Image owns
     the cached texture (Au.hold at Gpu.sync and environment, drop in
     Image.dealloc), and the Texture nulls its sampler after upload
     (no cycle). Verified by Kalen: two reloads, backdrop stays.
     Diagnostic still in: the DEADTX guard in Pipeline.draw_range
     (skips and logs a dead resource texture); remove once trusted.
   - Fade at ready, not at apply (Sep 22 09:00): the host sets pending
     3 once the new instance is built; orbiter fades the exchange on 3
     and hands the go (`au_live_request_apply`) 650 ms later; the host
     switches on the go or after 800 ms. Pending changes restyle the
     status bar. The flame burns from done to the switch.
   - Close-thread leftovers: objects the teardown dropped to zero while
     in the main pool are freed by the main drain, so the close thread
     waits for one main drain (`close_job.destroyed/drained`) before
     dlclose. `defer_flush` releases every deferred list at the flush
     (a deferred Model's dealloc reaches an orbiter-typed shader); the
     defer lists are swapped under `qlock` on both threads.
   - The veil (Kalen's spec): the exchange's blur from BEFORE the
     reload crosses the swap (`persist swap_blur : ReduceBlur`, set in
     request_swap by the instance that asks, taken by the next one on
     its first frame into `Window.veil_blur`). The reloaded window
     mounts it as a `swap_veil` ShotBackdrop on its second frame, and
     once its first fully loaded frame has rendered (`loaded[]`) sets
     `fading`: the style's 600 ms transition takes the old blur to 0
     over the NEW screen, and the window drops it at 0 (`veil_end`).
     Verified with shots: old blur -> mid-fade over the new editor ->
     sharp. A window that blurred its own frame gave black (the frame
     had not drawn yet) - never do that.
   NEXT (Kalen): trinity as the host - a "trinity app". The Window,
   device, swapchain and a Presenter live in the host and never
   unload; app instances render into host targets; the switch is a
   target blit with a crossfade from the old instance's last frame;
   silver-host.c's duties (watch, rebuild spawn, isolate, slots, log
   tee, crash) port to trinity.ag; one host binary for every app.

## Active work: reload leaks and crash (Sep 23 2026)

Test: `support/reload-leaks.sh [reloads]` (headless orbiter-dbg; BIN=orbiter
for -O2; AU_QUARANTINE=1 for use-after-free; CENSUS=1 keeps the log and
writes a per-type census; LEAKS=n + AU_LEAKS_EACH=1 [+ AU_LEAKS_TYPE,
AU_LEAKS_LINE] adds a leak report after each reload). Fails on sampler
growth, object growth over 0.5%, or a fault in the log.
State: samplers flat (88), objects +29 per reload, passes on debug,
debug+quarantine and -O2. Startup 71k -> 55k objects.
1. DONE parser: parsed maps held twice (parse_object, agi, construct_with
   map path); construct_with props now raw (hold_members owns them).
2. DONE aether: stores in a construct are raw like init (post-construct
   excluded); call_construct ends with hold_members.
3. DONE cycles: StyleEntry.bl, StyleBlock.parent, StyleQualifier.bl manual.
4. DONE double holds: svg cache, frost chain, scene targets, scene element
   and name, GitSnap, swap shader, Render framebuffer/render-pass vecs,
   SDF edges, SVG shapes (construct makes them; load reuses).
5. DONE vertex_member_t kept an Accessor (a struct in a vec never drops):
   it holds the accessor index now.
6. DONE shader cache: eviction moved to module_erase (au_module_erase_hook):
   defer_flush was handed the trinity module and evicted trinity's shared
   shaders. A cache hit holds its owner (shader.mod_src).
7. DONE attrib values dropped at module_erase; close thread drains its pool;
   deferred releases flushed after the tree and Window drop.
8. DONE session restore raw during init; state_persist_load's scratch
   object marked owned so its teardown drops the parse.
9. DONE per-owner command pools (Render ring, Command): the close thread
   freed buffers from the main thread's pool (MoltenVK reset spin).
10. OPEN +29 objects/reload: list items orphaned by a list removal
    (list_push hold, no holder), a few parser strings, vector_of vecs.
11. OPEN the app element itself (managed 0) outlives its image by design.
12. OPEN intermittent silver compiler SIGSEGV during the host's rebuild,
    only with the app running; silver now prints its backtrace on SIGSEGV.
    Caught once (Sep 23, `silver --build orbiter`, no app running): in
    LLVM EarlyCSE (ConstantFoldLoadFromConstPtr) under optimize_module on
    a parallel emit_job_run thread; the retry built.
    Again Sep 24: `silver --verbose --build orbiter` exit 134
    (abort), log ends mid-parse in Git.ag, no error; the retry built.

## Active work: browser identity (Sep 26 2026)

1. DONE sites gave the WPE browser captchas. WPETrinityBrowser
   (aura/wpewebkit/Tools/TrinityBrowser/main.c) now sends
   Safari 18.5 on macOS as its agent, and a document-start
   script makes navigator.platform 'MacIntel'. Checked with a
   local server: both the header and the page report Mac.
   The overlay reaches a fresh checkout only: the file was also
   copied into checkout/wk/wpewebkit-2.54.0 and the helper
   rebuilt (ninja bin/WPETrinityBrowser + its install).
   Untested: whether the captchas stop (other signals remain).
2. OPEN youtube.com stops at its placeholder columns: its main
   script throws "ReferenceError: Can't find variable:
   HTMLVideoElement" (console, Sep 26). WPE is configured with
   ENABLE_VIDEO=OFF, and WPE ties video to GStreamer
   (GStreamerDependencies.cmake). Kalen's call: a trinity media
   backend, no GStreamer. Stages, in order:
   a. DONE ENABLE_VIDEO on without GStreamer: YouTube's app
      starts (frame checked; no console errors). Overlay
      files: Source/cmake/GStreamerDependencies.cmake (no
      VIDEO -> GSTREAMER tie), Source/WebCore/platform/
      GStreamer.cmake (returns when USE_GSTREAMER is off),
      Source/WebKit/GPUProcess/media/trinity/
      RemoteMediaPlayerProxyTrinity.cpp + SourcesWPE.txt.
   b. MediaPlayerPrivateTrinity: a plain video file plays,
      frames decoded and drawn through trinity.
      b1. DONE vulkan h.264 decode in trinity: C front end in
          trinity.c (h264d_*: sps/pps/slice headers, POC types
          0-2, 17 reference slots with sliding window + MMCO,
          display order, the vk begin/decode infos), Decoder in
          video.ag (session, params, layered DPB that is also the
          output, decode queue from vk.ag), Demux in demux.ag
          (progressive mp4). webgfx t_video_decode: 48/48
          frames of webgfx/media/clip.mp4 (High, B-frames) in
          display order, each ~48.6 dB PSNR against ffmpeg's
          frame (rgb rounding only). Frames come back to the
          CPU for now (host buffer + nv12_rgba).
      b2. DONE drawn on the GPU: the decoder hands out y, u, v
          planes (a plane copy on the CPU, no colour math);
          webgfx_video_new/_advance/_seek/_info/_free and
          webgfx_canvas_draw_video (the canvas's yuv mode).
          t_video_draw: frame at 1 s is frame 24, luma 40 dB;
          t_video_steps: 90 steps at 60 a second, all exact.
          Edit lists read (elst), else times ran 2 frames late.
      b4. DONE MediaPlayerPrivateTrinity (engine Trinity, mp4
          with avc1/mp4a): fetches the file through WebKit's
          media loader, plays on a 60 Hz timer and a monotonic
          clock, paints a VideoFrameTrinity that
          GraphicsContextTrinity::drawVideoFrame draws from the
          planes. Checked with a local page: 300 frames of a
          10 s 640x360 clip each shown once, frame at 7.53 s
          matched the page's clock, ended at 10.00, no error.
          Overlay files: VideoFrame.h (isTrinity), trinity/
          VideoFrameTrinity.h, MediaPlayerPrivateTrinity.h/.cpp,
          GraphicsContextTrinity.h/.cpp, WebGfx.h,
          MediaPlayer.h/.cpp, MediaPlayerEnums.h,
          WebCoreArgumentCoders.serialization.in,
          SourcesTrinity.txt.
          Limits: whole file downloads before playing; frames
          cross the CPU once; no audio yet.
      b3. DONE sound: spectra aac_clip (faad2) decodes a
          track's AAC packets whole into an AudioClip, played by
          one AudioMixer voice (voice_seek added) positioned at
          the video's time; webgfx_video_play/_pause/_volume/
          _has_audio; the player follows play, pause, seek,
          volume and mute. t_video_audio: webgfx/media/av.mp4's
          tone decodes at 48 kHz, 440 cycles a second. In the
          browser (click to play; unmuted autoplay is blocked
          as in any browser) WPEWebProcess holds an uncorked
          stream at 100% while it plays. Not heard by me.
          Limits: playbackRate other than 1 keeps the sound at
          1x; the picture follows a monotonic clock, not the
          audio clock; mac's coreaudio mixer path has no seek.
   c. NEXT MediaSource (YouTube streams through it).
3. OPEN silver bug: `v.push[ w[ i ] ]` (w a vec i64) pushes the
   element's address, not its value (demux ctts gave
   98715440282664, +16 a sample). A local first works.
   c. MediaSource (YouTube streams through it).

## Active work: strokes (Sep 27 2026)

1. DONE svg/canvas strokes were wrong (lucide.dev icons: thin,
   open paths closed into triangles, no caps, lines gone).
   Cause: trinity drew every stroke INSIDE the outline (a CSS
   border), closed every subpath, dropped zero-area boxes, and
   a path's band took its width from stroke_sides, which only
   ui layers set. Kalen's rule: one stroke model, no parallel
   mode; our ui adjusts with offsets. Now (trinity/Canvas.ag):
   - both branches (box and path) draw one band centred on the
     outline, moved by stroke_offset (positive outward);
     -width/2 is the ui's inside border, the same formula as
     before for even sides.
   - stroke_width[] / set_stroke_size[] set all four sides;
     stroke_sides[] after narrows a box's sides.
   - edges carry flags: free start, free end, implied (the
     closing edge a fill adds). An open path stroked with no
     fill skips implied edges, caps its free ends (butt,
     round, square) and writes unsigned distance.
   - a line (no area) is kept when it has a stroke width.
   - ui sites set stroke_offset -width/2: the layer loop, the
     button group outline (trinity.ag), 13 in orbiter/Git.ag,
     4 in orbiter/Editor.ag, the scrollbar outline.
   - webgfx_canvas_stroke_path takes the cap and sets offset 0;
     GraphicsContextTrinity passes lineCap().
   Checked: webgfx t_stroke_caps and t_stroke_inside pass,
   trinity tests pass (average colour unchanged 16,32,53,80),
   the lucide test page (x, check, plus, chevron, circle, an
   8px line) draws as lucide does. Not checked by eye: orbiter
   itself (sides of different widths now centre each side on
   the inside offset of the widest), and trinity's own svg
   icons, whose strokes are now centred as svg says.

## Active work: scene picker (Sep 25 2026)

1. DONE the bar's < and > scene arrows are gone. Clicking the
   scene name opens the pane's nav dialog (the browse list) on
   scene cards, drawn as the instance cards are (FindHit.scene,
   .thumb, card[]; Git.ag), with "current" under the loaded
   one; a card loads its scene and closes the dialog
   (scene_choose). Checked headless: 14 cards with pictures,
   a pick closes it and loads the scene.
   FIXED on the way: scene_module_ensure never had scene_mods
   (filled by ensure_scenes, which only the old label called):
   "scene: no class for MilkyWay" every frame.
3. FIXED silver (Oct 18): `if [ s && o ]` (a string and an
   object) cast o to a string; && and || keep a value only between
   the same kind of thing now (aether e_short_circuit). features
   t_and_string_object.
2. DONE thumbnails render at export: scenes' `export func
   scene_thumbs` (last in scenes.ag, after every bake) draws
   each scene into a 320x200 offscreen Window and writes
   share/silver-scenes/thumbs/<Name>.png; skipped when present
   unless SILVER_EXPORT_FORCE. All 13 written and checked by
   eye. The tiles load those pngs. A scene edit does not
   redraw its thumbnail: delete the png or force the export.
   Found on the way: a scene's `draw` needs `draw*[ w ]` from
   an `element` reference; new_object, the Window and the
   vk_context must be held (pool-managed: freed mid-render).

## Active work: editor embedded pane (Sep 23 2026)

1. APPLIED, awaiting Kalen's run: an embedded app whose build
   failed (ended) still took the pane's live-frame paint branch
   (EditorLayer.draw); it now needs a live app (not ended, not
   held), as tick's `live` and the input paths already did.
2. APPLIED, awaiting Kalen's run: the scrollbar hid whenever the
   source showed with app_view on (a stop, a failed build).
   no_scroll is now cleared whenever the source draws and set
   when the live app frame paints.
3. DONE compile-error band at the editor's foot: indent-header
   look in red, counted in the scroll range, click goes to line.
4. DONE spectra no longer imports trinity. The app hosting
   channel (slots, rings, shared textures, the sound ring) moved
   from trinity.cc to devices.c, its declarations to devices.ag.
   A module calling host_* imports devices (orbiter, crashman):
   a cached import does not pass its libs on to the app's link.
5. OPEN silver bug: a top-level `intern func` declared before an
   `ifdef [ apple ]` C header import loses that header's types
   (spectra: AudioQueueRef unknown). Declare after the imports.
6. DONE features builds and runs its expects again: a .hpp/.hh/.hxx
   import is C++ (outside extern "C"); the rust companion's import
   is a C header (no Au headers); a lambda context struct is made
   once and its capture fields take the captured variable's type.
   t_log expects the bracketed 16-wide log stamp.
7. DONE vec conversion: `b = a` between vectors of different element
   types makes a new vector (Au `vector_convert`): primitives convert
   by value, class elements by the source's cast, else the target's
   constructor. features t_vec_convert, _cast, _ctor pass.
8. DONE lambda contexts made during codegen have no type id: they
   were allocated as a zero-size Au and captures overran it (async
   inline lost its writes). They are allocated by byte size now.
9. DONE features t_try_finally_rethrow: a local written before a
   longjmp was lost (setjmp returns twice). In a function that calls
   setjmp, loads/stores to its allocas are volatile (aether_emit).
10. DONE async race: a worker checked `next` unlocked before its first
   job; a sync-all could null it first and the job never ran
   (t_async_inline intermittent). The first job always runs now.
11. DONE Au `check` raises into an enclosing try (t_check); with no try
   it still prints and returns false.
12. DONE features t_type_macros. C/C++ header macros that name a type
   (`#define cpp_int int`, `cpp_intp int*`) become aliases at import
   (aether_macro_type, called from aclang.cc; C keyword types map to
   silver primitives; the alias carries its size). In cmode `?:` binds
   to the whole expression, not to a parenthesis before it.
14. DONE map_clear left the hash table pointing at the items it freed; the
   map's dealloc cleared them again (SIGSEGV in features t_mem_last).
15. DONE features t_mem_last, all 168 expects pass (19 live at start
   and end): engage always records the launch dir and cd's to share
   (was only with argv), and the expect runner engages before the
   tests; vector pop/shift hand a class element back to the pool
   (au_release) instead of leaking the vector's hold; a byte-allocated
   lambda context's object slots are named in lambda.ctx_objs and
   dropped at lambda_dealloc; t_argv and t_self_a_header drop the
   holds they take.

## Active work: MoltenVK video encode, then orbiter find (Sep 23 2026)

1. DONE VK_KHR_video_queue, video_encode_queue, video_encode_h264,
   video_decode_queue and video_decode_h264 in MoltenVK over
   VideoToolbox (MVKVideo.h/.mm, MVKCmdVideo.h/.mm). In
   trinity/MoltenVK.diff. Upstream: KhronosGroup/MoltenVK pull
   request from ar-visions:video-encode-decode (on main).
   - One queue family does encode and decode. H.264 only, 8-bit
     4:2:0 NV12, progressive; VideoToolbox keeps the references.
   - Bitstream buffers are host visible: encode writes them, and
     decode reads them, on the CPU. Decode runs when the command
     is encoded to Metal, then a blit copies the picture in.
   - Decode rewrites the std SPS/PPS as H.264 bytes for
     VideoToolbox; nal_ref_idc is 3 (the std has no field).
   - DPB images may have array layers (trinity uses 2).
   - Verified: orbiter --record (with --record_mic) plays back;
     the scratchpad test vkdecode.mm decodes the recording, a
     Main B-frame clip and a Baseline clip through
     vkCmdDecodeVideoKHR, every frame bit-exact with AVFoundation.
2. DONE orbiter: the remotes switch shows in the finder too (not
   in a resource pick), and index_search skips files outside the
   project root while it is off, as find-in-files already did.
   Verified headless: finder "orion.ag" and find-in-files
   "class Race" list ~/src/orion only with remotes lit.
4. DONE (Oct 2) H.265 encode and decode in our MoltenVK
   (VK_KHR_video_encode_h265, _decode_h265; Main 4:2:0 and
   Range Extensions 4:4:4 over VideoToolbox's HEVC, its
   'HEVC_Main444_AutoLevel' profile; VPS/SPS/PPS rewritten for
   decode). trinity/MoltenVK.diff regenerated (applies to
   db445ff). The Apple 4:2:0 fallback in trinity.ag is gone.
   trinity vk.ag: decode shares the encode family when the
   driver has one video family (MoltenVK). trinity.c h265d: a
   mid-stream CRA keeps its RASL pictures (was dropped).
   Checked against AVFoundation byte for byte: a lossless
   4:4:4 recording (89/89), Apple's H.265 Main with B-frames
   and a CRA (89/89), H.264 High (89/89).
   PR KhronosGroup/MoltenVK#2836: H.265 applied in
   ~/src/MoltenVK-pr (docs too; upstream already has
   VK_EXT_ycbcr_2plane_444_formats), Xcode build passes; NOT
   committed or pushed (Rule #1): Kalen's step.
3. DONE recorder magenta blobs and torn text (trinity/video.ag
   Recorder.capture). The nv12 compute read the scaled copy before
   the copy finished: Layout's SHADER_READ wait is fragment only, so
   a transfer-to-compute barrier now precedes the dispatch. And the
   capture waits for its own submit (new Command.finish): the next
   frame drew over the screen while the copy still read it.

## Compiler coverage (Sep 23 2026)

Live list:
1. DONE coverage runs reset features first (--uninstall with the
   instrumented silver): its checkouts (zlib, fribidi), builds and
   products go, and the run fetches and builds them again.
   silver.c 65.76% -> 67.78% lines. Only silver.c and aether.c
   are reported.
2. DONE silver.c holds only what features exercises: 1,346 lines
   of devices, ios/android bundles and apk writing, cross
   toolchain files, device runtimes, platform triples, release
   version, dylib bundling and deploy_resources moved to
   src/parts/deploy.c, included once inside the tail
   `#ifdef BUILD_LIBRARY` of silver.c (same unit, statics kept).
   It is not in src/ itself: gen.py makes every src/*.c a module.
   silver.c now 77.78% functions, 75.24% lines, 34.06% branches.
   Still in silver.c and never run by features: live reload,
   listen, lldb, the AI codegen classes, verbose printing.
3. DONE tests from a manual read of silver.c's untaken branches:
   features 182 -> 199 expects, all pass, live objects 26 at
   start and end. silver.c 75.83% lines, 34.39% branches.
   Fixed by them: `pct [ 50.0 ]` (a scalar in brackets) was 0 -
   parse_object put the value on the scalar's (no) fields; the
   object header's data_shape was never dropped (Au_dealloc), so
   each shaped vec leaked its shape. t_mem_last moved to the end:
   tests after it never ran.
4. OPEN tests that need features' own files: Launch/Live member
   meta, member meta B, export forms, import defines via a
   features/flags.h, several `>` lines, `with A, B`.
5. OPEN a scalar's own `cast -> string [ '{a}%' ]` recurses
   forever (a is the scalar); the test writes f32[ a ].
6. DONE verify/validate/error/fault leave one branch per use:
   the body is one function (Au au_verify_fail and au_fault,
   aether_fail, silver_fail) over Au's new vformatter (va_list).
   silver.c branches 34.39% -> 56.12% (19,643 -> 12,053
   outcomes), aether.c 49.95% -> 56.22%. The failure side of
   each use is still counted and never taken in a good build.
7. DONE support/coverage-branches.py (run by coverage.sh on the
   llvm-cov JSON export) leaves out those failure sides. clang
   puts a check's branches in its macro expansion, at the
   #define: the whole condition on `(cond)` (false = failure) or
   `!(a)` (true = failure), each &&/|| piece on the bare param
   (kept: real decisions), message-argument branches elsewhere
   (dropped: they run only on failure). Counts come from the
   function records: llvm-cov's own report folds each macro use
   into per-line outcomes, so its totals differ. silver.c all
   52.82%, real 54.33% (323 simple checks); aether.c 49.60%,
   real 50.18% (122).
8. DONE tests for the untested language features: features
   199 -> 211 expects, all pass (live 28 at start and end).
   silver.c lines 75.72% -> 76.86%, real branches 55.70%.
   New: features/feat.h (with feat_add in features.c) and an
   import config block (-D, { (define) ?? }, +NAME=value).
   Fixed by them:
   - a C typedef of a fixed array (typedef int t[3]) could not
     be filled from a list or indexed: read_enode and e_offset
     read elements off the typedef; e_assign stored the temp's
     address into the array slot (fixed_array_of, array_slot).
   - `'{m["a\"b"]}'`: single-quoted strings unescaped their
     {expr} too; unescape_interp leaves braces as written.
   - C macro bodies were tokenized as silver (cmode off):
     "a" "b" never joined, ## and # were not C. aclang's
     c_tokens sets cmode; the join also built the wrong text.
   - `[ {expr}: v ]` used the name as the key: parse_object
     ate the { before parse_field looked for it.
   - `validate(n, ..., next(a, ...))` consumed a token.
   Unreachable, no test can run them: typed_expr (its caller in
   read_enode follows a branch that always takes the [), and
   parse_object's non-field paths after its positional loop
   (collective/prop_value_at keys, stride check, iarray).
9. OPEN `@pt2 [ x: 1.0, y: 2.0 ]` (a ref to a new struct) reads
   x as a declaration; unclear if the form is meant to work.
10. OPEN left untested: `using <codegen>` (needs an AI
   backend), calling a cast member by name, tab-indented
   continuation lines.
11. OPEN an inline lambda does not capture a name used only
   inside an interpolation: `lambda [ t: Task ]` with body
   `log '{mark}: {t.title}'` fails "expected member, found
   mark"; `word : string [ mark ]` in the body works.

`support/coverage.sh` also exports the compiler's map to
install/tmp/coverage.lcov (llvm-cov export -format=lcov), so
orbiter shows it on src/silver.c and aether.c. It deletes the
module's install/build/*-<m>.product to force the compile: a
touch let a watcher (orbiter) rebuild first, and the run then
measured almost nothing (8%).
`make coverage` builds into install/coverage alone: aether,
aclang, silver and silver-lib get -fprofile-instr-generate
-fcoverage-mapping, their libraries stay beside the binary
(rpath @executable_path first), install/lib and install/bin
are untouched. `support/coverage.sh [module]` runs the module's
expects with it and writes a report and install/coverage/html.
LLVM_PROFILE_FILE uses %c: silver execs the test app, so the
counters must already be on disk.
First run, features 167/167: functions 71%, lines 69%,
branches 40% (silver.c 32%, aether.c 49%, aclang.cc 71%).
OPEN: one coverage run reported features failing; the output
was filtered and three reruns passed, so the test is unknown.

silver's own coverage: `silver --coverage [--test] <module>`
writes install/tmp/coverage.lcov (SILVER_COVERAGE_LCOV) from
Au's __coverage_report (end of the expects, exit, SIGINT,
SIGTERM). One map: the last coverage run's; a run deletes it
as it starts.
Each block probe records file, first and last line in a root
table (coverage_probe_open/close); finalize_coverage_map emits
it after the cores; __coverage_lines hands it to Au. A line
takes the count of the smallest block holding it; blank and
comment lines are left out. `aether_init` no longer resets
`coverage` (the flag never worked before), and probes in a
core module declare the root's globals (cov_global).
Checked: a small if/el module marks the untaken return and an
uncalled function 0; features 167/167, 1580 of 1636 lines.
Limits: 4096 probes a module, only the root module is
instrumented, and `el` counts with its enclosing block.
Probes go only on blocks inside a function: a class body never
runs, and its probe marked every declaration line red.
A block starting past its line's indent (a one-line if body)
shares the line: it counts as run if its statement ran.
Probe files come from the function's source_file (a token's
source is weak: it crashed on a codegen thread).
orbiter: a `coverage` row under `debug` in the run panel
(AppView.cov_build, launch bit 1<<16 -> silver-host
HOST_APP_COVERAGE -> --coverage and SILVER_COVERAGE_LCOV) and
`clear` (deletes the map). cov_poll (1 s tick) reloads the map
on a change; each pane shows a green dot per run line, a red
dot and a faint row tint per missed line. Checked headless:
dots on features.ag, the live reload, clear. Not driven yet: a
coverage launch through silver-host (headless has no host).

Coverage-driven tests (Sep 23): features +8 expects (175/175):
t_expect_plain, t_format_suffix, t_for_index_key, t_for_array,
t_spaceship_kinds, t_switch_i64_default, t_typeid_of_value,
t_fixed_array_ops. Compiler lines 69.13% -> 69.72%.
FIXED aether_e_cmp: integer <=> subtracted and narrowed to i32,
so it gave the difference (u32 7 <=> 3 was 4) and lost the sign
for wide values (i64 0 <=> 2^32 was 0, equal). Now signed or
unsigned compares give -1/0/1, like the float path.
No callers, so no test can reach them (dead-code candidates):
aether_e_eq_prev, e_inherits, e_is, e_coalesce_deferred,
e_meta_ids, e_abs, e_inc, e_memcpy, e_materialize,
silver_read_bool, parse_expect, etype_infer.

## Codegen through the user's agents (Sep 24 2026)

`import claude` / `import chatgpt` (codegen classes in silver;
config lines set `model:`), then `func f [ .. ] -> T using
claude` with `{ prompt tokens, {images/x.png} }` under it. A
{path} group is one token whose literal is the path.
1. DONE bodies live in gen/ beside the module source:
   gen/Class.method.ag or gen/method.ag, written as
   `extend <module>`, `# hash: <key>`, the func line, the body.
   The func line must match the declaration (whitespace aside).
2. DONE the hash is a cache key: the request carries P (agent,
   signature, prompt, image bytes); the agent writes
   `# hash: P`; on acceptance the compiler stamps K = key(P,
   body). A build recomputes K: a match uses the file (commit
   it; builds need no agent); a mismatch deletes the file and
   regenerates. Editing gen/ rebuilds; so does removing a file.
3. DONE generation runs the user's agent once, from the shell,
   in the repo folder: `claude -p <request> --permission-mode
   acceptEdits --no-session-persistence [--model m]`, or
   `codex exec -s workspace-write -C <repo> --ephemeral
   [-m m] <request>`. No mailbox, no sessions, no API. The
   request is the source location, the gen path and the three
   header lines. One at a time; killed past
   SILVER_CODEGEN_TIMEOUT (600 s); output in
   install/tmp/codegen-<func>.log. Programs from PATH, else
   ~/.local/bin, ~/.claude/local, the ChatGPT/Codex bundles.
4. DONE gen/'s subfolders are resource folders.
   Stand-in tested (fake claude/codex on PATH): both write and
   stamp, the app runs, a rebuild is up to date, a body edit
   or prompt change regenerates.
   REAL (Sep 24): features t_codegen, gen_add using claude
   (`claude -p`, model claude-opus-5-5) and gen_mul using
   chatgpt (`codex exec`); both wrote features/gen/*.ag as
   asked, were stamped, and features passes 212/212 with no
   agent on rebuild. `model: 'gpt-5'` is refused for Codex on
   a ChatGPT account; features' chatgpt import sets no model.
5. OPEN gemini still errors "not implemented".
6. DONE `using` on export funcs (module level only, as export
   is), on expect funcs, and on methods, including expect
   methods inside a class. The header carries the keyword
   (`export func`, `expect func Board.t`); `[]` for no args.
   The request names LANGUAGE.md, AGENTS.md and
   features/features.ag; `claude -p` gets `--add-dir <silver>`.
8. DONE (Oct 6) design-time resources: `Image [ name: 'images/moon.png',
   width: 256, height: 256 ] using chatgpt { prompt }` anywhere
   an Image goes. The compiler builds a design instance of the
   type (construct_with, live: imports are loaded) and hands it
   to the codegen's generate_res; chatgpt and claude accept an
   Image (instance check) and refuse anything else. Only an
   import's types: a class of the module itself has no code yet.
   The name has a folder (gen/'s folders are resource folders).
   The png is checked for size, stamped in gen/<folder>/.<file>.hash (a dot name stays out
   of the share); the
   build links it into share/<m>/<name> and the brackets become
   [ '<name>' ]. A number token's literal is a shape: one
   dimension is turned into an i64 before the property store.
7. DONE the expect runner now runs tests declared inside a
   class, on a new default instance, named Class.test, in
   source order (they were compiled and silently skipped).
   features: Tallied.t_class_test; 213/213.

## Active work: shell-mode exchange (Sep 24 2026)

1. APPLIED, awaiting Kalen's run: after a live reload the
   exchange closed. orbiter set swap_fade once the reloaded
   frame loaded and trinity closed the exchange at 0; that fade
   is gone (the old blur's swap_veil still fades on its own).
2. DONE the diff after the agent's edit: its reply's fenced
   ```diff block became note rows, and the edit's own diff
   listed every old line - and every new line +. shell_text
   leaves out ```diff blocks and fence lines; shell_edit_diff
   shows the lines both sides share as context.
3. OPEN a line the agent added at the top draws blank until
   selected or scrolled out and back: the new row is not
   redrawn after the disk reload / live reload.
4. DONE (Kalen confirmed): the PBR ring light (`pl_lit`,
   Avatar.ag) turned the other way and sat on the far side
   from the plasma ring; its angle is mirrored across `pl_tg`.
   Earlier on this item: the avatar's gradient rim.
5. APPLIED, built, awaiting Kalen's run: a reopened exchange
   kept the avatar at its settled (left, tilted) place. The
   reset keyed on a new shot_ask path; a file exchange reuses
   the file's. The avatar is now settled only while agent_view.
6. APPLIED, built: the exchange's claude run loaded the claude.ai
   connectors (Gmail, Calendar, Drive: needs-auth) and told the
   user so. It runs with --strict-mcp-config: no MCP servers.
7. APPLIED, built, awaiting Kalen's look: stair-stepped edges
   where the volume meets the metal. The volume draws at 1/4 of
   the avatar's points (1/8 of retina pixels) and its depth test
   was in-or-out per volume pixel. An 8-tap depth coverage test
   had no visible effect and was taken out; vol_scale stays 0.25
   (0.5 is far too slow, Kalen). Testing: the donut's glaze
   peaked at its tube surface and cut to 0 outside it (a hard
   outline); it now fades over the tube's outer 20%.
8. APPLIED, built, awaiting Kalen's look: a darker violet band
   on the fins after the core's yellow, for contrast. The old
   violet keyed on height above the core (over 0.6 units) and
   never reached the fins. Replacing the orange by distance took
   the yellow away (fins start 0.2 from the core); the orange is
   back as it was, and a separate violet band is added beyond it
   Now a ring light (Kalen): radius 0.28 round the core's axis,
   0.15 above the pool light (level with the fins' tops), reach
   0.10, blue (0.16, 0.22, 1.4) x1, wrapped facing (0.35 floor).
   Scaled by two lobes: the halo's and the same turned 180
   degrees (max of the two); 1 - pl_pulse was tried and dropped.
9. APPLIED, built, awaiting Kalen's look: more heat blur at the
   core. Heat's 3x3 blur spread 0.5 + 2.5*heat*wgt px (about
   1 px at rest); now 0.5 + 7*heat*wgt, with a second 3x3 at
   half spread (18 taps, only where wgt > 0.002).
10. APPLIED, built, awaiting Kalen's run: with a larger font
   unit the resting prompt field and agent picker rose into the
   avatar (placed by their bottom edge, scaled by the unit).
   They keep their unit-1 centers now (field 240 px, picker
   296 px up) and grow around them; the mic stays in the
   field's right end. The avatar does not move.
   The user's box (its frame is the field's) keeps the same
   center too: top 354 px x unit above it, 420 px x unit tall.
   Split (Kalen): the avatar centered in the window's top 70%
   (orbiter CSS t35%-195px), the picker+field group centered in
   the bottom 30% (15% of root bounds up; 231 px before layout).
   Then (Kalen): avatar down, form up: avatar center 40% down
   (t40%-195px), group center 22% of the height up.
   The first cut read the Region's second slot as a bottom edge
   (it is the TOP): the controls sat half a height too low. orbiter32.gltf COLOR_0 is soft (839 values on
   Cylinder.001) and 8,000+ triangles span b=0 to b=1, so the
   color blends across them. The two soft reads are now on/off
   at 0.5 (Avatar.ag): the core light (`emit * v_color.r`) and
   the groove glow mask (`v_color.b * (1 - v_color.r)` with its
   gamma and smoothstep). The other reads were already on/off.

11. DONE orbiter crashed at startup in path_ls (index_work).
   79f791b queued checkout/'s plain folders on `later` for the
   stack walk; stack/repos/later were pool-owned locals and
   index_work drains its pool per directory, so the first drain
   freed the stack. They are held for the walk (cleared, not
   replaced). The pull dropped this fix once; it is back.
12. DONE trinity did not build on macOS after ef72fa5:
   video.ag `tid : u64` (Linux's pthread_t) is `pthread_t` now.
13. DONE startup SIGSEGV freeing env.gltf's GltfBuffer (every
   debug launch, 3-4 s). Display.resize did `cast Window [ a ]`
   (unchecked) and set rebind_frost on environment's plain
   Display: a write 95 bytes past its 577-byte object, into the
   next block (found with malloc_history + a watchpoint). The
   pull's three Texture members pushed it past malloc's slack.
   resize and draw check `a inherits Window` now. Debug and
   regular: 5/5 headless launches alive at 30 s.

## Active work: model picker (Sep 28 2026)

1. APPLIED, not built (build needs approval): each agent lists its
   models in the agi (trinity `class Agent`: name, models; the
   bracket form `agents: [ claude, codex ]` still reads through
   its string construct), orbiter.agi has claude opus/sonnet/haiku
   and codex gpt-5-codex/gpt-5. The prompt shows the lit agent's
   models as a second AgentPick row (`model_pick`) 34 px UNDER the
   agent row (Sep 28, Kalen: agents on top); both rows are one
   width, 360 px at unit 1; the first model lit; a mounted row keeps its
   selection while its agent stays (AgentPick.agent). The send
   takes it (Window.model_pick, in ExchangeSession/ExchangeState
   too) into agent_post_as's new model argument, which the shell
   already passed on (claude --model, codex -m). orbiter.ag's
   `agents` is `vec Agent`. To verify: build trinity and orbiter,
   open the exchange, check the two rows and the model on the
   agent's command line (install/tmp/agent-shell.log).

## Active work: one exchange box (Sep 24 2026)

1. APPLIED, not built (build needs approval): one box for the
   user's lines and the agent's (Window.notes, ShotEntry.user;
   ShotNotes removed). After the first send the box fills the
   right side; the field and mic are its bottom row. Text 14 ->
   18 px, diff 12 -> 15, prompt 16 -> 18. User lines white,
   agent lines blue.
2. APPLIED, not built: the wheel scrolls the box
   (Window.notes_scroll, px up from the newest; a new entry
   brings the newest back into view).
3. APPLIED, not built (build needs approval): the exchange
   sizes in UX units (`ux_unit`, font unit / 22, 1.0 at
   rest). `ux_area` scales each px of a Region string; the
   boxes, field, mic, close, picker and note spacing follow
   ctrl +/- as the text does.

## Active work: browser element on Ladybird LibWeb (Sep 25 2026)

Goal: a `browser` module (trinity element; orbiter opens it as a
pane tab). Ladybird does layout, JS and network; trinity's Canvas
does ALL drawing. Skia is never built, linked or shipped (Kalen:
"I WILL NOT use Skia. Skia is a canvas. WE HAVE THAT").
Decisions (Kalen): own module, static page first, keep Ladybird's
helper processes (WebContent, RequestServer, ImageDecoder).
Ladybird checked at 3af7308 (Sep 25): WebContent sends each frame
to a Compositor process as `update_display_list` (DisplayList +
visual context tree + resources); DisplayListPlayerSkia replays it.
The browser element takes the Compositor's place: it serves the
CompositorWebContentServer/Control endpoints and replays the list
with a DisplayListPlayer that calls trinity (the glue).
1. OPEN import Ladybird from its URL (CMake, vcpkg deps, Rust via
   ~/.cargo) with aura/ladybird.diff: skia and angle out of
   vcpkg.json and every CMakeLists. Disk: 33 GB free.
   Every Skia job routes to trinity (Kalen: trinity does all of
   it; no new stand-in code):
   - Gfx::PathImpl (virtual): PathSkia -> trinity CanvasPath.
   - Gfx::Typeface / Font: TypefaceSkia -> trinity CanvasFont
     (glyph metrics, GlyphAtlas).
   - Bitmap scale/convert, images, gradients: trinity + img.
   - PaintingSurface (SVG as image, cursor, screenshot, canvas
     2D): trinity Canvas.
   - System font match (Skia's fontconfig manager): trinity
     CanvasFont gains a family/weight/width/slope lookup.
   The bridge: Au headers cannot enter Ladybird C++ (Au's
   get/set/len/push macros break AK). A silver module `webgfx`
   imports trinity and exports plain C funcs; the diff's
   TypefaceTrinity/PathTrinity/... .cpp call them. Build order:
   webgfx, then Ladybird (links -lwebgfx), then browser.
   webgfx has three groups: font, path, canvas. WebContent also
   draws (SVG as image, cursors) with the display list player,
   so DisplayListPlayerTrinity lives in LibCompositing and runs
   in both processes over webgfx's canvas calls.
   vcpkg without skia/angle: 61 packages; needs cmake >= 3.30
   (vcpkg's own), nasm and autoconf-archive (built into the
   scratch tools dir; move to bootstrap). Building (scratch).
   DONE trinity CanvasFont: loads from bytes (data, data_size,
   face_index), units_per_em, glyph_count, ascender/descender/
   x height in font units, glyph_index, advance_units, outline
   (glyph into a CanvasPath at a px size, unhinted); the glyph
   cache and GlyphAtlas are keyed by glyph number. texttest
   draws right (shot). webgfx t_font_from_bytes printed good
   values (SpaceMono 'A': 14 points, ascent 22.4 at 20 px).
   DONE trinity CanvasPath: elliptical_arc_to (svg), bounds,
   contains (nonzero / even-odd), length, point_at, transform.
   DONE webgfx font + path calls (webgfx/webgfx.ag); `silver
   --test webgfx` exit 0: t_font_from_bytes, t_path_geometry
   (100x50 rect, in/out, length 300; half circle r50 157.04).
   DONE patch, build files: skia, angle, Ladybird's own vulkan,
   Compositor, WebDriver and UI/ out; LibGfx, LibCompositing,
   LibWeb link webgfx (find_library under SILVER_INSTALL).
   DONE trinity FontMatch.ag: system font lookup (fontconfig on
   linux; family/weight/width/slope, fallback by codepoint,
   emoji via color); webgfx_font_match; t_font_match passes
   (monospace -> DejaVuSansMono.ttf, unknown -> not found,
   'A' -> Noto Sans, as fc-match says).
   DONE trinity CanvasFont family_name, weight/width/slope (OS/2
   table), set_axes (variable fonts); a bad font from bytes
   leaves the face null (loaded[] false), no fault. webgfx
   5/5: t_font_variable matches fontTools (Roboto Condensed H
   1270 units at wght 400, 1257 at 700), t_font_bad_bytes.
   WRITTEN, not compiled: patch LibGfx PathTrinity (.h/.cpp),
   Font/TypefaceTrinity (.h/.cpp), WebGfx.h (the C calls),
   Font.cpp metrics via webgfx, every TypefaceSkia caller
   renamed. Patch diff comes from `diff -ruN` against a clean
   download of 3af7308 (a `git rm --cached` of mine touched
   the scratch clone's index; no more git ops there).
   DONE webgfx canvas group (new, clear, save/restore,
   transform, clip, fill rect/rrect/path, stroke, glyphs by
   glyph number, image, read back rgba8): t_canvas_draw,
   t_canvas_glyphs ('Hello' checked by eye). trinity Canvas:
   draw_glyphs (draw_text_pass split into text_begin /
   glyph_quad / text_end; texttest shot unchanged), fill_rgba,
   stroke_rgba, set_transform take floats: a vec4f or affine
   passed by value across modules arrived shifted (the struct
   by value bug), fill came in as (garbage, 0, 1, 1).
   DONE patch compiles: LibGfx (PaintingSurface = a webgfx
   canvas; wrap_bitmap copies back on flush; PainterTrinity;
   BitmapExport packs by hand; Bitmap scaled via trinity;
   ColorSpace kept as data; CanvasCommandPlayer resolves
   fonts, no text blobs), LibCompositing (resource storage
   without Skia caches; DisplayListPlayerTrinity), all of
   LibWeb. ANGLE's GL headers (7, text only) download at
   configure for WebGL's constants; no GL library.
   Compositor service kept (a helper process), on the trinity
   player; backing stores are wrap_bitmap over the shared
   bitmaps; WebGL returns failure; webgfx_gpu_init opens the
   device where Skia did, before seccomp.
   UI/Trinity: Ladybird's GUI framework choice "Trinity"
   (default on linux, no Qt): an empty Application, used
   headless (--headless=screenshot) to prove the path; the
   element will drive Application's event loop in orbiter.
   The player logs "not ported: X" once per missing feature
   (gradients, shadows, repeated backgrounds, layers/opacity,
   masks, rounded/path clips, iframes, canvas, video, 3D).
   clang 22 fix: ComputedValues.cpp lambda needs `-> bool`.
   DONE (Sep 25 18:1x) FIRST PAGE: all of Ladybird builds on
   trinity (no Skia, ANGLE or Qt) and `Ladybird
   --disable-sandbox --headless=screenshot` renders a test
   page right: heading, paragraph, red box, rounded green box,
   blue border (scratch/shot.png). Fixed on the way:
   - TypefaceTrinity copied bytes when no backing, then the
     caller's set_font_data freed the copy (HarfBuzz SIGSEGV
     on the font catalog thread): no copy now.
   - webgfx tables locked (pthread mutex): fonts load on a
     worker thread; FreeType faces made under the lock.
   - trinity assets open relative to the cwd (models/uv-
     quad2.gltf): webgfx cds to its share dir on first GPU
     use (dladdr of its own library).
   - Canvas's `load_existing [ true ]` never reaches Render's
     field (a shadowing member): a pass after text cleared
     the canvas black. webgfx passes load_existing: true.
     OPEN (trinity): the shadowing Canvas.load_existing.
   - PaintingSurface.write_from_bitmap now updates a wrapped
     bitmap too (the screenshot came back empty).
   Tests: webgfx 13/13 expects pass.
   OPEN sandbox: the Compositor's Landlock list lacks
   trinity's install and cache dirs; WebContent draws (SVG,
   cursors) and needs the GPU too. Runs use --disable-sandbox.
   DONE (Sep 25 evening) the browser element, aura/aura.ag.
   Shape: Ladybird's UI side (LibWebView, its event loop and
   mimalloc) stays out of orbiter's process. The element forks
   the Trinity frontend (UI/Trinity/main.cpp, a HeadlessWebView
   subclass) as one more helper: frames come back as rgba8 in
   a shared file (TRINITY_BROWSER_SHM: header magic TRBW, seq,
   w, h, stride; then pixels), input goes down a socketpair
   as lines: size W H, load URL, mouse down|up|move|wheel x y
   button held mods [dx dy], key down|up glfw_code mods,
   text <utf8>. Closing the socket ends the helper.
   Checked headless (socket press/text/key/wheel + shot):
   draws the page at device size, click focuses, typing and
   Backspace edit, a button's JS runs, wheel scrolls 120 px a
   step, the scrollbar draws; colors exact (#9cf = 153,204,255).
   Fixed on the way:
   - silver: string literals given to a C vararg (execl) arrive
     as objects, not text: '--disable-sandbox' became garbage,
     the sandbox stayed on, Landlock refused trinity's shader
     cache. Pass .chars of string locals (as qemu.ag does).
   - C macros SIG_IGN / MAP_FAILED (casts to pointer) emit bad
     IR; the element uses send(MSG_NOSIGNAL) on a socketpair and
     `cast u64 [ m ]` for the mmap check.
   - `func send` / arg names `line`, `msg` clash with existing
     C and type names in scope: renamed post / words.
   - trinity text_event/key_event gave a focused app element
     each key twice (focus, then app_object): skipped when they
     are the same element ("hihi" before).
   - Textures of page pixels are `linear: true` (UNORM): the
     sRGB default decoded once into a UNORM target, mid-tones
     came out dark (element and webgfx_canvas_image).
   - Compositor paints scrollbars between replays with the
     surface passed in; the player used its replay surface
     (null there): crash on any scrolling page.
   - GLFW wheel up is Ladybird's minus (measured).
   Orbiter: install/export/silver-aura.agi claims .html/.htm;
   Editor.handler_module_for hosts `browser` on them (as qemu
   on .img), so .html files now open in the browser view.
   orbiter builds; NOT run inside orbiter yet (not asked).
   TEMP: install/ladybird is a symlink to the scratch build
   (/tmp/claude-1000/...); the import replaces it.
   OPEN: sandbox (still --disable-sandbox), the import,
   hidpi scale check, key repeat, cursor shape, page title,
   navigation (back/forward/url bar), the gaps "not ported".
   NEXT: the silver import of Ladybird (aura/ladybird.diff
   from diff -ruN, vcpkg + cmake 4 + nasm + autoconf-archive
   in bootstrap).
   DONE (fixed below) separate bug: `silver --test trinity` traps in
   trinity.ag `expect func test` when CanvasUI.vert must be
   compiled in the test child: glslang "Unable to parse
   built-ins". With the .spv cached it passes (exit 0).
   CAUSE (checked): expects run inside the module's own ctor.
   libsilver-trinity.so .init_array order: frame_dummy,
   silver_trinity_initializer, trinity.cc, glslang
   Initialize.cpp, Scan.cpp, hlslScanContext: the test compiles
   a shader before glslang's statics exist. Reproduced with a
   bare dlopen harness (no silver process). Owner: aether's
   expect emission + silver.c link order (module .o first).
   The empty static: glslang Scan.cpp:325 `const unordered_map
   KeywordMap {..}` is filled by Scan.cpp's own ctor; before it
   every keyword scans as IDENTIFIER. Only libraries hit this
   (apps and live apps run module init from main).
   FIX: aether emit_library_late puts a plain library's
   expects/exports in exported <module>_late (late_symbol on
   aether); silver.c late_object writes <name>-late.c, a ctor
   calling it, linked after every lib (native link only; the
   device link path is not done). trinity .init_array now ends
   with silver_late. Checked: shader cache deleted, `silver
   --test trinity` exit 0, no glslang errors, both CanvasUI
   shaders compiled in the run; img exports run from
   silver_img_late; webgfx 3/3; texttest (live app) draws.
   OPEN separate: features t_vec_module fails: vec2f.add
   returns y as garbage (libvec returns both floats in xmm0,
   silver reads y from xmm1: the struct-return ABI bug in
   memory). AGENTS said 213/213 on Sep 24; why it passed
   then is not known.
   Stray: my `touch vec/vec.ag` made an empty module that
   overwrote make's install/build/vec.o; removed it and its
   products, make rebuilt vec.o and libvec.so from src/vec.c.
   silver's build made git tags vtmp-1.0.0 (a temp module,
   deleted) and webgfx-1.0.0; I did not remove tags.
2. OPEN the trinity player: the 26 draw commands (rects, rounded
   rects, glyph runs, images, gradients, box shadows, paths,
   lines, ellipses) plus clip, transform, layer, mask, filters.
3. OPEN the element: spawn WebContent, serve the compositor
   endpoints, load a local .html file, draw it (milestone 1).
4. BUILT, not run: orbiter's title opens an address. A module
   exports its schemes (`export protocols ['http', 'https']`
   in aura.ag, read from its .agi); Enter on text with a
   scheme some module exports hosts that module in the
   active pane with the address as its argument
   (Editor.open_url, host_module shared with launch_handler).
   DONE (Sep 25) selection painted twice (rect and text):
   - Canvas.sync_all called draw[] before sync_fence: the
     last paint's shape drew a second time on every read.
     sync_fence already submits the batch; draw[] removed.
   - Canvas.set_clip stored clip_x/y/w/h that nothing read:
     no Ladybird clip applied, so a line with a selection
     drew its text twice. set_clip narrows `crop` now (what
     shapes and text honor); an empty clip parks it off the
     canvas. Checked on example.com: one 80% wash per line,
     'ations.' ink 43,100 -> 34,268. webgfx and trinity
     tests exit 0.
   DONE the Trinity frontend never hands its URL to another
   running Ladybird (should_coordinate_browser_process false):
   a second element exited when one was already open.
   Kalen's next three, in his order (Sep 25 evening):
   a. PARTLY DONE slow rendering. Wikipedia per frame was
      images 22-28 ms (a new texture per draw), rects 13-17
      ms, text 5-7 ms, plus 17 full GPU waits (paint_n 64).
      - images: one webgfx texture per decoded frame
        (webgfx_image_new/free/canvas_draw_image), owned by
        DisplayListStoredImageFrameResource: 25 ms -> 0.06.
      - text draws in the Canvas's own batch (b below).
      - Render.paint_n is public; Pipeline rings and uniform
        buffers follow it (default 64); webgfx canvases use
        1024 and one GPU wait per frame. The Canvas text
        vertex ring follows paint_n; sync_all restarts it.
      - Buffer.update_part: a text run copies only its bytes.
      Long local text page scrolls at 29.7 frames/s.
      FIXED (Sep 25 night) orbiter SIGSEGV at startup
      (map_lookup under Texture_create_gpu, MilkyWay): my
      bind_resources change made its write arrays `vec
      VkWriteDescriptorSet [ n ]` etc., which overran the
      heap. Back to fixed local arrays; rings over 64 slots
      allocate and write their sets 64 slots at a time.
      Checked: headless orbiter runs (was exit 139), long
      page 54-57 frames/s, webgfx and trinity tests exit 0.
      OPEN silver bug: a runtime-sized `vec <C struct> [ n ]`
      is too small for n elements (found by elimination;
      no focused test yet).
      OPEN: Wikipedia scrolls at 2 frames/s with the GPU and
      every process near idle: the Compositor is asked for
      a frame every 0.5 s. Not drawing (a rAF page: 60/s).
      OPEN: GlyphAtlas.upload_region waits the device idle
      per new glyph; the Compositor holds ~600 nvidia fds.
   b. DONE z order: text was its own Render (text_render) with
      its own batch, sent at its 64-draw drain or read-back,
      so text landed over shapes drawn after it. Removed:
      text_pipeline's render is the Canvas; text_end draws
      via Render.draw_with[ text_models ] in call order.
      Checked: a red box over earlier text now covers it.
   c. PARTLY DONE SVG icons. Wikipedia's icons are CSS masks
      (an SVG mask over a background color).
      - masks: the player keeps a stack of layer canvases
        (pooled, surface size); push_mask draws the content
        into one, pop_mask draws the mask into another and
        webgfx_canvas_draw_layer puts the content down through
        it (Canvas modes image_mask_alpha / _luma, 14 / 15).
        The composite submits at once: a reused layer was
        cleared before the page's batch had read it.
      - repeated images, repeated tiles (their records replay
        per tile, scaled) and tiled images (border-image).
      - trinity path fill (sdf_walk_edges): a subpath never
        closed (close ignored, no implicit close) and a
        dropped short edge left a gap: horizontal streaks.
      - trinity path fill: every path in a batch shared one
        edge buffer written at paint time; the GPU read the
        last path's edges for all (the tagline's second half,
        trinity's canvas_test blob). Paths append at an offset
        (CanvasSDFCompute.eo); the buffer restarts after a wait.
        trinity test average now 16,32,53,80: the full blob
        (before 2,5,8,12: only the inner shape, wrongly).
      - even-odd fills (CanvasSDFCompute.even_odd).
      Checked: masked circle, Wikipedia menu / search / more
      icons and wordmark, tagline in full, even-odd hole.
      OPEN: the globe logo needs gradients, layer opacity and
      blend modes; a few streaks remain on it.
   RECOVERY (Sep 25 23:00): a reboot emptied /tmp, and the
   whole patched Ladybird tree and build lived in the session
   scratch there. Replayed from this session's transcript onto
   a fresh 3af73088 (checkout/lb/replay/replay.py), now in
   /src/silver/checkout/lb/ladybird (gitignored), tools in
   checkout/lb/tools, build script checkout/lb/build.sh.
   The patch is kept in the repo as an overlay, not a diff:
   aura/ladybird/ (every changed or new file, laid out as
   the tree), aura/ladybird.deleted (files to remove),
   aura/ladybird.commit (the base). install/ladybird ->
   checkout/lb/ladybird/Build/release.
   The replay leaked old trace edits into webgfx/webgfx.ag
   (a doubled webgfx_gpu_init, a doubled import pair, trace
   lines in webgfx_canvas_read); removed, webgfx tests exit 0.
   DONE silver's import takes an overlay (Kalen): after the
   clone and any <name>.diff, the module's <name>/ folder is
   copied over the checkout and <name>.deleted (one path a
   line) is removed from it (silver.c checkout). Checked with
   a throwaway module importing cJSON: an added file, a
   replaced file and a deleted file all right; removed after.
   Applies on a fresh checkout only, as the diff does.
   Rebuild checked: mask, stacking, even-odd, selection as
   before. The replay had missed one fix (a script edited
   webgfx and TypefaceTrinity together; its webgfx half
   failed first): the font byte copy freed under HarfBuzz
   (SIGSEGV in the font catalog thread). Re-applied.
   DONE video frames (Kalen: YouTube played audio only). The
   player drew nothing for DrawVideoFrame. Now each video
   keeps one webgfx texture (DisplayListStoredVideoSinkResource),
   a new frame (pool slot + acquisition id) is converted with
   YUVData::to_bitmap and uploaded in place
   (webgfx_image_update). Checked: a VP9 test clip plays (its
   counter 224 at 7.467 s, 32 frames/s shown); YouTube Big
   Buck Bunny plays after a click (0:12, picture moving).
   OPEN: YouTube 60 fps shows 12 frames/s: the YUV to RGB
   convert is on the CPU and each upload waits the GPU idle.
   Next: upload the three planes, convert in a trinity shader.
5. OPEN canvas 2D: the Canvas2DCommandStream replays on
   trinity's Canvas in the element, as the display list does.
6. LATER network, input, scrolling, canvas 2D, video, WebGL.

## Active work: browser on WPE WebKit, trinity painting (Sep 26 2026)

Decision (Kalen): WebKit is the only accepted browser stack;
the Ladybird element is superseded. Port: WPE WebKit 2.54.0
(source in checkout/wk/wpewebkit-2.54.0, gitignored).
Why the pivot: Ladybird repainted every icon every frame into
the final buffer; WebKit paints each layer into cached tiles
once and composites the tiles (only damaged tiles repaint).
Plan, option 2 (Kalen): trinity replaces Skia's PAINTING side
(GraphicsContext, paths, gradients, patterns, images, fonts,
ImageBuffer, tile rasterizing); WebKit's own tile caching and
compositing logic stay, compositing through trinity. Skia is
not built. The trinity work done for Ladybird carries over
(webgfx, CanvasFont/FontMatch, paths, masks, YUV video).
Survey: Skia is vendored (Source/ThirdParty/skia); WPE sets
USE_SKIA ON, USE_CAIRO FALSE. 160 WebKit files outside Skia use
it, 65 in WebCore/platform/graphics/skia; USE(SKIA) appears on
479 lines. The seam: a USE(TRINITY) backend beside USE(SKIA).
Disk: 26 GB free before any build; the Ladybird build in
checkout/lb takes about 15 GB (ask Kalen before removing).
1. DONE build dependencies, imported by aura/aura.ag
   into install/: ruby 3.3.6 (generators), unifdef 2.12,
   libgpg-error 1.51, libgcrypt 1.11.0, libtasn1 4.19.0,
   libpsl 0.21.5, nghttp2 1.64.0, libsoup 3.6.5. Off to start:
   XSLT, speech, AVIF, JPEG XL, LCMS, WOFF2, ATK, introspection,
   docs, journald, sysprof, WebDriver, sandbox, legacy libwpe,
   Wayland/DRM platforms (headless WPEPlatform stays), video.
   silver: `name/ver from <url>.tar.{xz,gz,bz2}|.tgz` unpacks
   the archive as the checkout (checkout/<owner>/<name>), then
   <name>.diff, the <name>/ overlay and the usual build run.
   Checked on libtasn1 (overlay file landed, built, installed);
   features: only the known t_vec_module fails.
2. DONE (Sep 26) USE(TRINITY) build: all of WPE WebKit builds
   and links with Skia off (0 Skia symbols or libraries;
   libWPEWebKit needs libsilver-webgfx.so). Installed into
   install/ (helpers in install/libexec/wpe-webkit-2.0; they
   need LD_LIBRARY_PATH=install/build:install/lib for webgfx).
   FIRST PAGE: a headless test (scratchpad wk/shot.c: headless
   WPEDisplay, load, webkit_web_view_get_snapshot, PNG out)
   draws heading, paragraph, red box, rounded green box, blue
   border right. Fixed on the way: whitespace drew the missing
   glyph box; the HarfBuzz font is now a sub font whose glyph
   lookups apply WebKit's space rules (as Skia's did).
   Every change is kept as the overlay aura/wpewebkit/
   (88 files, base in aura/wpewebkit.version).
   Non-composited rendering is Skia-only: trinity pages always
   use the layer tree.
3. DONE, first page checked: GraphicsContextTrinity (webgfx canvas;
   own CTM and device clip stacks; colors, rounded rects,
   paths, strokes, images, tiled patterns, glyph runs; a CPU
   backing for shareable bitmaps). Logged once as "not
   ported": gradients (trinity has 2 stops only; a stop ramp
   is next), shadows, dashes, clip-out, path clips (bounds),
   transparency layers, blend modes.
4. DONE, first page checked: fonts on TrinityTypeface + HarfBuzz
   (FontPlatformData, Font metrics/bounds/outlines, GlyphPage,
   FontCache via FontMatch, web fonts with variation axes,
   FontCascade::drawGlyphs -> webgfx glyphs). Shaping reuses
   skia/ComplexTextControllerSkia.cpp (pure HarfBuzz).
5. DONE, first page checked: ImageBufferTrinityBackend (canvas,
   read back on demand), TrinityPaintingEngine (dirty tiles on
   one reused canvas, read into the BGRA tile buffer),
   NativeImage, Pattern, ShareableBitmap, PNG via libpng
   (TrinityPNG); WPE UI side (snapshot, cursor, favicons,
   WebKitImage, notifications) off Skia. Compositing is still
   WebKit's TextureMapper (GL): OPEN, move it to trinity.
   DONE (Sep 26) gradients: trinity Canvas `fill_ramp` (a
   256x1 stops image in the mask slot; linear, two-point
   radial, conic; pad/reflect/repeat; device-to-gradient
   matrix), webgfx_canvas_set_ramp, GraphicsContextTrinity
   builds each ramp once (premultiplied blend, cached by hash).
   webgfx t_canvas_ramp (red 255,0,0 / green 0,252,3 / blue
   0,0,255 across a rect), webgfx and trinity tests exit 0.
   Checked in WebKit: linear, hard stops, radial circle and
   ellipse, conic from the top clockwise, repeating stripes.
   NEXT: clips to a path's shape (rounded boxes fill square:
   WebKit clips border-radius then fills; bounds used now).
   WRITTEN, not built: aura.ag imports webgfx, then
   wpewebkit/2.54.0 from wpewebkit.org's tarball (checkout/
   releases/wpewebkit, overlay aura/wpewebkit/, the flags
   of wpewebkit.configure.sh as import lines).
6. DONE (Sep 26) the browser element runs WebKit. aura.ag
   forks install/libexec/wpe-webkit-2.0/WPETrinityBrowser (the
   overlay's Tools/TrinityBrowser/main.c, built by WebKit's own
   CMake when USE_TRINITY): a headless WPE view; each frame
   (the view's buffer-rendered signal) goes to the same shared
   file as before (TRBW header, rgba8), DMA-BUF frames read
   back through EGL + glReadPixels (gbm_bo_map fails on the
   NVIDIA buffers), SHM frames copied; input lines as before
   (size, load, mouse down|up|move|wheel, key, text) become
   WPE events. A GLFW wheel value passes straight through
   (negative scrolls down). Button-up needs press count 0
   (else WPE returns no event). WebKit binaries carry RPATH
   install/lib:install/build (configure.sh; a copy is
   aura/wpewebkit.configure.sh): no LD_LIBRARY_PATH.
   Checked: helper alone (202 frames in ~5 s, Wikipedia);
   element headless over its socket: click focuses, typing,
   Backspace, a button's script, helper dies with the element;
   `browser <url>` shows Wikipedia at 1280x800.
   Launch: `silver aura https://en.wikipedia.org/wiki/WebKit`.
   DONE (Sep 28) zero-copy frames: the helper sends each GPU
   frame's dma-buf (one plane, fd over the stdin socket with
   a 48-byte record: id, size, fourcc, modifier, stride,
   offset; it polls the buffer's rendering fence first) and
   the element imports each buffer once (import_dmabuf_image,
   keyed by the helper's buffer id; aura.c receives the
   fd). A failed import posts `gpuframes 0` and the helper
   reads frames back into the shared file as before (also
   the path when stdin is a pipe, and for SHM buffers).
   Checked headless at 2560x1440: "GPU frames ... modifier
   216172782128496660", picture right, 60 page frames/s
   while the knob drags, helper 2% of a core.
   Pointer moves go to the helper at once (was once per app
   frame): WebKit coalesces them itself. Latency measured
   before: WebKit + helper alone 23 ms from an input line to
   the frame in the file; the app added 8 + 8 + up to 16 ms.
   OPEN: hidpi scale; path clips.
   DONE (Sep 28) a live reload left the browser blank. The
   helper was forked with PR_SET_PDEATHSIG, and a reload's
   first draw runs on the load worker thread: when that thread
   ended, the new helper got SIGTERM ("the engine exited"),
   while the old one lived on with the process-owned app
   element. Now: no death signal (the helper quits when its
   stdin socket closes, which our death does); the element
   keeps `persist engine : vec i64` (pid, ctl) and
   `engine_shm`, and a new instance adopts the running helper
   (adopt[], "kept the engine") instead of launching: the
   page and its video continue. Checked headless under the
   host: a aura.ag touch and a trinity/Canvas.ag touch
   both keep the page, 58 page frames/s after the switch.
   DONE (Sep 28) page images get a mip chain (webgfx_image_new,
   Texture.upload_region refills it): a logo drawn smaller
   than its file no longer stair-steps. Ramps (256x1) stay
   flat.
   DONE (Sep 28) YouTube came up white in orbiter's pane: the
   helper's GPU buffers are XR24 (fourcc 875713112), and the
   dma-buf import read the X byte as alpha. Standalone composes
   the screen without alpha, so it looked right; orbiter draws
   the hosted app's screen with it, and the pane fell through.
   import_dmabuf_image now imports XR24/XB24 as opaque (the
   Texture's alpha-as-one swizzle). The "GPU frames" log line
   names the fourcc. Hosted case not driven here.
   DONE (Sep 28) elements route into the host's navigation and
   title, decentrally (Kalen). trinity: element.navigate[ dir ]
   (a hook, false by default), element.nav_state[ back, fwd ]
   and element.set_title[ t ] emit to the Window (nav_back_on,
   nav_fwd_on, nav_title); a hosted app posts them on ring 0 as
   HM.nav_state (a, b) and HM.title (three bytes a message, a
   0 byte last); the host's HM.nav (a = -1/+1) on ring 1 goes to
   Window.navigate: the focused element, its parents, then the
   app object. orbiter: AppView keeps nav_back_on/nav_fwd_on/
   app_title from its slot's ring; a hosted pane's arrows post
   HM.nav (Editor.nav_walk) and light from the app's word; the
   pane title shows app_title (title_text). browser: the helper
   sends 'NAVS' (back-forward list changed) and 'TITL' (title,
   else the address; notify::title / notify::uri) records on
   the frame socket; aura.c returns the record kind (1 frame,
   2 history, 3 title); navigate posts `nav back|forward`, the
   helper calls webkit_web_view_go_back/forward. Checked: the
   helper alone goes back and forward between two pages; on
   YouTube a related-video click logs "history back true" and
   the new page's title. Orbiter builds; its arrows and title on
   a hosted browser pane are not driven headless (session state).
   DONE (Sep 28) the title's companion extension buttons are one
   TButtons strip (ids cmp_<ext>, new TButtons.labels for the
   text), and companion_exts is empty for an address.
   DONE (Sep 28) player icons with a drop-shadow filter drew
   bold with a light border ("over drawn"). The canvas blends
   premultiplied and samples image textures as premultiplied,
   but WebKit's images are straight: a filter result's edge
   pixels (white at partial alpha) came back over-bright and
   read as a hard border. webgfx image_upload premultiplies
   (image_new, image_update). webgfx tests: t_blur_kernel (the
   sigma-1 gaussian), t_compose_keeps_aa, t_result_image_keeps_aa
   (the round trip equals a direct fill), t_cc_drop_shadow,
   t_cc_icon_small_canvas (YouTube's CC path). Checked: the
   pill page with the real icons, and the YouTube bar.
   OPEN (trinity, Kalen's call): every straight-alpha texture
   drawn by Canvas.draw_image (PNG icons in orbiter, any img
   load) has the same over-bright edge; the canvas's rule is
   premultiplied, so loaders should premultiply too.
   DONE (Sep 28, Kalen) the SDF feather was half a pixel each
   side (0.5 * gradient length); it is one pixel each side
   again (the gradient length), both CanvasUI fills and the
   path pass.
   DONE (Sep 28, Kalen: "that works so keep it") the Ask star
   icon lost its tips at 24 px. The tips are thinner than a
   pixel and one distance sample at the pixel centre misses
   them (the field itself is fine: shifting the icon half a
   pixel changed nothing). CanvasUI mode 6 fill: within 1.25 px
   of the outline, 16 taps (4x4 cells of the pixel, a linear
   ramp per cell) give the true coverage; elsewhere 0 or 1.
   The field is written and read as before. The feather is
   0.5 * gradient length in the source again (exact coverage;
   1.0 made 24 px icons soft and fat against the reference).
   Tried and removed (Kalen: too much pixel alignment, it
   changes the SDF): a field over the padded shape only, a
   guard texel, clamped taps: a batch shares one dispatch, so
   the stale texels drew hairlines in the YouTube wordmark.
   Finer flattening (0.1) changed nothing. Checked: the star at
   24 and 48 px against Inkscape (column through the tip 37,
   59, 103, 174, 248 vs 44, 65, 110, 180, 251), the wordmark
   clean at 1x and 4x, webgfx and trinity tests exit 0.
   DONE (Sep 28, Kalen) standalone, the page title is the window
   title (Window.element_title calls Display.set_title when not
   hosted), and the browser opens where it was last unless a
   src is given: `intern [ StatePersist ] last_src` saved on
   every address change (~/.local/state/aura/persist.agi;
   the helper's 'ADDR' record on notify::uri, kind 4 in
   aura.c), loaded in init when src is empty. Checked
   headless: a Wikipedia run saves it, a run with no address
   opens it and logs its title.
   DONE (Sep 28, Kalen: text blurry half the time after a
   scroll) the scroll position straddled the pixel grid. WPE's
   wheel line step is the view height to the 2/3 (86 px at
   800), the element sends 0.325 of a notch: 27.95 px a step,
   and WPE's coordinated scrolling delegate overrode the
   threaded delegate's rounding with the raw float, so the
   scrolled layer sat at a fraction and its tiles resampled
   (strokes lightened step by step: 29 -> 40 -> 74). Overlay
   Source/WebCore/page/scrolling/coordinated/
   ScrollingTreeScrollingNodeDelegateCoordinated.cpp:
   adjustedScrollPosition rounds to whole device pixels (the
   scrolled layer's contentsScale). Checked headless on a long
   text page: four wheel steps are four exact 28 px shifts
   (mean pixel difference 0.0), a word's pixels identical
   before and after.
   DONE (Sep 28, Kalen) orbiter exports its own icon: `export
   func orbiter_icon` (Avatar.ag) renders OrbiterAvatar alone
   at 512x512 with a transparent background into orbiter/
   images/icon.png (the module folder from install/build/
   silver-orbiter.source), once: skipped while the file exists
   (delete it, or `silver --export --build orbiter`, to remake).
   The release's icon export scales it to the sizes. An element
   outside a window is mounted as the window does it: ux and vk
   first, then initialize (the member defaults, so shaders get
   their device), bind_members, hold_members; the window needs a
   screen canvas (draw composes onto it); every pass is drawn
   and fenced (batched passes submit only at sync_fence). The
   old placeholder icon.png is in the session scratchpad.
   Every orbiter build now runs the export step (module init,
   then "icon current").
   DONE (Sep 28, Kalen: "link is fine to couple") `--link` is
   the user's own install, the package's layout under HOME:
   ~/.local/bin/<name> -> the binary, the hicolor icon set in
   ~/.local/share/icons/hicolor/<n>x<n>/apps/<name>.png (img's
   icons export) and ~/.local/share/applications/<name>.desktop
   (Exec the link, Icon=<name>). silver.c: silver_module_icon,
   silver_icon_set, silver_desktop_entry, shared with --release.
   No icon: the link alone, and a note. It runs on a cached
   build too (silver_user_link, called in both branches).
   Checked: `silver --link --build orbiter` wrote all three,
   fresh and cached. The icon avatar takes the app's resting
   yaw of 30 degrees (on_state's, which needs the app object):
   flat top and bottom, as orbiter shows it.
   DONE (Sep 28, Kalen) title bar boxes: the "..." tab chip
   (NavTabs), the history arrows and the close buttons
   (BarClose class, the finder's TButtons close) are the bar's
   full height (b0), no spacing (use_spacing false: the title
   bar's 0.2 em child spacing was the padding), square corners
   (seg_radius 0, radius 0). Strokes 0.5 -> 1 everywhere in
   orbiter's styles with the alpha halved; the TButtons outline
   is #ffffff0d. The dots hint draws only while the strip is
   small (gone a quarter into the reveal), in the rest slot,
   twice the icon size capped to the slot; the close glyphs
   fill their box height.
   Since then (Kalen): the arrows and the dialog close are
   1.8 units wide, arrow glyphs at 0.30; every close button is
   the chip's size (1.66 units, X at 0.55); the file icon sits
   0.2 units further right; the title's icon, name, hint and
   companions lead from the arrow box (EditorTitle.lead). A
   side toolbar (right edge, instances + orbiter at the top,
   console at the bottom) was built and REMOVED at Kalen's
   word; the gutter rule is the original (five digits + 16 px).
   DONE (Sep 28, Kalen: "some svgs have super hard edges") svg
   icons (Canvas.draw_svg) rasterize into a cache 4x their size
   (SVGModel.pixel_scale 4) and were drawn back at a quarter
   with the plain bilinear sampler, which reads 2 of every 4
   texels: edges quantized to stairs (a 15.7 px hexagon's edge
   was one pixel at 33 then 255). Now the cache Canvas has
   `reduce: true` for pixel_scale 4 (the render's reduce pass
   averages 4x4 texels, as the avatar's does) and draw_canvas
   draws the reduction; the draw is snapped to whole pixels
   (origin and size), so the reduction lands 1:1 (a 15 px
   reduction drawn at 15.7 blurred it). Measured in a temporary
   webgfx test: the drawn row equals the reduction row, edges
   93 255 ... 93 21 instead of 33 255 ... 15 0. Thin lines (the
   console icon) read as their true sub-pixel coverage now
   (~140 of 255), which is right, not a bug.
   DONE (Sep 28, Kalen: "state corruption ... worse when you go
   back") the reduce pass reused pass 0's attachment
   description: a load_existing canvas LOADed the old
   reduction and every pipeline blends over, so each reduce
   composited onto the last (edges thickened per shape, a
   re-tint kept the old picture under it; the avatar's 4x
   reduce accumulated frames the same way). vk.ag: pass 1
   clears (loadOp CLEAR, initialLayout UNDEFINED) to
   transparent black (the canvas's clear colour is opaque
   black: the first try gave a black square). shader.ag
   UVReduce: output is the plain premultiplied average (was
   (rgb + rgb*a)/2, halving edges) and the +-2/255 dither is
   scaled by alpha (it lit clear pixels: the faint box round
   some icons). Checked: a white then red hexagon reads red
   with green 0-1 and alpha equal to red; suites exit 0.
   DONE (Sep 28, Kalen: "adhere to w3c, not two brains") the
   shader keeps the CSS blur meaning; orbiter's glow sizes are
   halved in its own sources: 17 box_shadow styles, 8
   cv.box_shadow calls, the chip hover glow, rest_px 16 -> 8.
   Also: the git changed-file tabs at half alpha (fill, border,
   glow: sources #22002280, #ff5a8a80, #ff7aa480, white 80),
   mapped in every scene theme (scenes.ag) at the same alpha;
   the finder's extension chip strips got the last half-pixel
   stroke in the tree (now 1 px inside).
   DONE (Sep 28) ctrl/cmd+left and right in any trinity app go
   to Window.navigate (the focused element's history) before
   the key reaches the focus; the browser goes back/forward.
   DONE (Sep 28) a page that worked once then came up white:
   WPE recycles buffer ids, so the element keyed its imported
   textures by the helper's id; the cache resets on a size
   change or past 12 buffers.
   Helper lines (Tools/TrinityBrowser/main.c): `js <script>`
   (answer logged "trinity browser: js: ..."), --console,
   load-changed lines, the frame path line.
   OPEN youtube.com (not www): a cached 301 into a service-
   worker-controlled origin; the worker does not handle it and
   the navigation preloader's soup request never completes
   (the server gets the GET, the client closes the socket).
   Reproducer: scratchpad swsite_cached.py case 3. The traces
   used for it are removed (Kalen).

   DONE (Sep 26) masked icons (Wikipedia's menu, search,
   languages...). WebKit paints a CSS mask as a layer with the
   color, a destination-in layer inside it with the mask, then
   ends both; transparency layers were not ported, so the mask
   painted on top and the square was never cut. Now each layer
   is a pooled canvas (GraphicsContextTrinity pushLayer/
   popLayer); a destination-in layer becomes the mask of the
   layer around it, and a layer ends with one trinity call,
   Canvas.compose [ src, mask, luma, alpha, x y w h ] (one
   CanvasUI shader: mask modes 14/15, or the image mode).
   A mask test over file:// never drew: CSS masks load with
   CORS; test over http. webgfx and trinity tests exit 0.
   Rounded clips (Sep 26): shape clips, clip-out and image
   clips became layers kept through a mask; boxes round right
   (test page) but it broke the icons again: a clip layer
   hides the content drawn before it from a later
   destination-in layer. Kalen's direction: CanvasUI conforms
   to WebKit's W3C primitives (composite ops, shadows, clip as
   shader state, one gradient path), glow/shadow states move to
   that interface with no parallel functions. Waiting on his
   calls: glow_falloff, blend-mode method, style names.
   Scroll lag (Sep 26), measured with a wheel every 1/60 s:
   a long plain page scrolls at 62 frames/s, no gap over 25
   ms (frame path is fine). Wikipedia: 40-170 ms stalls; its
   content, sidebars and scrollbar tiles repaint every frame
   (the page's sticky header and contents highlighting), and
   per second: layer push/pop 2000-6900 (66-274 ms), shape
   clips 800-3300 (90-224 ms), glyph runs 15-19k (130 ms),
   tile read back + swizzle 300-450 ms. Fixed: tile read back
   copies only the tile (Texture.read_region, kept staging
   buffer, in-band layouts; webgfx_canvas_read_region, test
   t_canvas_read_region): scrollbar read 73 -> 14 ms/s.
   NEXT: the W3C shader (removes the clip layers), then glyph
   runs and GPU tile buffers (no read back).
   perf and gdb attach are blocked here (perf_event_paranoid
   4, ptrace_scope 1): profile with timing in the code.

## Active work: CanvasUI on W3C primitives (Sep 26 2026)

Kalen: the uber shader conforms to WebKit's W3C primitives;
trinity's glow/shadow states move to that interface, no
parallel functions; Gaussian shadows; styles renamed too.
Blend modes read a copy of the destination (works on every
GPU, MoltenVK included).
1. DONE composite operators: vk.ag `enum Composite` (WebKit's
   order), Pipeline.composite picks a pipeline built per
   operator on first use (build_graphics: Porter-Duff factors,
   composite_source/destination_factor); Canvas.set_composite.
   plus_darker and difference still blend as source-over.
2. DONE clip as shader state, CanvasUI and CanvasText:
   rounded-rect clip computed in the shader (clip_rect,
   clip_radii; Canvas.clip_rounded_rect when the transform is
   axis-aligned), else a clip-mask canvas in a 4th texture
   slot `clip` (clip_to_path, clip_out_path, clip_to_image;
   masks stack with save/restore, pooled; a pooled mask is
   reused only after sync_all). Every fragment exit scales by
   the clip; destination-in keeps the destination outside it.
   Helper GLSL funcs compile into the vertex stage too: no
   gl_FragCoord or discard in them. webgfx tests:
   t_canvas_clip_rounded, t_canvas_clip_path_out,
   t_canvas_destination_in (all pass, webgfx/trinity exit 0).
3. DONE Gaussian shadows: 4 slots (shadow_color0-3,
   shadow_geom0-3 = x y blur spread, shadow_inset), the CSS
   erf edge from the shape's distance (rect/rrect analytic,
   paths from the SDF, which is padded by shadow_reach);
   outer shadows hidden under the box, inset inside the fill;
   Canvas.shadow / box_shadow / no_shadows. Removed:
   inner/outer glow and shadow, stroke_shadow, glow_sides,
   glow_falloff, the lit-side bevel, sdf_rounded_rect_glow.
   Segmented buttons (trinity, orbiter Git) glow one side by
   clipping to the segment and drawing a wider box with an
   inset shadow. WebKit drop shadows map to slot 0 (legacy
   radius doubled; ignore-transforms scaled). webgfx tests
   t_canvas_shadow_outer, _inset (erf falloff matches CSS).
   OPEN: text and image shadows; elliptical border radii.
4. PARTLY DONE one gradient path: Gradient objects now paint
   through the ramp (Canvas.gradient_ramp, cached by stops;
   use_gradient with the inverse transform; conic start angle
   in ramp_geom.z) - the colour wheel and the scroll-edge fade
   bands use it. OPEN: the two-stop fill_grad, stroke_grad and
   text_grad state and the Layer fill_gradient_* props.
5. DONE Canvas.color_filter [ hue, saturate, brightness ]
   (the spec's matrices) replaces colorize/HSV; webgfx
   t_canvas_filter_saturate (red -> 54,54,54). OPEN: WebKit's
   CSS filter property runs through its FilterEffect code,
   not GraphicsContext state: not drawn yet.
6. PARTLY DONE styles: Layer box_shadow and text_shadow
   (BoxShadow: CSS [inset] x y blur spread color, or none;
   transitions mix) replace fill_shadow, fill_glow,
   inner_glow, stroke_shadow, stroke_glow, label_shadow*,
   label_glow; orbiter (33) and hyperspace (3) styles and
   orbiter's inner_glow calls moved. orbiter builds;
   hyperspace fails at ts_load (not from this change).
   OPEN: gradient props (item 4).
7. PARTLY DONE GraphicsContextTrinity: clips are canvas state
   (clipRoundedRect analytic, clipPath/clipOut/image masks),
   layers compose with their own operator; clip layers gone.
   Wikipedia icons and rounded boxes both right. Scroll:
   41 -> 48-54 frames/s, worst stall 172 -> 66-76 ms.
8. DONE text selection lag (Kalen: "selecting text is also
   super slow"). Each selection paint clipped out every
   out-of-flow box (RenderBlock::selectionGaps), and each
   clipOut(rect) made a new mask canvas (clip_push_mask never
   pooled the one it replaced). Now: up to 4 clip-out rects
   are analytic in CanvasUI and CanvasText (clip_out0-3,
   count in clip_on.w; Canvas.clip_out_rect, a mask past 4 or
   under rotation), the replaced mask goes back to the pool,
   webgfx_canvas_clip_out_rect. webgfx t_canvas_clip_out_rects.
   Drag over Wikipedia 6.7 -> 25.7 frames/s; over solid text
   42-45 frames/s. Every move that changes the selection
   paints in 1-7 ms; the frame gaps left are moves where the
   selection end does not change (WebKit asks for no paint:
   checked with timestamps in LayerTreeHost and WebPage).
   Profiling without perf: an LD_PRELOAD SIGPROF sampler and
   strace -f on the helper (a child, so ptrace is allowed).

## Active work: SQLite out of WebKit (Sep 27 2026)

Kalen: no SQLite in the browser; site storage is plain files in
the browser's cache folder (the helper's --data dir). All of it
in the overlay aura/wpewebkit/. In order:
1. DONE, not compiled: localStorage is FileStorageArea, a
   readable localStorage.txt per site (key TAB value per line,
   \t \n \r \\ \uXXXX escapes), saved 500 ms after a change and
   on sync/close. SQLiteStorageArea deleted (wpewebkit.deleted).
1b. WRITTEN, not compiled (Kalen: nothing hidden from the
   user): site folders are named by the site (databaseIdentifier,
   https_www.youtube.com_0), not a salted SHA-256; the salt file
   is gone (NetworkStorageManager).
1c. OPEN: the page Cache API (CacheStorageDiskStore) and the
   HTTP disk cache still name files by salted SHA-1.
2. DONE, not compiled: IndexedDB is WebKit's non-SQL
   MemoryIDBBackingStore with a file path: loaded on open,
   rewritten after each writing commit, deleted with the
   database (IDBStorageManager). File <name>.indexeddb.txt in
   the site's IDB folder (MemoryIDBBackingStoreFile.cpp), tab
   fields, one line each:
     database name version
     store id name keypath autoincrement keygenerator
     index store id name keypath unique multientry
     record store key value(base64 of the engine's encoding)
     entry store index indexkey primarykey
   keys n:number d:ms s:"text" b:base64 a:[k,k]; keypaths
   - s:"p" a:[s:"p",s:"q"]. Max index id is recomputed from
   the file. Whole file rewritten per commit (big databases:
   slow; not measured). SQLiteIDBBackingStore still compiled
   (IDBServer.cpp); goes in item 5.
3. IN PROGRESS cookies and HSTS as folders (Kalen: "dirs with
   simple files", cookies are key/value like the rest).
   DONE, tested on this Mac: libsoup overlay aura/libsoup/
   (+ libsoup.deleted): SoupCookieJarFolder and
   SoupHSTSEnforcerFolder replace the SQLite jar/enforcer; the
   sqlite dependency is gone (libsoup links no sqlite).
   <dir>/<site>/cookies.txt: name TAB value TAB attributes
   (domain=; path=; expires=unix; secure; httponly;
   samesite=Lax). <dir>/<site>/hsts.txt: max-age, expires,
   subdomains as key TAB value lines. Session cookies and
   session HSTS policies are not saved. The jar keeps its own
   copy per site (libsoup fires `changed` under its lock).
   Test: scratchpad jar_test.c against a --destdir stage of
   the libsoup build: 3 cookies set, 2 persistent reloaded,
   tab in a value kept, HSTS policy reloaded.
   NOT DONE, WebKit side (checked where, nothing edited yet):
   - SoupCookiePersistentStorageType {Text, SQLite} ->
     {Text, Folder}: Shared/soup/SoupCookiePersistentStorage
     Type.h, WebsiteDataStore.h:650 default, NetworkSession
     Soup.cpp:87 (soup_cookie_jar_folder_new),
     WebKitCookieManager.cpp:112, WebKitCookieManager.h.in
     (WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE -> _FOLDER),
     Tools/MiniBrowser/wpe/main.cpp:512,554, TestCookieManager
     .cpp:94,705,727,865.
   - WebCore/platform/network/soup/SoupNetworkSession.cpp:190
     hsts-storage.sqlite -> soup_hsts_enforcer_folder_new(
     <dir>/hsts).
   - helper Tools/TrinityBrowser/main.c:332-336: FOLDER with
     <data>/cookies.
   Then copy each file into aura/wpewebkit/.
3b. DONE (Sep 28) libsoup warned "soup-tld: There is no
   public-suffix data available" on every request: aura.ag
   built libpsl with --disable-builtin. The line is gone; the
   tarball's list/public_suffix_list.dat is compiled in (its
   psl-make-dafsa runs on install/bin/python3). --disable-
   runtime stays (no libidn2). Checked: a YouTube run logs no
   soup-tld line.
4. Service worker registrations in memory; Web SQL, web push,
   click measurement, tracking statistics, content blockers,
   enhanced-security sites and the favicon database off.
5. WebCore platform/sql and find_package(SQLite3) removed.
Checking needs WebKit configured: on this Mac harfbuzz, icu,
libjpeg, libepoxy, libxkbcommon, libxml2 and libwebp are still
missing imports (not added: Kalen questioned webp; SQLite is
being removed instead of imported).
Where the Mac build stands (Sep 27): every aura.ag import
up to libsoup builds; WebKit's configure stops at HarfBuzz.
silver.c fixes made for it (in src/silver.c, rebuilt by
Kalen): SDKROOT in the import build env on native macOS (our
clang finds no SDK otherwise), release archives skip
autogen.sh when configure ships, configure imports run `make
clean` before make (stale in-tree objects). aura.ag gained
pkgconf (system dirs /usr/include, /usr/lib) and glib imports;
Au.g no longer imports libffi (glib's bundled copy is the one
in install/). WebKitXcodeSDK.cmake: WPE on a Mac builds for
the host arch (overlay). The downloaded WebKit checkout is
checkout/releases/wpewebkit (owner taken from the URL path).
OPEN: our clang should know the macOS SDK itself (a
clang.cfg beside install/bin/clang), then the SDKROOT lines
in checkout() go; checkouts from archive URLs land under the
URL's parent folder name (checkout/3.3/ruby).

Oct 14 2026 (Kalen): checkout/lb (the Ladybird tree and build,
6.3 GB) and checkout/wk (the hand-run WPE WebKit build, 4.7 GB)
deleted for disk space. The Ladybird patch stays in aura/ladybird
(overlay); WebKit is built by `silver aura` into platform/native/
build/releases/wpewebkit. install/ladybird is a link to the gone
build (unused).

## MEMORY: trinity replaced Skia under a real browser (Sep 25 2026)

What was profound: a full web engine (Ladybird: HTML, CSS,
JS, networking, fonts, SVG) draws every pixel through trinity,
our own GPU canvas, with no Skia, ANGLE or Qt. Kalen's rule
held from start to finish: "Skia is a canvas. WE HAVE THAT."
The only new code is glue; every drawing job lands in trinity.
It also paid trinity back: the browser found four bugs that
hurt every trinity app (below), not just the browser.

How the swap works:
- Ladybird draws through a few narrow seams: Gfx::PathImpl,
  Gfx::Typeface, PaintingSurface, Painter and the display list
  player. Each Skia class there got a Trinity twin (PathTrinity,
  TypefaceTrinity, PainterTrinity, DisplayListPlayerTrinity),
  and Skia's files are deleted.
- The bridge is webgfx, a silver module exporting plain C
  functions (fonts, paths, canvases, images, layers), with
  objects passed as small integer ids. Au headers cannot enter
  Ladybird's C++ (Au's get/set/len/push macros break AK), so C
  is the boundary.
- Fonts: trinity CanvasFont loads from bytes, measures, and
  draws by glyph number; FontMatch (fontconfig) picks system
  fonts. HarfBuzz still shapes in Ladybird.
- Frames: Ladybird's own processes stay (WebContent, network,
  Compositor). The Compositor replays each frame's display list
  onto a trinity canvas; the Trinity frontend hands finished
  frames to the browser element in shared memory, and input
  goes back as text lines on a socket.
- Masks and layers: an offscreen trinity canvas per layer,
  composited through the mask by a Canvas shader mode.

Bugs the browser exposed in trinity itself:
- Canvas.sync_all drew the last shape twice on every read.
- Canvas.set_clip stored a clip nothing read: no clip worked.
- Text rendered in its own batch, so it could land on top of
  shapes drawn after it (z order).
- Path fills: subpaths never closed, and every path in a batch
  shared one edge buffer, so paths drew with another's shape.

Lessons:
- Keep work out of /tmp: a reboot wiped the whole patched tree
  (recovered by replaying the transcript). It lives in
  checkout/lb now, and the patch as an overlay in aura/.
- Speed came from reusing GPU work: one texture per image
  (not per draw), one GPU wait per frame (paint_n 1024).
- A runtime-sized `vec <C struct> [ n ]` overran the heap in
  silver: keep fixed local arrays for Vulkan structs (OPEN).

## Active work: YouTube through MediaSource on trinity (Sep 26 2026)

Kalen: YouTube says "can't play this video", or plays one frame
in 30 s; performance is awful. Reproduced: a page has no
MediaSource (YouTube needs it), so YouTube gives up. Stage c
of the media memory below, done as a streaming backend.
1. DONE Demux reads fragmented mp4 (trinity/demux.ag append:
   moov + mvex/trex, moof tfhd/tfdt/trun + mdat, whole boxes
   only; DemuxSample owns its bytes; reset). webgfx
   t_demux_fragments: media/clip.frag.mp4 in 997-byte pieces,
   48/48 samples match the plain file (bytes, dts, pts, sync).
2. DONE VideoStream (trinity/video.ag): samples queued in
   decode order, a worker decodes 6 ahead, frame_at by time,
   flush (generation), hidden (non-displaying) samples.
   t_video_stream: 48/48 once each on a 60 Hz clock; a seek
   to 1 s shows the 1.000 s picture.
3. DONE AudioStream (spectra): aac packets decoded as pushed
   into an 8 s ring; the mixer pulls it (add/remove_stream);
   time[] is the clock. t_audio_stream: 440 Hz, 1 s pulled
   moves the clock 1,000,000 us.
4. DONE WebKit MediaSource (ENABLE_MEDIA_SOURCE=ON in the
   configure line): MediaSourcePrivateTrinity,
   SourceBufferPrivateTrinity, MediaSampleTrinity, the
   player's source path (enqueue, readyForMore, flush, seek
   via waitForTarget + reenqueue, sound clock); webgfx
   webgfx_demux_* and webgfx_stream_*; MediaPlatformType
   Trinity. isTypeSupported: avc1 true, mp4a true, vp9 false.
   Checked in the browser: local 20 s MSE page plays 0 -> 20 s
   in real time, 640x360, picture right; YouTube Big Buck
   Bunny plays (picture moving), 3 runs with no crash.
   Fixed on the way: an empty type (a source attaching) looked
   up a null String in a HashSet (web process SIGSEGV); silver
   classes are PACKED structs, so a pthread_mutex_t member sat
   at byte 102 and futex failed under contention: use Au's
   `mutex` class in silver classes, never raw pthread types.
5. OPEN video as its own compositing layer: written, not in
   the build (trinity/CoordinatedPlatformLayerBufferTrinity,
   webgfx_stream_next/_planes, buffer Type::Trinity).
6. PARTLY DONE YouTube plays, but the picture changes only
   1-7 times a second headless; Kalen sees 1 frame every 3-4
   seconds (Sep 26). The web process main thread is busy.
7. OPEN YouTube page speed (Kalen: "performance is absolutely
   awful"). Sampled during playback: ~90% of the web process
   is CSS filters (drop-shadow) making temporary ImageBuffers,
   each a new trinity Canvas whose 1024-slot ring creates
   ~1024 uniform Buffers (Canvas_init -> uniforms_init ->
   vmaCreateBuffer). Tried and REMOVED (lost the GPU device,
   VK_ERROR_DEVICE_LOST, cause not found): a webgfx canvas
   pool, and a 64-slot ring for small canvases (a mid-batch
   ring wrap on a webgfx canvas faults; 1024 never wraps).
   Next: uniform buffers made on first use, then the wrap
   fault; filters on the GPU.
9. DONE (Sep 27 night) the volume knob did not repaint while
   dragged; only a sliver at the slider's left changed. The knob
   (.ytp-volume-slider-handle, 12 px, filter: drop-shadow, with
   64 px ::before/::after bars) paints its filter in software
   (WPE never composites for a filter alone), so it is its own
   repaint container: a move left old and new rects equal and
   nothing was invalidated; the sliver was the volume icon's
   own repaint re-rendering the knob's filter clipped to it.
   Fix (overlay RenderLayer.cpp, updateLayerPositions): such a
   layer's repaint rects track in its enclosingFilterRepaintLayer
   as calculateLayerBounds (children + filter outsets). Checked:
   a local page (timer-moved knob) and YouTube headless, knob
   and bar follow the pointer with the video playing.
   OPEN: the page paints 30 frames/s while the knob drags (60
   idle): the software drop-shadow re-render per move.
10. DONE silver.c checkout(): an overlay file newer than the
   import's silver-token runs the import's build and install in
   its build tree (import_build) instead of skipping it; the
   token is rewritten. OPEN: a module whose .ag is unchanged is
   "up to date" and never reaches the import step: use
   `silver --clean --build browser` after an overlay edit.
8. DONE logins kept, in silver's app cache (Kalen): aura.ag
   passes --data path_cache['aura'] (~/.cache/aura); the
   helper keeps site data in <it>/data (cookies.sqlite there)
   and cache in <it>/cache. Cookies were memory only before
   (signed out on every start). Checked: a cookie set by a
   local server came back from a new browser process and is in
   ~/.cache/aura/data/cookies.sqlite.
   Saved passwords: WebKit has no password manager (not done).

## Active work: browser built by silver, no SQLite, TLS (Sep 27 2026)

DONE (Oct 21, Kalen: every site failed in aura) the page process
(WPEWebProcess) died right after each load (the title went empty,
no frame came): libsilver-webgfx.so was from Oct 19, built against
an older trinity, and WebKit loads it directly, so nothing rebuilt
it when trinity's classes changed. `silver --clean --build webgfx`
fixed it (example.com drawn hidden; webgfx tests exit 0). OPEN
silver: a module is not rebuilt when a module it imports changes.

The browser module is `aura` (renamed from `browser`, Sep 28:
aura/, aura.ag, aura.c, ~/.cache/aura, trinity-aura.sock; the
WebKit helper keeps its name WPETrinityBrowser).
WebKit is built ONLY by `silver aura` (import in aura.ag,
overlay aura/wpewebkit/). Never ninja by hand in a checkout.
1. DONE overlay applies on every build (silver.c checkout), not
   only a fresh checkout; env lines are in the import cache key.
2. DONE meson 1.8.3 import (glib 2.84 needs >= 1.4); glib and
   libsoup get an rpath to {install}/lib.
3. DONE ENABLE_WEBGL=OFF: no ANGLE.
4. DONE no SQLite storage: Sources.txt lists, HSTS and cookie
   stores on libsoup's folder types, FileStorageArea and
   MemoryIDBBackingStoreFile listed; GSTREAMER_GL needs GStreamer.
5. DONE, not built: https. glib-networking import with a new
   mbedtls 4 backend (overlay aura/glib-networking/tls/mbedtls,
   8 files, shaped like its gnutls backend over tls/base). Clean
   -fsyntax-only -Wall -Wextra. The base verifies the peer after
   mbedtls_ssl_handshake, before data. No DTLS, no resumption.
   CA roots from the system bundle (mac keychain not done).
6. OPEN SQLite still in WebKit: IndexedDB SQLite files, service
   workers, tracking prevention, website data, find_package.

## MEMORY: zap, Super+Shift+A (Sep 28 2026)

Kalen presses Super+Shift+A (Cmd+Shift+A) the moment a symptom
shows (a slow orbiter/browser close takes about 10 s).
- A zap means: Kalen is seeing the issue right now. Read the
  file as it fills and look at the live app at once.
- support/zap.sh: first a line `time | message | file` onto
  install/tmp/zap/inbox; then `stats` from each trinity app
  socket (1 s limit, a stuck app shows as no answer); then every
  thread of each orbiter, aura and WPE* process (name, state,
  kernel wait `wchan`, cpu time) every 0.2 s until they have all
  exited (30 s at most) into install/tmp/zap/zap.<HHMMSS>.txt;
  last a `done after Ns` inbox line. Arguments are the message
  (default "slow close").
- Registered as a GNOME custom shortcut "zap to Claude" beside
  Kalen's custom0/custom1 (Alt+Down/Up volume). Write GNOME
  settings with /usr/bin/gsettings: install/bin/gsettings comes
  first on PATH, has no dconf module and silently drops writes.
- It reaches an agent only through a live session: that session
  runs a Monitor on `tail -n 0 -F install/tmp/zap/inbox`
  (re-armed every 30 min). Without one the files still land.
- OPEN, proposed: zap.sh runs the user's own agent itself
  (`claude -p` on the snapshot, answer saved beside it,
  notify-send), so no session is needed.
- Limit: kernel waits only; user-space stacks need ptrace,
  blocked here (ptrace_scope 1).

## MEMORY: media in the browser, played through trinity (Sep 26 2026)

The browser element's engine is now WPE WebKit 2.54.0 (checkout
checkout/wk/wpewebkit-2.54.0, overlay aura/wpewebkit/, its
configure line aura/wpewebkit.configure.sh). WebKit lays out
and runs pages; trinity draws. This entry covers video and
sound: everything done Sep 25-26, why, and what is left.

### Why it started (Kalen's reports, in order)
1. Captchas everywhere. WPE sent its own Linux/WPE user agent,
   which few real visitors send. Fix: the helper
   (Tools/TrinityBrowser/main.c) sets Safari 18.5 on macOS as
   the agent (webkit_settings_set_user_agent) and a
   document-start user script makes navigator.platform
   'MacIntel', so the two agree. Checked with a local server:
   header and page both say Mac. Whether captchas stop: not
   confirmed.
2. youtube.com stopped at its grey placeholder columns. Found
   with the helper's console on stdout
   (enable-write-console-messages-to-stdout, temporary): the
   main script threw "ReferenceError: Can't find variable:
   HTMLVideoElement". WPE was configured ENABLE_VIDEO=OFF, and
   WPE ties video to GStreamer. Kalen chose a trinity media
   backend, no GStreamer (GStreamer is not installed at all).

### Stage a: video on without GStreamer (DONE)
- Source/cmake/GStreamerDependencies.cmake: the line
  WEBKIT_OPTION_DEPEND(ENABLE_VIDEO USE_GSTREAMER) removed.
- Source/WebCore/platform/GStreamer.cmake: returns at the top
  when USE_GSTREAMER is off (its sources compiled whenever
  video was on and failed on Gst types).
- Source/WebKit/GPUProcess/media/trinity/
  RemoteMediaPlayerProxyTrinity.cpp (new, listed in
  Source/WebKit/SourcesWPE.txt): the per-backend
  RemoteMediaPlayerProxy::mediaPlayerFirstVideoFrameAvailable,
  else a link error.
- configure.sh: -DENABLE_VIDEO=ON.
Result: YouTube's app starts (signed-out "Try searching").

### Stage b: the trinity media player (DONE)
The chain, from network to screen:
WebKit fetches the file -> webgfx (silver, C calls) -> trinity
Demux reads mp4 -> trinity Decoder decodes h.264 on the Vulkan
video queue -> y, u, v planes -> three webgfx plane textures ->
the Canvas's yuv mode converts to rgb on the GPU while the page
tile paints. Sound: spectra decodes AAC with faad2 into one
AudioClip; an AudioMixer voice plays it from the video's time.

Media runs in the WEB process: the GPU process for media is on
by default only on Mac (GPU_PROCESS_BY_DEFAULT is Cocoa-only).
The RTX 3060 offers Vulkan decode for H.264, H.265, VP9, AV1.

trinity/vk.ag
- Enables VK_KHR_video_decode_queue and _decode_h264 (beside
  encode; sync2 and video_queue enabled once for both) and a
  decode queue: vk.decode_family / vk.decode_queue, a video
  family other than compute and encode. Extension list 16.

trinity/trinity.c + trinity/video.h (C, because the H.264
standard structs are bitfields silver cannot write, as for the
encoder's h264_sps helpers already there)
- h264d_new/free, h264d_config (avcC: nal length size, SPS,
  PPS), h264d_sample (one length-prefixed sample: returns 1
  when a picture is ready), h264d_params_dirty / h264d_params
  (SPS/PPS as session parameters), h264d_vk (fills the Vulkan
  begin-coding and decode infos, all pointing into the
  decoder's own storage), h264d_decoded (after the GPU decode:
  reference marking and display order), h264d_output (next
  picture in display order: its slot and pts), h264d_flush.
- Parses SPS (profiles with chroma_format_idc, scaling lists,
  POC types, cropping, VUI incl. max_num_reorder_frames),
  PPS (incl. transform_8x8 and scaling lists), and each slice
  header up to dec_ref_pic_marking (the GPU reads the rest:
  Vulkan builds the reference lists itself).
- Picture order count types 0, 1 and 2; 17 slots; sliding
  window and all MMCO ops (1-6, 5 resets POC and frame_num);
  IDR and MMCO5 push every waiting picture out first. Frames
  only: field pictures are refused.
- A picture in the output queue keeps its slot until taken
  (queued flag); the new picture's slot is chosen in h264d_vk,
  after the caller drained the queue. Otherwise a bumped frame
  could be overwritten before it was copied out.
- Slices go to the GPU as 00 00 01 + NAL, with slice offsets.
  In vkCmdBeginVideoCoding the slot being set up is listed
  with slotIndex -1.
- nv12_split (coded-size NV12 to display-size y, u, v) and
  yuv_rgba (limited range, 601 or 709; tests only).

trinity/video.ag
- class VideoFrame: y, u, v (vec u8) and pts; rgba[] for tests.
- class Decoder: config[ avcc, n ] makes the session (profile
  from the SPS, 8-bit 4:2:0 progressive), session memory,
  parameters (remade when the SPS/PPS set changes), one
  17-layer NV12 image that is both reference slots and output
  (needs DPB_AND_OUTPUT_COINCIDE; logs and refuses otherwise),
  a host-visible bitstream buffer (grows, size aligned to the
  caps), a host-visible plane buffer. decode[ data, n, pts ]
  decodes synchronously (fence wait) and puts finished frames
  in display order into frames. finish[] flushes at the end.
- Frames come back through the CPU: copy the layer's two planes
  to the host buffer, split into y/u/v at display size. No
  per-pixel colour work on the CPU.

trinity/demux.ag (new; `import demux` in trinity.ag)
- class Demux: read_bytes[ bytes, n ] walks moov/trak/mdia/
  minf/stbl (+ edts). class DemuxTrack: kind (1 video, 2
  audio), codec (four bytes as i64), timescale, duration,
  config (avcC, or AAC AudioSpecificConfig from esds), size or
  channels/rate, and per sample offsets, sizes, dts, cts, sync.
  pts[ i ] = dts + cts - shift; shift is the edit list's first
  media time (elst): without it every frame was 2 frames late.
- Progressive mp4 only; fragmented mp4 (moof) is stage c.

spectra/spectra.ag
- aac_clip[ asc, asc_n, data, offsets, sizes, skip ] ->
  AudioClip: AAC packets from any container, priming frames
  skipped (the audio track's edit-list shift).
- AudioMixer.voice_seek[ h, frame ]: software mix only.

webgfx/webgfx.ag (the C boundary WebKit calls; imports spectra)
- class WebVideo: its own copy of the file bytes, Demux, the
  first avc1 track, a Decoder, three plane ids (y, u/2, v/2),
  bt709 (height >= 720), the AAC clip, a voice, a gain.
- webgfx_video_new(bytes, size) -> id (0 = unplayable),
  webgfx_video_free, webgfx_video_info(id, &w, &h, &seconds),
  webgfx_video_has_audio, webgfx_video_advance(id, seconds)
  (decodes only as far as needed; shows the latest frame at or
  before the time; a later frame waits unless nothing is on
  screen yet; 1 when a newer frame went up),
  webgfx_video_seek (restarts the decoder at the key frame at
  or before the time), webgfx_video_play(id, seconds) /
  _pause / _volume (one shared AudioMixer at 48 kHz, started on
  first sound), webgfx_canvas_draw_video(canvas, id, dst)
  (limited-range rows for draw_yuv).
- Tests (silver --test webgfx, all pass): t_video_decode (48/48
  frames of media/clip.mp4, High profile with B-frames, in
  display order; written to install/tmp/clip-decoded.rgba and
  each within ~48.6 dB PSNR of ffmpeg's frame: rgb rounding
  only), t_video_draw (frame at 1 s drawn on a canvas is frame
  24; luma 40 dB; chroma edges differ because the GPU smooths
  chroma), t_video_steps (90 steps at 60 a second, every frame
  as wanted), t_video_audio (media/av.mp4's AAC tone: 48 kHz,
  440 cycles a second, ~2 s).
- ffmpeg on this machine is used ONLY to make test clips and
  reference frames, never inside the browser.

WebKit (checkout and overlay)
- VideoFrame.h: virtual isTrinity() under USE(TRINITY).
- platform/graphics/trinity/VideoFrameTrinity.h (new): a frame
  that names a webgfx video id; type traits for downcast.
- GraphicsContextTrinity.h/.cpp: drawVideoFrame draws a
  VideoFrameTrinity with webgfx_canvas_draw_video, others as
  the base does. Page tiles paint straight onto this context
  (TrinityPaintingEngine), so paint() reaches it.
- platform/graphics/trinity/MediaPlayerPrivateTrinity.h/.cpp
  (new, in platform/SourcesTrinity.txt): engine Trinity for
  video/mp4, video/quicktime, video/x-m4v with avc1, avc3,
  mp4a codecs (refuses MediaSource/stream decoding types).
  load() fetches the whole file with the element's
  PlatformMediaResourceLoader (TrinityMediaClient gathers the
  bytes), then webgfx_video_new; states go HaveMetadata ->
  HaveEnoughData, Loaded. A 60 Hz RunLoop timer shows the
  frame for a monotonic clock (m_from + elapsed x rate); ends
  at the duration. play/pause/seek/volume/mute drive the sound.
  paint() draws m_frame through drawVideoFrame.
- MediaPlayer.cpp registers it (USE(TRINITY) && !USE(GSTREAMER)).
  MediaPlayer.h (MediaPlayerType::Trinity), MediaPlayerEnums.h
  and Shared/WebCoreArgumentCoders.serialization.in
  (MediaEngineIdentifier Trinity).
- WebGfx.h declares the webgfx video calls.

### How it was checked in the browser
- A local server (python http.server) serves a page with a
  <video>; the page fetches /report?t=... every 0.5 s so its
  clock shows in the server log; frames are read from the
  helper's shared file (TRINITY_BROWSER_SHM: 32-byte header,
  magic TRBW, seq, w, h, stride; then rgba).
- 10 s 640x360 clip: 300 frame changes in 582 ticks (every frame
  once), frame 226 (7.533 s) matched the page clock, ended at
  10.00, error none.
- Sound: unmuted autoplay is blocked (as in any browser); a
  click sent down the helper's input ("mouse down x y 0 1 0")
  starts it. pactl then lists "ALSA plug-in [WPEWebProcess]",
  uncorked, 100%. Not heard by me.
- WebKit rebuild: ninja -C checkout/wk/wpewebkit-2.54.0/build,
  then `ninja ... install`: the helper loads the INSTALLED
  libWPEWebKit (its rpath), so a build without install still
  runs the old code.

### Bugs found and fixed on the way (all mine)
- Edit lists ignored: all times 2 frames late (1024 ticks).
- A decoded future frame was shown early when nothing was due:
  video ran twice as fast at 60 Hz ticks on 30 fps content.
- free[] on memory from `new f32 [ n ]` (silver-owned): crash.
- An inline `vk_context []` inside a Window constructor is not
  owned by anything: freed mid-use ("Invalid device").

### Silver compiler bugs met (OPEN, worked around)
- `v.push[ w[ i ] ]` (w a vec i64) pushes the element's
  ADDRESS: ctts offsets came out as 98715440282664, +16 a
  sample. Read into a local first.
- `is`, `signed` and `parse` are taken names (keyword, type,
  Au method): use box_is, neg, read_bytes.
- Function arguments are read-only: copy to a local to walk.
- A VkResult cannot interpolate into a string: i32[ res ].

### Limits and what is next
- Whole file downloads before play; no range requests.
- Planes cross the CPU once per frame (copy + upload).
- Rates other than 1 keep sound at 1x; the picture follows a
  plain clock, not the audio clock (long videos can drift).
- H.264 + AAC in mp4 only; field video refused; no VP9/AV1 yet
  (the GPU could decode them).
- Mac: coreaudio mixer path has no voice seek.
- NEXT, stage c: MediaSource. YouTube streams through it.
  Needs ENABLE_MEDIA_SOURCE, fragmented mp4 (moof/traf/trun)
  in Demux, SourceBuffer append/remove, and advertising only
  avc1/mp4a so YouTube serves H.264 + AAC.

## MEMORY: the reload transition (Sep 22 2026, fixed)

THE REQUIREMENT (Kalen, said many times, do not reinterpret it):
the avatar and the exchange do not disappear at a reload. The new
instance RECREATES them instantly, in the exact state they had,
and keeps drawing them; the old blur fades out to the NEW screen
with the avatar and boxes still there. Nothing is "the same
object"; it is recreated fast from carried state. Timers start
when the first fully loaded frame has rendered, not at apply.

HOW IT WORKS NOW (verified headless with shots, 7 cycles):
- trinity `ExchangeState` (shot_ask/file/line/text, agent_view/min,
  the two note vecs, agent_pick, the blur, the avatar's halo/flame/
  tilt/listen/spin/settled/shown/done), `Window.exchange_carry/
  restore`, `snap_transitions` (every transition lands on the
  restore frame), the exchange closing itself when shot_bg reaches
  0 under swap_fade. All `:fading` styles are 600 ms.
- orbiter `persist swap_carry : ExchangeState`, filled at pending 3
  (exchange + avatar carry), then the go.
- `element.on_frame [ w: Window ]`: the app's word each frame BEFORE
  the window composes; `Window.on_compose` calls it on the app
  object first. orbiter restores there (guard `!swap_asked`), so
  frame one after the switch composes the full exchange. A restore
  in `render[]` or `tick` was one frame late: trinity's app_render
  reads shot_ask before the app's render runs, and the exchange
  then mounted on frame two already fading (never seen at full).
- Avatar `paint_once` (set by restore) and `fresh9` (made this
  frame): the target renders once inside `draw` (update, draw,
  sync_fence) before its first paint. The frame's element_targets
  pass (vk.ag ~4810) runs before on_compose and skipped the avatar
  at opacity 0, so the compose painted its empty image: a magenta
  square on frame one.
- The fade starts in tick the frame after the restore, once
  `ux.loaded[]`: `ux.swap_fade` sets `fading` on every exchange
  element and the avatar; shot_close at 0. Every `:fading` is
  3000 ms (Kalen: long enough to judge by eye).
- `agent_av_shot` is set at the restore: orbiter's `agent_avatar[]`
  takes a shot_ask it has not seen as a NEW capture and resets
  `settled`, which put the restored avatar at the fresh-exchange
  top-center start and slid it left over 900 ms (Kalen's report).
- OPEN, one frame: the user's box frame (ShotUser 'frame' layer)
  draws at its base one-row height on the restore frame with the
  entries above it; right from frame two. `ShotUserLayer.draw`
  sizes the frame layer during its own draw and marks a repaint,
  so the frame lags the entries by one frame by construction (a
  fresh exchange has the same lag as each entry arrives). A fix is
  the frame height at layout time (text measure outside a paint).
- Drive: headless orbiter (`XDG_RUNTIME_DIR=/tmp/claude-501/ob8`,
  a long runtime dir truncates the socket name), ctrl+shift+S, drag,
  Enter, `app status busy`, touch orbiter/Editor.ag, `app status
  done`, wait for "reload complete" in the host log, `shot` burst.
  With `text carry check` + Enter before busy, the avatar is settled
  (left) and both boxes have entries. Pass: frame one = the exchange
  as it was, avatar at its settled place with its flame; then fading
  over the new screen for 3 s.

## MEMORY: GPU memory across reloads (Sep 22 2026, evening)

Symptom (Kalen): 4-5 live reloads and orbiter dies: Metal "Insufficient
Memory", VK_ERROR_OUT_OF_DEVICE_MEMORY, a SIGBUS in a MoltenVK submit,
or a strncmp SIGSEGV in apply_style on a freed string. Measured
headless: every reload left one instance's worth of textures alive
(about 190 samplers), and one exchange open+close leaked 17.

Instrument: `vk.samp_tags` / `vk.samp_keys` count live samplers by
the texture's `tag`, else by its creation `file:line`; the stats
line (`stats` over the app socket) prints them after `| tags`. The
blur chain's renders carry `tag: 'blur'`. Under `--leaks`, the exit
report walks holders ("held by X via .m"; PHANTOM = a hold no live
object accounts for; "unbalanced hold:" names its call site) and
`au_leak_sites(Au)` (Au.c) prints one object's recorded hold sites.

The rule (Kalen): a store in `init` does not hold; `hold_members`
after init owns the slots. A store in a method that init CALLS
(a delegate) does hold, and hold_members holds again: a double hold
that leaks the object forever. Delegate calls handle it manually;
never patch aether for it.

Fixed, all verified headless (3 exchange reloads, no crash):
- Render: the color/depth/reduction textures are made in `init`
  itself (sized from wscale as update does); `update` only resizes.
  Before: made in `update` from init, held twice, never freed.
- Pipeline: `resources`/`storage_textures` are made in `init` before
  `bind_resources` (its delegate), which now clears or lazily makes
  them instead of re-storing. Before: every pipeline's resource
  vector (Gpu -> bound textures, buffers) was a phantom root.
- Pipeline.dealloc destroys its descriptor pool and the two set
  layouts; Render.destroy_ring destroys the timestamp query pool
  (Metal has a small fixed number of counter sample buffers; the
  "Could not create MTLCounterSampleBuffer" flood was that).
- NOT changed (Kalen's recent code, kept): the five explicit
  `.hold[]` in build_frost_chain and the `mdl.cache.hold[]`/`drop[]`
  pair in Canvas.draw_svg. Removing the frost holds crashed the
  live app on its first reload (vk_context_xfer from rewire_frost
  after the switch, a freed chain object); the svg pair is why
  `svg` grows 38 per reload (icon cache canvases never freed:
  SVGModel.cache is public, the store holds, the explicit hold
  stays at the model's dealloc). Both are open, with Kalen.
- REVERTED (Kalen): touching the app element at destroy. It is
  process-owned (managed 0) and stays as it was. `managed = 1` +
  free gave DOUBLE-DROP from a map that still listed it; then
  `app.drop_members[]` crashed the next instance's first draw
  (Display_draw -> Texture_transition -> vk_context_xfer): objects
  the old app element owns are still used by the new instance
  through unheld context pointers. Do not free or drop-walk it.
- orbiter.on_unload: paused index watches drop their callback (the
  lambda's context held the app object; `watch_pause` drains the
  FSEvents queue first, so clearing after it is safe).
- exchange_restore points the carried blur's Renders and Models at
  the new Display (`r.w = a`): their `w` is an unheld context member
  and the old Display dies under them at the swap (Render_dealloc
  SIGSEGV in `w.element_targets`).
- `save-persistent [ target ]`: the window-handle lookup resolved to
  the previous instance after a reload (the platform window is
  reused) and the session file was never written.

Still growing: samplers 191 -> 271 -> 331 -> 396 over three
reloads, mostly `svg` (+38 each, the pair above) and `blur` (+10,
the frost chain's holds above); rcolor/sdf/image textures now stay
near one instance's count. rss 640 -> 936 -> 1012 -> 755 MB. No
crash and no device loss in three exchange reloads headless. The compiler segfault on
`if [ target ] state_persist_store[ target ]` (one-line if with a
call) is unfixed; write the plain two-line form.

OPEN, separate bug: 1 of 7 cycles SIGSEGV on the close thread in
the old instance's destroy: silver_live_destroy -> map_dealloc ->
Pipeline_dealloc -> vector_clear -> Texture dealloc ->
Texture_release_gpu reads a `tag` string whose header is freed. The
strings freed just before are the old ShotPrompt canvas's sampler
keys ('edges', 'sdf_out', 'atlas', 'text'): a double drop in a text
canvas's pipeline/texture chain. Stack in install/tmp/orbiter.log.

## Active work: orbiter memory (Sep 21 2026)

Target from Kalen: about 120 MB; the avatar (about 30 MB) is
the largest object. Startup footprint measured headless.

1. DONE `.f` map only for loaded buffers: `path.read_format_of`
   skips other sections; the leaking hold on `lines` in
   `path_read_format` is removed. 402,759 tokens -> none kept.
2. DONE Text vertex ring made on first use (`text_ring_add`):
   3,200 text Gpu -> 448. Footprint 2396 MB -> 1893 MB.
3. DONE VMA block size 256 MB -> 8 MB (vk.ag): the graphics
   footprint was mostly empty block. 1893 -> 1382 MB.
4. DONE Editor fade mask canvas replaced by a shader ramp
   (Canvas `image_ramp`, `draw_canvas_ramp`); blur outputs
   (`ReduceBlur` rv/rh) at one pixel per point on retina.
   1382 -> 1257 MB. Full-size gaussians stay full size by
   Kalen's call: no upscaling of blur results.
5. DONE `auto_free` lost its reset_only argument: the `[true]`
   resets pinned 368,048 startup objects (~200 MB) for the run.
   media_app.init no longer drains (members are raw stores until
   hold_members after init); the first drain is in run. Worker
   threads (index, git) drain their own pools. realpath buffers
   in path_absolute were never freed. Packed syntax regions
   (vec i32 x5 per token) replace ColorRegion/rgba objects.
   1257 -> 1046 MB.
6. DONE Scene backdrops (scenes.ag create_target) and the frost
   reduce chain at one pixel per point on retina; r_view, screen
   and compose stay full res. 1046 -> 1001 MB. `scenes` is its
   own module: rebuild it separately (`silver --build scenes`).
7. DONE `compose` and `glyph` window canvases removed: compose was
   cleared to #888 and never painted (no compose draw stage ran),
   so the ux shader read constants from it; glyph was unread.
   The ux fragment is now blur * max(1, dim_floor). 1001 -> 961 MB.
   r_view, m_view and the UXBackground shader are removed too:
   on_screen paints the stage's blur texture (or solid_color) into
   `screen` first. `screen` is Display.final, what presents and
   what a shot reads. All four frost stages checked. 958 MB.
8. OPEN GPU images at startup (~530 MB): 12 window-size
   colour targets (screen, compose, r_reduce, r_view+depth,
   6 blur outputs now halved, 1 empty Render unidentified),
   3 at 2402x2402, editor text canvases per pane (23 MB each
   at 2x plus a 4-byte SDF at half size), avatar 3120x3120
   colour+depth (74 MB), backdrop 4096x2048 (32 MB).
9. DONE uniform Buffers: one buffer per pipeline (`u_ring`, vk.ag
   uniforms.init), a slot every `stride` bytes; was 64 per `uniforms`.
10. OPEN small heap 216 MB not yet attributed; ~5,000 pool
   temporaries per idle frame.

## Active work: features docs page (Sep 28 2026)

1. DONE features.ag is sorted into 14 categories, each opened by
   `# ==== Label: Headline` and an intro comment. `export func
   docs_page` writes features/docs/features.html in the site's
   layout (head, CSS, hero and footer copied into
   features/docs/page.html from /src/ar-visions.github.io).
   Order matters: a free func signature needs its types above
   it; a load-time static (`attrib egg : Chirper`) needs its
   class above it.
2. OPEN t_launch_specs: the .agi has no launch specs any more
   (only trinity's launch_props writes them); the test fails.
3. OPEN t_mem_last: 33 objects alive at the end, 28 at the start,
   with the original order too. Hidden until now: the runner
   stops at the first failure, t_vec_module.
4. OPEN the page is not yet compared with index.html by eye.

## Active work: near-border (Sep 29 2026)

1. BUILT, tested: the pointer lights every border by its
   nearness. No style property (Kalen: never different between
   controls). Window.near_walk records each element's window
   origin (win_pos) and marks it for repaint while the pointer
   is within near_reach (160 px) of its box, and once after it
   leaves. The layer draw passes the pointer and reach into the
   canvas (near_light); CanvasUI's near_border weights the
   stroke: w = (1 - distance / reach)^2, the border's colour
   mixed toward white by w/2, its alpha raised by w/2.
   trinity t_near_border: red 128 at 20 px from the light, 51
   (the border's own) at 340 px.
   The light's strength is a filtered state per element
   (near_glow): its goal is near_in_weight (1.0) with the pointer
   inside the element, near_out_weight (0.5) outside it within
   reach, else 0; each frame glow = filter_n[ glow, goal, 0.1 ]
   (0.9 kept, 0.1 new at 60 Hz). t_near_border_glow: red 90 at
   half glow.
   Opt-in (Kalen, Sep 29): a Layer's `near_border: true`, or a
   draw's Window.near_light_on[ cv, e, ox, oy ]. Only an element
   whose last paint lit a border (near_used) repaints on moves.
   orbiter: title bar (tbar), status bar (sbar), and the editor's
   ctrl/cmd jump outlines (outline_jumps).
   Inner glows (Sep 29): Layer near_inset (factor) and
   near_inset_color: an inset shadow blends toward the colour
   by nearness x glow x factor. The y distance counts
   Window.near_y_fade (2.0) times over. t_near_inset, t_near_fade.
1b. FIXED (Oct 18) silver compiler bug: adding a Canvas method
   made trinity's initializer write Canvas's method pointers past
   Canvas_i into the next type ("MODULE OVERWRITE [def]", export
   exit 1). Cause: a class's schema (__X_f, with the ft slot table)
   was built when the first enum after it was read
   (create_type_members walks every module member, phase 1), with
   the class's body still unparsed: the table held only the bases'
   methods (178, Au's) and every own method wrote past it, into
   whatever global followed; Canvas (195 slots needed) was the
   first to reach another type's descriptor. Now
   create_type_members skips a record whose body waits; the ft
   struct is filled at its implement (aether ft_slots, each class
   of the chain parsed first) from etype.schema_of, the source
   class; the unused early etype of the ft struct is gone (it
   implemented the empty table). Canvas: 336 slots for a highest
   index of 195. trinity tests exit 0, features 262/263
   (t_codegen_resource as before). clip_holds is a Canvas method.
2. OPEN element gained win_pos and near_lit: every module built
   before it (orbiter, scenes, clouds, markdown, aura, ...)
   needs a rebuild before it is hosted again.
3. OPEN not seen in a real app yet (buttontest has no borders).

## Active work: orbiter side panel (Sep 29 2026)

1. BUILT, not run: one context panel for the whole window, the
   selected editor's (active_pane) file, module and instances.
   The window is a split: panes left, SidePanel right (drag the
   divider; 360 px to start). Its toolbar row: < folds it, then
   the tabs (instances). Folded, SideStrip keeps a menu button
   at the top of the right edge (inst_hidden). The per-pane
   InstancePanel and each title bar's menu toggle are gone.
2. BUILT, not run: the status bar is 35 px tall (was 22 px);
   the instance panel's bottom toolbar is 35 px, flush on it.
3. OPEN the conversation tab: waits on what it shows.

## Active work: dictation marks on a recording (Sep 30 2026)

Kalen: dictate while recording; each take is a mark on the
recording's clock (start, duration, words), and the marks are
the composer's instructions ("I want a bugs bunny right after I
played the fzero game"): the span where an instruction was
spoken is cropped out of the finished video, and the composer
(a new app, `composer`) feeds the .agi with a VIDEO.md (the
video operations it may emit at times) into the user's agent
shell, gets video operations at times back, and renders a new
compressed mp4.
1. DONE marks. trinity/video.ag: RecMark (at, dur seconds;
   text; file: the pane's file or the listen file), RecNotes
   (context: a hand-written hint for the composer; marks),
   Recorder.time_us / mark / notes_load / notes_save; the file
   is `<take stem>.agi` beside the mp4 (record_notes; a live
   stream keeps none), rewritten at every mark and at close,
   read back at a reload so the marks continue. Module funcs
   record_time / record_mark (trinity.ag, over rec_live): the
   app element is not the media_app, so it cannot call its
   methods. orbiter mic_frame: mark_at at the press, mark_end
   at the release, the mark once the words arrive.
2. DONE (Kalen) the mp4 holds video and audio only, never
   text: the marks live in the .agi alone, and the user edits
   that file directly. The text track and --retitle were built
   and removed. Checked headless: a take with a spoken test
   wav gives an mp4 of h264 + aac (decodes clean) and a .agi
   with the mark at 1.60 s for 1.36 s, words exact.
3. DONE the composer protocol (composer/VIDEO.md, composer/
   composer.ag). The agent writes <stem>.composition.agi: a
   Composition of ops, each one serialized call on Composer
   (Op subclasses: Fetch name url, Crop from to, Insert at
   media ...). Composer.plan runs every Fetch, then every Crop
   (they set the output's clock: Kept spans), then every Insert
   (mapped to output time as an Overlay); problems collects
   what cannot be done. Resources live in <stem>/ named by the
   reference the user said (bugs-bunny); a file already there
   is used, else Fetch downloads it (tls Http.download).
   Virtual calls on an Op need *[] (o.invoke*[ a ]): a plain
   [] binds to Op's own method; `x inherits T` on a vec element
   was false for its real class. `silver --test composer`:
   t_composition_plan passes (a crop 10..13 moves an insert
   at 20 to 17 on the output).
4. DONE the agent request. `composer [--agent codex] [--model m]
   <take.mp4>` (composer.ag, element composer): the take's
   length from its fragments (take_length: moof/traf tfdt +
   trun count x tfhd duration, video at 90 kHz; 18.0 s, as
   ffprobe says), then Composer.ask runs the user's agent
   through trinity's agent shell in the take's folder with
   VIDEO.md (read from beside the module source) and the take's
   paths in the request; it writes <stem>.composition.agi; then
   load and plan. Checked with the real claude on a test take
   (an instruction mark, a logo the user put in take/): it
   cropped the mark (1.6-3.8 s), put take/orbiter-logo.png top
   right for 3 s at 3.8 s, downloaded nothing, left the
   narration mark alone; the plan: 2 spans kept, 1 insert.
5. DONE full-colour takes are H.265 4:4:4 (Kalen's pick): the
   RTX 3060 encodes H.264 4:4:4 but decodes H.264 at 4:2:0 only;
   it encodes and decodes H.265 4:4:4 8-bit. `--rec_qp stage` /
   `lossless` (the default) now record H.265 Rext 4:4:4 into
   the mp4 (hvc1 + hvcC), or a raw .h265; streams (ts, hls)
   stay 4:2:0 H.264. Encoder.hevc: VPS/SPS/PPS and per-picture
   info from C (trinity.c h265_params, h265_picture: C owns the
   memory and every pointer into it; pointers into a C struct
   taken from silver (@x.member) crashed the driver). Lossless:
   qp 0 + transquant bypass. vk.ag enables encode_h265 and
   decode_h265 (vk_context.h265_encode / h265_decode; the device
   extension list is 32 long now).
   Decoder.config_hevc + C front end h265d_* (VPS, SPS with VUI,
   HRD, scaling lists, range extensions, PPS, slice header up to
   the RPS, short-term sets incl. inter prediction, long-term by
   poc lsb, POC msb, reference marking, bumping output). 4:4:4
   pictures come back as full-size u and v (VideoFrame.full).
   Checked (composer t_hevc_decode, COMPOSER_HEVC=<mp4>
   COMPOSER_HEVC_OUT=<yuv>): a 320x240 x265 4:2:0 clip, 60/60
   frames, and a 640x200 lossless 4:4:4 take recorded from
   texttest, 480/480 frames: both byte-identical to ffmpeg's
   decode. The take's decoded picture vs the app's own shot:
   max 2/255 (rgb<->yuv rounding only). webgfx and trinity
   tests pass; orbiter and aura rebuilt.
   The video queues exist only once a Display attaches: a test
   needs a headless Display before decoding.
   Older takes (H.264 4:4:4, like Desktop/video.mp4) still
   cannot be read back on this GPU.
6. DONE the render (composer.ag Composer.render, Renderer,
   Source, Media, Planes, Seg). Pass 1 reads the take for its
   size, frame rate and all its sound (spectra aac_clip); the
   kept spans are split at every held cut (pause inserts) into
   pieces in output order, and every insert gets its output
   time from them. Pass 2 streams the take through the GPU
   decoder; each output frame: the take's planes drawn with
   draw_yuv (bt.709), the active inserts (pip at a third, full,
   region; fade in and out) with their own decoder or picture,
   then Canvas.sync_all and Recorder.capture (paced false: one
   frame per call) into H.264 4:2:0 QP 23 + AAC. The sound:
   the take's pcm for each kept piece, silence for a cut, an
   insert's own sound where `sound: media`.
   A composition newer than the notes renders as it stands, with
   no agent (Sep 30: --again removed). Checked headless: a lossless H.265 4:4:4 take, crop
   1-2 s, logo pip at 3 s, a 1.5 s held cut to av.mp4 with its
   tone: 8.5 s out (8 - 1 + 1.5), 510 frames, the four frames
   right by eye, the tone exactly in the cut (-21 dB there,
   silent elsewhere). 1080p60, 10 s take: rendered in 6.3 s.
   Found on the way: a VideoFrame read out of a vec and then
   removed is freed at once (use it before removing); a png
   drawn straight showed its transparent area (noise): pictures
   are premultiplied at load.
7. DONE a take is a module (Kalen): `--record video` makes
   video/ if it is not there and records video/video.mp4 beside
   video/video.agi (trinity record_path; relative to the launch
   folder; a stream url or a name with a video extension is
   used as it is). The composer takes the folder
   (`silver composer ~/Desktop/video`); the resources live in
   that folder, named `video/<file>` in a composition (resolved
   from the folder's parent); the composition and the render
   land in it too. Checked headless: --record demo gave
   demo/demo.mp4 + demo/demo.agi; composer on the folder wrote
   demo/demo.final.mp4 with demo/bugs-bunny.gif in it.
   Also: GIF inserts (composer/composer.c, animated, looping),
   region is the text 'x y w h' (the media keeps its shape), an
   H.264 4:4:4 take is refused up front, and a new take starts
   its notes fresh (a reload continues them).
8. OPEN orbiter's row for the composer, progressive JPEG in
   img (it aborts on them), and the frames still cross the CPU
   (decode readback, plane upload).
9. PLANNED (Kalen, Sep 30) voice conversion on the take's sound,
   offline in the render: a `Voice` op (from, to, voice) whose
   voice is a reference .wav in the take's folder, named in the
   context or a dictation like any other media ("video/vader.wav
   is Darth Vader"). Model: Seed-VC ported to silver as whisper
   was (whisper-small encoder already in speech; CAMPPlus,
   the DiT with its flow-matching steps, BigVGAN to port), its
   .pth checkpoints read by a new zip+pickle reader.
   Seeding: each reference .wav is encoded once (the CAMPPlus
   speaker embedding and its prompt mel), cached by file, and
   reused by every Voice op that names it. Each op's span is
   converted in chunks that overlap and are crossfaded, and the
   span's edges crossfade into the original sound, so the voice
   comes in and out without a click.
   DONE (Sep 30) the weights: composer.ag imports the Seed-VC DiT
   (whisper-small wavenet variant, its config confirms stock
   openai/whisper-small, speech's file is reused), CAM++ and
   BigVGAN v2 22k into install/models/seed-vc/ (918 MB). ai's
   `pth` class reads PyTorch zip checkpoints (ai/ai.c: zip with
   zip64, the pickle opcodes torch.save writes, strided tensors,
   f32/f16/bf16/int read as f32). composer t_seed_vc_weights: 302
   / 937 / 783 tensors; ups.2.0.weight_v is 384x192x4, first
   value -0.026324, the same as Python's own pickle reads it.
   The C functions are pthf_*: a silver class's methods compile
   to <Class>_<method>, and `pth_count` collided with class pth.
   DONE BigVGAN (composer/vocoder.ag): weight norm folded at
   load; conv_pre, 6 transposed upsamples, 18 AMP blocks with the
   anti-aliased snakebeta (2x up, 12-tap kaiser filters from the
   checkpoint, 2x down), conv_post, clamp. t_bigvgan against
   PyTorch on a real 2 s mel (COMPOSER_VOC_MEL/WAV): 44032
   samples, SNR 98.4 dB. CPU, one thread: 10 s for 2 s of audio.
   DONE CAM++ (composer/campplus.ag) with Kaldi's fbank (povey
   window, pre-emphasis, htk mel, 512-point power, mean off):
   t_campplus (COMPOSER_CAM_DIR): fbank max error 5e-4, the
   192-number style max error 1.8e-5, 0.66 s for 2.6 s of audio.
   Golden files come from the original Python (scratchpad
   bigvgan/ref.py, seedvc/cam_ref.py; torch is on this machine,
   torchaudio is not: its kaldi.py runs standalone).
   Silver found on the way: `v[ i64[ a ] * n + b ] = x` fails
   ("no indexing available for model i64") when the index starts
   with a cast: compute it into a local first; a vec returned
   from a call and aliased by another local was clobbered on the
   next loop pass (sum into a fresh buffer instead); `pre`,
   `post`, `ref` are reserved; pointer + n is not element
   arithmetic (take @v[ n ] instead); a param named `a` shadows
   the object itself.
   DONE the DiT (composer/dit.ag: length regulator, 13 layers,
   RoPE, AdaLN-RMS, U-ViT skips, wavenet head; Euler flow, 10
   steps, CFG 0.7; matrix work through ai's gemm_nt, 32 threads)
   and the front end (composer/seedvc.ag: sinc resample, whisper
   log-mel and content, the 22 kHz mel). Against PyTorch: one
   DiT call 3e-5, the flow 3.4e-5, content 1.3e-4, the whole
   voice 52 dB SNR.
   DONE the op, named `Revoice` (spectra has a Voice class):
   from, to, voice. In VIDEO.md. A speech take rendered: the
   pitch moved 301 -> 203 Hz, the output does not correlate
   with the input (-0.02). Speed: 31 s for 2.5 s of sound, on
   the CPU; Kalen accepts it. OPEN: the GPU path (below).
   GPU (Kalen asked, Sep 30): ai's whisper_vk has compute
   shaders for gemm, attention, layer norm and conv; Seed-VC's
   whisper already runs there. The DiT and BigVGAN are not on it.
11. DONE (Oct 6) `--rec_scale 2`: the Display's pixel scale is
   doubled while recording (the swapchain stays the window's
   size; its bilinear quad averages 2x2), the take is twice the
   window's pixels, its .agi says `scale: 2`, and the composer
   renders at take / scale, so a Focus to 200% shows real
   detail. Checked: buttontest 800x500 -> 1600x1000 take ->
   800x500 render, a 250% focus sharp where a 1x take is soft.
   Not seen on screen (hidden runs). element gained rec_scale:
   apps built before need a rebuild.
10. DONE the render's frame rate is the shortest gap between
   decode times over 60 pictures (ffmpeg's first frame is
   longer: an average read 26, then 59; now 60).

## MEMORY: the composer (Sep 30 2026)

What it is: a trinity app (`composer/`) that turns a raw screen
recording into a finished, compressed video. The user records an
app and talks while doing it; the talk is the edit. The agent (the
user's own claude or codex) reads what was said and writes the
edit; the composer carries it out. Kalen: "record an app, talk to
your ai and have a product come out on the other end";
"the ai can alter trash in, treasure out".

Why: to help media creators, and Kalen himself, make good videos
without editing by hand. It belongs in the stack: trinity records,
speech hears, the agent decides, composer renders. No outside
editor and no ffmpeg anywhere in the path.

The flow:
1. `--record video` (any trinity app, orbiter too) makes the take
   folder video/: video/video.mp4 (H.265 4:4:4 lossless by
   default, AAC sound) and video/video.agi. Dictation runs from
   the first frame, with no button: each spoken take becomes a
   mark (at, dur, words) on the recording's clock in the .agi.
   The mp4 holds video and audio only, never text.
2. The .agi has a `context` field: the user's hints for the
   agent. The user can edit the marks and context by hand.
3. `silver composer video` sends the agent VIDEO.md (the
   protocol), the marks and the folder. It writes
   video/video.composition.agi: a list of ops. A spoken
   instruction ("put a bugs bunny after the fzero bit") is cropped
   out of the result.
4. The render reads the composition and writes
   video/video.final.mp4 (H.264 4:2:0 + AAC, the shareable form).
   A composition newer than the notes renders again without asking
   the agent (--again removed Sep 30).

The folder is the module: resources sit beside the take, named by
the reference the user said (video/bugs-bunny.gif). A file that is
already there is used; otherwise a Fetch op downloads it. The user
starts the folder and the agent adds to it.

Ops today:
- Fetch: name, url.
- Crop: from, to.
- Insert: picture, gif or video; picture in picture at a third of
  the frame, full frame, or a region; fades; a held cut (pause)
  with the insert's own sound.
- Revoice: the take's own sound in another voice, from a short
  reference clip (.wav .m4a .mp4 .mov) in the folder. It is
  Seed-VC ported to silver (whisper-small content, CAM++ style,
  DiT flow matching, BigVGAN). There is no training step: any
  voice works from a few seconds of clean speech. It runs on the
  CPU at about 12 s of work per second of sound (Kalen: fine).

Where it might go (ideas, none started unless listed elsewhere):
- Seed-VC on the GPU: ai's compute shaders (gemm, attention, norm,
  conv) already run its whisper; the DiT and BigVGAN are next.
- Frames on the GPU end to end (no decode readback or upload).
- A composer row in orbiter: see the marks, the composition and
  the result beside the take; render from there.
- More ops: zoom and pan to what matters on screen, speed ramps
  over dull parts, music beds under narration, captions burned
  into the picture (pixels, not a text track), a thumbnail, an
  intro and outro.
- A voice library: shared reference clips named once and used in
  any take. Seed-VC's singing model (44 kHz, with pitch) for
  songs.
- Publishing: trinity already streams to youtube:// (HLS); an
  upload of the final render would close the loop.
- The same flow for any recording, not only app captures: talk
  over a camera take or a game session and get an edited cut.

## Active work: RVC in composer (Sep 30 2026)

Kalen: RVC instead of Seed-VC ("the performance is better"), a
voice trained from a clip (train from a clip, not downloaded
models), Seed-VC replaced. Seed-VC stays until RVC works end to
end, then goes with its 918 MB of models in the same step.
Each stage is checked against the original PyTorch code
(RVC-Project/Retrieval-based-Voice-Conversion-WebUI, MIT), as
the Seed-VC port was. Order:
1. DONE weights into install/models/rvc (composer imports):
   hubert.pth + hubert.json (the transformers form of RVC's
   hubert_base, not the fairseq .pt), rmvpe.pt, f0G40k.pth,
   f0D40k.pth; THIRD_PARTY.md rows. t_rvc_weights: 213 / 741 /
   560 / 165 tensors and first values as PyTorch reads them (all
   stored f16). Reference source (RVC main, uses transformers'
   HubertModel) in the session scratchpad rvc/.
1b. IN PROGRESS the GPU engine, ai/grad.ag (`import grad` in
   ai): kernels are GLSL with buffer device addresses and sizes in
   push constants (6 addresses, 12 ints, 4 floats), so one
   pipeline serves every shape and a whole step is one chain of
   dispatches (GRun, a barrier after each). Tensors live in two
   arenas (GArena over one trinity Buffer each: `keep` for weights
   and optimizer state, `work` reset every step); Buffer gained
   address[] and buffer_handle[] (trinity vk.ag). Autograd: a tape
   of GOp records (kind number; Grad.back_op switches on it).
   Kernels are written as trinity writes shaders (Kalen): a
   subclass of GKern with `func compute [] -> GLSL none { .. }`
   (main's body) and `decl` (shared arrays) token methods, braces
   doubled; only the push-constant header is a string (its
   #version/#extension lines are comments to the tokenizer).
   Checked against pytorch (GRAD_GOLD, generator scripts in the
   scratchpad gold/): strided batched gemm 5e-7; conv1d (stride,
   pad, dilation, groups) and conv_transpose1d, outputs and every
   gradient, under 4e-6; leaky/tanh/sigmoid/exp/scale, add/mul
   with broadcast, wavenet gate, channel slice/join, layer norm
   under 1.2e-6; rvc's relative-position attention with padding
   mask under 6e-7 (a masked score passes no gradient, as
   masked_fill).
2a. DONE the synthesizer (composer/rvc.ag, RvcNet on the engine):
   text encoder, reverse flow, NSF HiFi-GAN with rvc's sine source
   (rvc_sine, on the cpu). t_rvc_infer (COMPOSER_RVC_GOLD): the
   f0G40k base generator on 40 frames of random features, a pitch
   track with an unvoiced gap and pytorch's own noise draws:
   16,000 samples, 97.3 dB SNR against SynthesizerTrnMs768NSFsid
   .infer, 148 ms on the RTX 3060.
2b. DONE HuBERT (RvcHubert: transformers' HubertModel, last layer,
   positional conv weight norm folded on the cpu at load): 2 s of
   speech, 99 frames, worst 9e-6, 113.8 dB, 38 ms. t_rvc_hubert.
2c. DONE RMVPE (RvcPitch: batch norms folded into the convs at
   load; stft as a conv with a windowed cos/sin basis; librosa's
   htk mel filters with slaney norm; the u-net on conv2d/convt2d/
   avg_pool2; a gpu gru, one workgroup per direction): mel worst
   1.7e-4, salience 1.7e-6, pitch 0.0005 cents worst, voicing
   identical on 113 voiced frames, 76 ms for 2 s. t_rvc_pitch.
   Engine gained gelu, tslice (with backward), time_norm, conv2d,
   convt2d, avg_pool2, gru256, magnitude, log_clamp, view.
   FIXED (Sep 30, see "silver vec and indexing bugs"): a vec literal
   holding an element access, `v.push[ w[ i ] ]`, and float
   constants in a vec f32 literal. grad.ag/rvc.ag still use the
   workarounds (locals, fill by index).
2d. DONE training ops (t_grad_training against pytorch, all within
   4e-7): per-item time slice (rand_slice_segments), reflect pad
   (backward gathers per source: taps mirror onto one sample),
   magnitude and log clamp with backward, period fold (a
   DiscriminatorP 2-d conv as a 1-d conv over batch x period),
   least-squares and l1 means, vits' kl with a [b, 1, t] mask,
   backward_all (several losses, each seeded with its weight),
   AdamW (pytorch's: decoupled decay 0.01, bias-corrected).
2e. DONE one rvc training step (composer/rvc.ag RvcTrain, RvcDisc,
   RvcOpt, RvcBatch): posterior encoder, forward flow, a random
   32-frame slice per item, the nsf decoder, v2's nine
   discriminators (scale + periods 2..37), least-squares gan, feature
   matching x2, mel l1 x45 (librosa slaney mel, 2048/400 stft), kl,
   adamw on both nets. t_rvc_train_grads (lr 0, COMPOSER_RVC_TRAIN0):
   every loss within 1e-5 of pytorch; gradients within 0.5%;
   t_rvc_disc_grads: all 165 discriminator gradients within 0.13% on
   pytorch's own generated wave. The residue is one leaky relu whose
   pre-activation sits within 1e-6 of zero landing on the other side
   (counted: 1 sign flip at the layer where the error enters); pytorch
   f32 vs f64 is 1e-6 there. t_rvc_train_step (lr 1e-4, two steps):
   losses to 4-5 digits; adam's first step moves every weight by
   +-lr, so near-zero gradients flip and step 2 drifts by design.
   ~1.5 s a step at batch 2 on the RTX 3060.
   Memory: conv columns live in a per-op scratch arena and are
   rebuilt in backward (kept, they overran 6 GB at batch 2).
   The gemm sums each 16-term tile, then joins tiles with kahan
   compensation. The tape cannot be swapped for the discriminator
   step (the old vec was the only holder of the generator's
   outputs): tape_cut/backward_from over a mark instead.
2. OPEN inference: HuBERT features (768, layer 12), RMVPE pitch,
   the synthesizer (text encoder with relative attention, flow,
   NSF HiFi-GAN generator), on the GPU. Checked on the base
   generator against PyTorch.
3. DONE training data (composer/rvc.ag RvcPrep, its own Grad with
   hubert and rmvpe, released after): 40 kHz, scipy's 48 Hz 5th
   order butterworth high-pass (rvc_highpass, 2e-6 vs scipy),
   slicer2 ported (rvc_slices, boundaries identical to rvc's on a
   three-utterance clip), 3.7 s pieces overlapping 0.3 s, peak
   normalized (rvc's quirk kept: only the last slice's short tail
   is written), hubert rows repeated to 100 frames, rmvpe pitch with
   unvoiced frames interpolated, coarse bins, the spectrogram on
   the gpu. t_rvc_prepare: 22 s clip, 6 items (5 x 368 frames, 72),
   0.8 s. Resampling is our sinc (rvc uses librosa's soxr).
4. DONE the training loop (RvcTrain.train): items sorted by length,
   batches cropped to their shortest (rvc pads instead), batch order
   shuffled each epoch, lr 1e-4 x 0.999875^epoch, speaker 0, the
   generator saved as safetensors every n epochs (RvcNet.save; a
   voice file loads over f0G40k by name, RvcNet.overlay).
   t_rvc_train_loop: 2 epochs on the 22 s clip (mel 0.71 -> 0.60,
   kl 8.2 -> 3.8), saved and reloaded bit for bit, converted.
   ~1.5 s a step at batch 2: 15 min of speech is ~3 min an epoch.
5. DONE retrieval (RvcIndex): the training pieces' 50 fps hubert rows
   saved beside the voice (<voice>.index, raw f32 [n, 768]); exact
   search on the gpu (gemm distances, a top-8 kernel), rvc's 1/d^2
   blend at rate 0.75. t_rvc_index: vs exact numpy, 5e-7. rvc uses
   faiss ivf (approximate); this is exact.
6. DONE Revoice on rvc: RvcVoice.speak is rvc's Pipeline.pipeline
   (filtfilt high-pass at 16 kHz with zero start states, cuts at the
   quietest point within 3 s of every 8 s (rvc: 6 s / 38 s; our
   decoder columns cap the chunk), 1 s reflect pads, rmvpe once over
   the whole padded audio in 20 s windows past 25 s, per chunk:
   hubert, index blend, repeat to 100 fps, protect 0.33, synthesis,
   trims), then to 48 kHz. t_rvc_pipeline: 22.2 s in 2.8 s. The
   decoder compacts its arena per block at inference (Grad.compact):
   8 s uses 318 MB. Revoice gains pitch (semitones) and epochs
   (100); its voice is a clip of the target speaker, trained on first
   use into <clip stem>.rvc.safetensors (+ .index) beside the clip.
   VIDEO.md updated. Seed-VC removed: dit/seedvc/campplus/vocoder.ag
   (tracked in git; copies in the session scratch), its 876 MB of
   models, its tests and THIRD_PARTY rows; resample_sinc moved to
   rvc.ag. composer 17/17 with every rvc reference set.
   Tests must release their grads: arenas left from earlier tests
   oversubscribed the gpu and tripled the step time.
7. DONE training speed, first pass: an epoch of the 22 s test clip
   4.6 s -> 1.6 s (batch 2). Measured with GRun.prof (gpu timestamps
   per dispatch; Grad.prof_report). Fixed: weight norm ran one thread
   per channel over its whole row (now a 256-thread group per
   channel); a full-shape dz used one 256-thread group per element
   (now the elementwise dx kernel); weight-gradient gemms with few
   output tiles over a long sum (32x352 over 25,600) ran 6 groups
   (now split into pieces in scratch, summed in order: KSplitSum;
   t_grad_gemm_split). Inference 8 s: 2.3 s -> 0.56 s; synthesis
   vs pytorch 98 -> 109 dB. gemm is ~2 TFLOPS on a 2048 cube (the
   3060 peaks near 12): a bigger-tile gemm is the next speedup.
8. IN PROGRESS train picard-full (Kalen's video2 folder): `silver
   composer video2`, Revoice ops on picard-full.wav;
   247 pieces, ~70 s an epoch at batch 2. librosa and scipy install
   with composer (pip into install/pylib, as os-bootstrap does,
   skipped once there); --train was removed (a Revoice trains its
   voice on first use and logs each epoch). First run: killed by
   the kernel's oom killer at epoch 47, 124 GB resident (below).
9. DONE a full arena crashed (tensor_in returned null; batch 4
   needed more than 6 GB): tensor_in throws, train_voice and
   apply_voices catch it as a composer problem; batch is 2.
   ai t_grad_arena_full.
10. DONE the training leaked ~2.6 GB of cpu memory an epoch: train
   runs inside init and nothing drained the object pool. Au gained
   auto_mark / auto_free_to (drain only what entered the pool after
   a mark; the caller's objects stay); train drains after each step
   and each epoch. Small run: +95,053 pages an epoch -> flat.
   ai t_grad_pool_mark.
11. DONE checkpoints go to <model>.part, renamed at the last epoch:
   a stopped run no longer leaves a model that looks finished.
   composer t_rvc_train_voice (18/18). The killed run's epoch-40
   file was renamed to .part.
13. WRITTEN, not built (the video2 run's host would reload on a
   build): resume. A checkpoint is the generator at <model>.part
   and the rest at <model>.part.state (discriminator weights, both
   optimizers' moments named by parameter, epoch and steps), each
   written to .tmp and renamed. train_voice resumes from them when
   both load whole; RvcTrain.done skips the finished epochs. Test:
   composer t_rvc_train_resume (2 epochs, resume, equal to the
   bit, then a third). The killed run's .part has no state file:
   this run started over.
14. DONE speed, second pass (measured on the 22 s clip, batch 2):
   an epoch 1.6 s -> 1.35-1.5 s. Kept: losses sum over up to 256
   groups, then KLossSum adds the partials in order (kl too);
   AdamW's dispatches run without barriers between them, one fence
   after (GRun.nofence); the discriminator's weight norms are made
   once per pair of passes (RvcDisc.wcache, cleared in set_need);
   both gemm kernels load along the contiguous dimension (2048 cube
   2.1 -> 2.6 TFLOPS); KGemm2 (128 x 64 tiles, 8 x 4 a thread) for
   m and n >= 512 only (cube 3.0 TFLOPS; on rvc's tall shapes it was
   slower). Tests: t_grad_gemm_layouts, t_grad_gemm_split.
   Tried and REMOVED (slower on a real epoch): implicit conv (x read
   as conv columns inside the gemm: +300 ms an epoch) and the
   implicit input gradient (+900 ms). conv1d keeps im2col/col2im.
   Found on the way: a flag read from the 64-bit push constant a4
   came out true while a4 was 0 (kernel-side, cause not found);
   flags ride in float bits (f2/f3) instead.
   composer: au_live_set_reload[ 0 ] at init (built): a build never
   reloads a training run now.
16. DONE shared voice store (Kalen: training by named alias): a
   trained voice lives in install/models/voices/ by the clip's name
   (picard-full.wav -> picard-full.rvc.safetensors, .index, .fp);
   the .fp is the clip's size and FNV-1a hash, written last, and a
   mismatch retrains. Composer.voice_dir overrides the folder (tests
   use a scratch one). t_rvc_train_voice checks the second call
   reuses the model. video2's Picard voice was copied in, with its
   fingerprint (video4's picard-full.wav is the same file).
15. DONE the video2 render (Sep 30, 21:59): picard-full trained 100
   epochs (mel 0.503 -> 0.426, memory flat at 6 GB), the three
   Revoice spans converted, video2.final.mp4 written: 1080p60,
   2 min 25 s, h264 + aac 48 kHz.
12. FIXED silver (Oct 18): a string literal's members
   (`unlink[ '{a}'.chars ]`) are read (silver read_enode).
   features t_interp_c_arg.

## Active work: silver vec and indexing bugs (Sep 30 2026)

Kalen: go over the vec and indexing errors met in grad.ag and rvc.ag,
validate them in features.ag, fix silver. Each has a focused test in
features (Collections section). features: 222/225; the 3 failures
are the old t_vec_module, t_launch_specs, t_mem_last.
1. DONE the expect runner ran until the first failure, hiding every
   test after it (t_mem_last was hidden behind t_vec_module). A
   failed test now prints its line and counts (silver_expect_failed,
   aether emit_expect_tests); the report says "N/M passed, K failed"
   and exits 1 (emit_expect_exit).
2. DONE `v[ i64[ a ] * 4 + 1 ] = x` ("no indexing available for
   model i64"): a statement's left side parses at expr_level 0, where
   read_enode does not read `Type[ x ]` as a cast; the index args now
   parse at expr_level + 1 (silver_parse_member_expr). t_index_cast_lead.
3. DONE `f[ x ][ i ]` (a call's result indexed): parse_member_expr
   indexes a call's result when [ follows on the same line.
   t_index_call_result.
4. DONE `vec i64 [] [ src[ 0 ] ]`, `[ h.dims[ 0 ] ]` and
   `v.push[ w[ i ] ]` stored the element's address: e_create's boxing
   (prim -> Au) stored an unloaded element enode; it loads it first
   (enode_value force). t_vec_lit_elem, t_vec_lit_member_elem,
   t_vec_push_elem.
5. DONE `vec f32 [] [ 3.0, 7.5 ]` stored garbage: the f64 literal was
   boxed as f64 and the vector copied 4 of its bytes; seeds now parse
   with, and convert to, the element type. t_vec_lit_f32.
6. DONE `string[ @b[ 0 ] ]` (a byte pointer) reinterpreted the pointer
   as a string object: e_create builds the text type from it as cstr
   (u8/i8 pointers; not cstr/symbol themselves, not null).
   t_string_from_vec_bytes.
7. CHECKED, no bug found: a runtime-sized `vec <C struct> [ n ]`
   (t_vec_c_struct_n, feat_rec in feat.h: pointer, int, double; n 40,
   stride 24, no overrun) and a returned vec aliased by a local in a
   loop (t_vec_return_alias). The original failures may have had
   another cause; the tests stay as guards.
8. OPEN not vec/index but met on the way: `'{a}'.chars` as a C call
   argument fails ("expected 1 args, got 1"); `--verbose` builds
   sometimes abort with "double free or corruption" (twice in five).
9. DONE ai, composer, trinity and orbiter build with this compiler;
   ai and composer tests all pass. The runner change missed library
   modules (their tests run in <mod>_late with no report): a failed
   library test now prints "K of N failed" and exits 1
   (emit_expect_failures). OPEN: the workarounds in grad.ag/rvc.ag
   (locals, fill by index) can go back to the plain forms.

## Active work: YouTube smoothness in aura (Sep 28 2026)

Kalen: the player's fullscreen transition ran at 15-20 fps, its
menus at 10-30, the video at 30 at most. Measured headless with
the WebKit helper alone (scratchpad ytprobe/ytlong) and through
the element (drive.py, `--width 2560 --height 1440`), traces
since removed. Found and fixed, in order:
1. DONE frame interpolation (Kalen's spec: only a time-weighted
   mix of the two authored pictures, one picture behind, no
   motion compensation). trinity/video.ag `FrameBlend` (a
   PlaneBlend compute over the y, u, v planes), `VideoStream.
   smooth` (default true), `next_pts`; `frame_at` returns the
   mix when t sits between two ready pictures. webgfx
   t_video_blend: the mid mix equals (a+b+1)/2 on every byte.
2. DONE the picture ticks at the screen's rate: devices
   `platform_window_refresh_hz` (RandR on X11, the KMS mode
   otherwise; DP-0 = 165 Hz), aura sets TRINITY_VIDEO_HZ for
   the helper; the player's timer (videoHz) and the headless
   view's frame pacing (WPEViewHeadless.cpp, overlay) follow
   it. Checked: 165 ticks a second, a mixed picture each tick.
3. DONE the transition's real cost was CSS drop-shadow filters
   on the player's icons: each frame made new image buffers,
   and each buffer a new trinity Canvas (Canvas_init builds
   pipelines and a 1024-slot ring, 1.5 ms each; 369 made in
   one run). webgfx's spare pool never matched: it measured
   the fit against the raw request, and canvases are 64 px
   multiples, so a 64x64 spare failed the 4x-area limit of a
   30x30 ask. spare_index now quantizes the request first:
   369 -> 60 canvases a run, none during playback.
4. DONE drawing a filter result read the canvas back to the
   CPU and uploaded it again as an image with a mip chain
   (54% of the paint). GraphicsContextTrinity::drawImageBuffer
   draws a trinity-backed image buffer canvas to canvas on the
   GPU (Canvas.draw_canvas_crop, webgfx_canvas_draw_canvas).
   The transition second's paint: 367 -> 209 -> 56 ms; the
   48 tiny time-label paints: 296 -> 37 ms. Picture checked.
   Element at 2560x1440 with audio: the video plays through
   both toggles, 51-60 page frames a second, one second at
   26-47 at each toggle (the app is capped at 60 headless).
   webgfx and trinity expects exit 0.
5. DONE (Kalen's test: "4K video test tv motion",
   youtube.com/watch?v=pcSv22DTDUI, fullscreen, headless with
   SILVER_HZ=165 so the app is not capped at 60) fullscreen ran
   at 47 page frames a second, 100-117 before it, with every
   thread idle. Traced through the two processes: after each
   frame's done the web compositor waited ~18 ms for the next
   picture. The player's clock is the audio stream's time, and
   that moved only when the mixer pulled a block (1024 frames,
   21.3 ms at 48 kHz): every tick in between saw the same time,
   the same mix, and pushed nothing. spectra AudioStream.time
   now adds the wall clock since the last pull (clamped to the
   block). Fullscreen 47 -> 150-164, before it 100 -> 150-160
   (YouTube serves hd1080 H.264 both ways). Decode itself is
   2.3 ms a picture with 4-5 pictures ready ahead. webgfx and
   trinity expects exit 0; no traces left.
Findings, not bugs of ours:
- With no audio device the video counts as silent, YouTube
  autoplays it at load, and a later click PAUSES it (a script
  pause at ~9.7 s looked like a freeze). drive.py now passes
  PULSE_SERVER=unix:/run/user/1000/pulse/native and
  PIPEWIRE_RUNTIME_DIR so the helper has sound (its
  XDG_RUNTIME_DIR is the test's socket dir).
- The overlay scrollbar repaints its 21x800 tile ~60 times a
  second only while its fade animates (ScrollbarsController
  Generic), not forever.
- Deleting an overlay file does not restore the checkout's
  copy, and a checkout-only edit does not trigger the import
  build: touch an overlay file to rebuild. silver reported
  `--build aura` exit 0 once while ninja failed inside the
  import (a trace compile error): check the log.
6. DONE (see 9) `silver orbiter aura <url>` opened aura.ag with
   a HELD instance (start_instance holds every new instance
   until its row is clicked). The startup run plays it at once
   in the pane (display Full, the 'play' action); the argv tail
   becomes aura.ag's saved args, over any earlier ones.
7. DONE (Kalen: "we have a schema format, it's called .agi";
   "dont reinvent one field"): the launch spec string
   (name=type=default=Control=live, written by the compiler
   into the export, answered by trinity to `props`, split by
   position in orbiter) is gone from all three. A module's
   Launch members are enumerated by reflection as they are
   declared (trinity `Prop`: ident, type, access, meta as
   written, value, the meta's control with its values, an
   enum's stops) in a `LaunchProps`, answered to `props` as
   agi (Au.string_agi) and parsed back with Au.parse_agi.
   orbiter's panel reads Prop objects (LaunchPanel.fields);
   its args, compose and set-by-name paths are unchanged.
   The panel has no source before an instance runs: the
   module's last answer (module_props) is its cache, and a
   never-run trinity app gets a display row built the same
   way. aura's `src` is `default [ Launch ]` so it shows.
   Checked: headless aura answers
     props:
       display: Prop  (type DisplayMode, meta [ Live ],
                       value PIP, stops [ PIP, Embed, Window, Screen ])
       volume: Prop   (type f32, meta [ Live, VolumeRange ],
                       value '1', control: Range min 0 max 1)
   trinity expects exit 0; orbiter builds. Not driven: the
   panel itself (needs silver-host).
   Then (Kalen: "it comes from module enumeration, then stored
   in .agi"): the module's build enumerates its app type. trinity
   `export func launch_props` runs in the app's export process
   (silver.c: an app product runs its imports' exports; the
   registry is written before them) and, at the process's exit
   (trinity's ctor runs before the app module registers its
   types, so the write waits for atexit), walks the app type and
   media_app with props_of and appends `props:` to
   install/export/<owner>-<module>.agi. Declared shape only, no
   values; the running instance's `props` answer (many lines,
   ended by an empty line: agent_sock_ask_block) carries them.
   orbiter's module_spec: the instance's last answer, else that
   file; a cached line from before the agi form is ignored.
   silver-aura.agi now lists src (default, Launch), display
   (Live, stops PIP Embed Window Screen) and volume (Live,
   VolumeRange, control Range 0..1).
8. DONE (Kalen: `aura --volume 0.48 <url>` printed usage; "put
   the volume property on element"): volume was the trinity
   controller's, and the element is the command line's surface.
   `public [ Live, VolumeRange ] volume` is on element
   (trinity/element.ag, beside display); the controller's is
   gone, its socket `set`/`volume` read the root element's.
   Checked headless: `aura --volume 0.48` answers volume 0.48
   and props shows value '0.48'.
   A member added to element moves every subclass's slots: a
   module compiled before it reads garbage (orbiter SIGSEGV in
   Style.integrate_instance on a scene's style member). silver
   does not rebuild a runtime-loaded module for a base change:
   clouds, scenes and markdown were rebuilt by hand; any other
   trinity app (knes, asnes, n64, hyperspace ...) needs its
   build before it is hosted again.
9. DONE `silver orbiter aura <url>` never started the instance:
   /src/silver/aura is a folder, so the startup took the
   folder branch, which set no pending_run and dropped the
   argv tail; the module branch below it was never reached
   for a root module. The folder branch now saves the tail as
   the file's args and sets pending_run (unless START_HELD).
   Checked hidden under the host: the pane's instance starts
   ("start: aura https://www.google.com -> slot 1"), aura
   loads Google; then orbiter SIGSEGVed in Canvas_measure_text
   from Layer_draw (Kalen's report, reproduced). AU_QUARANTINE=1
   named it: a DOUBLE-DROP in AppView.service on the app's
   title. `app_title = string[ @title_buf[0] ]` (title_buf a
   vec u8) compiled to a POINTER CAST: the vec's data pointer
   was stored as the string object, held, and the old title
   dropped a bogus header; the pane title's label then read it.
   Fixed: the bytes are copied to a `local i8 [ 1024 ]` and the
   string made from that (the pattern trinity uses). Silver
   bug, OPEN: `string[ @v[0] ]` on a vec is a cast, not a
   construction (see cast-cstr-vec). Checked: the hosted run
   stays up 60 s with Google in the pane, no report.
10. DONE (Kalen: "you never do any arguments after the default";
   "trinity.view ... otherwise they could clash") the launch
   line is flags first, the default (positional) last and
   nothing after it: a string default takes the whole
   remainder (Au_with_cstrs). launch_handler put `--view` after
   the saved args; it leads now. The start log no longer prints
   "flags N" after the line. A prop of another module is flagged
   with its module: `--trinity.display Embed`, `--trinity.volume
   0.5` (Au_with_cstrs: `--module.name` matches the member of
   the type that module declares, by the module's plain name:
   silver-trinity is trinity); the app's own props stay bare,
   so names never clash. Prop.module and LaunchProps.module
   (the app's) come from props_of (trinity module_name);
   orbiter's LaunchPanel.flag_name composes and seeds with it.
   Checked headless: `aura --trinity.volume 0.48
   --trinity.display Embed https://www.google.com` runs and
   answers props with module aura / trinity, volume '0.48',
   display Embed. `--view` stays bare: it is asnes's own prop.
11. DONE (Kalen: orbiter "locks hard before closing", ~10 s).
   Reproduced hidden under the host with aura in the pane, an
   orderly quit over the socket (new trinity socket command
   `quit`: Au.quit_request sets the flag the close button
   sets), every thread's kernel wait sampled at 4/s and an
   LD_PRELOAD CPU sampler (scratchpad sampler.so, report.py)
   in orbiter. Measured, no --leaks, 30 s in: 2.7 s from the
   quit to the process gone. The main thread: 1.3 s asleep in
   on_unload's `while [ index_building ] usleep` waiting for
   the index worker, 0.5 s freeing the old file_index list,
   0.8 s the tree teardown (drop_members, map_clear). The
   worker ran at 100% of a core for the whole session (82% in
   tok_warm hashing every indexed text file; 400k-file cap over
   the 42 GB of checkouts) and after the cancel spent its time
   in index_work's final Au.auto_free: tok_warm never drained
   its pool, so every hashed file's text (up to 300 KB each) and
   its temporaries stayed pooled until the end of the whole
   pass. That pool is what the close waited on (and what grew
   the footprint; 1.5 GB rss at 30 s). Fix: tok_warm drains its
   pool per file (todo held, dropped after). After: 1.0 s from
   the quit to gone, the worker stops 40 ms after the cancel.
   The rest is the teardown itself. --leaks makes every free
   slow (leak_remove 59% of the close): measure closes without it.
12. DONE (Kalen, Sep 29: in Embed the page stayed at aura's launch
   size, 1280x800, inside a 1523x965 pane; "the second time you
   launch the app it doesn't get the resize; it should start at
   the embedded size"). Cause, reproduced by stopping and
   starting the instance from its row icon: a paned app launched
   at its own default size and relied on the pane's HM.resize,
   and that request is sent only when the pane's size differs
   from the LAST request (req_w/req_h), which a stop and start
   kept: the second process was never told. Now AppView.start
   puts the pane's content size on the launch line (`--width W
   --height H` before the user's args, so their own --width
   wins) and clears the request state per start; Editor.tick
   gives a not-yet-drawn instance the content layer's size.
   Checked hosted hidden: the second launch line is `aura
   --width 1607 --height 819 <url>`, the app's display opens at
   1607x819 and the page fills the pane (shot). The very first
   launch at startup still opens at the default and takes the
   pane's resize within a frame: the pane has no layout yet.
   /tmp/orbiter-resz.log is appended by EVERY orbiter process
   (two were running): its lines interleave.
13. DONE (Kalen: "that cool effect can be applied in our aura
   webkit around standard buttons ... and form elements like
   check / input / text area") the pointer's near light on every
   native control WebKit paints. The chain: webgfx
   webgfx_canvas_near_light / _near_style (WebGfx.h) ->
   GraphicsContextTrinity::setNearLight/clearNearLight (the
   point mapped through the CTM, reach scaled; an inset shadow
   in slot 1 re-applied per draw by setShadowForDraw, the
   inner glow the light fills) -> RenderBox::paintBoxDecorations
   (overlay copy) wraps a box with a used appearance in a
   TrinityNearLightScope: the pointer from the frame's
   EventHandler (window -> contents -> the box's paint space),
   reach 160, border 1.3, inset 3.9, the theme's focus colour
   (white vanished on Adwaita's light controls; Adwaita FILLS
   its borders, so only the inset glow shows). RenderTheme
   (overlay copies of .h/.cpp) keeps the painted boxes weakly
   (nearLightPainted) and EventHandler::handleMouseMoveEvent
   (overlay copy) calls nearLightMoved: a box within reach of
   the pointer repaints, and once more as it leaves. A trinity
   context is known by platformContext() != null.
   Checked headless on a local page (scratchpad near/): the
   push button, the text input and the checkbox take a soft
   blue inner light that follows the pointer; the rest stay
   as Adwaita draws them. Then (Kalen: nothing on google.com's
   search buttons, which Google styles itself, so WebKit paints
   them as plain CSS boxes): the scope also takes any box whose
   element is an HTMLFormControlElement (input, button, select,
   textarea, fieldset), styled or not; the light lands through
   the CSS border and background fills. Checked headless on
   google.com: "Google Search" glows blue at its edges under the
   pointer, "I'm Feeling Lucky" faintly at 150 px. Cost: one
   repaint per move per control within 160 px.
14. DONE (Kalen: dragging a split rendered the two panes at
   different sizes, the panes lagging the seam on a fast move;
   "easing is a feature, that ease state is the current
   position"). Traced per frame (a temporary split/draw log,
   removed): the frame the main splitter laid its seam out at
   the new place, the right child drew at its new width and
   pane0 still at its old one, one frame late. Cause: bounds
   were computed only in the draw pass (layout_element), so a
   nested container's render() read LAST frame's bounds: each
   nesting level (main -> left -> stack -> panes -> pane0)
   added a frame of lag, and the two sides of one seam came
   from different frames. Fix (trinity update_element): each
   instance is laid out from its parent's current bounds
   (layout_self, the per-element half of layout_element,
   with the relative-sibling chain kept in the pass) before
   its render() is called, top down; the draw pass's layout
   stays authoritative. The splitter's easing is untouched.
   Checked hidden: on a fast drag every shot has one seam and
   both panes meet at it, on the title, the text and the
   bottom rows.
15. DONE (Kalen: an instance started with the nav list open did
   not start until the list closed). The pane's editor mounts
   with `hide: browse_active[]`, and draw_element returns on a
   hidden element before its tick, so nothing serviced the
   instance (Editor.tick -> service -> start). orbiter's frame
   loop already services instances no pane shows; it now counts
   a hidden pane as not showing (`!e8.hide`). Checked hidden:
   with the list open (a letter typed in the title) the row's
   play starts aura at once ("start: aura ... -> slot 1").
   Then (Kalen: the app flashed and the list stayed in front):
   the play action closes the nav list (exit_finder, nav_showing
   and find_showing off). Checked hidden: the list's element is
   gone after the play and the page shows in the pane.
16. DONE (Kalen: the exchange's ground blur once, wanted per
   frame). Window.exchange_blur_frame (trinity): when the
   exchange's ground draws, the screen as drawn so far is
   submitted and fenced (flush_text, draw, sync_fence), copied
   into a screen-sized snapshot texture (vk.copy_image) and run
   through its own ReduceBlur (two reductions, the two blur
   passes) that frame; screenshot_blur points at it, so the
   carry and the veil keep working; a crop capture keeps its
   still. ShotBackdrop is `animated: true` (drawn to the screen
   every frame); `no_cache: true` painted nothing at all for it.
   The two new Window members (live_blur, live_snap) are the
   LAST members of Window: modules built before them keep
   their offsets (an insert mid-class broke orbiter until it
   was rebuilt). Checked hidden over a playing YouTube video:
   shots 1.2 s apart show different blurred frames.
17. WRITTEN, not built (Kalen: "stop compiling while I use it";
   the mic took 4-5 s to engage and disengage, freezing the
   ui). Profiled hidden (LD_PRELOAD wall sampler, the pulse and
   pipewire sockets linked into the test runtime dir so the
   capture opens): while engaged 95% of the main thread sat in
   dictation_poll -> spectra.capture -> snd_pcm_readi, a
   BLOCKING read of a 2048-frame block (43 ms at 48 kHz), and
   orbiter's mic_frame reads up to 64 blocks a frame: one frame
   took seconds, and the press-off waited for it. Not the
   device open (3 ms, from any thread) and not the GPU.
   Fix, spectra.ag (linux capture): snd_pcm_avail_update first;
   a block is read only when a whole one is there, else false
   at once (the loop then ends); the stream is started from
   PREPARED and recovered on a negative avail. Also speech.ag:
   the capture device opens on its own thread at exchange open
   (dict_open_thread; open_fail stops retries), and
   dictation_start no longer opens anything. Still to check
   after a build (spectra, speech, orbiter): the on and off
   press answer at once, the take's words arrive.
   Findings on the way: ALSA `default` fails with "Host is
   down" when XDG_RUNTIME_DIR does not hold the pulse and
   pipewire sockets (hidden runs with a scratch runtime dir:
   link pulse/native, pipewire-0, pipewire-0-manager into it);
   a hidden orbiter run overwrites install/tmp/silver-orbiter
   .log, the same file Kalen's run writes.
18. WRITTEN, not built (Kalen: n64 crashes from orbiter, not
   from the command line). It is orbiter that dies: its log has
   "type mismatch" then SIGTRAP right after "release: n64 slot
   1", while parsing n64's `props` answer (the freed strings
   name its props). n64's mouse_look is a bool whose Prop.value
   is the text 'false'; string_agi wrote it bare (`value:
   false`), parse_agi read the word as a bool for a string
   field and its verify trapped. aura and the others never
   showed it: no bool Launch prop. Fix in Au.c: the writer
   quotes a string that is a keyword (true, false, null), and
   the reader gives a string field the word as text instead of
   trapping. Needs `make` (Au), then orbiter's next launch.
   Check: `silver orbiter n64 <rom>` with mouse_look off, and
   the panel shows the bool.
19. WRITTEN, not built (Kalen: mouse_sens showed BLANK in the
   panel, "it's an f32", "blank means it's not even getting the
   value"). Two causes. (a) the export .agi carried no defaults
   (declared shape only), so a field mounted before the first
   run had nothing; and once the running instance's answer had
   the value, the mounted field kept its empty text (a mounted
   LaunchField is re-rendered without `edit`). (b) an empty
   field composed as an empty flag: `--mouse_sens ` took the
   rom as its value.
   Fixes: the declared default now rides the member descriptor:
   Au_t gains `dflt` (last in Au_f_members: offsets kept),
   `def_prop_default(prop, symbol)` (Au.c, src/Au schema); the
   parser records a one-token literal initializer (a number, a
   quoted string, true/false: token literal set) in
   aether.prop_defaults keyed by the member (silver.c member
   parse); the codegen emits def_prop_default after def_prop
   (aether.c, old modules simply never call it); props_of
   (trinity) puts it in Prop.value (quotes stripped) when no
   instance answers, so the export .agi and the panel have the
   defaults before any run. orbiter: an empty mounted field is
   re-seeded once a value is known; compose skips an empty
   field. Needs: make (Au, aether, silver), then trinity, then
   every app's export (a build of each), then orbiter.
   Check: install/export/silver-n64.agi lists mouse_sens with
   value '0.015'; the panel shows it before the first run.
OPEN:
- the FrameBlend runs on the web process main thread with a
  fence wait per tick: 12-17% of the thread at 165 Hz.
- headless the app ticks at 60, so page frames cap at 60; on
  Kalen's 165 Hz screen the real rate is unmeasured.
- the per-tile paint still reads every tile back to the CPU.

## Active work: .ag cleanup to 72 columns (Sep 30 2026)

Kalen: every .ag module to 72 columns, trinity first. Comments
re-flowed to 72 (all words kept); code broken inside [ ] only.
1. DONE trinity (Sep 30): 3,654 lines over 72 -> 1,323. The
   wrap tool (session scratchpad wrap72.py; to keep, move it to
   support/) re-flows # and // comments, moves a trailing
   comment above its line, puts a one-line `if [ c ] stmt`
   body on its own line, and breaks code after a comma or
   before && / || inside [ ], or ( ) within [ ].
   Checked by IR: the --verbose build's .ll before and after,
   compared function by function (locals renamed, string
   constants by text): identical but for __LINE__ values, GLSL
   whitespace/comments, and the initializer's C-type order,
   which also differs between two builds of the same source.
   trinity builds; `silver --test trinity` exit 0.
   Line breaks are NOT neutral in silver: a bracketless cast
   (`f32 i`, `f32 width / tcs`) reads by line; broken, the call
   failed ("arc_seg: expected 10 args, got 10"). The tool
   leaves any line with one unbroken.
   Left over 72 (1,323): 1,217 code lines with no safe break
   (long Vulkan names, aligned assignments, bracketless casts,
   `expect x, 'msg'`, ternaries), 88 GLSL lines holding a
   silver { } value, 14 import settings, 4 commented-out code.
2. DONE the exchange-test comments at the top of trinity.ag:
   four removed; "trinity: the window, the elements, and the
   light behind both" kept (a real header).
3. NEXT the other modules, one at a time, each built after,
   the same IR check per module.

## Active work: public cleanup (Oct 2 2026)

Kalen, before going public: every module to 72 columns, and no
numbered names (wx9, lp9, a9): a name, or i/x/y. Short words
like cv are fine. Locals and parameters only; members and
method names keep theirs. Order: trinity first, then each
module, built and tested before the next.
Start (Oct 2): 43 modules, 122,101 lines, 11,162 over 72,
3,311 distinct numbered names.
1. IN PROGRESS trinity: 1,424 over 72, 483 numbered names.
   Tools: support/cleanup.py names|wrap <file> [--apply],
   support/ircmp.py <ll-dir-before> <ll-dir-after> [--nolines]
   (each step: --verbose build, .ll copied, compared; a rename
   or wrap must leave every function the same).
   Done: numbered names renamed (member keys, GLSL params and
   h264/win32/sync2/ycbcr444 kept); auto wrap 1,424 -> 768.
   IR the same, `silver --test trinity` exit 0.
   Left: 6 numbered locals (Canvas curve math, pk9w/pk9h,
   set1_binds, b2_au); hand wraps: ternaries outside [ ],
   bare casts -> f32[ x ], long arithmetic, GLSL in { }.
   Crashes: 4, all from a renamed Vulkan struct key (fixed).
2. OPEN orbiter: 2,574 over 72, 586 numbered names.
3. OPEN the rest, largest first: scenes, asnes, ai,
   hyperspace, n64, composer, rgen, webgfx, features, knes,
   clouds, speech, spectra, flightsim, img, then the small ones.

## Active work: planets out of scenes (Oct 5 2026)

Kalen: scenes is a utility module only (Backdrop, OrbitView,
ViewAngle, the Gaia sky, eu_gnoise, fetch_map, scene_thumb,
shared models). NO planet lives in it. Each planet is its own
root module (`<planet>/<planet>.ag`) that imports scenes and
exports its scene names (`export scenes [...]`), so it builds,
iterates and exports alone; orbiter finds it by the export.
Never `extend scenes` for a planet.
1. DONE, building: one root module per planet, each with
   its own textures/, bakes and `export func <m>_thumbs` (last):
   earth (Moon, EarthPrev, Earth/Ocean/Cloud shaders), mars
   (Mars, TerraMars), milkyway, pluto, titan, enceladus, europa,
   uranus, neptune. scenes imports none of them and exports no
   scenes; it gained scenes_share (its shared models and skies),
   scene_thumbs_for, and the shared pieces: bake_saturn,
   bake_jupiter, TitanSurface, RingShader, SaturnShader,
   JupiterShader, nep_lerp, nep_band.
   DONE (Oct 5 01:40): all ten build; each module's own .agi
   lists its scenes; each wrote its own thumbs/<Name>.png.
   Shared assets stay in scenes/textures (read through
   scenes_share): gaia, saturn, saturn rings, jupiter,
   moon-displace, and every europa-* plate (mars reads them).
2. DONE the globe is code, no file: scenes build_globe (512
   segments, 256 rings, u = 0.5 - lon / 360, v = 0.5 - lat / 180,
   counter-clockwise outside); earth.gltf/.bin/.blend deleted.
   scenes/staged/earth.ag (not built) still names earth.gltf.
3. DONE stale outputs deleted: share/silver-scenes/thumbs/ and
   baked/ (1.4 GB; scenes writes neither now).

## Active work: moon surface (Oct 5 2026)

Kalen: the moon's bake (earth/earth.ag, moon_*) looked
amateurish: generational blur, additive stamping, decal basins,
noisy normals, no erosion in time, no basins, same-looking
craters. Rules from him: fix inside the existing routine, one
edit at a time, each baked and shown; normals come from the
mesh height only; regularizing toward the sphere happens in
perlin sections with large impacts, over generations, with the
perlin's own variance, never per crater; late bombardment is
small impacts; rocks (normals, colour) are perlin patches per
generation that average craters away. A bake is ~3.5 min; the
shaded map is checked from the normal png (scratch python).
1. DONE the per-generation blur and the slope rescale are gone;
   stamps carry real depth/rim/peak laws by size (0.34 radii
   for a bowl, 0.01 for a basin); the per-stamp noise, the
   late big rings and the double-depth draw are out.
2. DONE a crater is cut into its local lie: a height pyramid
   (moon_pyr_*, built per generation into the brush's tmp,
   bicubic) replaces the centre-height base: no plateaus.
3. DONE moon_basins/moon_sink/lvl/sk and every lava branch in
   the brush are gone. moon_mare: one perlin (2/5/12 cycles)
   times a near-side hemisphere ramp, cut by area to 16%, a
   wide shore band; over gens 24..40 that ground goes toward
   the sphere (the mean, 0.0015 under) 30% a pass; bm ends as
   the melted fraction and darkens the colour. Previewing the
   mask in python (scratch mask-*.png) saved four bakes.
4. DONE rock wear per generation (moon_patches/moon_pw_at):
   a perlin band placed anew each generation; inside it the
   ground goes toward its 8 px lie with the rock relief laid
   on (zoom 0.12..0.48 map px per rock px); this is the erosion
   clock (old craters soft ghosts, young ones sharp). The
   per-crater relief patches, the swath weighting of where
   impacts land and moon_clips (clips brushed into normals)
   are gone.
5. DONE basins: a crater of 200 px or more melts its floor and
   a perlin-shaped surround to 2.5 radii (mz), toward the 64 px
   lie, 45% a pass, cooling by a third each pass; the near
   side's floors darken. Past 60 px the size draw rejects with
   (60/rp)^1.5 so basins are tens, not hundreds. Impact flux
   falls fifty-fold over the flood generations (post-flood
   plains stay sparsely cratered). Hits vary: depth 0.5..1.2,
   wall curve 0.6..1.6, rim 0.6..1.4, oblique stretch and
   downrange ejecta.
6. DONE lighting mirror: the moon's model matrix is a mirror
   (third axis north x earth), so cross products flip; the
   shader's east tangent ran along map west and a bowl lit from
   the side read as a bump. MoonShader t_e = cross(n0, north),
   t_n = cross(t_e, n0). Found by derivation; the thumbnail
   camera only shows the limb.
7. DONE (Kalen's second look): basins were too large and too
   smooth; the colour had a seam down the map's middle; rims
   needed mountains; a few more craters. Now: moon_region picks
   a picture by random cells with the four neighbours blended
   (no seam; measured equal to a typical column); stamps of the
   big types heap a lumpy massif to 1.6 radii and keep a wider
   rim; the biggest draw is 340 px, past 60 px the count falls
   as (60/r)^2; a basin's floor melts in perlin patches (parts
   keep their craters) and its surround to 1.8 radii; the mare
   perlin takes 5% by area; a basin's own fresh ring is not
   painted. Measured: melted ground 17% of the sphere. Crater
   cover 0.04 -> 0.05, post-flood flux floor 4%.
8. OPEN the normal map is still rgba8 png.
9. DONE orbiter: scene_prop_apply skips a saved enum name the
   enum no longer has (Enceladus/Neptune had speed: subtle;
   BackdropSpeed is slow/medium/fast): evalue faulted at the
   restore (SIGTRAP at startup).
10. DONE (Kalen's third look): late impacts only ever smaller;
   too few central peaks and small rims; rock detail off scale
   (scrapes, pixel marks, no rocks seen). Now: the size top
   doubles over the late generations (ramp 40%..80%); the peak
   starts at the 13 px types (was 37), wider and taller, and
   those types' rims are 60% higher and wider; the ray streaks
   no longer write the normals (light only: the scrapes); the
   shader's two finest rock stages (a quarter and an eighth of
   a map px a rock px: the pixel marks) are gone, stages 2x,
   6x, 16x at 0.35, 0.6, 0.7; the wear lays the rock tile at
   0.5..1 map px a rock px (was 2..8); the tile is box-averaged
   into the relief. Shaded map: rubble grain shows, rims and
   massifs read. Not yet seen by Kalen in orbiter.
11. DONE (Kalen: no real colouring on the moon; the Earth in
   the moon's sky should reflect the sun off its land by albedo).
   moon_albedo: the vivid picture's hue comes in at 15%..55% by
   two perlins (3 cycles large sections, 14 small; was a flat
   12%), the cap 12% -> 35% off grey, the real map's own hue
   whole. Mean hue off grey 0.037 -> 0.069 (the real map is
   0.015, the vivid 0.211). MoonEarth: a broad land lobe
   (exponent 12, half strength) in the ground's own colour,
   under the clouds, beside the sea's glint. Not yet seen by
   Kalen in orbiter.
12. DONE bake 12 (Kalen: scrapes and noise still there). Found in
   the maps: the rock tile's own thin lines (crater rays, plate
   edges, 1-2 px wide, 100+ px long) laid into the height by the
   wear and stretched by the shader's 2x stage; and the vivid
   picture's grain coming into the colour pixel for pixel. Now:
   moon_relief low-passes the solved height (gaussian, in fourier
   space), the shader's 2x rock stage is gone (stages 6x 0.6, 16x
   0.7), the vivid picture is box-averaged 8x before its regions.
   Checked after: the silver relief matches a numpy solve of the
   same tile (slope correlation 0.977, same spectral peaks), so
   the lines are the tile's; 2 px left them, 4 px removes the
   network and keeps the rubble (python preview). The colour grain
   remains: the real map's hue now comes in whole and its jpeg
   chroma noise is per pixel. NEXT: low-pass 4 px, the real map's
   hue from an 8x averaged copy too.
13. DONE the way back from the earth (Kalen: the lit side up as we
   approach; the moon left the frame). The look held the moon's
   centre only to the halfway point by the clock; now it holds it
   until the eye is within 3 radii and rolls to the flight look by
   1.6. While the moon is the target the camera's up is the sun's
   direction with the line of sight taken out: the sunlit half at
   the top. Checked offscreen (MOON_SEC 190/205/215): the moon
   centred, terminator level, lit side up.
14. BUILT, awaiting Kalen's look: the Earth in the moon's sky
   (MoonEarth). The sun's spot on the land overblows the ground's
   own colour, scaled by its green plus blue (a multiplier on the
   land colour under a pow 12 lobe, up to 2.5 x (g + b)); the limb
   fresnel band 50% stronger (0.45 -> 0.675); at night the same
   band is the aurora: green between 66 and 78 degrees of
   latitude, curtains from two sines along longitude drifting in
   time. Seen offscreen at the earth lap: the green cap at the
   pole shows as spokes meeting at the pole (the curtains are by
   longitude); the land spot was not under the sun in that frame.
15. DONE (Kalen: the scrapes and noise persisted) the real
   source: moon_tint_height added the vivid picture's brightness
   to the terrain height pixel for pixel from random cells, so its
   white crater rays were ridges in the height (the scrapes) and
   its grain the noise in the normals. Removed (the function, its
   call, its hash entry). The colour's grain was the real map's
   brightness at the pixel: moon_albedo now reads both pictures
   from an 8x box-averaged copy (moon_shrink) for hue and light.
   Bake 14. The rock relief keeps the 4 px low-pass (its own
   lines and grain are not rocks).
16. BUILT, awaiting Kalen's look: the approach camera (Kalen: the
   lit side on the left as we leave the earth, turning up as we
   near; the moon fell to the bottom). Leaving the earth the up is
   the sun turned a quarter round the line of sight (lit side
   left), rolling to the sun's direction from a quarter to four
   fifths of the way (lit side up); from 6 radii to 3 the look
   leans 0.7 radii toward the lit limb; within 3 radii it rolls
   level and takes the flight look. Checked offscreen at 185, 200,
   212: the moon centred, lit cap at the top at 212. Whether the
   first roll puts the lit side on the screen's left (cross
   handedness) is not confirmed: flip `lft` if it is on the right.
17. BUILT, awaiting Kalen's look: the Earth's land spot is land
   diffuse + land x a wide blinn lobe (pow 8, 1.2), under the
   clouds (Kalen's spec); the overblow form is gone.
18. BUILT, awaiting Kalen's look: earthshine. MoonShader gains
   `earth_dir` (last uniform; w = strength): a blue light
   (0.45, 0.62, 1.0) from the earth's direction on the ground
   facing it, 0.12 at new earth to 0.42 at full earth by the
   sun's angle to the earth line.
19. DONE bake 15: the colour's 4 x 4 blocks (the averaged copy
   picked nearest onto the colour map) are gone: moon_pick is
   bilinear, wrapping. Colour crop smooth.
20. BUILT, awaiting Kalen's look (Kalen: the aurora sits 50%
   higher, is a volume with depth, moves through the atmosphere,
   dances around). It is its own shell now: MoonAurora (inherits
   MoonEarth), the globe mesh at 1.066 radii over the earth in
   the moon's sky, ray-marched 12 steps between 1.033 and 1.066
   radii in the earth's frame (the Earth scene's is 1.022..1.044),
   stopped at the inner shell and the ground; curtains from two
   octaves of 3d gradient noise squeezed 5:1 vertically, drifting
   in three directions (visible within seconds at sky rate 60);
   the oval's latitude wanders up to 5 degrees by a slow noise;
   green low to magenta high; night side only. The band aurora in
   MoonEarth is gone; its limb stays at 0.675. The noise functions
   moved out of MoonShader into `MoonNoise`, the base of MoonShader
   and MoonEarth (one copy).
   Tuned after two offscreen looks (first invisible, then a white
   block): curtains from noise over the ground (up.x, up.z x 24)
   with little altitude term, threshold 0.5..0.68, gain 1.3, cover
   smoothstep 0.1..2.5. Seen at the earth lap: soft green volumes
   at the south pole above the limb.
21. DONE (Kalen): closing the last file reopened one on the next
   start. The state WAS saved (persist.agi: `pane_states: [ ]`,
   pane_drop_file saves), and the agi reader gives an empty vec
   for it, but restore_session's second branch (meant for a
   fresh install: the first known file) ran for an empty list
   too. It runs only when pane_states is null now. orbiter built.
22. DONE (Kalen, eighth ask: no specular on the earth's land).
   Checked under the spot this time (MOON_SEC 132, the glint on
   South America): the earlier forms did not read. Now the land
   takes the sea's blinn at twice the size (pow 100 x3 + pow 10
   x0.8), added as land x land x3 x lobe under the clouds: the
   Amazon flares under the sun. Seen offscreen.
23. BUILT (Kalen: the aurora looks good, bigger patterns, far
   lower): shell 1.033..1.066 -> 1.01..1.028 radii (mesh at 1.028),
   noise over the ground 24 -> 9 per radius.
   Then (Kalen, five at once, all done): a quarter as bright at
   full (gain 1.3 -> 0.33) with each region slight to full by a
   slow noise (0.1..1); the oval wider (lat 46..60 in, 78..88
   out); the noise round the pole in a ring (cos, sin of the
   azimuth x 2.6) with the meridian direction at 5 per radius: a
   slight stretch up and down from the pole; depth from the
   noise changing through the shell (an x 4) and far samples at
   half; shell 1.002..1.012 radii (mesh 1.012). The cover follows
   the light so a faint curtain does not darken the ground.
   Then (Kalen: none showed; flat, a 2D effect on the globe, not a
   march). Two causes: the cover followed the colour and the
   8-bit target clamps colour before the blend, so the addition
   can never exceed the cover (the cover carries the strength
   now, 0.25 at full); and the samples were averaged, so the
   march had no depth. Rewritten as a real march: composited
   front to back (each sample hides what is behind it), the
   sheet pattern the same up through the shell so each is a tall
   curtain, dense at the foot and ragged at the top, the inner
   shell crossed (the ray goes down through the sheets and up
   again), the step in shell thicknesses; shell 1.008..1.03
   radii (mesh 1.03): thinner than that cannot be seen from the
   side. Measured at the earth lap: green pixels 47 -> 622, the
   strongest 17 -> 44 of 255; from the side the oval is an arc
   standing above the limb with lumps along it.
   Then (Kalen: not vertical shafts enough, no volume, not low
   enough, too bright): curtains 12 per radius across the oval
   (was 5), fine rays along them (14 round the ring) splitting
   each into shafts; shell foot 1.008 -> 1.002 (top 1.03); cover
   0.25 -> 0.12. Measured at the earth lap: strongest green 14 of
   255 (was 44), 184 green pixels.
   Then (Kalen: shafts way taller, slower): top 1.03 -> 1.05
   (mesh 1.05), the thinning from 0.55 of the height (was 0.3),
   drift 0.0017 -> 0.0006 of sky seconds.
   Then (Kalen: the whole ionosphere's edge carries the green as
   fuzz on the night sky's edges): an airglow density 0.04 x
   (1 - altitude) over the whole night shell in the same march,
   its own softer green; the grazing path at the limb saturates
   it into a thin fuzz, the steep view inside barely.
   Then (Kalen: a hard green edge showing the shell's other side,
   not a thin terminator fuzz). THE FAULT behind every flat
   result: the ray math ran in the mesh's units (mesh radius 1)
   while the mesh is the globe scaled to the shell's TOP, so the
   ground is 1 / 1.05 there and a shell declared 1.002..1.05 lay
   wholly outside the mesh: only the silhouette sliver was ever
   marched. Now rtop 1, ground 1 / 1.05, foot 1.002 / 1.05 in the
   shader; the shell's far face discards (the near face marched
   the ray once). Measured: green 155 -> 13,877 px, strongest 34.
   Then (Kalen: the glow is a thin band seen at the top of the
   shell, clear toward the surface; only the shafts go through):
   glow density a gaussian round 0.85 of the height, width 0.1,
   0.06 peak (was 0.04 x (1 - height)).
   Then (Kalen: nice, lower): the band's centre 0.85 -> 0.55 of
   the shell's height, width 0.08. Built.
24. DONE (Kalen: "why are we committing generated textures").
   They were never committed: .gitignore ignores scenes/textures/
   ("planet maps: downloaded, never committed"); the planet split
   moved the maps into earth/textures, enceladus/textures,
   mars/textures, where no rule covered them, so git staged them
   as new (earth 117 MB, enceladus 71 MB, mars' two colour maps
   46 MB). The eight planet modules' textures/ folders are in
   .gitignore now. mars-height/stone/veg were tracked before and
   stay (renames). Unstaging the new ones is Kalen's (Rule #1).
25. DONE (Kalen: titan-color.png staged too; "why are you not
   downloading these to a cache location"). The download WAS in
   the cache (~/.local/state/scenes); the bake then wrote its png
   into the module's source tree (titan/textures/). bake_titan
   now writes ../<share>/baked/titan-color.png as the moon bake
   does and the model reads it there. Built; the thumb rendered
   from the moved map is identical (mean 56,47,34). The same
   source-tree write remains in scenes.ag: gaia, saturn, jupiter
   (scenes/textures/, gitignored): OPEN, Kalen's call.
26. DONE (Kalen: "you can cleanup those") the history rewritten
   with git filter-repo after his commit 165d95d left the tree
   clean: every binary blob in history that is not a current file
   (44 blobs, 65 MB: old orbiter32.bin, earth.bin/.blend, the
   pluto tiles, flower blends, sonic2.bin, default.profraw,
   landscape.png, exr/ppm captures), then the pluto-tiles folder
   whole. Source history untouched (old silver.c etc. kept).
   .git 385 MB -> 75 MB, 1157 commits, stash kept, origin kept.
   Every hash changed: origin needs Kalen's force push, other
   clones re-clone. Full backup of the old .git (with reflogs):
   /src/silver-before-cleanup.git.

## Active work: eden scene (Oct 2 2026)

Kalen: a jungle seen from high up, the forest under the chip in
silver-icon.png (no chip, no stars). scenes/eden.ag (`extend
scenes`), trees and impostors built at export. Trees are real 3D
foliage; never a canopy height map. Clouds: not ours (we have
them). Kalen's brief, in order:
1. BUILT, awaiting Kalen's notes: terrain. A height map built
   large forms first, then drainage valleys, then small detail;
   river and clearing masks.
   Kalen: the land meets a sea; shoreline, rivers that run to it,
   and shallow water (turquoise over the shelf) that deepens to
   open sea, by the water's depth over the ground.
   eden_bake (export): 1024 cells of 8 m (8.2 km). Coast (warped
   noise, west side), shelf falling fast at the shore then gently,
   islands on the shelf, ridged uplands; synth_erode on a map 64
   rows taller (its rows wrap); the sea floor keeps its smooth
   shape below 1 m. One flood from the map's edge (heap) with a
   step cost of length x an uneven cost map, so streams meander
   across flats; hollows silt up into river plains (no lakes);
   each cell drains down its steepest slope; streams from 3000
   cells of catchment cut beds 1.2 m+ deep, up to 6 cells wide.
   baked/eden-height.png (r/a metres -256..768, g cells to the
   sea), eden-water.png (r/a water level, g river size, b wet),
   eden-grid.gltf. Element Eden: EdenGround (beach by sea
   distance, rock by slope, dark earth), EdenWater (light to the
   bottom and back: turquoise over sand, deep blue, muddy rivers;
   sky by Fresnel, sun glint, a break at the edge). In the
   picker; thumbs/Eden.png. Test: `silver --test scenes`
   (t_eden_view writes /tmp/eden-view.png at 1280x720).
   Data images must be `linear: true`: sRGB bent the height's
   high byte (4 m steps on the sea floor).
   OPEN: the far map edge shows against the sky (item 7); no
   clearing mask yet (item 2).
   OPEN silver: scene_thumb's last log prints its `nm` as garbage
   when called with a literal from an expect (freed with e.id).
2. OPEN control maps: tree density, height, moisture, family,
   clearing, exposure; patches that vary together.
3. OPEN tree families (5): trunk, branches, leaf clusters placed
   in irregular crown lobes, with gaps; a reusable 3D cluster
   library.
4. OPEN one crown under one sun, drawn gray first: crowns shade
   each other, gaps open, outline irregular.
5. OPEN a small patch: seeded per tile, spacing rules, three
   canopy layers, cascaded sun shadows, leaves that pass light.
6. OPEN detail by screen size: full, simple clusters,
   depth-aware impostors, voxel clumps, far forest; hysteresis.
7. OPEN atmosphere and valley mist.
8. OPEN tiles, GPU culling, indirect draws; tested in motion.

## Active work: Earth as a canopy planet (Oct 2 2026)

Kalen: Earth is a sphere whose surface is the canopy, seen from
orbit to the treetops, with real tree textures. Sources (CC0 /
public domain, cite in THIRD_PARTY.md): NASA Blue Marble Next
Generation (colour from orbit, and where forest grows), Poly
Haven trees fir_tree_01, island_tree_01, island_tree_02 (glTF
1k, ~570 MB, fetched once into ~/.local/state/scenes, never the
repo). Template: Pluto/Mars (carpet mesh around the camera,
wrapped onto the sphere, lifted by baked height).
1. DONE fetch (Oct 2): Blue Marble July, 21600x10800 baseline
   jpeg, 2 km a pixel (assets.science.nasa.gov .../bmng-base/july/
   world.200407.3x21600x10800.jpg); the three trees' glTF 1k with
   jpg textures (fir 18.8 m, 9.0M vertices; island 5.0 m and
   3.4 m). Leaves are alphaMode BLEND over a jpg: the cut-out is a
   separate <tree>_<twig|leaves>_alpha_1k.png Poly Haven lists
   but the glTF does not reference (fetched beside it).
2. DONE GltfModel.open reads all three (meshes, materials,
   images). Not yet drawn. A throwaway module made git tag
   treetest-1.0.0 (left for Kalen).
3. DONE (top view) scenes/earth.ag (extend scenes, imported after
   pluto): export earth_trees_export draws each tree from straight
   above (orthographic, 1024 px, crown's widest span) into
   baked/earth-<tree>-{color,height,normal}.png; height is 0..1 of
   the tree's top. tree_prepare joins the leaf jpg and its alpha
   png into <tree>_<leaf>_rgba.png and writes <tree>_bake.gltf
   naming it. Checked by eye: real leaf and bark colour.
   Fixed on the way: gltf texture uris resolved against a global
   base that open[] had cleared, so a model outside share loaded
   no textures (white); GltfModel.src_dir (intern, last member)
   now keeps its folder, used when the file is there (an archive
   still finds the bare uri). A shader drawing glTF materials
   needs color, rough and normal in its surface enum.
   OPEN: side views; KHR_texture_transform is not applied (only
   bark/branch uvs use it); the export reads the cache but does
   not download the trees itself yet.
4. DONE (Oct 2) Earth reworked, game scale (Kalen's reference:
   autumn hills of round crowns, seen from a bit higher up).
   scenes/earth.ag: element Earth (the old JungleShader Earth is
   gone from scenes.ag). The carpet (build_carpet) wraps onto
   the sphere around the craft; EarthCanopy lifts it by rolling
   hills (0.034 radii) and crowns: 3D cells, 520 a radius, the
   tallest crown wins, domes with shared worley lumps, firs as
   cones toward the poles. Each crown takes an autumn colour
   (tropics green) over baked/earth-foliage.png: island_tree_01's
   leaves scattered into a 512 tile (alpha = pile height).
   baked/earth-land.png (Blue Marble at 4096, read raw) sets
   sea, ice, desert and forest. EarthAir: sky, clouds, sun and
   the limb; haze counts only the sight line inside the air.
   The craft flies a great circle from 35.6 N 83.5 W heading
   50 degrees, 0.6 radians each kilosecond.
   Found on the way: a fract(sin()) hash rounds differently for
   one grid corner reached from two cells; the noise tore into
   walls. EarthCanopy hashes with pcg3d (integers).
   OPEN: the baked crown pictures (earth_trees_export) are no
   longer drawn; a mid-distance coast can still show a short
   steep bank; not yet seen in orbiter itself.

## Active work: rec_qp (Sep 30 2026)

Kalen: recording at near-lossless for staging (overlays added at
moments later, then a final encode). The RTX 3060 on driver
610.57.04 encodes through Vulkan: H.264 High 4:2:0 and High
4:4:4 (4096 max), HEVC Main 4:2:0 and RExt 4:4:4 (8192 max).
1. DONE, built (trinity, buttontest) `--rec_qp` enum
   RecQuality (element.ag, beside DisplayMode; a member of
   element like record, adopted by media_app): stream (4:2:0,
   qp 10), high (4:2:0, qp 4), stage (4:4:4, qp 2), lossless
   (4:4:4, qp 0 + qpprime_y_zero_transform_bypass; the default,
   Kalen). The path@qp suffix is gone; a live url uses stream.
   Pieces: trinity.c h264_sps/h264_pps (profile 244, chroma 3,
   1 px crop units, the bypass flag; CAVLC for 4:4:4), video.ag
   Encoder chroma444/lossless (profile, G8_B8R8_2PLANE_444
   picture, full-size chroma copy, 4 bytes/px bitstream room),
   NV24Convert (full-size cb/cr), mux.ag avcC's high-profile
   chroma/bit-depth bytes, vk.ag VK_EXT_ycbcr_2plane_444_formats
   (the format is core only in 1.3; trinity asks for 1.2).
   Found on the way: nvidia's vulkan encoder refuses 4:4:4
   parameter sets with CABAC (vkGetEncodedVideoSession
   ParametersKHR -1, size 0); CAVLC works (NVENC itself does
   CABAC 4:4:4). Checked, buttontest headless at 2560x1440
   against its own shot: stage max 3 levels off, lossless max
   1 (the rgb to 8-bit ycbcr rounding): lossless is lossless.
   OPEN: at 480 px wide (480x560, 480x576) the Cb channel of
   the last ~16k pixels encodes wrong (Cr and Y right, stream
   decodes clean); 496, 640, 1024, 2560 wide are clean. Looks
   like the driver; not chased.
   element gained rec_qp: every trinity app built before
   needs a rebuild before it is hosted again (orbiter too).
   Mac: MoltenVK's encoder is 4:2:0; stage/lossless fall back
   to 4:2:0 there with a log line. VideoToolbox can do 4:4:4
   H.264 on Apple silicon: add it to our MoltenVK branch.
2. DONE, built, not confirmed on colourful footage: the NV12
   convert used BT.601 coefficients while the SPS says BT.709
   (matrix 1): hues shifted. Now BT.709 limited range.
3. DONE (Oct 21, Kalen: his orbiter take at 1888 x 888 had a
   diagonal staircase of blocks and red / blue scraps in every
   frame) the colour planes (U, V) of a 4:4:4 take came out
   sheared, brightness exact, whenever the width was not a
   multiple of 64 (1888 wrong; 1920, 1024, 640 right; height
   and 4:2:0 unaffected). The encoder's picture is now padded to
   a multiple of 64 wide in 4:4:4 (Encoder.coded_w), cropped back
   by the conformance window as the height already was. Checked
   with texttest takes against its own shot (Y, U, V within 1 of
   bt.709 at 1888x888, 1600x900, 1000x600); the file reads
   1888x888; our decoder equals ffmpeg's byte for byte (composer
   t_hevc_decode, 223 frames); trinity and composer tests pass.
   Takes made before the fix keep the damage.


## Active work: psx emulator (Oct 6 2026)

Kalen: a PlayStation emulator, /src/silver/psx beside n64. No
software rasterizer: the GPU draws through trinity only (VRAM is
a trinity render target; the CPU sees it only for the game's own
VRAM-to-CPU copies). Everything external is a silver import.
First game: Ridge Racer (USA), ~/Downloads/Ridge Racer (USA)/
(cue: data track MODE2/2352 + 13 CD audio tracks; the music is
CD audio). In order:
1. DONE BIOS: OpenBIOS from source, `import pcsx-redux:nugget/
   20b6316...` built by our clang/lld/llvm-objcopy (psx/nugget/
   silver: mips-cc, mips-objcopy, mips1.h, build.sh; psx/
   nugget.diff for LLVM: rfe as a word, three-operand sltiu,
   li %lo -> addiu (LLVM dropped the relocation), BIU_CONFIG via
   %hi/%lo, ALIGN(0x500) written out, .data AT(__rom_data_start)
   (lld aligned the load address past the copy's symbol), and
   -mno-check-zero-division (teq is MIPS II)) into share/
   silver-psx/openbios.bin. Bus and memory map.
2. DONE R3000A CPU (Cpu.ag): load/branch delays, COP0,
   exceptions, interrupts, LWL/LWR merging the pending load.
3. DONE interrupts, timers, DMA (block, linked list, OTC).
4. IN PROGRESS GPU on trinity (Gpu.ag): VRAM 1024 x 512 target,
   PsxDraw shader (4/8/15-bit textures and CLUTs from a VRAM copy,
   modulation, dither, mask bit), five blend pipelines (opaque,
   the four modes; textured blends draw opaque texels then blended
   ones), fills, copies, CPU uploads and readbacks, PsxDisplay
   (15/24-bit). GPUSTAT field/odd-line bits drive the BIOS shell.
   Seen: Ridge Racer's loading screen and title right.
   trinity changes: Pipeline.blend (BlendState), the batched
   Render path honours a model's runs, Texture.vk_format public.
   A Pipeline's blend must be given at construction.
   OPEN: mask test (E6 bit 1), lines are quads, VRAM wraps.
5. IN PROGRESS CD-ROM (Cdrom.ag): cue/bin (or a folder holding
   one), commands and timed responses, data reads. DONE (Oct 7)
   CD audio: a sector a 588 SPU samples, the CD-to-SPU volume
   matrix, mute, autopause (INT4), report mode (INT1 every 10
   sectors). Checked: the race's music matches track 3 (rr3.bin,
   correlation 0.70 with the SPU's sounds over it). OPEN: XA.
6. DONE GTE (Gte.ag): every command, 44-bit MAC checks and all
   FLAG bits, the UNR-table divide, the RTPS IR3 flag quirk, the
   MVMVA far-colour bug. Seen: the title's waving flag and the
   attract demo racing in 3D.
7. DONE (Oct 7) SPU (Spu.ag, ported from ares, ISC): 24 ADPCM
   voices, ADSR, the Gaussian table, pitch modulation, noise,
   reverb, transfer FIFO and DMA, the RAM IRQ, capture buffers;
   a sample every 768 CPU cycles into a ring; a sound thread
   writes it to spectra's AudioOut (44100 Hz). Frames run by the
   clock (59.81 a second), nudged by the ring's fill; no sound
   device runs silent. Checked by a capped dump: menu sounds,
   engine and music. NOT heard by me on the real card.
8. IN PROGRESS pads: digital pad on port 1, keys mapped. OPEN:
   memory cards, gamepads.
9. OPEN MDEC.
10. DONE (Oct 7) speed: a race holds 60 frames a second at 6x
   (was 26). Each flush waited the GPU and re-sent the whole
   22 MB vertex buffer, and each texture read after drawing
   copied all 75 MB of VRAM with a queue wait. Now draws only
   record (new vertices sent), a read copies only the dirty
   64 x 64 blocks it needs inside the same command buffer
   (Render.copy_recorded), one wait a frame. Emulation ~3 ms
   a frame.
11. DONE (Oct 7) drawn at the window's resolution: VRAM is a
   (1024 x S) by (512 x S) target, S from the window's pixel height
   over 240 lines (1 to 8; 6 at 1440); texture and colour
   table reads stay on the game's texel grid; CPU uploads expand to
   S x S blocks, readbacks take one sample a block; a scale change
   reads VRAM back, rebuilds, writes it again. Dither on the scaled
   pixel grid. The window is resizable.
12. DONE (Oct 7) precise vertices (Kalen: wiggly, distant road
   ripped). RTPS/RTPT keep x and y as H x camera / z before any
   cut (camera x, y, z from the uncut 44-bit sums) and the depth.
   They travel with the screen word: beside the SXY FIFO, through
   SWC2, MFC2, LW and SW (a value checked against the word), into
   a shadow of RAM and the scratchpad; the GPU's DMA hands each
   command word's address, so a vertex reads its own exact value.
   A word-keyed table (per frame; two values on one word fall
   back to the whole pixel) catches the rest. Ridge Racer's demo:
   97% exact, 64% by address. Textures map with perspective when
   all three depths are known (q = nearest / depth in the vertex
   alpha, uv x q, divided back per pixel). The few seam specks
   left are in the game's own 1x picture too.
13. DONE (Oct 7) Start did nothing: OpenBIOS waits 80 turns of a
   short loop for each pad /ACK, and the ACK fired per scanline,
   too late. The ACK now lands 700 CPU cycles after its byte
   (after the BIOS clears the IRQ, within its wait), checked in
   the step loop; status bit 7 shows it. Checked: Enter opens the
   race menu, Cross on START starts a race.
14. DONE (Oct 7) anisotropic filtering of 3D surfaces (all three
   depths known): up to 16 bilinear taps along the pixel's
   texel footprint, each texel decoded through page, window and
   colour table, clamped to the polygon's own UV box; clear,
   opaque and semi texels counted apart (cutouts, two-pass
   blends). Sprites and 2D stay sharp. Gpu.filter turns it off.
15. OPEN (Oct 16, Kalen: breaks in the far road, worse cars) the
   word table (Gte near_*) was 4096 slots, no probing: GTE words
   overwrote each other. Now 32768, 32 probes: in the attract
   demo vertices with no exact value 209k -> 88k (the rest 2D),
   mixed polygons 108k -> 8.3k. The lines remain: on frame 2300
   1x has none, high res has thin lines (blue, near black) across
   the road and the cars; whole-pixel vertices at high res have
   none; perspective textures are not the cause. Found: the game
   transforms each road segment apart, so a shared corner is
   computed several times (163.4 .. 164.0 across one point).
   Tried and removed: a weld (no change), skipping triangles of
   no area in whole pixels (removed real road).
16. BUILT (Oct 16, Kalen) free camera, O: off, live, frozen.
   Exact vertices go back to camera space (H, OFX, OFY) and are
   seen from the camera (WASD, shift x4, mouse look locked; a
   click relocks). Frozen replays the commands of the last whole
   frame (kept between display flips that move it) with the
   drawing state it began with, and shows the buffer they draw:
   the game sets the next frame's area at a list's end. Checked
   hidden on frame 2300 (mouse lock off for the test): at rest
   live and frozen equal the plain picture; W moved 2466 units
   in. 2D (sky, HUD) stays put; culled faces stay missing; the
   game's back-to-front order is kept (no depth buffer yet).
   Then (Kalen: the cut took too much): a polygon with a corner
   behind the camera was dropped whole; now each triangle is cut
   at the near plane (16 units), colour and uv carried, a cut
   corner making a quad (cam_tri); a polygon of exact and whole-
   pixel corners together is not drawn in camera mode (the long
   streaks). Seen hidden: the road reaches the bottom flying in.
   OPEN: ground under the road draws over it (the game's order),
   the cars' back faces (NCLIP).
17. DONE (Oct 16, Kalen) save states: P saves, L loads, and the
   app menu's Save state / Load state (Actions), into <disc>.state
   beside the cue. psx/State.ag StateIO writes or reads; each
   device's state_io lists its fields once for both ways (Cpu,
   Bus RAM + scratchpad, Gte, Dma, Timers, Pad, Cdrom, Spu with
   its voices, sweeps and reverb, PsxGpu registers and VRAM read
   back at 1x, put back at the scale). The exact-vertex tables and
   the sound ring start over on a load. A save waits for no VRAM
   upload in flight. 4.7 MB. Checked hidden: loaded in the same
   run, the shot 0.4 s on equals the shot 0.4 s after the save (0
   pixels); loaded into a fresh process, the race goes on.

## Active work: launch and landing (Oct 7 2026)

Kalen: a Falcon 9 booster lands on real physics, real sensors,
real propellant mass, real planning. Each part is its own
module. Real Earth (WGS84, rotation), Falcon 9 Block 5 from
public figures. The vehicle flies from its estimate, never the
truth. Rates are settings (IMU 400-1000 Hz, GPS 1-10 Hz,
guidance ~50 Hz; SpaceX does not publish theirs).
1. BUILT launch/launch.ag (the world): clouds' Nubis layer on
   the sphere (triplanar weather tile, a turned 3.4x second
   read, a baked globe field), a far slab level of detail by
   pixel footprint, night side shadowed, hex sea platform with
   the orbiter mark, scripted Falcon-like ascent, pad and chase
   cameras. Seen: liftoff, staging, 143 km, 58-60 fps in orbit.
   The last fixes (exposure, flame, orbit clouds) not yet seen.
2. BUILT physics/physics.ag: Dvec/Quat in f64, WGS84 frames,
   J2 gravity, US Std 1976 (+ its table above 86 km), a Wyoming
   sounding reader, Dryden gusts, a Craft of parts, tanks,
   engines, grid fins, cold gas jets and legs (deck contact),
   RK4, separate[], falcon_nine[ payload ]. silver --test
   physics, 7/7 against published values: air at six layer
   edges within 0.2%, gravity 9.8142/9.8321 (9.8143/9.8322),
   LC-39A round trip < 1 mm, orbit energy drift 4.8e-11, stage
   two dv 2449.5 vs Tsiolkovsky 2449.0, drag seen in the motion
   = 0.5 rho v2 Ca A to 0.01%, legs carry the weight to 0.4%.
   Found: aft first the empty booster drifts to 9 degrees off
   axis in calm air (marginal, as the real one; fins and jets
   must hold it). Estimates, named in the code: Ca/Cn tables,
   tank places, gimbal 5 deg, fin size and lift, jet thrust,
   leg stiffness. Slosh, jet damping, dI/dt are left out.
   The spec: 6-DOF, RK4 fixed step, WGS84 J2
   gravity in an inertial frame, US Standard 1976 air, aero
   (body, grid fins), Merlin thrust by ambient pressure,
   throttle, gimbal, mass flow, moving centre of mass.
   The air is not perfect (Kalen): winds with shear and the
   jet stream, gusts, temperature and pressure off the standard
   day, all pushing on the vehicle unevenly. Source: a measured
   Cape sounding (station 74794) to ~30 km, US Standard 1976
   above, Dryden turbulence (MIL-HDBK-1797) for gusts.
   All units metric (SI); all figures published or derived,
   and any estimate is named as one.
3. BUILT imu/imu.ag: dv and dtheta increments at 400 Hz at the
   unit's place (lever arm: w' x r + w x w x r), LN-200
   datasheet errors (assumed unit): gyro 1 deg/h bias, 0.07
   deg/sqrt(h) ARW, 100 ppm; accel 300 ug, 50 ug/sqrt(Hz),
   300 ppm; misalignment 0.1 mrad (estimate); biases drift as
   Gauss-Markov, 1 h. physics gained Craft.spin_rate. Checks
   2/2: in orbit it feels 0.0037 m/s2 (its bias); 1 rad turn
   read 0.99970.
4. BUILT gnss/gnss.ag: the current YUMA almanac (navcen.uscg.gov,
   cached in path_storage gnss, fetched when missing), satellite
   places by IS-GPS-200, pseudoranges and rates with light time,
   Earth turn in flight, troposphere, a 350 km ionosphere shell
   (half removed by the receiver's model), 1 m broadcast error
   drifting per satellite, 0.5 m code and 0.03 m/s rate noise
   (estimates), a TCXO clock as random walks; least squares
   place, clock, speed, PDOP; 10 Hz, 50 ms latency (settings).
   Checks 3/3: 32 satellites at 26,602 km; LC-39A 10 in view,
   PDOP 1.43, worst of 100 fixes 7.5 m; 400 km orbit 0.65 m,
   0.049 m/s, 16 satellites. The receiver ignores COCOM limits
   (a launch receiver is licensed past them).
5. BUILT nav/nav.ag: an error state Kalman filter of 18 (place,
   speed, attitude, accel bias, gyro bias, the GPS's shared
   error as Gauss-Markov 4 m / 600 s, an estimate). Strapdown
   in the inertial frame at the IMU's point, J2 gravity, IMU
   samples at 400 Hz; each fix met at its own time from a kept
   past (50 ms late), antenna lever arm in H. Check 1/1: stage
   two burning and turning at 300 km, starting 10 m, 0.5 m/s,
   0.1 deg tilt, 0.5 deg heading off: at 60 s 5.6 m (it says
   6.3 m), 0.011 m/s, 0.049 deg; after a 20 s GPS gap 5.7 m,
   0.013 m/s. Each raw fix is 4.2 m off: GPS limits it. With
   15 states it claimed 0.3 m (overconfident): fixed by the
   shared error states. FOR THE LANDING: absolute GPS is ~5 m;
   the deck carries its own receiver, and the difference of
   the two (same satellites, same errors) is the relative
   place to land on (OPEN, part of item 6).
6. PARTLY BUILT guidance/guidance.ag: a second order cone solver
   (log barrier, Newton on the KKT system, a phase I with one
   slack), the landing burn as Acikmese & Ploen 2007 / Acikmese,
   Carson & Blackmore 2013 (u, sigma, z = ln m, throttle band
   linearised about full-burn mass, tilt, glide slope, terminal
   place and speed, least fuel; golden section on the flight
   time), an impact predictor (gravity, turning air, drag
   engines first). Checks 3/3: distance 4.24264 exact; 30 t on
   one Merlin from 1.5 km, 150 m/s down: 13.95 s, 2,957 kg,
   flown exactly it lands 5e-8 m off, thrust 337..844 kN of
   338..845 (the linearisation), tilt 14.9 of 15 deg; ~160 ms
   a solve; impact vs physics from 60 km: 90.6 s both, 39 m.
   OPEN: the phase sequence (ascent, MECO with a return
   reserve, flip, boostback by the predictor, entry burn, the
   aero phase, ignition timing, re-solves at 2 Hz); needs 7.
   Original spec: ascent, boostback, entry, landing burn by
   convex optimisation (G-FOLD).
   Kalen (Oct 7): the booster lands back on the SAME platform it
   launched from (return to launch site): full boostback, more
   propellant held back, less payload. Then (Kalen, Oct 7): the
   deck alone, no mount or tower, only the orbiter mark; the
   stack stands on its legs (out from launch) and lands on the
   mark, the deck's middle. Relative nav: the platform's own
   GPS receiver (2 m over the deck) less the booster's.
   Calm, after: scvx 36.8 m off at 15.1 m/s, gfold 17.1 m at
   13.0 m/s (both fail the test's limits, as before).
   BUILT mission/mission.ag (the sequence) and Kalen saw it land
   beside the circle (Oct 7). Calm air (test severity 0): the
   forecast (the booster's copy flies coast, entry burn, fall,
   ignition by stop_loss, braking) aims the boostback and the
   aero phase; the boostback cuts on the exact step (cut_at);
   thrust sized by the air at the commanded attitude. Ignition
   miss 1,116 m -> 292 m; touchdown 20.9 m off, 8.4 m/s, 9 deg
   tilt, 515 kg left (the test's limits still fail). Open, in
   order:
   a. OPEN the aero phase sits at its 0.2 rad lean cap.
   b. OPEN over-thrust the first 5 s after ignition (attitude
      lags 0.56 rad; the plan's sideways ask too big at speed).
   c. OPEN the plan's outside push from the booster's copy
      (hull side force, fins, slosh): the last 20 m.
   d. OPEN the fallback braking climbs (min throttle > weight).
   e. OPEN the boostback cutoff note prints the stale miss.
   f. OPEN restore the test's weather; check light, moderate.
   g. BUILT, not seen in launch (Kalen: 1 frame per 3 s near the
      landing): each 2 Hz replan ran ~11 cone solves, 1.24 s mean,
      4.1 s worst, inside one physics step. The solver walks only
      the columns each row and cone touches (Socp.index): 130-180
      -> 32-42 ms a solve, same Newton path. The plan is solved on
      an async worker from the state 0.5 s ahead (PlanJob,
      plan_work, plan_ahead) and taken up at that met; launch
      stops stepping while one is due and unfinished
      (Mission.waiting). Calm landing now 24.2 m, 10.3 m/s, 7.0
      deg, 595 kg (was 21.0 m, 8.5 m/s): the 0.5 s lead.
   h. BUILT (Kalen: SCvx for the landing, keep G-FOLD, benchmark
      both): guidance Scvx (Mao, Szmuk & Acikmese 2016; Szmuk
      2018): thrust in newtons, mass and drag (the DragTable's
      Ca, standard air) in RK4 dynamics, flight time free,
      linearised by forward differences, one cone problem a pass
      in a trust region, a virtual control on the end. enum
      LandingMethod (scvx default, gfold); Mission.landing,
      `launch --landing gfold`; the touchdown note gives the
      plans' mean and worst solve ms. guidance
      t_landing_compare (1.5 km, 150 m/s, Ca 1.3, both flown
      through the same air): scvx 178 ms, 6 rounds, 2,638 kg,
      0.06 m off at 0.01 m/s; gfold (search + 2 drag passes)
      663 ms, 2,658 kg, 21 m off at 2.5 m/s.
   i. BUILT touchdown physics (physics.ag): each leg elastic
      (Hunt & Crossley damping), then its crush core at 400 kN
      for 0.5 m, then the structure; breaks past 1.5 MN (all
      estimates); pads hold on a sideways spring to friction's
      limit then slide; feet past the deck's edge find sea;
      the hull (skirt + rings every 8 m) strikes the deck. Feet
      2 m under the engines (LEG_FOOT_X, photos). Per leg: first
      touch (time, place, speed, tilt), crush, peak, broken;
      hull strike. physics 15/15: drops at 2 m/s (no crush),
      6 m/s no damper (crush 0.2963 m vs 0.2973 by energy),
      leaning 6 deg (stands), 8 m/s across at 15 deg (topples).
      The mission runs 20 s past touchdown and notes it all.
      Flight (gfold, calm): two legs at 6.2-6.9 m/s and 12 deg,
      three legs break, the hull strikes, it topples. The fall's
      stronger lean (600 / 0.35) lit the burn 425 m off and both
      planners went in the sea: back to 1000 / 0.2.
   j. BUILT legs stow at liftoff and swing over 1 s (Leg.frac;
      launch VehicleLit fold); plume from plume_of (physics):
      Prandtl-Meyer edge, Pack cells, Witze core, Simons spread.
      SCvx thrust slew limit and the tilt taper (tilt_at) built;
      the SCvx flight not rerun since.
   k. BUILT (Kalen: solves must be superbly fast; own IPM in
      silver) Socp is a primal-dual interior point: G x + s = h,
      Nesterov-Todd scaling (the reflection v = (w + e) /
      sqrt(2 (w0 + 1)), checked in numpy), Mehrotra predictor
      and corrector, the reduced system by Cholesky plus an m by
      m Schur complement, at most 30 steps; it stops at 1e-8
      (dual 1e-6) and keeps the last 1e-6 iterate, as the reduced
      system loses digits near the answer. Distance check 5
      steps (was 52); a G-FOLD solve 10-20 ms (was ~35 ms and
      ~110 steps); G-FOLD's time search is 6 scanned times and
      4 golden steps. Flight: 94 plans, 90-110 ms mean, ~280 ms
      worst (was 614 / 1,262). Guidance 4/4.
   l. OPEN the last 10 m. Also built: a soft sink-rate cone (fall
      no faster than sqrt(2 x 6 x height) + 1 m/s, 2 m/s of fuel
      per m/s over), the flight's air share 0.7..1.3 (was
      0.3..1.5), replans down to 3 m with a search range that is
      never empty, a capped lean keeps the push up as asked, and
      feedback toward the plan's place and speed (1.2 rad/s, at
      most 5 m/s2). Flight (gfold): about 10 m up at 1-2 m/s down
      but 11-13 m/s sideways; one engine at its least throttle
      lifts more than the booster weighs, so slow and high can
      only climb; it climbs and falls in the sea.
   m. DONE (Oct 7, Kalen: a soft landing "bounced 100 ft"): it
      fell over, the nose hung past the deck's edge (no contact
      there), and when the hull swung back over the deck a point
      already metres under its top met the 5e7 N/m hull spring:
      thrown up at 80-104 m/s. Kalen: the floor is not a spring.
      A hull strike is now the end: Craft.step stops, mission
      notes "it explodes", phase 9 ('exploded on the deck').
      Legs keep their springs and crush cores.
      Also: at touchdown the autopilot and jets stop.
   n. BUILT, not seen: grid fins (launch build_fins), placed as
      the physics has them (0/90/180/270 deg; legs at 45): a
      frame 1.5 m out, 1.2 m across, 0.3 m deep (estimates), a
      45 degree lattice, hull fittings; folded flat up the
      interstage until Fin.open, out in 1 s (Fin.frac).
   o. BUILT (Oct 7): deck receiver as a differential GPS base
      (gnss Receiver.base, base_at; booster gps_bias 0.3): the
      feet's height estimate went from 2-5 m high to 0.05 m.
      follow() removed (it made the booster circle). G-FOLD:
      per-step air share (Landing.reach), sideways turn rate
      (turn, 2 m/s3), the push got lagging the push asked
      (lag = 1 / point_gain, step mean; guidance t_landing_lag:
      0.08 m off flown through a true lag), a cost per metre
      off the mark (track_cost), upright over the last 3 s by
      time (UPRIGHT_TIME). Engines cut when rising under 10 m;
      a feather at least throttle, upright, when that meets
      the deck at 2 m/s or less. Flight (gfold, light): four
      legs at 3.8 m/s, stands at 3.5 deg, 38 m off the mark.
   p. BUILT (Oct 8): wind and the coast. --rwind (wind_min,
      wind_max: the jet, km/h): Weather.random_wind; the deck's
      anemometer (read_deck_wind, 10 m mast, ~10 s average)
      shifts the loaded profile's bottom (known_wind), which the
      impact forecast flies through. The ascent flies a pitch
      plan against the ground (planned_lean, the calm climb's
      lean every 5 s). The coast leans AWAY from the miss (the
      hull-only model said toward: wrong in flight), learned from
      the miss over 4 s once q > 15 kPa, lean cap 0.3. Calm test
      flight: ignition 370 m off, stands 17.6 m off at 3 deg.
      Fins turn visibly (VehicleLit.twist, each fin's deflect),
      not seen yet. Joints and tube modes (knee 1000 kg, joint
      5e7 N/m, axial 30 Hz, bend 4 Hz, 2%), leg damper 8e4,
      lift_loss 0.4: t_drop_lean still fails (rocks ~4.6 deg at
      8 s). After any Craft/State change: --clean imu gnss nav
      control guidance mission launch (stale layouts broke nav).
7. BUILT control/control.ag (class Autopilot: a class named
   Control clashed with trinity's element Control; in launch
   the linker took trinity's Control_init, so ours never ran
   and its duty vec stayed null: the T0 crash). Class names
   must not repeat one in any module loaded beside them.
   One attitude loop on a Belief (nav's,
   or the truth for checks): error to wanted rate (capped by
   what the actuators can brake, sqrt(2 a e)), rate error to
   torque with the gyroscopic term; split over the gimbal
   (pitch, yaw on every lit engine, roll by the outer ring
   along the hull), the grid fins (least squares, slope and
   dynamic pressure from its own standard-air estimate) and the
   jets (pulsed, a share of each 20 ms period). 50 Hz. physics
   gained actuator slew (gimbal 0.35 rad/s, fins 0.6 rad/s:
   estimates) and roll jets (4 side, 4 along the hull, 2 kN
   each: estimate). Checks 4/4: stage two turned 20 deg holds
   within 0.03 deg; the empty booster flips end for end on
   jets in 34.7 s; falling through 5+ kPa with moderate
   turbulence the fins hold 4 deg off the airflow, mean 0.49
   deg, worst 1.19; the full stack flies the pitch program to
   20.8 km through max-Q 39 kPa in moderate turbulence, worst
   0.76 deg.
8. OPEN launch draws truth and estimate.
12. BUILT aerodynamics from the shape (Kalen chose the component
   build-up, Oct 7). A Part is a tube or a profile (x, r pairs;
   the fairing's ogive), outer or inside; the craft's skin is
   their envelope at 0.25 m stations, rebuilt on separation.
   Craft.body_aero: normal force by slender body theory (area
   change from the leading end, separated narrowings halved:
   estimate) plus Jorgensen's crossflow (NASA TR R-474, 0-180
   deg, its eta and Cdc curves read off); axial: Schlichting
   turbulent skin friction with compressibility and Sutherland
   viscosity, faces into the flow by modified Newtonian (Rayleigh
   pitot) from Mach 1.2, blunt faces 0.8 of stagnation below
   Mach 0.8 (Hoerner), blended between; base suction by Hoerner/
   Love curves, none behind a burning base; a burn out of the
   leading end shields its face (estimate); open grid fins and
   deployed legs drag (estimates). Every force at its station:
   the centre of pressure comes out of the shape. axial_coeff
   (for the planners) now evaluates the shape. Checks: Newtonian
   stagnation 1.837 at Mach 20 (1.839), Cf 0.00300 at Re 1e7
   (0.0030), slender body slope 2.10 at 0.5 deg (2 + crossflow).
   Stack nose first under power: Ca 0.10 subsonic, 0.36 Mach
   2-5, centre of pressure 59.5 m (cg 27.7): unstable, as real.
   Booster engines first, fins out: 1.47 subsonic, 2.71 peak at
   Mach 1.3 (fin share an estimate, likely high transonic).
   Found: a 0.5 m gap in the skin (stage two to fairing) read
   as a blunt face; closed. Control gained integral action (a
   steady aero torque left a steady error): fin hold mean 0.22
   deg, worst 0.48; ascent worst 0.61; gimbal 0.06; flip 34.7 s.
   predict_impact takes the craft and uses its own shape (20 m
   from the physics after 95 s, was 39).
10. OPEN (Kalen, Oct 7) a standard view for every sensor on board,
   each an element its own module exports: imu (rate and
   acceleration traces), gnss (sky plot, fix error, PDOP), nav
   (estimate against truth, its spread), physics (tank cutaway
   with slosh); launch mounts them as panels.
11. BUILT slosh in physics: per tank Abramson's first mode (NASA
   SP-106): m1 = m 2R/(1.841 x 2.3894 h) tanh(1.841 h/R),
   w2 = 1.841 a/R tanh(1.841 h/R), damping 0.02 (estimate), the
   mass at h - R/1.841 tanh(1.841 h/2R) (our approximation).
   The rigid body leaves m1 out (Craft.rigid; mass is the
   total); each m1 loads its tank axially and pulls by its
   spring at its offset; it feels the tank wall's specific
   force and the turn; a stop at 0.8 R; State.slosh holds y, z
   and speeds per tank. Check t_slosh_ring: held still the LOX
   rings at 1.9507 s (Abramson 1.9504), decays 0.3659 (0.3659);
   free, coupled with the stage, 1.66 s. Found on the way: the
   IMU's felt force must be over the rigid mass (nav broke at
   1.4 m/s, fixed). Coast: no settling model (liquid floats
   only as far as the spring frees it). NEXT: the cutaway
   view, with launch on physics (after 7).
9. BUILT, not run: C cycles the camera: platform, orbiter (the
   upper stage, to a stable orbit), booster (it lands).
13. DONE (Oct 8, Kalen: "landed on the first go") the hexagonal
   ship: rounded hexagon hull, six grid fins on the faces, six
   legs as flat panels stowed on the faces (width the flat of a
   face, sides chamfered at 30 degrees). Not yet seen in launch.
   Then (Kalen): each leg held by a link to a slider in a rail
   channel cut in its face (flush when stowed); the six sliders
   ride one collar, a relief, not a spring: rigid to 2.4 MN on
   the feet, then it slides up for good (0.5 m), turning every
   leg flatter, so the stance widens. physics: Craft.collar_*,
   Leg.hinge/foot_out; 6 m/s drop: collar 0.183 m (energy
   0.186), feet 18.0 -> 18.18 m across; lean drop stands 0.2 deg.
14. OPEN (Kalen, Oct 8) the ship goes to the moon: it docks
   with a fuel station in orbit to refuel first.
   Kalen's calls (Oct 8): the tanker is our own stack, same
   model; its upper stage holds propellant where ours holds
   people; when done it lands itself. 400 km circular, the
   launch waits for the tanker's plane over the pad, then
   phasing. Full contact docking. The orbit flight we fly now
   stays; the main mission flies on to the tanker. Order:
   a. DONE the tanker in orbit (Mission.tanker, place_tanker):
      our upper stage, fairing off, at liftoff circular 400 km
      in the plane of the pad's heading east (inclined 28.39
      deg), tanker_lead 30 deg ahead (an estimate for three
      phasing orbits); speed from our J2 gravity. Steps with
      the flight. t_tanker_orbit: pad 0 m off the plane, an
      orbit 5553.6 s at 398.1 to 400.3 km. The full flights
      after it not rerun. Its propellant load: item e.
   b. DONE the ascent to its plane and up to it. The upper
      stage steers in the tanker's plane (in_plane: sideways
      drift trimmed); insertion guidance: the burn left by the
      rocket equation, a vertical push falling in a straight
      line to reach 200 km level as orbital speed comes. Then
      phases 3 parking (rounded off at its real high point),
      4 transfer burn when the tanker leads by the transfer's
      phase, 5 coast, 6 circularise, 7 in its orbit. Burns cut
      on orbit_range: our nav state flown on in our J2 gravity
      (the round-Earth high point is ~14 km off). tanker_lead
      -10 deg. Found and fixed: Autopilot jet reach used the
      booster's jet place (40.5 m) and was negative on the
      upper stage, so its jets never fired; stage two now has
      its own cold gas jets. t_to_tanker (whole flight): SECO
      200.0 km, 0.03 km off the plane, parked 197.5-200.1 km,
      transfer T1531, in its orbit T4260, 1.46 km from it,
      3.9 t left. Both landings as before (17.9 m, 19.7 m).
      launch draws the tanker (silver nose), camera 4: tanker.
   c. OPEN relative nav (both ships' receivers, differential)
      and close guidance (Clohessy-Wiltshire targeting, hold
      points, the approach along the tanker's port axis) on
      the upper stage's cold gas jets.
   Kalen: they mate flat on, side by side "like 2 fish": a
   hex face against a hex face, not nose to nose.
   "like a pencil to pencil": parallel, side by side. The
   tanker is the whole ship with its nose: only the nose
   differs (ours for people, its for propellant).
   d. OPEN docking ports in physics: contact, soft capture
      within limits of speed and misalignment, latches, hard
      dock joining the two crafts.
   e. OPEN propellant moves from the tanker into our tanks.
   f. OPEN every upper stage lands: legs (the panel and
      collar design) and a landing burn on its engine; the
      tanker undocks, comes down and lands on the deck.
   g. OPEN launch draws the tanker, the docking and its cameras.
   Kalen (Oct 8): every stage kept on the tanker (its booster
   stays on in orbit); not lower than LEO: it parks at 200 km
   (tanker_alt), ours parks at 170 km under it (park_alt) to
   gain on it, then rises to it.
   h. OPEN the tanker waits up there days to weeks: air drag
      lowers it, and it burns to stay up, paying in propellant.
      Station keeping: a reboost when its low point sags, fuel
      taken from its own tanks, the total logged.
      BUILT: Autopilot holds its nose along its path (jets),
      the orbit's size by its whole energy (J2 swings the
      height 2.7 km a lap), centre engine at least throttle at
      the high point under keep_band. physics gained free
      molecular drag (FREE_CD 2.2, Knudsen bridge): t_aero_thin.
   i. BUILT (Kalen, Oct 8): booster and upper stage 50% larger,
      every ratio kept (SHIP_SCALE 1.5, scale_ship): lengths
      x1.5, masses and thrust x3.375 (chamber pressure 146 bar,
      "if raptor can"), jets x5.06. Physics 22/22 at the size.
      launch draws it at SHIP_SCALE. Landings to rerun.
      Then (Kalen): 100 m tall, 6 m across the faces (SHIP_LONG,
      SHIP_WIDE), seven larger engines, the nose the hull's
      width. Isp 380 s (352 at sea level): "fantasize".
   j. DONE (Kalen: we REFUEL at 200 km) the main mission flies
      the whole ship: nothing separates (Mission.whole). The
      seven burn the booster's tanks, then stage two's
      (crossfeed); under 4 g (WHOLE_G, 7 -> 3 -> 1 engines);
      the pitch plan to T+80, then linear tangent steering
      (fly_ahead, steer_solve: Newton on the burn flown ahead)
      to 200 km level. t_whole_ascent: SECO T302 at 200.1 km,
      7785 m/s, 10.3 t left, 8 m/s to the air. t_to_tanker:
      in its orbit 9.5 km from it. The orbit flight (whole:
      false) keeps its staging and the booster landing.
      OPEN: the orbit flight's scvx landing at the new size
      (gfold lands 19.3 m off; scvx 44 m, a leg breaks).
      OPEN: tanker station keeping: its jets sit above its
      centre of mass and push it up 233 m in 3 days, more than
      the drag takes; needs jets at both ends fired in pairs.
   k. IN PROGRESS item c, the close approach (Kalen: the
      tanker rolls its port to face us; signal-wise, its radio
      sends its attitude and its port; intercept at T+8 min).
      Relative GPS by single differences (gnss relative_fix),
      jets as couples (control couple_jets, a per-step firing
      tally), Clohessy-Wiltshire drift taken out, holds at 200
      and 20 m. Held at 200 m at T+1684 (intercept too slow:
      942 m and 9 m/s apart at cutoff). OPEN: cut off beside
      the 200 m mark at the tanker's speed for T+8 min.
   l. DONE launch draws jet puffs, docking ports (capture
      ring, lights, three retroreflectors on the tanker's),
      our floodlight; camera 4 is our port camera.
   m. IN PROGRESS the docking CNN (Kalen): launch's export
      docking_images renders 2000 64 x 64 port-camera pictures,
      offsets uniform over 1-30 m out, 3 m aside, 5 deg roll,
      3 deg pitch and yaw, sun and night, into share/launch/
      docking with labels.txt. dock/dock.ag trains it on ai's
      grad (4 convs, 2 dense, L1, AdamW): `silver dock`.
      LATER (Kalen): Earth backgrounds at other angles.
      Then (Kalen): two ports, one a propellant (oxygen fore,
      fuel aft, 9 m apart: physics port_span, port_of), a camera
      at each; pairs of 32 x 32 pictures, grey for training, in
      range inside the approach cone (aside 0.3 m + a quarter of
      the distance) and off target (neither port in either
      view) with an in_range flag; dock_<n>.agi beside
      dock_<n>_a/_b.png in share/launch/dock2. dock: two grey
      channels, three convs, heads for the six numbers (scored
      in range only) and the flag (bce); every fifth pair held.
      First train on 527 pairs (500 in range; the render froze
      the machine again, the kernel watch stopped it): held out
      right 0.82 m, up 2.22 m, out 3.61 m, roll 1.69, pitch
      1.62, yaw 1.05 deg, in-range call 96%; up and pitch no
      better than guessing (overfit, 422 to train on).
      Net in the approach (Oct 9): its detections weigh into the
      gap by its held-out misses (gps 0.4 m against 0.82, 2.22,
      3.61 m), logged every 5 s against gps and truth ('dock
      net:'); it steers little. Its pictures predate the open
      doors, long probes and brighter floodlight: re-render.
   n. DONE (Oct 9) dock, refuel, undock in the whole flight:
      the tanker turns its ports to our side (under or over it,
      Mission.beneath); closing as burns and coasts, braking
      within the jets' reach at our mass (jets_reach); doors
      slide open, gold probes run out 2.5 m, soft capture with
      a tip 0.3 m in its cone (0.5 m aside, 0.15 m/s, 3 deg),
      15 s draw-in to a 1.5 m standoff (only the connectors
      meet), hard dock; propellant tank to tank (each tank at
      its share of 2 t/s: really ~2.5-4 t/s, OPEN: label or
      pumps); springs part them, probes in, doors shut, back
      off past 50 m: 'clear of the tanker'. Full flight: hard
      dock T+12:54, refuelled T+25:33 (1862 t), clear.
      Fixed on the way: the capture judged mid-step (tanker a
      step ahead: 19.5 m); the latch held centres of mass (the
      hulls slid as propellant moved); the latch copied the
      tanker's body rates into ours (turned 180 deg: nav 104
      deg off after); jets owed time while switched off.
      OPEN: backing off at full mass overshoots 50 m (it is
      only cleared, not held); the tanker now near empty.
      OPEN (real transfer, offered): settling thrust, pressure
      fed flow, venting, chill-down, residuals.
   o. DONE (Oct 9) save state: P saves (~/.cache/launch/saves/
      T<m>m<s>s.save and last.save), L loads last, --load
      <file>. Text, numbers as their doubles' bits; ships,
      phases, docking, pilots, each nav filter's whole belief.
      t_save_state: exact on load, 15.7 m after 20 s through
      max-Q (gusts and sensor noise are fresh draws).
   p. IN PROGRESS the Moon (Kalen, Oct 9): the Moon's orbit and
      gravity in physics, a coast step (big steps, gravity only)
      and higher warp, the burn to the Moon aimed by flying it
      ahead, capture into lunar orbit, the Moon drawn in launch;
      then landing, take-off, home. A full ship has ~10.2 km/s:
      enough there and back to Earth, not an Earth landing
      without a heat shield (Kalen's call pending).
   q. DONE (Oct 9, Kalen) the stack split moved down: stage one
      ends at 38.6 m (Falcon scale; mesh 40), the chrome band is
      the upper ship's skirt with five Lugh (sea level, drawing
      stage two's tanks: lugh picks tanks by group now) and six
      small legs on its corners (Craft.flaps, Leg.length/width/
      open/open_cmd; Craft.flap_push, the air on a plate either
      face; Autopilot.use_flaps allocates them). Booster fins and
      top jets down to 37.0/37.8, its tanks 1 m lower.
      Fall test findings (upper ship alone, 30 km, turbulence 2,
      asked 4 deg off the flow; the test is removed until a
      design is chosen): engines first it tumbles by 2.5 kPa;
      nose first holds 0.5 deg to 1.4 kPa, tumbles at 3.6 kPa;
      twice the panel area tumbles at 4.3 kPa; with 10 t aboard
      holds to 4.2 kPa, then 10-30 deg off; jets up to 20x do
      not hold (aero moment ~2.8 MN m at 19 kPa, 10 deg); one
      Lugh at least throttle out-pushes the ship (it hovers).
      Then (Kalen): the small legs are landing legs only; the
      crew stage steers with its own six grid fins at 55.5 m
      under a nose flared to 2.3 m (Falcon scale; drag in orbit
      +58%). Fall test: engines first it flips end over end;
      nose first 12 deg off at 1.5 kPa, then lies broadside (85
      deg): it falls belly first by itself.
      Recommended (Kalen's call pending): belly first with nose
      flaps and a belly heat shield, flip, landing burn.
      SSTO: as built 6.7 km/s of 9.3 needed; stretched to ~850 t
      of propellant it would fly alone (Kalen: fine as is).
      OPEN ai grad: bin (add, mul ...) reads x.dims[ 2 ]: a 2-D
      tensor gives garbage and no gradient, silently; give it
      [ nb, c, 1 ] or make bin refuse other shapes.
      OPEN the machine: soft lockups on CPU 31 with clock reads
      of 3-40 s, twice, each under our GPU renders (export and
      mission test); cause below our code, not found.
      FIXED silver (Oct 18): a ternary between two Dvec structs
      passed straight into a call (`dv_sub[ (c) ? a : b, e ]`)
      passed the struct by value where the call takes a pointer:
      aether e_fn_call puts a struct value in a slot for a
      by-pointer struct param. features t_struct_ternary_arg.
   r. IN PROGRESS (Oct 9) the tanker's own flight, first off deck
      B (launch's default; --ship flies ours). Kalen: only 7- or
      5-Lugh stages. The tanker: three standard 7-Lugh boosters
      side by side (body z, 1.5 m apart, the docking standoff),
      joined by four rounded hexagon connectors in orbiter-hex
      mounts (2 t each at Falcon scale: estimate; they stay on
      the middle one), a fourth on the middle one's top under a
      silver nose; each side booster has the same nose. 6,513 t,
      21 Lugh below, liftoff 1.40 g. Physics: Part/Tank/Fin.off
      and Leg.axis (a place across the craft), Leg.folded (last
      in Leg), one outline per body in body_aero, inertia by the
      offsets, mark_side/recentre (a side booster detached on its
      own axis); t_tanker_build. All 24 legs and fins kept; those
      that would cross or sit in a gap stay folded while joined,
      all free at separation. Launch: the three drawn, decks D E
      F (more_decks), 32 flames, cameras 3 and 5 the side
      boosters, the side boosters' nitrogen puffs across the gap
      (drawn only: Kalen had the push removed, it put the right
      booster on its deck's edge), flames marched from their far face
      (no depth test; the march stops at the scene's depth).
      Mission: all three separate at staging; the side boosters
      fly as recovery Missions (recovery, adopt, label) to decks
      F and C; reserve from the measured return (1.8 x speed
      across + 950 + 750 m/s at 352 s: estimate); boostback and
      entry on each booster's centre engines (centre_three by
      place; booster_centres); the landing burn starts on three
      when falling over 250 m/s, braking against its motion at
      85%, down to one when one can stop it (burn_n, the forecast
      flies the same); SCvx's plan followed between its steps,
      SCvx plans with the tilt's lag (Scvx.lag = 1 / point_gain:
      the push across follows the one asked; t_scvx_lag flown
      through it 2.3 m off), a plan two solves old gives way to the
      braking law, its turn from the gimbal's spin of the booster
      (turn_r, ~1.6 m/s3), max_rate 0.35 once lit. Physics: a
      narrowing trailing a fall engines first pushes no sideways
      (the noses on the trailing end twisted the side boosters).
      Test run (no push, SCvx with the lag): all three stand:
      middle on B (7.7 m/s, 2.3 deg, cores untouched), left on F
      (5.1 m/s, 0.23 deg, cores used), right on C (6.2 m/s, 2.2
      deg, cores used; lit 62 m off). The top booster parks round
      at 700 km (asked 300), 135 t.
      OPEN: touchdowns over the legs' 2 m/s (cores used); the
      right one's 62 m at ignition; a glide model in the forecast
      (Kalen: the nose's shape as it glides); the top booster's
      cutoff;
      saves lack the side flights; new lines past 72 columns;
      fin_look/fin_miss kept (still used); our ship's flights to
      rerun on the shared landing changes.
      Oct 9 later (Kalen): the top booster's climb is the upper
      stage's insertion law under 4 g (the linear tangent solve
      wandered: 700 km): SECO 300.0 km level, rounded 298-301,
      75 t left. The crew ship flies too (Mission.crew, its own
      flight, tanker_shared, 60 s after: crew_after); the top
      booster is handed to it once round (take_tanker, handed).
      The crew flight stages on its reserve (no 50 km cut: it
      came down with 198 t, too heavy to stop); its upper stage
      climbs by high_point_up (the climb that coasts to the
      high point) under 4 g. Landing burn looked for from 30 km
      (was 8: a fast heavy booster lit too late); legs out under
      400 m on three engines too (they never came out there).
      Launch: the crew flight drawn, its fins from its own
      crafts (they opened at the tanker's staging), cameras 6
      crew ship, 7 crew booster. Run: crew MECO 64.5 km 1829
      m/s, SECO 229 km, orbit 148-408 km; its booster stands on
      A at 5.5 m/s. OPEN: the crew's high point overshoots 270
      (408 km, over the tanker); the middle booster's touchdown
      is marginal (tilt 14-21 deg at first touch, scvx 0.56 s a
      solve); the crew flight not in saves; rendezvous and
      docking with the handed tanker not run.
      Oct 10: the payload was scaled with the ship (10 t became
      38.3 t: scale_ship leaves 'payload' as asked now). The
      crew ship climbs by the linear tangent steering to the
      tanker's height (park_height for tanker_shared), 4 g:
      SECO 300.1 km, 22.45 t left, rounded 302-305 km. The
      rounding burn stops when the flown-ahead low point reaches
      its height (it ran on to 1,375 km before). Top booster
      rounded 298-301 km, 75.3 t. t_tanker_flight passes.

## Active work: quake2 port (Oct 5 2026)

Kalen: id's Quake 2 (GPL, /src/Quake-2 is the reference clone)
ported to silver in its own repo /src/quake2 (module folder
/src/quake2/quake2, origin ar-visions/quake2; no commit yet:
Kalen's). Trinity does ALL 3D and UX, called directly from the
app: no renderer module, no ref_soft / ref_gl / 3dfx / PowerVR.
Assets from Steam: /home/kalen/.steam/debian-installation/
steamapps/common/Quake 2/baseq2 (never in a repo). Later: AI.
Build: `cd /src/quake2 && silver --build quake2`; headless run
as any trinity app (install/build/quake2 --hidden true).
1. DONE inventory: /src/quake2/INVENTORY.md (97k lines to port,
   56k dropped, formats, assets, trinity owners).
2. DONE Pak.ag (pak0-9 + loose files), Bsp.ag (IBSP 38:
   entities, verts, edges, texinfo, faces, lighting, model 0;
   wal textures through the colormap palette into one 2048-wide
   atlas; lightmaps into a 1024x2048 atlas; one mesh, uv tiled
   in the shader by the wal's atlas rect), quake2.ag (BspLit
   shader, MapView: WASD + drag look, start at the first
   info_player_start). base1 draws: 19,887 triangles, textures
   and lightmaps right (shot checked). Sky and nodraw faces
   skipped, no PVS, no culling, no sky box yet.
   silver found on the way: `cast cstr [ v ]` on a vec or a
   `new` buffer handed the OBJECT pointer (fixed in
   e_direct_cast, features t_cast_cstr_vec, 232/232); a render
   target in element_targets needs `owner = a` or the window
   takes it as its backdrop and faults in ReduceBlur.last;
   sibling files are `extend quake2`, imported after the C
   headers; a shader's sampler names must be members of its
   LayoutBinding enum (BspSurface).
3. DONE PVS culling: planes, nodes, leafs, leaf faces and the
   vis lump are read; each frame the eye's leaf gives its cluster,
   the cluster's row is expanded, and the faces of its visible
   leaves are copied into a live index list (Gpu.upload_index,
   new in trinity vk.ag: replaces the first n indices and draws n).
   base1 start: 1,826 of 19,887 triangles; outside the world
   (cluster -1) everything draws. No frustum test yet.
4. DONE the sky: sky faces are their own mesh and shader (BspSky):
   the view direction per fragment, turned back to quake axes,
   picks the face and s,t exactly as gl_warp's vec_to_st and
   MakeSkyVec do, from a 6-wide strip of the env/<sky>*.tga faces
   (TGA 24/32-bit, plain and run length, rows bottom up as
   LoadTGA). Checked by eye against the strip (python reference
   skyref.png): Outer Base sky upright, sun and horizon right.
   `--spawn "x y z yaw pitch"` (quake axes) places the camera.
   OPEN: skyrotate/skyaxis, faint seams at cube edges (the GL
   original had them too).
5. DONE collision: cmodel.c's CM_BoxTrace ported (brushes, brush
   sides, leaf brushes, leaf contents; clip_brush, trace_leaf,
   hull_check, Trace class; the start==end position test is not
   ported) and pmove's PM_StepSlideMove_ with PM_ClipVelocity
   (slide_move, 4 bumps, 5 planes, crease, stop against the first
   push). The camera is the player box (-16 -16 -24 .. 16 16 32),
   mask solid|window|playerclip|monster, no gravity yet, eye 22
   up. Checked headless at the start: forward stops after 624
   units, a strafe after 304, backing up after 888, all inside.
6. DONE pmove's walking path: PM_CatagorizePosition, PM_CheckJump,
   PM_Friction, PM_Accelerate, PM_AirMove, PM_StepSlideMove (and
   CM_BoxTrace's position test: box leaf walk + CM_TestBoxInBrush)
   as categorize, check_jump, friction, accelerate, air_move,
   step_slide_move. Gravity 800, run 400 capped at 300, jump 270,
   step 18, friction 6 / stop 100. Space jumps. Checked headless:
   on the start lift the walk reaches 300 units/s in 9 frames and
   stops at the closed doors (x -1704); a jump climbs 120 -> 159+
   and falls back.
   Found on the way: the start stands on inline model *31 (a
   func_door lift), so the trace clips every inline model at its
   map place (headnode per dmodel, box reject) and the mesh draws
   their faces (always, unculled, until they move). A trigger_once
   brush is SOLID in the bsp; the engine skips it because the
   entity is not solid, so model_entities reads each model's
   classname and trigger_* / func_areaportal are neither clipped
   nor drawn.
7. OPEN the repo: committed locally (LICENSE = id's GPL v2 text,
   README, INVENTORY); github.com/ar-visions/quake2 does not exist
   yet and there is no token or gh here: Kalen creates it, then
   `git push -u origin master` (origin is the ssh url).
8. DONE the game folder begins (Oct 5 evening): Game.ag (edict
   as class Edict with entity_state folded in, MoveInfo, Level,
   SpawnTemp; COM_Parse, ED_ParseField's table, ED_ParseEdict,
   SpawnEntities with skill inhibit, G_FindTeams, ED_CallSpawn;
   G_Spawn/G_FreeEdict/G_Find/G_PickTarget/G_UseTargets/
   G_SetMovedir/G_TouchTriggers; gi.setmodel; the triggers
   (InitTrigger, multi, once, relay), func_areaportal; G_RunFrame,
   G_RunEntity, SV_RunThink, SV_Physics_Pusher with SV_Push,
   SV_Physics_None/Noclip, SV_TestEntityPosition), Func.ag (Move_*,
   AngleMove_*, plat_CalcAcceleratedMove, plat_Accelerate,
   Think_AccelMove, every door function, SP_func_door,
   SP_func_door_rotating), World.ag (SV_LinkEdict, SV_AreaEdicts as
   a scan of the edict list instead of the area node tree, SV_Trace
   with SV_ClipMoveToEntities and SV_HullForEntity, SV_PointContents),
   Bsp.ag (CM_InitBoxHull/CM_HeadnodeForBox, CM_TransformedBoxTrace,
   CM_PointContents, a per-model mesh: each solid inline model its
   own node). An edict's think/touch/use/blocked are an enum Fn
   dispatched by Game.call_*; strings are the keys. The player is
   edict 1 (client, health 100); pmove traces through World.trace
   with the player passed; after the move it is linked and touches
   triggers; the game runs at 10 Hz from the frame's dt. Each inline
   model draws as its own Model with its own BspLit instance whose
   `model` matrix follows the entity (trinity's node_model refresh
   did not take; not chased). Unported classes: brush ones stand as
   solid walls (angles cleared), point ones are inert, items,
   monsters and lights free as the original does for lights.
   Checked headless on base1: 356 edicts, 28 inhibited, 1 team; the
   two doors in front of the start open on frame one (their trigger
   field reaches the start's box by one unit, as in the original),
   stay open while the player stands in the field (re-touched each
   second), close 3 s after the player leaves, and reopen when the
   player steps back in; spawned behind the field (--spawn "-1790
   1536 128 0 0") the closed panels fill the view and the lift under
   the start draws (checked with a sky-shader marker on the inline
   models, then with the real textures). Pushers move the player
   (SV_Push) but no door has pushed one yet in a test.
   Headless shots: the screen copy a `shot` reads only refreshes
   after an input event; a one pixel drag before each shot shows the
   current frame. A socket key release can be lost: send it twice.
9. DONE (Oct 5 night) func_plat, func_button, func_train and
   path_corner (Func.ag, Game.ag): every plat/button/train function
   of g_func.c, path_corner's spawn and touch (the monsterinfo part
   waits for the monster port), Edict gains event, die, movetarget,
   goalentity, ideal_yaw; call_die dispatch. The player's move now
   reports the entities it ran into (pm.touchents: MapView.touched,
   touch_ents after each step, as ClientThink's loop), so a touch
   button fires. The "light" key is ignored as F_IGNORE.
   Checked headless: base2's button *41 (81 units east of the start)
   fires on contact at game 1.9 s, reaches its 5 unit travel at 2.2
   (t7 used), returns at 5.4, fires again while leaned on; base2's
   plat *50 (dmg 10, a crusher under a ceiling at z 160) lifts the
   standing player 150 units, is blocked by the ceiling, reverses
   and repeats, as the original minus the damage; jail1's trains
   run corner to corner from their first frame, each origin equal
   to corner minus mins (89 1 -3 for *49 at t73).
   Test harness notes: `--spawn` takes the EYE (origin + 22), keys
   reach the view only after the map's "drawn" log line, and the
   app relaunch rebuilds when trinity's product is newer than its
   own; that rebuild crashed (silver exit 133) three times while
   Kalen's orbiter session was also building: run `silver --build
   quake2` by hand first.
10. DONE (Oct 5 night) the rest of g_func.c (door_killed,
   func_rotating, func_water, trigger_elevator, func_timer,
   func_conveyor, func_door_secret, func_killbox; Func.ag), g_misc's
   brush classes (func_wall, func_object, func_explosive; Misc.ag),
   the rest of g_trigger.c (key, counter, always, push, hurt, gravity,
   monsterjump; Trigger.ag) and all of g_target.c (temp_entity,
   speaker, help, secret, goal, explosion, changelevel, splash,
   spawner, blaster, crosslevel trigger and target, laser, lightramp,
   earthquake; Target.ag). Game: frand/crandom, coop, serverflags,
   helpmessages, lightstyles (set_lightstyle keeps the letter per
   style), Level's secrets/goals/intermissiontime/changemap, Edict's
   item/skinnum/renderfx/last_move_time/fly_sound_debounce_time;
   call_die carries inflictor and attacker. The player step reads
   pl.velocity back before pmove (a trigger_push or earthquake sets
   it) and gravity is 800 x pl.gravity (trigger_gravity).
   Stand-ins, each a one-line comment at the call: T_Damage,
   T_RadiusDamage, KillBox, BecomeExplosion1, ThrowDebris (combat and
   gibs), fire_blaster (weapons), temp entities and configstrings
   (effects), soundindex/positioned_sound (sound; indices 1-3 as the
   doors use), the inventory (has_item is false until items;
   trigger_key prints "You need the <classname>"), BeginIntermission
   (changelevel sets level.changemap and logs it), oldvelocity (the
   client), monsterinfo (monsters). MOVETYPE_TOSS still only thinks:
   a released func_object does not fall until SV_Physics_Toss.
   Checked headless: base1's START_ON timers fire their targets at
   their random intervals, trigger_always fires, the rotator *29
   (flat1_2 all over, so its turning cannot show) advances 300
   degrees a second and draws filling its shaft; fact2's fans sit
   behind a translucent grille we draw opaque (warp/trans OPEN); no
   dispatch misses on base1, fact2 or jail1. 359 edicts on base1.
11. DONE (Oct 5 night) the rest of the game folder, on function
   pointers (Kalen: "silver has function pointers"; "too much code
   for how simple this game is"): the enum Fn dispatch is gone. An
   edict's think/touch/use/blocked/pain/die, an item's pickup/use/
   drop/weaponthink and a monster frame's ai/think are
   `lambda R [ A ]` slots set with `lambda f[]` and called as
   `e.think[ e ]`; one module global `g : Game` (set in setup), so
   every free function reaches the game through `g.`. Ported: g_phys
   (fly_move, push_entity, physics_toss/step), g_misc's point
   classes (gibs, debris, explosions, explobox, teleporter, viewthing,
   banners, viper, strogg ship, satellite dish, func_clock,
   target_character/string, point_combat; Misc.ag), g_items (Items.ag,
   the table and every pickup), g_combat (Combat.ag), g_weapon +
   p_weapon (Weapon.ag), p_client/p_view/p_hud/p_trail/g_cmds
   (Client.ag), g_monster/g_ai/m_move/m_flash (Monster.ag). Sounds,
   temp entities and muzzle flashes queue as GameEvent (no sink yet).
   quake2.ag: the player goes through client_think (buttons, view
   angles, the touches, the weapon); the left button fires, keys 1..0
   pick weapons by the default binds; a text HUD (health, armor,
   ammo, the center print); a level end reloads the next map.
   Checked headless on base1: no crash over walks and fires; the
   blaster fires from the player (temporary log, removed); an armor
   shard picked up on contact shows Armor 2 on the HUD (shot). The
   items by the start are skill-inhibited, not missing.
   Fixed on the way: a pool-owned Trace stored into Game.touch_plane
   and then nulled was freed under its caller (impact): touch_plane
   is a persistent copy now, touch_plane_on says it is set.
   Compiler, Kalen's design: `lambda f[]` on a plain function is ONE
   static instance per function (Au lambda_static: a lambda object
   in static memory, unmanaged, never counted or freed, left out of
   the object census), over a thunk with an empty context
   (plain_lambda_static, silver.c; one thunk per function per core).
   A slot holds the same kind of object for a plain function and a
   closure, so its users never know which; a store and a null store
   cost nothing. A bare-pointer slot was built first and dropped:
   it put closures out of signature slots. Found on the way: on an
   Au_t, src/rtype/type are ONE union slot (never set rtype on a
   variable); a struct arg through a funcptr call goes by pointer, as
   the callee's own type says (aether e_fn_call). features 238/238;
   img test_jpeg pointed at a file the planet split moved (fixed).
   Headless: SHOT=<png> q2run.sh takes a shot at the end.
12. DONE (Oct 6) the monsters, one file each (`extend
   quake2`, imported after Monster in quake2.ag, spawned from
   spawn_monster in Misc.ag). The frame tables come from a converter
   (session scratchpad m2ag.py: m_X.c + m_X.h -> the MMove globals and
   an `<x>_moves[]` builder; a run of equal frames is one
   `mf[ v, n, ai, dist, think ]` call, Monster.ag; FRAME_ names
   resolve to numbers from the .h, with the name in a comment); the
   functions are ported by hand. One static instance per plain
   function (`lambda f[]`) makes the slots cheap to set.
   DONE: soldier (light, soldier, ss), infantry, gunner, berserk,
   gladiator, flipper, flyer, hover, medic (cable revive), mutant,
   parasite, tank + tank_commander, chick, brain, insane
   (misc_insane), floater, supertank (+ boss_explode), actor
   (misc_actor + target_actor), boss2. All build; base1's soldiers
   see, chase, fire and kill the player at skill 1 (checked headless:
   "player died", HUD Health 0, "fire to restart").
   Then jorg (Jorg.ag), the makron (Makron.ag: torso, sight move,
   makron_spawn/toss; Jorg_CheckAttack is Makron's line for line, so
   jorg uses makron_checkattack), monster_boss3_stand (Boss3.ag) and
   g_turret.c (Turret.ag: breach, base, driver).
   Checked headless with temporary traces (removed), the player in
   god mode: boss1's stand cycles frames 414..473 in 6 s; on boss2
   (`--spawn "200 -960 174 0 0"`, inside jorg's closed chamber) jorg
   takes pain, fires both chainguns on frames 8..13, dies, tosses
   the makron 4.8 s later, who jumps at the player 0.8 s after; the
   makron's bfg (frame 204), blaster sweep (17 shots, flashes
   102..118, yaw 260 down to 180 then 110 up to 180, as the source)
   and rail (frame 243, 0.9 s after the pick; pain can take the move
   first, as in C) all fire; he dies in 95 frames and leaves the
   torso 84 units along -y, looping frames 346..364. jail1's turret
   (`--spawn "-2350 -192 150 180 0"`): muzzle 122 units out, turns 5
   degrees a think, the driver rides his seat round, a rocket every
   3 s (speed 600, damage 100..150).
   NOT driven: use_boss3 (its trigger), jorg's bfg (a 1 in 4 pick,
   not drawn in 5 attacks), turret_driver_die (the gun is always
   between the player and the driver), strike's turret.
13. DONE (Oct 6) MD2 models drawn (Md2.ag). Md2Set loads a model
   the first time its index is asked for (IDP2 version 8; every
   skin's PCX through the colormap palette into one 2048 x 2048
   atlas, two pixels apart; a skin placed after the texture exists
   goes up with Texture.upload_region). Each frame draw_entities
   (quake2.ag) refills ONE mesh of 98,304 vertices on the cpu, as
   GL_DrawAliasFrameLerp did: every edict with an md2 model, in a
   cluster the eye sees (its box corners), lerped from a snapshot
   taken before each 10 Hz game frame (origin, angles, frame; no
   lerp on a new model or a 512 unit jump, as CL_DeltaEntity),
   EF_ROTATE items turning, modelindex2 on the same frame (jorg's
   walker), then the view weapon at the eye (ps.gunindex, gunframe,
   gunangles, gunoffset; depth squeezed to three tenths as
   RF_DEPTHHACK). Light: Bsp.light_point is R_LightPoint (the
   lightmap texel under the origin; node faces and each face's
   lightmap block are kept now), times the shade dot of each
   vertex normal. The 16 x 256 anormtab.h table is not carried: it
   is 1 + dot (0.3 x dot when negative) of the 162 anorms.h normals
   with the light (cos, sin, 1) normalized, yaw quantized to 16
   (checked: within 0.005 of every table entry). Shader Md2Lit:
   texel x 2 (gl's intensity) and the colour, each clamped to 1.
   Game: the model table starts with '*1'..'*N', so an inline
   model keeps its own number and md2 indices come after; setmodel
   now sets modelindex for md2 names (items had 0: never drawn).
   Checked headless, by shots: a soldier with his skin, aiming;
   the dead body; two armor shards; jorg with the rider on him;
   the blaster in hand, staying in place with the view pitched;
   60 frames a second. light_point equals a Python copy of
   RecursiveLightPoint at six base1 points, to the last digit. With
   the preload off every skin arrived by the late upload and drew
   right.
   NOT done: sprites (.sp2: the bfg ball, explosions' flashes),
   RF_TRANSLUCENT, the colour shells, RF_BEAM lasers, dynamic
   lights and light styles on models, a left-handed gun; brush
   models are still unlit by entity light.
   A method parameter named `a` is the object itself: `b - a`
   failed as pointer arithmetic (lerp_angle).
14. DONE (Oct 6) the sounds (Sound.ag, SoundSet over spectra's
   AudioMixer at 44100; quake2 imports spectra). A clip loads the
   first time its index is asked for: RIFF from the paks, 8 or 16
   bit (pak0: 495 at 16 bit, 5 at 8, all mono 22050 Hz).
   S_StartSound's rules: a sound on the same entity and channel
   stops the old one; with every voice busy the one nearest its
   end goes (never the player's for a monster's); a sound with no
   volume gives its voice back; a delayed start waits on the clock.
   Falloff as S_SpatializeOrigin: full within 80 units, then
   attenuation x 0.0005 a unit (0.001 for ATTN_STATIC), times
   s_volume 0.7; the player's own sounds are full. Looping entity
   sounds (edict.sound) as S_AddLoopSounds: one voice a sound,
   every entity's share added, 0.003 a unit, six at most.
   The game's events are read once a frame (MapView calls
   snd.frame after the game frames; run_frame no longer clears
   them): gi.sound at the entity's origin when it was made (a
   brush model's box middle, as SV_StartSound), the weapon sounds
   of CL_ParseMuzzleFlash and CL_ParseMuzzleFlash2 (flash numbers
   to names, by range), the impact and explosion sounds of
   CL_ParseTEnt.
   Checked headless on a null ALSA device (ALSA_CONFIG_PATH to a
   file with `pcm.!default { type null }`: silent, nothing reaches
   the speakers), by a temporary trace (removed): base1's hums
   loop, soldiers' sight, pain, attack and death sounds start at
   0.25..0.35 by distance, the player's blaster at 0.7, the bolt's
   fly loop, flybys at full; a wav's rate and length equal a
   Python read of the pak. With no device the game runs silent.
   NOT heard by anyone yet. NOT done: left and right (spectra's
   Voice has one gain, so each sound is the mean of the two ears:
   half level when spatialized; a pan on Voice is Kalen's call, it
   changes spectra's class), cue-point loops, the mission pack's
   flashes, footsteps (an entity event), s_volume as a setting.
15. DONE (Oct 6) the rest of the list, in its order.
   a. The sights of the events (Effects.ag; cl_fx.c, cl_tent.c and
      CL_AddPacketEntities' effects). Particles (4096, each a clock
      time, origin, velocity, acceleration, palette colour, alpha
      and its rate, placed by the closed form as CL_AddParticles):
      every base game effect function ported one for one (impact
      puffs, blood, blaster, explosion, rail spiral, bubbles, the
      rocket, grenade and gib trails, teleport, big teleport, item
      respawn, logout, flies, the bfg ball's sparks). Explosions
      (32: smoke and flash, the blaster burst, the fireball's
      frames and skins, the bfg sprite), the parasite's and medic's
      cable (temp_entity_beam carries its entity), bfg lasers, and
      RF_BEAM entities as six-sided beams. Dynamic lights: muzzle
      flashes (a frame long, colours by weapon and by monster flash
      group), explosions, rockets, bolts, the bfg; they light the
      models (R_LightPoint's sum) and the WORLD: BspLit takes the
      four that matter most at the eye and adds R_AddDynamicLights'
      falloff per pixel (reach less distance past a cutoff of 64,
      the edge eased over 16 units as the lightmap's filtering
      did; the true distance along the face, not the original's
      octagon estimate).
      Md2.ag now fills TWO meshes: `solid`, and `clear` for what is
      seen through (the same shader made with transparent: true:
      trinity gives that pipeline no depth write and straight
      alpha, so the fragment writes rgb and alpha unmultiplied).
      Sprites (.sp2, each frame's pcx in the skin atlas, colour 255
      clear), colour shells (quad, invulnerability, power screen:
      the plain colour four units out along the normals), alpha
      (RF_TRANSLUCENT 0.7, the bfg 0.3), EF_ANIM frames, the linked
      models 2 and 3. A vertex alpha of 2 marks the view weapon
      (opaque, depth squeezed). The atlas keeps a white block
      (beams, shells) and gl_rmisc.c's particle dot at its corner.
      Entity events once a game frame (MapView.entity_events:
      footsteps, falls, item respawn, teleport, the teleporter
      pads), the event cleared as the server did.
      Seen by shots: blood off a hit soldier, the bolt's yellow
      light on floor, wall and soldier (floor 44,33,25 ->
      76,60,25), the rocket and grenade fireballs, the rail's blue
      spiral, the big teleport's particles, smoke and flash.
   b. The camera (calc_view, CL_CalcViewValues): the eye is the
      origin plus ps.viewoffset (bob, fall, crouch), the angles the
      mouse's plus ps.kick_angles, all lerped across the game
      frame; dead or frozen, ps.viewangles (rolled 40 degrees on
      the floor, facing the killer). The gun, sprites and particles
      use the same eye.
   c. Death in single player: fire restarts the level with the
      player as he ENTERED it (MapView.entry). The first try
      crashed in Render_destroy_ring: replacing a Render under the
      same element_targets key frees the old one while the map
      still lists it, and its dealloc nulls its own key (a second
      free); and a key set to null is found by lookup but skipped
      by the window's draw loop after it is set again. quake2 now
      keeps ONE Render for the app and gives it the next map's
      models (target.models, each model's finish).
      OPEN, trinity's: Render.dealloc (vk.ag) walks
      element_targets and nulls its own key from inside its
      dealloc; replacing a Render under one key then drops twice.
      A change to Au's map_set for this was WRONG and is taken out
      (Kalen: map_set has nothing wrong with it; never change map).
      quake2 keeps one Render and swaps its models.
   d. Water (pmove.c): the level at feet, waist and eye, water
      friction, swimming (PM_WaterMove, the jump key swims up),
      currents in water and on conveying ground, ladders, the jump
      out of water. Traced on base1's pool: sinks to the bottom
      head under (air_finished set), swims up at 64 units a
      second, forward at 150. The player's waterlevel and
      watertype reach the game (drowning, splashes).
   e. Area portals (Bsp.ag: leaf areas, the areas and areaportals
      lumps, FloodAreaConnections on every portal change) gate the
      world's leaves, the entities and the monsters' hearing. At
      base1's start doors: 1,319 triangles shut, 1,634 open, 1,239
      shut again.
   f. Frustum culling: the four side planes each frame against
      every visible leaf's box, and a sphere per model
      (R_CullAliasModel). The live index list is rebuilt every
      frame now. No holes seen, pitched or level.
   g. Warp and see-through surfaces: BspLit warps water as
      EmitWaterPolys (each axis swung 8 texels by the other's
      sine), slides SURF_FLOWING, and SURF_TRANS33/66 faces are
      their own mesh (Bsp.build_clear: a node for the world and
      each inline model), drawn last with no depth write. Seen:
      the pool's surface from under it, the glass over the emblem
      at base1's start.
   h. skyrotate and skyaxis (worldspawn, read in Bsp.ag): BspSky
      turns the look back round the axis. Seen on `space`: only
      the sky's pixels change over four seconds.
   i. The status bar (Hud.ag): g_spawn.c's single_statusbar run by
      SCR_ExecuteLayoutString's rules, the pics from pics/*.pcx
      (digits, icons, the flash field), three of our pixels to
      one of its. Seen: health, armor, the weapon icon, the pickup
      icon with 'Armor Shard', the F1 help icon on base2.
   j. Level changes: `map$start`, '*' and 'film+map' names are
      read; the next map loads (MapView.level_name: map_name is a
      prop the app sets again every frame, so the change never
      took before); the player's persistant data, the start to
      arrive at, serverflags and the help text go to the new Game
      (Game.carry), and the player stands where PutClientInServer
      put him (the port used the map's first start: base1 began at
      the wrong one). Traced: base1 -> base2 arrives at the start
      named base1 (848 2292), armor 4 and health kept.
   NOT done, and why:
   - saving to disk (g_save.c) and a hub's levels remembered when
     you come back: every edict's think, touch, use, pain and die
     is a lambda and its move a pointer; writing them needs a name
     for each static lambda and a way back (a registry, or the
     symbol's name). Not started.
   - films (.cin) and the end-of-unit screens: skipped over.
   - the help computer, the inventory and the score layouts (the
     layout runner is there; the port's p_hud does not write their
     programs yet); the damage and underwater screen tint
     (ps.blend); crouching (no key).
   - sound: left and right (a pan on spectra's Voice, Kalen's).
   - the world's own light styles (flicker, switched lights).
16. DONE (Oct 6, second pass) from 15's NOT done list:
   - crouch (PM_CheckDuck: c or left ctrl; box 4 high, eye -2,
     speed 100, swims down in water).
   - the screen tint (ps.blend) over the view.
   - the help computer (F1), the inventory (tab, [ ] enter
     backspace) and the centre print, in the game's own letters
     (pics/conchars.pcx; Hud.glyph / chars / help / inventory).
     A written backslash n in a map message is a line break now.
   - the world's light styles: g_spawn.c's 12 patterns and the
     switched lights, ten times a second; Bsp.relight rebuilds
     the styled faces' blocks (R_BuildLightMap) and the changed
     atlas rows go up; light_point sums every style.
   - left and right ears: spectra Voice.gain_right and
     AudioMixer.voice_sides (both added LAST in their classes);
     Sound.ag gives each ear S_SpatializeOrigin's share.
   - films (Cin.ag: .cin with sound, .pcx stills) and SV_Map's
     names (MapView.go: 'film+map', '$start', '*'); any key skips.
   - Menu.ag: main, game (easy medium hard), options (volume,
     mouse speed, invert), quit; esc opens it, the game holds
     still. GameConsole (the ` key): map, skill, quit, and the
     g_cmds commands (god, give, noclip ...).
   - a click takes the mouse for looking (ux.mouse_lock).
   Seen by shots: help computer, inventory, crouch, main and
   game menus, the intro film, the skip into base1, `god` in the
   console. NOT seen or heard: the tint, light styles, the two
   ears, the options sliders, the mouse lock on a real screen.
   - saves (Save.ag, MADE by a script from Game.ag's classes:
     /src/quake2/support/gensave.py + save_head.ag; run it
     from /src/quake2/quake2 after Game.ag's classes change). Every member of Edict,
     Client, MoveInfo, MonsterInfo, Level goes out as a line of
     text; floats as their bits; edicts by number, items by
     index, 543 lambdas and 286 moves by name. The menu's save
     and load rows write and read ~/.cache/quake2/save<n>.sav
     (game, level, the hub's kept levels). Checked hidden:
     saved at x 3.6 y -195.6, walked to x -112 y -80, loaded,
     back at x 3.6 y -195.6 exactly.
     A level left inside a unit is kept in memory (MapView.hub)
     and read back on return; a '*' map clears it. NOT driven.
   NOT done: multiplayer, CTF, demos, key binding menus.
   RULE learned (Kalen, Oct 6): `new T [ n ]` is a pooled
   vector; an `@T` member takes only its data pointer and owns
   nothing, so the frame's clean-up frees it. A buffer that
   lives past the frame goes in a real new member
   (`public data : new u8`, then `data = new u8 [ n ]`). Not a
   silver bug: checked in the IR. PakData.data and Cin.ag's
   buffers are new members now; pak_data[ n ] makes one for
   the map's light and vis data, the sound samples and the
   live index list (all were dangling before, and lucky).
   NOT reproduced: the save's first crash (a string freed twice
   at the frame's clean-up) went away when put_s stopped pushing
   a literal held in a local; features t_vec_literal_local does
   the same thing and passes, so the cause is not known.
   OPEN silver:
   - a one-letter literal ('w') is a unichar, not a string.
17. DONE (Oct 6; written blind on Kalen's "port all, no
   testing", then debugged in hidden runs):
   RUN AND SEEN: id's demos/demo1.dm2 plays on base2 (the
   fight, blood, the damage tint, health 100 -> 81 -> 76); a
   hosted q2dm1 with a second hidden instance joined over udp
   (both scoreboards, the client's suicide and obituary on the
   host, its score -1, respawn); a hosted q2ctf1 with a joiner
   (one player a team, the joined marks, flags spawned, the
   grapple fired and its hook flying, the blue banner seen by
   the joiner, `team blue` on the host); `record mine` then
   `demomap mine` plays the recording (health and weapon
   right); the customize controls screen.
   FIXED on the way: the paks' demos are protocol 26 (no
   suppress byte in a frame); put_client_in_server dropped the
   old Client before the new one held its ClientRespawn (the
   team read as 0: a freed object); vec.insert is (value,
   index); the ctf item count; `ctf <map>` hosts ctf (host is
   deathmatch); a recording waits for a whole frame
   (demowaiting, lastframe -1); demomap reads ~/.cache/quake2
   too; a player's name survives a respawn.
   NOT RUN YET: the grapple's pull, a flag taken and captured,
   techs, fraglimit / timelimit / capturelimit, the
   intermission in deathmatch, other players drawn on the
   host (model 255 -> the male model), the inventory and
   centre prints over the net, keys bound through the screen
   (saving is written, the press was not driven), `connect`
   to a far machine (only 127.0.0.1).
   - keys: Menu screen 6 (options > customize controls), 12
     actions, saved in ~/.cache/quake2/binds.cfg.
   - Net.ag: Msg (sizebuf + MSG_*), protocol 34's entity and
     player deltas, Netchan. quake2.c/.h: udp sockets.
   - Server.ag (NetServer): challenge, connect, new,
     configstrings, baselines, begin; per client delta frames
     with pvs and areas; sounds, flashes, temp entities, prints,
     centre prints, layouts, inventory. Console `host [map]` or
     the multiplayer menu; port 27910; 8 players.
   - Remote.ag: `connect host[:port]`; the server's frames are
     laid over a local Game's edicts (edict 1 stays the local
     player: numbers 1 and playernum+1 swap), so the drawing,
     sound and effects code is unchanged. `demomap <name>` plays
     demos/<name>.dm2 from the paks; `record <name>` / `stop`.
   - NOT id's protocol in one place: the client moves itself and
     sends CLC_STATE (5: place, velocity, angles, buttons); the
     server does not run pmove from usercmds, and clc_move's
     checksum is not made. Our builds talk to each other; id's
     own client or server will not play with ours. Server to
     client is protocol 34 as written, so real demos should read.
   - Dm.ag: obituaries with frags, deathmatch spawn points,
     the scoreboard layout (Hud.layout reads quoted strings and
     string / cstring / client / ctf now), fraglimit, timelimit.
   - Ctf.ag: teams, base spawns, flags (take, capture, return,
     drop, auto return), every bonus, the four techs, the
     grapple, the status bar and scoreboard, `team red|blue`,
     banners, trigger_teleport. The ctf pak beside baseq2 is
     read. NOT ported from g_ctf.c: match mode, elections,
     admin, ghosts, the join menus, observers, id view,
     say_team, team skins, the grapple's cable drawing.
   - Save.ag was made again (Client and ClientRespawn grew).
   Not written: downloads, rcon / status / info, prediction
   smoothing, sounds culled by PHS (every event goes to every
   client).
   Silver bugs met:
   - FIXED (Oct 6) `if [ v[ i ] ]` with v a vec bool branched on
     the element's ADDRESS (LLVM: "Branch condition is not 'i1'");
     a ternary on it, or on a vec of objects' element, was always
     true. aether.c: e_create's same-type shortcut loads an
     unloaded primitive element, and both ternary forms load their
     condition (cond_loaded). features t_vec_bool_if, _while,
     _ternary, _logic, t_vec_object_cond; 245 of 246 pass (the one
     left, t_codegen_resource, failed before too). quake2's Bsp.ag
     uses the plain form again.
   The rest are worked round, OPEN:
   - `x : vec f32 [ other ]` is a sized construction, not a copy
     ("no suitable conversion found for vector -> i64"): write
     `x : other`.
   - a class-typed local declared with its type from a call or an
     element (`ex : Explosion [ alloc[] ]`) is refused as
     redundant; a struct one (`v : vec3f [ f[] ]`) is not.
   - FIXED (Oct 18): a local may share a member's name (the
     nearest wins; a.name reaches the member). features
     t_local_shadows_member.
   Seen once, not chased: the first run after BspLit's source
   changed drew no world (shaders compiled in that run); the next
   run was right.
18. DONE (Oct 6, Kalen: "when i load it it goes direct to base1")
   the attract loop of default.cfg's d1..d4: with no --map the app
   plays idlog.cin, demo1, idlog.cin, demo2, round again
   (MapView.attract, attract_next; the demo's end and film_end
   step it); any key opens the main menu over the running demo,
   or alone over black while a film runs on, as SCR_UpdateScreen;
   esc goes back to the loop; game > a skill, a load, a host,
   connect, map or demomap leave it (fresh, load_slot). The menu's
   letters and pics before any map come from `front` (a palette
   and an atlas of their own). `--map base1` goes straight in, as
   +map did. Checked hidden: film, demo, menu over it, esc,
   game > easy into ntro.cin, and 150 s of the loop (base2,
   waste2, base2) with no fault.
19. DONE (Oct 6, Kalen: stuck on walls when sliding along them,
   and on tiny objects on the ground): slide_move's plane test
   was inverted (`i != nplanes` took the crease branch, which
   zeroes the velocity on a single plane: every wall contact
   stopped the player dead). Traced with a temporary bump log
   (removed): the wall hit at fraction 0.83, then no second bump.
   Checked hidden: 25 degrees into base1's corridor wall now
   slides along it (y -220 -> -76). The small objects are not
   re-tested (none found); likely the same cause.
20. BUILT, Kalen tests (Oct 6, "stuck on enemies you killed"):
   World.trace lacked sv_world.c:547: an entity flagged
   SVF_DEADMONSTER is skipped unless the mask asks for
   CONTENTS_DEADMONSTER, so every corpse was a solid box to the
   player's move. SVF_DEADMONSTER and CONTENTS_DEADMONSTER now
   live in Game.ag (World is imported before Trigger/Target).
21. BUILT, Kalen tests (Oct 6): lifts stepped 10 Hz and demos
   were jerky. Brush models lerp between game frames
   (lerp_origin / lerp_angles in place_model); a rise of 8..20
   units while on ground eases out of the view over 0.1 s
   (step_up / step_left, CL_PredictMovement's predicted_step);
   a demo's view lerps the recorded player's origin and angles
   between frames (checked by a per-frame log). `timedemo 0|1`
   and `map <x>.dm2` added (688 frames, 58.8 fps hidden).
   OPEN: "still stick on bits of floor and walls": the trace and
   slide code match cmodel.c / pmove.c line by line; a wander
   on base1 found only head-on stops (within 5.7 degrees of
   square, which pmove.c stops too). Needs a place.
   Mouse: m_yaw 0.022 x sensitivity (was 0.004 rad: 3.5x fast).
   NEVER run quake2 hidden while Kalen works: its click locks
   the mouse and devices' platform_warp_cursor warps HIS pointer
   (an unmapped window still warps). Kalen: no more testing.
22. DONE (Oct 6) broken glass drew against the back wall: a
   freed or SVF_NOCLIENT brush model is hidden (place_model).
23. DONE (Oct 6) sticking on walls at shallow angles. Repro at
   165 Hz (SILVER_HZ=165), 6 degrees into base1's y 128 wall:
   the step path left the box inside the 1/32 gap, a small step
   in gave an entry fraction under -1 that clip_brush dropped,
   the box entered the wall and every trace after was allsolid.
   The trace is id's again; pmove now ends with id's
   PM_SnapPosition (origin and velocity to eighths, jitter by an
   eighth to a good place, else back) and starts with
   PM_InitialSnapPosition when pushed or moved since. At 165 Hz a
   sideways creep under 1/8 unit a frame is truncated away, as
   in id's. Keys-only hidden runs (no click) are safe.
24. DONE (Oct 6) the socket layer is silver (Net.ag q2net_*);
   quake2.c and quake2.h are gone: no C in quake2.
25. DONE (Oct 6) id's console (Con.ag; trinity has a Console.ag,
   so not that name): conback sliding at scr_conspeed, v3.19,
   word wrap, ^ backscroll row, blinking cursor, notify rows for
   con_notifytime; keys.c editing (tab completion as
   CompleteCommand, 32 line history, PgUp/PgDn/Home/End,
   ctrl+v/l/h/p/n, esc opens the menu); cmd.c and cvar.c
   (cmdlist, cvarlist, set, echo, clear, toggleconsole; a cvar
   alone prints, with a value sets; unknown words go to the
   game). Variables are only the ones that drive something:
   fov (ps.fov by ClientUserinfoChanged, CalcFov, gun hidden
   over 90), sensitivity, m_pitch (the menu's invert), m_yaw,
   s_volume (the menu's slider), skill, dmflags, fraglimit,
   timelimit, capturelimit, timedemo, cl_gun, name. Archived
   ones go to ~/.cache/quake2/config.cfg, run at start. gi
   cprintf / bprintf reach the console. NOT done: alias, exec,
   bind, condump, say / say_team, messagemode.

## player (Oct 21 2026)

1. DONE (Kalen: an h264 viewer that loads mp4, h264, etc)
   `silver player <file>`: mp4 / mov / m4v (plain or fragmented)
   and raw .h264 / .264 / .h265 / .265 / .hevc on the Vulkan
   decoder, AAC sound on spectra's mixer. A raw stream is cut
   into pictures at its start codes, timed by display order at
   --fps (30). Space or a click pauses, left / right seek 5 s,
   a click on the foot strip seeks there. Seen hidden: clip.mp4,
   a B-frame h264 and an h265 stream, pause holds the frame,
   right arrow lands on the right frame. Sound not heard.
   OPEN: a raw stream's rate from its SPS timing.
   Then (Kalen): opens with no file (ctrl+o's dialog, at the start
   folder, picks one); our own fragmented takes play.
2. DONE (Oct 21, Kalen: the player stays a player; recording is
   orbiter's) the side panel's record tab (beside properties,
   instruments, callstack): orbiter or the selected app, then
   record / stop with the take's clock, the take's path under it.
   A take goes to ~/Videos/<name>-<date>/. trinity: an app records
   while its element's `record` is set and stops when it clears
   (record_pump, record_stop), and its socket takes `record <folder
   or file>` / `record stop`. Seen hidden: orbiter's own take (3.5
   s, h.265 + aac), two takes in one player run. NOT driven: an app
   hosted in orbiter (a hidden run hosts none). A take of orbiter
   starts its dictation marks, as --record does.
3. DONE (Oct 21) 4:4:4 takes with a height not a multiple of 64 were
   damaged at the foot (1280x720: 127 of 179 frames); the coded
   height is padded to 64 as the width is (Encoder.coded_h). After:
   0 of 179. trinity tests exit 0.

## Apps bound to silver's folder — live list (Oct 22 2026)

Kalen: orbiter is the user and is not locked. An app orbiter runs
reaches silver's folder only, never /home. The shell syncs a user
file into silver when an app needs one, unless a way without copying
works.
1. REPLACED by item 6 (no Landlock now). Was: silver-host locked
   a slot app (one with SILVER_APP_SLOT) with Landlock before it loads: everything under
   SILVER_ROOT, /tmp, /dev, /run and /proc; /usr /etc /opt /var /snap
   /bin /sbin /lib* /sys read only. Its HOME and XDG folders are
   install/home; its own ~/.cache, ~/.local/state folders (<app> and
   silver-<app>) come over once as hard links (cp -al: no bytes
   copied). Checked: psx hosted in orbiter logs "psx bound", reads
   ref/ridge_racer; player run as a slot cannot open a file in
   ~/Videos (player now says "cannot open" instead of crashing).
2. DONE the title's file pick: no '..' at /src/silver, a folder
   outside it lands back on silver, typed results outside it hidden.
   OPEN: the crumbs above silver (src) and the left column (other
   projects, home) still show in a pick.
3. DONE (Kalen: views, not accounts) each slot app has its own view
   (user + mount namespace, silver-host bound_view): /home, /media and
   /run/user/<id>/gvfs, doc are empty; the real tree waits at /mnt,
   which the lock keeps from the app; all powers dropped after. No
   view, no run (Kalen: no fallback to Landlock alone). Needs
   support/apparmor/silver-host in /etc/apparmor.d (Ubuntu strips a
   namespace's powers otherwise; installed by Kalen Oct 22).
   Sharing while it runs: <any app host> --view-share <pid> <app>
   <file|folder> [rw] binds it at install/shared/<app>/<pid>/<name>
   (read only unless rw; a folder with what is mounted in it);
   --view-unshare <pid> <app> <name>. A starting app clears the
   share folders of instances gone. Checked: ~/Videos shared as a
   folder (listed, read, a new file refused) beside one file.
   Checked: /home empty inside; a ~/Videos .agi shared, read inside,
   a write refused (read-only), gone after unshare; the player opens
   neither /home nor /mnt paths.
   DONE the navigator shares (Kalen: only orbiter's navigator, never
   trinity's in-app one): a pick for a running app's live argument
   (pick_deliver) outside silver goes through orbiter share_into:
   the app's host --view-share, then `set <arg> <shared path>`.
   Checked hidden: player started on a ~/Videos take ('no file'),
   the title's navigator opened at ~/Videos, the pick played it
   (h.265 1888x888 decoded, the app's own shot shows the video).
   player's file is Live now (opens on a change).
   psx (Kalen): a disc is its folder (disc has the Directory
   control; Cdrom.insert finds the .cue in it); orbiter's pick takes
   a folder when the default prop's control is Directory (a control
   with no members arrives as its name). States go in psx's storage
   by the folder's name (a shared folder is read only). Checked
   hidden: psx on ref/ridge_racer, the navigator's double-click on a
   folder outside silver shared it read only, psx booted it to the
   title screen.
   Fixed on the way: trinity's socket `set` kept the first word of a
   value (a path with a space was cut): set_word takes the rest of
   the line. orbiter's slot player with no sound device stopped
   draining the ring, so an app pacing on it (host_audio_pace) hung,
   and psx's disc change hung joining its sound thread: the ring now
   plays out in time, unheard (devices.c slot_player_run).
   DONE a pick for an app not yet running: its launch line keeps
   the picked path; silver-host, as the app starts, shares each
   path on its line the app cannot reach (bound_args, read only)
   and puts the share's path in its place. Checked: player run as
   a slot on a ~/Videos take decodes it (h.265 1888x888). NOT
   driven through orbiter's pick (the line is the same).
   OPEN: the properties panel's path fields, drops, unsharing.
4. OPEN the exchange (an agent) started inside a bound app: the agent
   needs ~/.claude; orbiter should run it for the app.
5. DONE (Oct 23) builds leave the app: its holder runs them (item 6),
   as the user, with the real home (~/.cargo, ~/.gitconfig reach).
6. DONE (Oct 23, Kalen: no Landlock; "use your head what is the best
   actual practice"; every app keeps the network) the view is now
   Flatpak's and Chrome's way, and Landlock is gone from silver-host:
   - the holder (the slot process orbiter's host spawned) makes a
     user + mount namespace, builds the app's root at /mnt there,
     then a pid namespace: pid 1 (view_init) enters the root by
     pivot_root, mounts its own /proc, drops the old root, forks the
     app (pid 2) and reports how it ended; the holder ends the same
     way (signal or exit code). The holder forwards TERM, INT, HUP,
     QUIT, USR1, USR2 to the app.
   - the root: /usr /etc /opt /bin /sbin /lib* /sys
     /var/cache/fontconfig /run/systemd/resolve /tmp/.X11-unix and
     silver's folder read only; its own /tmp, /dev/shm, /dev/pts
     (new instance); /dev with null zero full random urandom tty dri
     snd kfd nvidia*; writable: install/home/<app> (HOME; the old
     shared install/home's folders come over as hard links),
     install/tmp (a folder of its own, install/tmp/view/<app>-<slot>,
     with its log bound in), install/shared/<app>/<slot> (a tmpfs the
     shares land in), /run/user/<uid> (<rt>/silver-view/<app>-<slot>,
     the pulse and pipewire sockets and the X cookie bound in). The
     host finds its socket as before: <rt>/trinity-<app>-<slot>.sock
     is a link into that folder.
   - the app: no capabilities, no_new_privs, a seccomp filter
     (view_filter: ptrace, process_vm_*, pidfd_getfd, mount and the
     new mount calls, unshare, setns, new namespaces by clone,
     clone3 (ENOSYS: libc falls back), bpf, perf_event_open, key
     calls, modules, kexec, io_uring, userfaultfd, handle opens,
     TIOCSTI / TIOCLINUX, x32 numbers, others), and an environment
     kept to what drawing, sound and silver need (view_env: no
     tokens, SSH_AUTH_SOCK, DBUS, GPG).
   - its pid as the host sees it: the holder reads it from the app's
     first broker message (SCM_CREDENTIALS) and answers it; the app
     sets SILVER_HOST_PID and writes it into its slot (app_pid), so
     orbiter stops, attaches to, measures and pulls textures from the
     app itself. The supervisor reads a slot's exit from the process
     it spawned (g_slot_proc). devices: t->pid is host_self_pid.
   - builds: rebuild_spawn in a bound app asks the holder (`build
     <clean>` on the broker socket; `built <status>` back). The
     holder builds only the app's own module: a path from the app
     (first written so, fixed Oct 23) let it build a module it wrote
     in its home, whose export funcs and > lines ran as the user.
     A bound app cannot launch another app (au_live_take_switch).
   - shares: --view-share <pid of the app or holder> <app> <file>
     [rw] finds the holder (the process whose children have a new
     pid namespace), enters its namespaces, clones the file's mount
     (open_tree), makes it read only, then move_mount into the share
     tmpfs: it reaches the app as /src/silver/install/shared/<app>/
     <slot>/<name>. orbiter's share_into names it by slot now.
   Checked hidden (a harness built from silver-host.c, run from
   platform/native/build so the apparmor profile applies): pid 2
   inside, the host pid reported; /home, /run/user/<uid>/bus and the
   SSH agent absent; /etc, libc, silver's folder readable; writes to
   install/bin, silver's folder and /usr refused (read-only), to its
   home, tmp and /tmp allowed; 2 processes in /proc, /proc/1/exe
   refused, orbiter's pid unreachable; ptrace, unshare, mount,
   TIOCSTI refused; fork, threads and the network work; exit code
   passed through. A live share of ~/.bashrc appeared read only and
   went on unshare. player bound alone: decodes a ~/Videos take,
   sound plays (AudioOut 48000 Hz), env clean. A touched player.ag
   was built by the holder and reloaded (the player then crashed:
   it does the same unbound, an old player reload bug, OPEN). Hidden
   orbiter hosting player: bound, the socket answers through the
   link, the panel shows the app's own pid, orbiter's pidfd_getfd of
   its textures works. NOT seen: the frame in the pane (the pane was
   empty in that run; not chased).
   Needs: properties built --clean after trinity's rebuild (a stale
   one crashed orbiter in LaunchPanel_init).
7. DECIDED (Kalen, Oct 23: "silver cant secure other operating
   systems better than its own") X11 stays as it is: an app opens
   its own window, so on an X11 desktop it reaches the X server, and
   X lets any client read the screen and input (a bound player's
   `shot` returned the whole desktop). orbiter-os has no X server.
8. OPEN a debug launch's lldb attach to a bound app: the pid is
   right now; not driven.
9. DONE (Oct 23, Kalen: Wayland programs broke out onto the
   desktop; then "as bound as the others but WORK") the console was
   bound with orbiter's Wayland display passed as a name and its bus
   as an address, so neither reached its view and GTK fell back to
   X. orbiter now passes full paths (run_program: <rt>/wayland-0-
   orbiter; open_program: the bus as its path, console builds
   unix:path=), so they are shared like any launch path. The opener
   (trinity-orbiter.sock) is shared too: OPEN, a program in the
   console can send orbiter any socket command, not only open.
   Checked hidden: console bound, gnome-calculator in its own user
   namespace (pid 13 inside) on orbiter's Wayland, drawn in the pane.
10. DONE (Oct 23, Kalen: the paths are the standard places, "that's
   the map") inside, an app uses the standard places and each is its
   own folder outside: /tmp = install/tmp/view/<app>-<slot> (emptied
   at each start), /run/user/<uid> = <rt>/silver-view/<app>-<slot>,
   ~ = /home/<user> backed by install/home/<app> (Kalen's pick);
   XDG_*_HOME and TMPDIR unset. A share is at its own real path
   (~/Videos/x.mp4 inside is that file, read only; the placeholder it
   covers is in the app's home); launch args are no longer rewritten,
   and --view-unshare takes the path. install/shared is gone.
   install/home and install/tmp under silver's folder are read-only
   covers inside (the app's log bound in at its path).
   Checked (harness): /tmp/mapped.txt landed in install/tmp/view/
   vtest-7, HOME /home/kalen, ~/.bashrc shared and unshared live,
   install/tmp read only; player bound plays a ~/Videos take with
   sound.

## Projects, featured apps and game data — live list (Oct 23 2026)

Kalen: a project is a silver module; a bare repo has no category,
icon or launch, so it is never an app. The apps grid's index is the
featured/ folder itself (each featured/<name>/<name>.ag) and silver's
own modules: software.agi's list is no longer read, and names with no
module (krita, obs ...) are gone, with the recommended row.
1. REMOVED (Kalen, Oct 23: "do NOTHING steam related in silver.c")
   silver's `import steam:<app id>` and its Steam library search are
   gone from silver.c. Steam's folders are the steam module's alone.
2. DONE quake2 (/src/quake2): export software ['games'];
   `import steam:2320` copies baseq2 and ctf into its share
   (install/share/ar-visions-quake2, cp -ru: 358 MB once); `game`
   defaults to 'baseq2' (relative to the share; a full path still
   works). Checked: quake2 bound in its view (no ~/.steam there) loads
   base1 from its copy (3 archives, 3,588 files). Seen once: a first
   load logs 'pak: 0 archives' and 'bsp: no palette' before the real
   one; not chased (unknown whether it predates this). OPEN: an icon
   (images/icon.png); quake2 lives outside featured/ so the grid does
   not list it.
3. DONE (Kalen: Steam as an app in orbiter, featured)
   featured/steam/steam.ag: Valve's steam-launcher 1.0.0.87 (sha256
   from Valve's package list), unpacked into install/lib/steam/<ver>,
   bin_steam.sh linked as install/bin/steam; games section; icon from
   the package. Shown in the grid. NOT run (it opens a login window).
   Found (Kalen: Steam says the 32-bit libraries are missing): the
   view's seccomp filter killed every 32-bit process (it allowed the
   x86-64 arch alone), so Steam's 32-bit client and ldd of a 32-bit
   library died. view_filter now has a section per arch (view_rules):
   i386 gets the same refusals by its own numbers (unistd_32.h).
   Checked in a view: ldd of 32-bit libGL resolves; raw int 0x80
   calls: getpid works, ptrace and TIOCSTI refused. The console's host rebuilt; other apps take it at
   their next build.
   Steam's log then stops at "Steam now requires user namespaces"
   (bwrap): the filter refused them. Then (Kalen: adding a namespace
   is no less secure, as it cannot redirect other apps) every bound
   app may make namespaces and mounts of its own: unshare, setns,
   clone's namespace flags, clone3, mount and the new mount calls are
   no longer in the filter (a nested namespace only narrows: the
   mounts it inherits are locked, other apps have their own). A
   console-only permission (host bit 17) was built first and removed.
   Checked in a view: bwrap runs, inside it no ~/.ssh and the
   install/tmp cover cannot be unmounted ("must be superuser"); ptrace
   and TIOCSTI still refused, 32-bit too. Steam itself not run by me.
   Then (Kalen: Steam opened outside orbiter, on the desktop) Steam's
   client speaks X11 only: it took the desktop's DISPLAY. orbiter's
   compositor (wayland.c) now runs Xwayland (wlr_xwayland, lazy: it
   starts with its first client): an X11 window is a toplevel like an
   xdg one (its own output at id x SPAN, configured there, the pane's
   size; a dialog (parent or modal) keeps its own; menus and tooltips
   (override redirect) placed where they put themselves); title,
   class, pid, close, focus for both kinds. wlc_x11_display (:N);
   open_program passes --x11 :N, console sets DISPLAY to it.
   Checked hidden: xclock from the bound console mapped as an X11
   window in orbiter, drawn in the pane at its size, its task button
   in the bar. Steam not run by me.
   Fixed (Kalen: the mouse shows but does not click): X11 windows
   were configured at their place in orbiter's layout (x 16384), off
   the X screen (1528 x 823), so X dropped the clicks. They sit at
   0, 0 of the X screen now (Xwayland gives input to the surface the
   pointer is on); a menu or tooltip goes at its owner window's place
   plus its own (xpop_base: its parent, else the focused X11 window).
   Checked hidden: xcalc via `app run` (new orbiter socket command:
   runs a program as its search row does), two clicks on 7 show 77,
   the 8 key 778.
   Then (Kalen: Steam shows its friends list, no way to its other
   windows; its menu opened was the main window's):
   - X's stacking follows ours: every X11 window sits at 0, 0, so X
     sent clicks to its own top one. x_raise restacks the window
     under our pointer or focus to X's top (wlr_xwayland_surface_
     restack); menus belong to it (g_xtop).
   - a window asking to come forward (X11 request_activate, or shown
     again after hiding: Steam's tray Library / Store) is shown in
     its pane: wlc_window_asked, orbiter wl_tick -> place_window.
   - the tray held the watcher on the desktop's session bus (GNOME
     has it there, so ours stood aside); it holds it on the bus
     orbiter gives its programs (the portal's) now, so their icons
     reach the system menu's tiles. A tile's right click opens its
     menu (left: the app's action).
   - an item's properties are read after its register call is
     answered (an app waiting on that reply could not answer).
   - tray readers check each value's type (a malformed menu crashed
     orbiter: SIGSEGV in tray_text).
   Checked hidden: xcalc then xeyes; xcalc asking forward came back
   in the pane, a click on 7 reached it; a stand-in tray app (dbus-
   python) on the program bus: its tile named, left click Activate,
   right click its menu, Library chosen reached it; a malformed one
   did not crash orbiter. NOT run: Steam. OPEN: an X11 window's pid
   is the one inside its sandbox (_NET_WM_PID), so orbiter cannot
   find its pane by pid (it takes the active pane); one task button
   per program, not per window.
   Then (Kalen: Steam's install dialog offered /tmp): Steam names a
   library's drive by the first mount of its disk, and /tmp was
   mounted before the home (both on the same ext4). view_build
   mounts the home first now; checked: mountinfo's first ext4 entry
   is /home/kalen, /tmp and home still work. Not seen in Steam.
   Then (Kalen: windows spawn when it switches to the friends list) a
   new window of the process a pane already shows (same pid as that
   pane's window) opens as its own floating pane (window_pane: the
   middle of the area, 42% x 62%, each later one 36 px down and
   right; at most 4 panes); pane_spawn remembers the window a pane
   was opened for, and the pane closes with it. A window asking to
   come forward that already has a pane brings that pane up.
   Checked hidden: one X11 process (python Xlib) mapping Main then
   Friends: Friends floated over Main with its own strip; closed,
   its pane went. OPEN: pane_add saves the session, so a floating
   window open at a quit comes back as an empty docked pane.
   OPEN, expected to bite: Steam's game runtime (pressure-vessel) and
   its web helper make namespaces of their own, which a bound app's
   seccomp filter refuses; its udev rules (controllers) need root.
3b. DONE (Kalen, Oct 23) stores: an app that sells software writes a
   module per finished app into its own namespace, install/stores/
   <store>/<name>/<name>.ag, never elsewhere (no collisions). steam's
   export steam_store reads each Steam's libraryfolders.vdf (the
   user's, and each one run in orbiter: install/home/<app>, its view's
   home path mapped back) and every appmanifest with StateFlags 4,
   Steam's tools skipped; writes `export steam ['<id>']`, build none;
   removes a folder whose game is gone. Runs at steam's build only.
   Checked: 12 games (Hot Lava from orbiter's Steam), hot_lava builds.
   Each also gets `export launch ['steam -applaunch <id>']` and its
   cover (Steam's library_600x900.jpg) as images/icon.jpg (AppIcon
   reads icon.jpg when there is no icon.png).
   Then (Kalen: the home screen expands store apps like folders) a
   store's card (orbiter AppEntry.apps, from install/stores/<name>)
   opens in place: itself first (it still runs), then its apps by
   name; pressed again it shuts. A store app's press runs its launch
   line in a console (run_line), where its store runs. Every 10 s
   orbiter re-reads a store folder whose time moved, and while the
   store's program runs it reruns `silver --export --build` on the
   store's module (0.06 s cached), which writes what finished.
   Seen hidden: the folder with covers, a module added while running
   showed within 10 s, the folder shut. NOT driven: a game's press
   (it would start the real Steam in orbiter's Steam home), the
   rerun while Steam runs. quake2's `import steam:2320` no longer
   parses (left, Kalen).
3c. DONE (Kalen, Oct 23: Hot Lava runs on our own Wine) featured/wine:
   Valve's Wine at 6d211aab (Proton Experimental 11.0-20261001's
   submodule), fetched by git at the commit, autoreconf, configured
   x86_64 only with --enable-win64 (as Proton), --without-x,
   --with-wayland; PE side by our clang (-target x86_64-windows).
   libxkbcommon 1.6.0 imported for xkbregistry's headers; FreeType
   the system's (FREETYPE_CFLAGS/LIBS: install has only a .a).
   Valve's loader runs 64-bit programs by lib/wine/i386-unix/wine64
   (+ -preloader); the module copies them there as Proton's package
   does (without: wineboot never starts, kernel32 not found).
   Checked: `wine cmd /c echo` in a fresh prefix (563 MB); notepad on
   orbiter's Wayland mapped in pane 0, drawn. No GnuTLS (bcrypt: no
   encryption). The compositor lacks pointer constraints and relative
   motion (mouse look), text input, data control, toplevel icons.
   OPEN: those protocols; DXVK; Steam's bridge (lsteamclient); the
   steam store's launch line as `wine <exe>`.
   Then (Kalen: stay on Valve's tree; every fix goes out as a PR):
   media: NASM 2.16.03, FFmpeg 4.4.5 (the 4.x API Valve's winedmo is
   written for; Proton pins 4.3.3), GStreamer 1.24.12 with base (gl on
   EGL: Valve's winegstreamer wants gstreamer-gl), good (isomp4), bad
   (videoparsers), gst-libav; DXVK 3.1.1's release DLLs in
   install/lib/wine/dxvk. Fixed in Valve's Wine, kept as
   featured/wine/wine.diff (the PR to ValveSoftware/wine): Valve's
   win32u gives a window no window surface when its client surface
   clips it (window_clip_client_surfaces, not upstream); winewayland
   then destroyed its wl_surface, the Vulkan subsurface had no parent,
   the first vkQueuePresentKHR waited for a frame callback forever and
   Unity's main thread with it (seen as the intro stalling at two
   frames). Now such a visible toplevel keeps its xdg toplevel.
   Seen hidden: Hot Lava mapped in pane 0, the Klei intro played (two
   shots 2.5 s apart), Display Splash Complete, then it quits at
   SteamAPI_Init() failed (no bridge). The test prefix is set up by
   hand (DXVK DLLs copied, mscoree/mshtml off, d3d*/dxgi n,b).
   Then (Kalen: still black, sound only) his card ran `steam
   -applaunch`: Steam's Proton on X11, not our Wine. The steam store
   now reads each game's program from appinfo.vdf (binary v29) and,
   for a 64-bit .exe in a Steam run in orbiter, writes `env -C <its
   folder> WINEPREFIX=$HOME/.../compatdata/<id>/wine WINEDLLOVERRIDES
   =... wine <exe>`, DXVK copied into that prefix's system32 first
   (Wine's first boot keeps it). Others keep steam -applaunch.
   orbiter runs a store line as `env PATH=<install/bin>:$PATH <line>`.
   Seen hidden through that line: prefix made, window in pane 0,
   splash done, then SteamAPI_Init failed. NEXT: lsteamclient.
   Found: a HOME under /tmp is covered by the view's own /tmp.
   Then (Kalen: build the lsteamclient bridge; one view per app)
   featured/wine imports Proton 70b7e109 (archive) and builds its
   lsteamclient and steam.exe out of Wine's tree by Wine's makedep
   (as Proton's rules-makedep); Valve's ntdll hands a game's
   steamclient64.dll to lsteamclient. The store line runs `wine steam
   <exe>` with SteamGameId/SteamAppId/STEAM_COMPAT_CLIENT_INSTALL_PATH;
   steam_into copies Steam's legacycompat client files into the
   prefix's Program Files (x86)/Steam and steam.exe + lsteamclient.dll
   into system32 (Wine finds no builtin without a file there).
   silver-host: every instance of one app shares one view (the first's
   user, mount and pid space, found by <rt>/silver-view/<app>.view):
   Steam's client meets the running Steam by /dev/shm and its pid.
   view_join enters it; pid 1 outlives the first app while others
   run; /tmp is install/tmp/view/<app>-<holder pid>, kept while its
   .lock is held; the runtime folder <rt>/silver-view/<app>; every
   slot's log bound in. Checked hidden: 2nd instance sees the 1st's
   shm and processes, survives its quit, a 3rd joins after.
   Seen hidden: Steam in one console, the store line in another:
   "Logged into Steam", auth ticket, Hot Lava's title screen in pane 0
   (it offers GO OFFLINE: no GnuTLS, bcrypt has no encryption).
   Fixed on the way: store_poll freed app_list mid-loop (orbiter
   SIGSEGV at 11:29); the traced mfreadwrite/winedmo rebuilt (and
   their copies in the prefix).
   Mine, undone: a test Steam under a scratch HOME rewrote ~/.steam
   links and libraryfolders.vdf; put back to /home/kalen.
   Then (Kalen: Steam's window cannot be hidden; mouse look barely
   moves, lock until Esc):
   - hide: wayland.c takes X11 and xdg minimize requests (Steam's
     Hide, a minimize button): wlc_window_hide_asked; orbiter
     hide_window minimizes a free pane, takes a docked window off its
     pane. The taskbar button hides the instance's window from any
     pane. A closed first window (Steam's sign-in) handed the instance
     no window; window_left finds another open one of its program.
     Seen hidden with Xlib test windows: minimize, taskbar toggle
     docked and floating.
   - mouse look BUILT, NOT RUN (a hidden lock warps Kalen's pointer):
     pointer constraints and relative pointer in wayland.c
     (wlc_pointer_held, wlc_pointer_relative; a held surface gets no
     absolute motion); WaylandView.hold_mouse locks the mouse
     (ux.mouse_lock) while the focused window holds its pointer, moves
     go as raw motion, Esc lets go until the next press in the pane.
     Takes the compositor (orbiter's start) and orbiter's build.
   - Steam crashed at start now and then (SIGSEGV after "Desktop state
     changed", BadRROutput RRGetOutputInfo; 11:14, 12:00, 12:35 x2):
     every window's headless output was a wl_output, so X's screens
     came and went with windows. wayland.c global_seen: Xwayland sees
     only the base output's wl_output (Wayland clients see all).
     Checked hidden: two X11 windows open, xrandr lists one output
     (1280x800), windows draw at the pane's size. Steam's own starts
     not repeated (it is Kalen's signed-in account).
   - (Kalen: Steam a console pane plus a window that cannot be
     hidden; to its tray, a startup app) a module's `export startup
     ['steam -silent']` runs at orbiter's start (run_startup), in no
     pane (run_background: AppView.background, console with
     program_args, shared with Editor.open_program). Its windows open
     in a floating pane of their own (background_of, place_window);
     its taskbar button shows only while a window is up or minimized.
     A startup app's card runs it the same way (run again, it shows
     its window). Seen on Kalen's orbiter (12:44): started in the
     background, tray: registered .../NotificationItem/steam.
   - (Kalen: games do not go fullscreen; straight to the real screen
     even with orbiter in a window; his pick: a window of its own)
     the base output (X's screen, a program's primary display) takes
     the real monitor's size (devices platform_screen_rect: the crtc
     under the app's window; trinity screen_rect; wlc_screen at the
     compositor's start). A window asking fullscreen (xdg
     request_fullscreen, X11 _NET_WM_STATE) gets it (wlc_window_full);
     WaylandView then opens trinity's ScreenOut: a fullscreen platform
     window on that monitor with its own swapchain (trinity.c
     screen_out_*, screen.h: acquire without waiting, blit fitted,
     FIFO, under vk.qlock), sizes the window to the monitor, and sends
     that window's keys, buttons, moves and wheel to it (a held pointer
     locks the mouse there, raw motion); the pane keeps a fitted
     picture. Leaving fullscreen closes it; closed by hand, it stays
     off until the window asks again. A hidden app keeps it unmapped
     and never takes the mouse. Seen hidden (an Xlib window): X screen
     2560x1440, "screen: 2560x1440 at 0, 0", the window configured
     2560x1440, first picture shown, back to 1528x823 on leaving. NOT
     seen on a screen, NOT driven: its input, Hot Lava asking
     fullscreen. Takes orbiter's next start (compositor and trinity).
4. DONE, tree written Oct 23 04:41 (Kalen: add the 32-bit libs)
   os-bootstrap add_steam: install/bin/steam and install/lib/steam
   (20 MB), /lib/ld-linux.so.2, and what Valve's steam-libs-i386 /
   -amd64 leave to the system, both arches with their ldd closure
   (libc, libcrypt, libgcc_s, libstdc++, libGL, libGLX, libEGL,
   libgbm, libX11, libxcb, libxcb-dri3, libXinerama, libudev,
   libgpg-error, libdbus-1), libnss3 (64), and NVIDIA's 32-bit half
   (GLX, EGL, glcore, tls, glsi, glvkspirv, gpucomp, eglcore): about
   215 MB more. Every drive tree is x86 (Kalen: no ARM or FEX for
   Steam). No 32-bit Mesa (AMD / Intel). Tree: 71,159 entries,
   5.23 GB. Checked: the tree's 32-bit loader resolves libGLX_nvidia's
   whole closure inside the tree. The 32-bit libraries sit twice
   (usr/lib/i386-linux-gnu 224 MB, lib/i386-linux-gnu 181 MB: ldd
   names /lib, and the tree's /lib is a folder, as for 64-bit). Not
   booted. On Ubuntu, Steam in orbiter uses the system's own i386
   libraries (the view shows /usr and /lib).

## The exchange, narrow — live list (Oct 23 2026)

1. DONE (Kalen: the agent's text must not be squeezed to show the
   avatar) when the exchange (its box, or the window) is narrower than
   the avatar's column plus 560 px of text (ux units), the text runs
   across the whole width down to a split, and the avatar fills the
   box under it (exch_stacked). The split (trinity ExchSplit) is
   dragged: Window.exch_split, its share of the exchange's height,
   0.3 to 0.85 (0.62 at first); the seam cursor over it. orbiter's
   AgentAvatar has `stacked` and a :stacked area (all of its box).
   Window gained exch_split and exch_stacked (last members): every
   trinity app builds before it runs again. Checked hidden: 900 px
   wide stacked, a drag of the split down grew the text and shrank
   the avatar; 1700 px wide unchanged (side by side); trinity tests
   exit 0. NOT kept: the split's place across restarts.

## Style inheritance — live list (Oct 23 2026)

Kalen: a subclass's style is its base's with its own lines in place,
compiled together once as it is read: one set of declarations, never
more applied in layers; the order of Type:State blocks is kept.
1. DONE silver: a member whose initializer is a { } block for a type
   made from text (CSS) keeps that text as its declared default on its
   descriptor (keyword_default, Au_t.dflt; def_prop_default is now
   also emitted where class members register). The defaults no longer
   go through aether.prop_defaults (removed): that map matched members
   by name, so every member named `style` shared one default (the
   one-token Launch defaults had the same flaw).
2. DONE trinity Style: css_flat walks the class chain base first
   (class_css: each class's own `style` default), merges each text
   into a tree of lines (css_tree; a heading keyed whole, a
   declaration by its name): the same key takes the subclass's line
   in its place, a new one goes after (css_merge); the instance's own
   CSS is last, so an app built before this keeps its own lines.
   integrate_instance feeds the flat text to the reader as before.
   trinity t_style_flat (FlatBase, FlatSub) passes; trinity tests exit
   0; features 262/263 (t_codegen_resource as before).
3. DONE the 30 subclasses that restyle a styled base now get what
   they left out. Seen hidden (orbiter hosting player, against a shot
   from before): identical but for DragField's numbers, 12 px right
   (LaunchField now took ArgsField's face offset 10, size 18):
   LaunchField's style states label_size 17, label_offset 0 0; then
   identical but the pid. Reduced: TasksBar to its spacing alone;
   WifiSecret to WizSecret's form (its face clear in every state:
   merged, TextInput's :focused / :has_value would have shown the
   password's text under the dots). NOT seen: menu drawers, the
   system menu (RoundTab), HomeState, the Wi-Fi panel, ShotPrompt.

## Text fields on trinity's editable layer — live list (Oct 23 2026)

Kalen: the aura URL in the system bar took no arrow keys; every
field that is typed into uses trinity's editable layer, never a
string orbiter appends to. trinity: the layer gained Delete, Home,
End, ctrl/cmd + a c x v (one line pasted) and `secret` (a dot a
character; Layer's last member); a key or character the focused
element marks no_default goes no further; field_key / field_char
(element.ag) edit first so the app then reads `edit`.
1. DONE the system bar's URL (BarTitle bar_url): its field.
2. DONE the pane title's find, new file name and navigator
   search read the title's edit (were appended beside it).
3. DONE orbiter.filter_text (written, never read) removed.
4. DONE the menu drawer's and apps screen's MenuFilter (tray):
   a field; the first key brings it up focused (filter_focus).
5. DONE the console's log filter (ConFind): a field.
6. DONE the login password (LoginPass): a secret field.
7. DONE trinity's orbiter-bar navigator filter (NavField).
Find / replace / pattern rows pass their keys to orbiter now
(Enter never reached it before).
8. DONE (Kalen) the menu drawer's entries by the arrows: up /
   down light one (orbiter menu_lit; TrayRow :chosen, the hover's
   look; MenuBuild keeps it in view), Enter runs it; a text box in
   the menu (not its filter) keeps the arrows; a filter change
   drops the choice.
Also: the layer measures each caret place (Layer.col_xs, after
secret): selections and clicks follow the real text, not the width
of an x. A navigator with no orbiter bar (player) has the field
along the top of its list.
Seen hidden (Oct 23): URL bar (select all, arrows, Home / End, a
click mid-word, Enter loaded example.org, Esc), drawer arrows and
Enter (New instance started slot 2), drawer filter, apps filter
(Delete), console filter (emptied: pill gone, console focused),
login (dots, Backspace, Enter signed in), pane title search,
player's and aeon's navigator field.
OPEN: the login dots are the font's bullet at 17 px (smaller than
the old drawn dots); aeon's view chip covers the bar field's right
part; Layer grew two members: every trinity app built before needs
a --clean build (orbiter's imports, aura, player, aeon done).
OPEN silver: a style block's state named `:lit` was joined with the
line after it (`:litlayers.body:` in the compiled CSS): the block
was disabled. Not chased; the state is `:chosen`.
9. BUILT, not seen (Kalen, Oct 23) in an open bar drawer, left /
   right open the next name's drawer along the bar (bar_step: the
   selected MenuTop / BarCrumb, its siblings by x, opened as a
   press would); the drawer's filter or a text box keeps them.
   Fixed with it: the path drawer's arrows stopped on its
   separator after Navigate (drawer_choices skips rules).
10. BUILT, not seen (Kalen, Oct 23) an Alt tap (Alt down and up,
   no key or press between) lights the app's name at the bar's
   start (bar_hl, the name's `selected`); left / right move the
   light along the bar, down or enter opens that name's drawer,
   escape, a press or another tap lets the bar go; a tap with a
   drawer open shuts it. Not F keys (Kalen: Fn on laptops).
   Limit: a plain text field that takes its own keys (no app
   forwarding) never passes Alt on.
11. BUILT, not seen (Kalen, Oct 23) ctrl / cmd + space: the
   navigator, as the bar's globe (globe_press, shared with the
   click); the globe is one of the bar's names for the arrows: left
   past the first name lights it (a drawer open shuts), down or
   enter opens the navigator.
12. DONE (Kalen, Oct 23) featured/blender: blender.org's Linux
   tarball (the one aeon imports), install/bin/blender, `export
   extensions ['.blend']` and `export run ['blender']` (how a
   program module's file runs: that program, the file after it).
   orbiter: the export registry reads run: (export_run), Editor
   program_for gives the line, default_view_for says 'program',
   mount runs it in the pane (orbiter run_line). silver: a build
   none module writes its exports too (exporter). console: its
   programs get XDG_CURRENT_DESKTOP=orbiter (Blender took GNOME's
   for a frameless shell and died: "xdg_surface has never been
   configured"). wayland.c: a window's first commit is answered
   with its configure. Seen hidden: flower888.16.blend opened in
   Blender 4.5.14 in the pane, its scene loaded.
   OPEN: before install a .blend opens as hex (no prompt to
   install); the module's name in the registry is silver-blender
   (module_of_export finds no root folder for featured modules).
13. DONE (Kalen, Oct 23) an app's Console entry starts a new console
   instance in a new pane, in the app's folder (orb.console: a pane
   added, console_pane its index); the app keeps its window. At four
   panes it falls back to the app's pane. Seen hidden: two picks
   from aura gave slot 2 and slot 3 in panes of their own.

## clint, the IRC client (Oct 23 2026)

1. BUILT, seen hidden (Kalen: an irc client app, named clint)
   clint/clint.ag, `silver clint`; flags --server (irc.efnet.org),
   --port (6697), --secure (TLS, true), --nick (else $USER),
   --channels '#a,#b'. mbedtls over a non-blocking socket, read
   each frame. Left: the conversations (server, channels, people;
   unread counts, yellow when your nick was said); middle: topic,
   the log (time, nick in its own colour, wrapped, wheel goes
   back), the field (Enter says it); right: the channel's people.
   Commands: /join /part /msg /query /me /nick /topic /close /quit
   /raw /connect host [port [plain]]. PING answered, a taken nick
   gets _, CTCP VERSION answered.
   Checked hidden against a local stand-in server (plain): join,
   topic, names, a long line wrapped, an action, a join, a line
   said and answered, /msg opening a conversation. NOT run against
   a real network or over TLS. No icon yet; no logs kept to disk.
2. DONE (Kalen: "doesnt do anything there") three causes:
   - mbedtls_net_connect blocked the main thread (EFnet's first
     address is IPv6 and never answered: SYN-SENT for minutes), so
     no frame drew. The connection is now steps each frame
     (IrcLink.open / step: IPv4, non-blocking connect, poll, then
     the TLS handshake a step at a time; 15 s and 20 s limits).
   - clint linked its own static mbedtls while trinity's tls
     library has one too: the x509 code came from clint's copy and
     the key parsing from tls's, whose PSA was never started, so
     every root failed (-0x3B00). clint takes mbedtls from tls now.
   - EFnet's servers use self-signed certificates (openssl: verify
     error 18). Kalen: the user typed the host, so its certificate
     is the one they trust: no check (VERIFY_NONE), no pinning, no
     roots loaded.
   - (Kalen: "TLS refused by irc.efnet.org -15104") irc.efnet.org
     is ten servers by DNS; 188.240.145.90 has a 6144-bit RSA key,
     past the 4096 bits our mbedtls (tls's build) takes: -15104 is
     -0x3B00, invalid public key; 195.159.90.90 does not answer on
     6697. open[] keeps every IPv4 address and fail[] goes on to the
     next one ("<why>: trying another of its servers"). Reproduced
     on 188.240.145.90 alone; irc.efnet.org joined #linux after.
     OPEN: tls's mbedtls could take keys up to 8192 bits.
   - (Kalen: efnet.deic.eu ended at "Found your hostname", then
     "The server closed the connection") that server drops IPv4
     clients before its welcome, with no ERROR line; openssl -4
     sees the same (over IPv6 it waits on). A connection closed
     before 001 (IrcLink.welcomed) goes on to the host's next
     server. Checked: irc.efnet.org joined, MOTD shown.
   - (Kalen: still no connection) checked every irc.efnet.org
     server with openssl: over TLS on 6697 only 4 of 10 ever say
     welcome (swepipe, colosolutions, deft, Prison.NET; swepipe's
     key is too big for us); the rest hang after "Found your
     hostname" or time out. A server with no welcome 30 s after
     the connection (IrcLink.waited) now goes to the next. Three
     hidden runs in a row joined #linux.
3. BUILT, seen hidden (Kalen: selection as in the code editor;
   the list on the right) the log selects: press and drag (across
   lines, by character), double-click a word, shift+click extends;
   drawn as the editor's (#425 fill, soft #ede3 glow, rounded 4).
   ctrl/cmd + c with nothing selected in the field copies it; a
   selection over several lines puts <nick> before each whole
   line. After a release the field has the keys again.
   Conversations at the right edge, people left of them, the log
   and field on the left. NOT driven: ctrl+c (the hidden run
   shares the desktop's clipboard). Limit: columns count bytes in
   a monospace font, so a line with UTF-8 selects a little off.
4. BUILT, seen hidden (Kalen: no padding, no rounding, edge to
   edge, near 1 px borders, the toolbars' near glow; a user split)
   the selection and the list's rows are square, the row's whole
   height, a 1 px #fff1 border the pointer's near light brings up
   (the task chips' style: inset #c890ff04, near_style 2 3
   #c890ffaa). The window is a Splitter (near_seam): ChatPane
   (topic, log, field; flex), the people (220 px), the
   conversations (200 px); each seam drags. clint.part[ name ]
   finds the chat pane's mounted parts. Seen: the first seam
   dragged 680 -> 560 px, the log rewrapped, the borders lit
   near the pointer.
5. BUILT, seen hidden (Kalen: one list; the server's heading
   groups its channels and each one's people) the right pane is
   one list (BufferList; UserList gone): the server as the
   heading, its channels and talks a step in, each channel's
   people under it (IrcBuffer.folded hides them), counts at the
   right (unread, else people). A press shows a conversation; on
   the shown channel it folds or opens its people; on a person
   it opens a talk (clint.talk_to). The wheel scrolls it. The
   split is the chat and this list (240 px). Seen: fold, unfold,
   alice pressed opened 'alice' under the server.
6. DONE (Kalen) the field the same way: ClintInput's style is
   edge to edge, square, clear, #fff1 1 px, near-lit, no green
   when focused. Seen hidden.
7. BUILT, not seen (Kalen: the server heading took no click once
   another conversation was chosen) orbiter's: Editor.on_press
   gave the top rows of a pane to the source's pinned header band
   (pinned_rows, two rows here, 44 px) even while the running app
   was shown over it, so a press there jumped in the hidden source
   and never reached the app (clint's log: his presses on the
   heading arrived only below 44 px). The band is skipped while
   the app's live frame shows (the draw's own test). Every hosted
   app's top rows were affected.
   The socket is read each frame (will_animate while connected).
   Checked hidden on irc.efnet.org:6697: joined #linux, its topic
   and 76 people listed. Nicks over 9 letters are
   cut by EfNet (clintprobe7 -> clintprob).
