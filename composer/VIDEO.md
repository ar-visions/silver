# VIDEO: the composer's operations

The composer turns a take into a finished video. A take is a
folder, a module of its own: `--record take` makes `take/` (if it
is not there) and records into it. Inside it, one stem:

- `take/take.mp4`: video and audio only.
- `take/take.agi`: the context and the dictation marks.
- `take/take.composition.agi` and `take/take.final.mp4`: the
  agent's composition and the render.
- the resources, in the same folder, built up by both. The user
  starts it with the files they have and names them in a
  dictation or the context ("use take/intro.mp4 at the start");
  the agent adds the images and clips it finds for the marks.
  The folder only grows: each run adds to it, the user can add
  or remove between runs, and the composition references it all as `take/<file>`.

The agent reads the `.agi` and this file, gathers the resources,
and answers with a composition: a list of operations in agi. The
composer renders it into a new, compressed mp4. Nothing is
ever written into the source take.

## Time

Every time is in seconds on the take's own clock, the same
clock the marks use (`at`, `dur`). The output's clock is what
remains after the crops; the composer maps it, the agent never
does.

## Input: the take's .agi

```
context: 'a speedrun session, keep it tight'
marks:
	e0: RecMark
		at: 12.40
		dur: 3.10
		text: 'put a bugs bunny clip right after I finish this race'
		file: '/src/silver/asnes/asnes.ag'
```

- `context` is the user's hint for the whole take.
- A mark is one dictation: spoken at `at` for `dur` seconds.
- A mark that is an instruction to the composer is always
  cropped out of the output: its words were meant for the
  composer, not the viewer.

## Output: the composition

The agent writes `take.composition.agi` beside the take. Each op is
one serialized call: the composer runs every Fetch, then every
Crop, then every Insert (composer.ag: Op, Fetch, Crop, Insert,
Composition):

```
source: 'take.mp4'
output: 'take.final.mp4'
ops:
	f0: Fetch
		name: 'bugs-bunny'
		url: 'https://example.com/bugs-bunny.mp4'
	e0: Crop
		from: 12.40
		to: 15.50
		why: 'instruction to the composer'
	e1: Insert
		at: 41.00
		media: 'take/bugs-bunny.mp4'
		from: 3.0
		to: 9.0
		place: pip
		why: 'mark e0: right after the race ends at 41 s'
```

Operations are applied in time order. `why` names the mark the
operation came from, so the user can check it and edit the file
before rendering.

## Rendering

`composer <take>` asks the agent for the composition, then
renders it. When the composition is newer than the notes (the user
edited it, or nothing new was said), it renders as it stands with
no agent; new notes ask the agent again.
The output is H.264 4:2:0 at a constant QP of 23 with AAC sound
at 48 kHz: small, and it plays everywhere. The take is read on
the GPU (H.265 4:4:4 takes included); the take's sound follows
its kept spans, a cut is silent unless `sound: media`.

## Operations

### Fetch

A resource by its reference name: the words the user said,
made a file name (`bugs-bunny`). The file is `take/<name>.<ext>`.
If a file of that name is already in the folder (any extension:
jpg jpeg png webp gif mp4 mov webm), it is used as it is and
nothing is downloaded; else `url` is downloaded into it.

| field | meaning |
|---|---|
| name | the reference name, the file's name without extension |
| url | where to get it when the folder does not have it |

### Crop

Removes the span `from`..`to` of the take, picture and sound.

| field | meaning |
|---|---|
| from | start of the span removed, seconds |
| to | end of the span removed, seconds |

### Insert

Puts a resource over the take. `media` is a file in the take's
resources folder, named from beside the take with the stem
folder first (`take/bugs-bunny.jpg`): a picture (png, jpg), an
animated gif (it loops for the insert's `dur`) or a video (mp4). The take keeps playing
underneath unless `place` is `full`.

| field | meaning |
|---|---|
| at | take time the insert appears |
| media | `take/<file>`: the stem folder, then the file |
| from, to | the part of a video resource used, seconds; a picture uses `dur` instead |
| dur | how long a picture stays, seconds |
| place | `pip`, `full`, or `region` |
| corner | for `pip`: `tl`, `tr`, `bl`, `br` (default `br`) |
| size | for `pip`: the insert's width as a share of the frame's width (default 0.33, a third) |
| region | for `region`: the text `'x y w h'`, each a share of the frame (0..1, top left origin); the media keeps its shape, centred in it |
| volume | a video resource's sound, 0..1, mixed over the take's (default 0) |
| fade | seconds of fade in and out (default 0.25) |
| sound | `take` keeps the take's sound (default); `media` plays the insert's instead |
| pause | `true`: a cut to the media. The take holds while it plays and resumes after; the output grows by its length |

Most inserts are picture-in-picture at about a third of the
frame, over the take. To cut to another video or a picture
instead, use `place: full`:

- without `pause`, the media covers the take while the take runs
  on underneath (a cutaway; the take's sound continues unless
  `sound: media`).
- with `pause: true`, the take stops at `at`, the media plays for
  its length, and the take resumes where it stopped. Everything
  after moves later in the output by that length.

### Focus

Zooms the picture onto one part of it, for example a panel or a
line of code the user is talking about. The zoom moves there over a
moment and back after (a decay, each frame a share of the rest of
the way), so the take keeps playing and its sound is untouched. The
region keeps the frame's shape: its larger side sets the zoom, a
region half the frame's size is 200%. Find the region on the still
of the mark that asks for it (see the marks' stills).

| field | meaning |
|---|---|
| from | take time the zoom starts toward the region, seconds |
| to | take time it starts back out, seconds |
| region | the text `'x y w h'`, each a share of the frame (0..1, top left origin) |
| speed | the share of the way it moves each frame at 60 a second (default 0.08; higher is quicker) |

### Mute

Clears the user's voice from `from` to `to`, for a stretch that
should not be heard (a slip, a cough, words meant for the agent
only). With the mic on its own track only the voice goes and the
app's sound stays; without one the take is silent there. The
picture is untouched. The span fades out and back in over 50 ms.

| field | meaning |
|---|---|
| from | take time the voice is cleared from, seconds |
| to | take time it comes back, seconds |

### Revoice

The take's own sound from `from` to `to`, spoken in another voice.
It is voice conversion, not text to speech: the words, timing and
delivery stay the user's, only the voice changes, so it stays in
sync with the picture. `voice` is a recording of the target voice
in the take's folder (.wav, or the sound of an .m4a/.mp4/.mov):
minutes of clean speech from that one speaker, the more the better
(10 to 30 minutes is typical). The user names it in the context or
a dictation ("video/picard.wav is Picard"). The span fades in and
out of the original voice; with the mic on its own track only the
voice changes and the app's sound stays as it was.

The first time a voice is used, the composer trains a model of it
(RVC); that takes about an hour or more. Trained voices are kept
by the clip's name in one shared store, so any take that names a
clip of the same name and content reuses it at once, from any
folder; a changed recording trains again.

| field | meaning |
|---|---|
| from | take time the new voice starts, seconds |
| to | take time it ends, seconds |
| voice | `take/<file>`: the target voice's recording |
| pitch | semitones to shift the voice (e.g. -12 an octave down) |
| epochs | training length on first use (default 100) |

Use a pitch shift when the target's voice sits far from the
user's (a man's voice into a woman's: about +12).

## Rules for the agent

- Look in `take/` first: a file the user put there and named in
  a mark or the context is the one to use.
- Add to it through Fetch: for a mark that asks for a picture or
  clip the folder does not hold, search the web for the one that
  fits best and write a Fetch op with a direct link to the file.
  The agent never downloads; the composer runs every Fetch first,
  saving `take/<name>.<ext>`. Never remove or replace a file
  already there (a Fetch for a name already present is skipped).
- `media` names only files in `take/`. A resource that could
  not be found is left out, and the Crop of its mark says so in
  `why`.
- Every instruction mark gets a Crop over its own `at`..`at + dur`.
- A mark that is not an instruction (narration meant for the
  viewer) stays in, and gets no operation.
- Times come from the marks and the context; never invented
  beyond the take's length.
