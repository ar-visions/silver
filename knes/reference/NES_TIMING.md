# knes — NES timing + behavior spec

Clean-room, documentation-only, same method as rgen's M68K_TIMING/VDP_TIMING.
Primary open sources:
- **NESdev wiki** (nesdev.org) — canonical community reference: PPU frame timing,
  APU register/mixer behavior, mappers, controller protocol. The most complete
  open console documentation anywhere; treat as primary for everything non-CPU.
- **MOS MCS6500 family datasheet / programming manual** — 6502 instruction set
  + cycle counts (incl. page-cross penalties). Datasheet-settled.
- Hardware test ROMs (blargg's cpu/apu/ppu suites) — final tiebreaker.

## Clocks (NTSC) [settled]
- Master 21.477272 MHz ( = 236.25 MHz / 11 )
- CPU (2A03) = master/12 = 1.789773 MHz · PPU (2C02) = master/4 (3 dots/CPU cycle)
- Frame = 262 lines × 341 dots = 89342 dots (NTSC odd-frame skip: −1 dot when
  rendering — **not yet modelled**); vblank flag + NMI at line 241 dot 1;
  flags clear at pre-render line 261.

## 6502 [settled — MOS datasheet]
- Implemented additively in knes.ag: base cycles per (op, mode) + 1 on page
  cross for read AB,X / AB,Y / (ZP),Y; stores never take the discount.
- Branches: 2 / 3 taken / 4 taken across page. NMI 7, BRK 7, RTI 6.
- Decimal mode absent on 2A03 (D flag settable but ignored) — matches knes.
- Unofficial opcodes: NOT implemented — logged via trace. Some games use them
  (rare); add from the NESdev unofficial-opcode table when one appears.

## PPU [NESdev — well settled, our model is simplified]
Current knes model is FRAME-based (render then step lines). Known simplifications
to revisit, in impact order:
1. **Mid-frame scroll/ctrl changes** — split-screen effects (SMB status bar uses
   sprite-0 hit + mid-frame scroll write) render from end-of-frame state. We fake
   sprite-0 hit at the computed line; real games needing per-line scroll splits
   will show wrong splits until a per-line renderer lands (rgen lesson: asnes
   moved to per-line snapshots for exactly this).
2. Loopy v/t register model ($2005/$2006 share internal t; fine-x) — we keep
   simple scroll_x/scroll_y + addr; wrong for games that write $2006 mid-frame.
3. 8-sprites-per-line limit + overflow flag — not enforced (flicker absent).
4. Odd-frame dot skip, $2002 race conditions, palette emphasis bits, grayscale.

## APU [NESdev — settled]
- Pulse ×2 (duty 12.5/25/50/75, sweep, envelope), triangle (linear counter),
  noise (15-bit LFSR, mode-6 tap), lengths from the 32-entry table, frame
  counter 4/5-step @ 240 Hz. All implemented per NESdev tables.
- **DMC (delta PCM) deferred** — drums in some games (SMB3 etc) use it.
- Mixer: linear approximation (492/557/323 per-unit weights vs NESdev's
  nonlinear formula) — revisit if mixes sound off.

## Mappers
- Mapper 0 (NROM) only: SMB1, Donkey Kong, Excitebike, Balloon Fight...
- Next: MMC1 (1), UxROM (2), CNROM (3), MMC3 (4 — needs scanline IRQ).

## Controller [settled]
$4016 strobe → reload shift from pad state; reads shift out A B Sel St U D L R.
