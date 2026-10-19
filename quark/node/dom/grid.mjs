// CSS grid: tracks, areas, placement, sizing, alignment
import { parseLength, splitSpaces } from './css.mjs';
import { Box } from './layout.mjs';

// a track sizing function: { min, max } each px number, 'auto',
// 'min-content', 'max-content' or { fr }
function parseTrackList(v, cs, pctBase, avail, gap) {
  v = (v || 'none').trim();
  if (v === 'none' || v === 'subgrid' || v === 'masonry') return [];
  const out = [];
  const toks = tokenize(v);
  const one = t => {
    t = t.trim();
    if (t.startsWith('minmax(')) { const [a, b] = splitArgs(t.slice(7, -1)); return { min: size(a), max: size(b) }; }
    if (t.startsWith('fit-content(')) { return { min: 'auto', max: size(t.slice(12, -1)) }; }
    const s = size(t);
    return { min: s, max: s };
  };
  const size = t => {
    t = t.trim();
    if (t === 'auto') return 'auto'; if (t === 'min-content') return 'min-content'; if (t === 'max-content') return 'max-content';
    if (/fr$/.test(t)) return { fr: parseFloat(t) || 0 };
    const n = parseLength(t, cs.ctx(), pctBase);
    return n == null ? 'auto' : n;
  };
  for (const t of toks) {
    if (t.startsWith('[')) continue;
    if (t.startsWith('repeat(')) {
      const inner = t.slice(7, -1);
      const comma = inner.indexOf(',');
      const count = inner.slice(0, comma).trim(), list = tokenize(inner.slice(comma + 1)).filter(x => !x.startsWith('['));
      let n = parseInt(count);
      if (count === 'auto-fill' || count === 'auto-fit') {
        const tr = list.map(one);
        const minW = tr.reduce((a, x) => a + (typeof x.min === 'number' ? x.min : typeof x.max === 'number' ? x.max : 0), 0);
        n = avail != null && minW > 0 ? Math.max(1, Math.floor((avail + gap) / (minW + gap * tr.length))) : 1;
        for (let i = 0; i < n; i++) for (const x of tr) out.push({ ...x, autoFit: count === 'auto-fit' });
        continue;
      }
      if (!(n > 0)) n = 1;
      for (let i = 0; i < n; i++) for (const x of list) out.push(one(x));
      continue;
    }
    out.push(one(t));
  }
  return out;
}
function tokenize(v) { const out = []; let cur = '', d = 0; for (const c of v) { if (c === '(') d++; if (c === ')') d--; if (/\s/.test(c) && d === 0) { if (cur) out.push(cur); cur = ''; continue; } cur += c; } if (cur) out.push(cur); return out; }
function splitArgs(v) { const out = []; let cur = '', d = 0; for (const c of v) { if (c === '(') d++; if (c === ')') d--; if (c === ',' && d === 0) { out.push(cur); cur = ''; continue; } cur += c; } out.push(cur); return out; }
function parseAreas(v) {
  const rows = [...(v || '').matchAll(/"([^"]*)"|'([^']*)'/g)].map(m => (m[1] ?? m[2]).trim().split(/\s+/));
  const areas = new Map();
  rows.forEach((row, r) => row.forEach((name, c) => {
    if (name === '.' || !name) return;
    let a = areas.get(name);
    if (!a) { a = { r0: r, r1: r + 1, c0: c, c1: c + 1 }; areas.set(name, a); }
    else { a.r1 = Math.max(a.r1, r + 1); a.c1 = Math.max(a.c1, c + 1); a.c0 = Math.min(a.c0, c); a.r0 = Math.min(a.r0, r); }
  }));
  return { areas, rows: rows.length, cols: Math.max(0, ...rows.map(r => r.length)) };
}
// grid-row/column-start/end into { start, end, span } (1-based lines, 0 = auto)
function parseLine(v, areas, axis, isEnd) {
  v = (v || 'auto').trim();
  if (v === 'auto') return { line: 0, span: 0 };
  const sp = v.match(/^span\s+(\d+)$|^span\s+([a-zA-Z][\w-]*)$|^(\d+)\s+span$/);
  if (sp) return { line: 0, span: parseInt(sp[1] || sp[3]) || 1 };
  const n = parseInt(v);
  if (!isNaN(n) && String(n) === v.replace(/\s.*$/, '')) return { line: n, span: 0 };
  const m = v.match(/^([a-zA-Z][\w-]*?)(-start|-end)?$/);
  if (m) { const a = areas.get(m[1]); if (a) { const startL = axis === 'row' ? a.r0 + 1 : a.c0 + 1, endL = axis === 'row' ? a.r1 + 1 : a.c1 + 1; if (m[2] === '-start') return { line: startL, span: 0 }; if (m[2] === '-end') return { line: endL, span: 0 }; return { line: isEnd ? endL : startL, span: 0 }; } }
  return { line: 0, span: 0 };
}
function placement(cs, axis, areas, count) {
  const area = cs.get('grid-area');
  let sv = cs.get(`grid-${axis}-start`), ev = cs.get(`grid-${axis}-end`);
  const s = parseLine(sv, areas, axis, false), e = parseLine(ev, areas, axis, true);
  const norm = l => l < 0 ? count + 2 + l : l;
  let start = s.line ? norm(s.line) : 0, end = e.line ? norm(e.line) : 0;
  let span = 1;
  if (start && end) { if (end < start) [start, end] = [end, start]; span = Math.max(1, end - start); }
  else if (start) span = e.span || 1;
  else if (end) { span = s.span || 1; start = end - span; }
  else span = s.span || e.span || 1;
  return { start, span };
}

export function layoutGrid(L, box, innerW, innerH) {
  const cs = box.cs;
  const rowGap = L.gapOf(cs, 'row-gap', innerH), colGap = L.gapOf(cs, 'column-gap', innerW);
  const areaInfo = parseAreas(cs.get('grid-template-areas'));
  let cols = parseTrackList(cs.get('grid-template-columns'), cs, innerW, innerW, colGap);
  let rows = parseTrackList(cs.get('grid-template-rows'), cs, innerH, innerH, rowGap);
  const autoCols = parseTrackList(cs.get('grid-auto-columns') || 'auto', cs, innerW, innerW, colGap);
  const autoRows = parseTrackList(cs.get('grid-auto-rows') || 'auto', cs, innerH, innerH, rowGap);
  const flowCol = (cs.get('grid-auto-flow') || 'row').includes('column');
  while (cols.length < areaInfo.cols) cols.push(autoCols[cols.length % Math.max(1, autoCols.length)] || { min: 'auto', max: 'auto' });
  while (rows.length < areaInfo.rows) rows.push(autoRows[rows.length % Math.max(1, autoRows.length)] || { min: 'auto', max: 'auto' });
  // items: boxes, text runs as anonymous blocks; absolutes aside
  const items = [];
  for (const k of box.children) {
    if (k.kind !== 'text' && (k.position === 'absolute' || k.position === 'fixed')) { k.staticX = box.cx(); k.staticY = box.cy(); L.absQueue.push(k); continue; }
    if (k.kind === 'text' && !k.text.trim()) continue;
    let it = k;
    if (k.kind === 'text' || k.kind === 'inline') { const anon = new Box(null, k.cs, 'block'); anon.anonymous = true; anon.parent = box; anon.children = [k]; anon.cs = L.page.anonymousStyle(box.cs); k.parent = anon; it = anon; }
    items.push(it);
  }
  const order = items.map((k, i) => ({ k, i, o: parseInt(k.cs.get('order')) || 0 })).sort((a, b) => a.o - b.o || a.i - b.i).map(x => x.k);
  // placement
  const placed = [];
  for (const k of order) {
    const c = placement(k.cs, 'column', areaInfo.areas, cols.length), r = placement(k.cs, 'row', areaInfo.areas, rows.length);
    placed.push({ k, c0: c.start ? c.start - 1 : -1, cs: c.span, r0: r.start ? r.start - 1 : -1, rs: r.span });
  }
  const occupied = new Set();
  const key = (r, c) => r * 100000 + c;
  const take = p => { for (let r = p.r0; r < p.r0 + p.rs; r++) for (let c = p.c0; c < p.c0 + p.cs; c++) occupied.add(key(r, c)); };
  const free = (r0, c0, rs, cs2) => { for (let r = r0; r < r0 + rs; r++) for (let c = c0; c < c0 + cs2; c++) if (occupied.has(key(r, c))) return false; return true; };
  for (const p of placed) if (p.r0 >= 0 && p.c0 >= 0) take(p);
  let cursorR = 0, cursorC = 0;
  for (const p of placed) {
    if (p.r0 >= 0 && p.c0 >= 0) continue;
    if (!flowCol) {
      if (p.c0 >= 0) { let r = 0; while (!free(r, p.c0, p.rs, p.cs)) r++; p.r0 = r; }
      else if (p.r0 >= 0) { let c = 0; while (!free(p.r0, c, p.rs, p.cs) && c + p.cs <= Math.max(cols.length, 1)) c++; if (!free(p.r0, c, p.rs, p.cs)) { c = 0; } p.c0 = c; }
      else {
        let r = cursorR, c = cursorC;
        for (;;) { if (c + p.cs > Math.max(cols.length, p.cs)) { c = 0; r++; } if (free(r, c, p.rs, p.cs)) break; c++; }
        p.r0 = r; p.c0 = c; cursorR = r; cursorC = c + p.cs;
      }
    } else {
      if (p.r0 >= 0) { let c = 0; while (!free(p.r0, c, p.rs, p.cs)) c++; p.c0 = c; }
      else if (p.c0 >= 0) { let r = 0; while (!free(r, p.c0, p.rs, p.cs) && r + p.rs <= Math.max(rows.length, 1)) r++; if (!free(r, p.c0, p.rs, p.cs)) r = 0; p.r0 = r; }
      else {
        let r = cursorR, c = cursorC;
        for (;;) { if (r + p.rs > Math.max(rows.length, p.rs)) { r = 0; c++; } if (free(r, c, p.rs, p.cs)) break; r++; }
        p.r0 = r; p.c0 = c; cursorR = r + p.rs; cursorC = c;
      }
    }
    take(p);
  }
  // implicit tracks
  for (const p of placed) {
    while (cols.length < p.c0 + p.cs) cols.push(autoCols[cols.length % Math.max(1, autoCols.length)] || { min: 'auto', max: 'auto' });
    while (rows.length < p.r0 + p.rs) rows.push(autoRows[rows.length % Math.max(1, autoRows.length)] || { min: 'auto', max: 'auto' });
  }
  if (!cols.length) cols.push({ min: 'auto', max: 'auto' });
  if (!rows.length) rows.push({ min: 'auto', max: 'auto' });
  // item measures across the inline axis
  for (const p of placed) {
    L.edges(p.k, innerW);
    const wp = L.widthProp(p.k, 'width', innerW);
    const edges = p.k.pl + p.k.pr + p.k.bl + p.k.br + p.k.ml + p.k.mr;
    p.minW = (typeof wp === 'number' ? wp : L.intrinsicWidth(p.k, 0) + p.k.pl + p.k.pr + p.k.bl + p.k.br) + p.k.ml + p.k.mr;
    p.maxW = (typeof wp === 'number' ? wp : L.intrinsicWidth(p.k, Infinity) + p.k.pl + p.k.pr + p.k.bl + p.k.br) + p.k.ml + p.k.mr;
    const mn = L.widthProp(p.k, 'min-width', innerW); if (typeof mn === 'number') { p.minW = Math.max(p.minW, mn + p.k.ml + p.k.mr); p.maxW = Math.max(p.maxW, mn + p.k.ml + p.k.mr); }
    const mx = L.widthProp(p.k, 'max-width', innerW); if (typeof mx === 'number') { p.maxW = Math.min(p.maxW, mx + p.k.ml + p.k.mr); p.minW = Math.min(p.minW, p.maxW); }
  }
  const colSizes = sizeTracks(cols, placed, 'c0', 'cs', p => p.minW, p => p.maxW, innerW, colGap, cs.get('justify-content'));
  // items laid out at their column width for their heights
  const justifyItems = norm(cs.get('justify-items'), 'stretch'), alignItems = norm(cs.get('align-items'), 'stretch');
  const colStart = offsets(colSizes, colGap);
  for (const p of placed) {
    const k = p.k;
    const cellW = span(colSizes, colGap, p.c0, p.cs);
    let js = norm(k.cs.get('justify-self'), justifyItems);
    const autoW = k.cs.get('width') === 'auto' && !k.replaced;
    const fw = js === 'stretch' && autoW ? Math.max(0, cellW - k.ml - k.mr) : (autoW ? Math.min(Math.max(0, cellW - k.ml - k.mr), p.maxW - k.ml - k.mr) : null);
    k.flexItem = true; k.flexRow = true;
    L.layoutBlockFixed(k, cellW, innerH, fw, null);
    p.minH = k.h + k.mt + k.mb; p.maxH = p.minH;
    const hp = L.heightProp(k, 'height', innerH); if (hp != null) { p.minH = hp + k.mt + k.mb; p.maxH = p.minH; }
  }
  const rowSizes = sizeTracks(rows, placed, 'r0', 'rs', p => p.minH, p => p.maxH, innerH, rowGap, cs.get('align-content'));
  const rowStart = offsets(rowSizes, rowGap);
  // content alignment when the tracks leave room
  const totalW = colStart.total, totalH = rowStart.total;
  const jc = cs.get('justify-content'), ac = cs.get('align-content');
  const shiftX = contentShift(jc, innerW == null ? 0 : innerW - totalW, cols.length), shiftY = contentShift(ac, innerH == null ? 0 : innerH - totalH, rows.length);
  const x0 = box.cx(), y0 = box.cy();
  for (const p of placed) {
    const k = p.k;
    const cellX = x0 + shiftX.start + colStart.at[p.c0] + shiftX.gap * p.c0, cellW = span(colSizes, colGap, p.c0, p.cs) + shiftX.gap * (p.cs - 1);
    const cellY = y0 + shiftY.start + rowStart.at[p.r0] + shiftY.gap * p.r0, cellH = span(rowSizes, rowGap, p.r0, p.rs) + shiftY.gap * (p.rs - 1);
    const as = norm(k.cs.get('align-self'), alignItems);
    const autoH = k.cs.get('height') === 'auto' && !k.replaced;
    if (as === 'stretch' && autoH && Math.abs(k.h + k.mt + k.mb - cellH) > 0.01) { const js = norm(k.cs.get('justify-self'), justifyItems); const autoW = k.cs.get('width') === 'auto' && !k.replaced; const fw = js === 'stretch' && autoW ? Math.max(0, cellW - k.ml - k.mr) : (autoW ? k.w : null); L.layoutBlockFixed(k, cellW, innerH, fw, Math.max(0, cellH - k.mt - k.mb)); }
    const js = norm(k.cs.get('justify-self'), justifyItems);
    let dx = k.ml, dy = k.mt;
    const freeW = cellW - k.w - k.ml - k.mr, freeH = cellH - k.h - k.mt - k.mb;
    if (k.autoML && k.autoMR) dx += freeW / 2; else if (k.autoML) dx += freeW; else if (js === 'center') dx += freeW / 2; else if (js === 'end' || js === 'flex-end' || js === 'right' || js === 'self-end') dx += freeW;
    if (k.cs.get('margin-top') === 'auto' && k.cs.get('margin-bottom') === 'auto') dy += freeH / 2; else if (k.cs.get('margin-top') === 'auto') dy += freeH; else if (as === 'center') dy += freeH / 2; else if (as === 'end' || as === 'flex-end' || as === 'self-end') dy += freeH; else if (as === 'baseline') dy += 0;
    L.moveBox(k, cellX + dx - k.x, cellY + dy - k.y);
  }
  box.contentW = totalW; box.contentH = totalH;
  return totalH;
}
function norm(v, d) { if (!v || v === 'normal' || v === 'auto' || v === 'legacy') return d; return v; }
function offsets(sizes, gap) { const at = []; let x = 0; for (let i = 0; i < sizes.length; i++) { at.push(x); x += sizes[i] + (i < sizes.length - 1 ? gap : 0); } return { at, total: x }; }
function span(sizes, gap, from, n) { let s = 0; for (let i = from; i < from + n && i < sizes.length; i++) s += sizes[i]; return s + gap * Math.max(0, n - 1); }
function contentShift(v, free, n) {
  if (!(free > 0)) return { start: 0, gap: 0 };
  switch (v) { case 'center': return { start: free / 2, gap: 0 }; case 'end': case 'flex-end': return { start: free, gap: 0 }; case 'space-between': return n > 1 ? { start: 0, gap: free / (n - 1) } : { start: 0, gap: 0 }; case 'space-around': return { start: free / n / 2, gap: free / n }; case 'space-evenly': return { start: free / (n + 1), gap: free / (n + 1) }; }
  return { start: 0, gap: 0 };
}
// the track sizing algorithm, simplified: intrinsic tracks from the
// items in them (one track spans first), then fr and stretch
function sizeTracks(tracks, placed, startKey, spanKey, minOf, maxOf, avail, gap, contentAlign) {
  const n = tracks.length;
  const base = new Array(n).fill(0), limit = new Array(n).fill(Infinity);
  const isFr = t => typeof t.max === 'object' && t.max && 'fr' in t.max;
  for (let i = 0; i < n; i++) {
    const t = tracks[i];
    if (typeof t.min === 'number') base[i] = t.min;
    if (typeof t.max === 'number') limit[i] = Math.max(t.max, base[i]);
    else if (isFr(t)) limit[i] = Infinity;
  }
  // contributions of items spanning one track
  const intrinsicMin = t => t.min === 'auto' || t.min === 'min-content' || t.min === 'max-content' || (typeof t.min === 'object' && t.min && 'fr' in t.min);
  for (const p of placed) {
    if (p[spanKey] !== 1) continue;
    const i = p[startKey]; const t = tracks[i];
    if (!t) continue;
    if (t.min === 'auto' || t.min === 'min-content') base[i] = Math.max(base[i], minOf(p));
    else if (t.min === 'max-content') base[i] = Math.max(base[i], maxOf(p));
    if (t.max === 'auto' || t.max === 'max-content') { limit[i] = limit[i] === Infinity ? maxOf(p) : Math.max(limit[i], maxOf(p)); }
    else if (t.max === 'min-content') limit[i] = limit[i] === Infinity ? minOf(p) : Math.max(limit[i], minOf(p));
    if (limit[i] < base[i]) limit[i] = base[i];
  }
  // items spanning several intrinsic tracks: their need shared out
  for (const p of placed) {
    if (p[spanKey] <= 1) continue;
    const idx = []; for (let i = p[startKey]; i < p[startKey] + p[spanKey]; i++) if (tracks[i] && intrinsicMin(tracks[i]) && !isFr(tracks[i])) idx.push(i);
    if (!idx.length) continue;
    const have = span(base, gap, p[startKey], p[spanKey]);
    const need = minOf(p) - have;
    if (need > 0) for (const i of idx) { base[i] += need / idx.length; if (limit[i] < base[i]) limit[i] = base[i]; }
  }
  for (let i = 0; i < n; i++) if (limit[i] === Infinity && !isFr(tracks[i]) && tracks[i].max !== 'auto' && tracks[i].max !== 'max-content') limit[i] = base[i];
  const sizes = base.slice();
  const gaps = gap * Math.max(0, n - 1);
  if (avail == null) {
    // indefinite: intrinsic tracks take their limits, fr tracks their base
    for (let i = 0; i < n; i++) if (!isFr(tracks[i]) && limit[i] !== Infinity) sizes[i] = limit[i];
    return sizes;
  }
  let free = avail - gaps - sizes.reduce((a, b) => a + b, 0);
  // grow intrinsic tracks toward their limits
  if (free > 0) {
    const growable = [...Array(n).keys()].filter(i => !isFr(tracks[i]) && limit[i] > sizes[i]);
    let left = free;
    for (let pass = 0; pass < 4 && growable.length && left > 0.01; pass++) {
      const each = left / growable.length; let used = 0;
      for (const i of growable) { const add = Math.min(each, limit[i] - sizes[i]); sizes[i] += add; used += add; }
      left -= used;
      for (let j = growable.length - 1; j >= 0; j--) if (limit[growable[j]] - sizes[growable[j]] < 0.01) growable.splice(j, 1);
    }
    free = left;
  }
  // fr tracks share what is left (a track's base holds it up)
  const frs = [...Array(n).keys()].filter(i => isFr(tracks[i]));
  if (frs.length && free > 0) {
    let frSum = frs.reduce((a, i) => a + tracks[i].max.fr, 0), pool = free + frs.reduce((a, i) => a + sizes[i], 0);
    const fixed = new Set();
    for (let pass = 0; pass < 8; pass++) {
      const unit = frSum > 0 ? pool / frSum : 0; let changed = false;
      for (const i of frs) { if (fixed.has(i)) continue; const want = unit * tracks[i].max.fr; if (want < base[i]) { fixed.add(i); pool -= base[i]; frSum -= tracks[i].max.fr; changed = true; } }
      if (!changed) { for (const i of frs) if (!fixed.has(i)) sizes[i] = Math.max(base[i], unit * tracks[i].max.fr); break; }
    }
    free = 0;
  } else if (free > 0 && (contentAlign === 'stretch' || contentAlign === 'normal' || !contentAlign)) {
    // auto tracks stretch to fill
    const autos = [...Array(n).keys()].filter(i => tracks[i].max === 'auto');
    if (autos.length) for (const i of autos) sizes[i] += free / autos.length;
  }
  // auto-fit: empty tracks collapse
  for (let i = 0; i < n; i++) if (tracks[i].autoFit && !placed.some(p => p[startKey] <= i && i < p[startKey] + p[spanKey])) sizes[i] = 0;
  return sizes;
}
