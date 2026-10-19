// CSS: tokens, declarations, shorthands, stylesheets, selectors and
// the cascade into computed values
export function camelToDash(s) { return s.replace(/[A-Z]/g, c => '-' + c.toLowerCase()).replace(/^(webkit|moz|ms)-/, '-$1-'); }
export function dashToCamel(s) { return s.replace(/^-(webkit|moz|ms)-/, '$1-').replace(/-([a-z])/g, (_, c) => c.toUpperCase()); }

// ---- declarations: "a: b; c: d !important"
export function parseDeclarations(text) {
  const out = [];
  let i = 0, n = text.length;
  while (i < n) {
    let colon = -1, depth = 0, j = i, quote = 0;
    for (; j < n; j++) {
      const c = text[j];
      if (quote) { if (c === quote) quote = 0; else if (c === '\\') j++; continue; }
      if (c === '"' || c === "'") quote = c;
      else if (c === '(') depth++;
      else if (c === ')') depth--;
      else if (c === ':' && colon < 0 && depth === 0) colon = j;
      else if (c === ';' && depth === 0) break;
    }
    if (colon > 0) {
      const raw = text.slice(i, colon).trim();
      // a custom property keeps its case; the rest lower
      const name = raw.startsWith('--') ? raw : raw.toLowerCase();
      let value = text.slice(colon + 1, j).trim();
      let imp = false;
      const m = value.match(/!\s*important\s*$/i);
      if (m) { imp = true; value = value.slice(0, m.index).trim(); }
      if (name) out.push([name, value, imp]);
    }
    i = j + 1;
  }
  return out;
}
export function serializeDeclarations(props, priority) {
  const parts = [];
  for (const [k, v] of props) if (!isLonghandOfPresentShorthand(k, props)) parts.push(`${k}: ${v}${priority.get(k) ? ' !important' : ''};`);
  return parts.join(' ');
}
function isLonghandOfPresentShorthand(k, props) {
  for (const [sh, longs] of SHORTHANDS) if (longs.includes(k) && props.has(sh)) return true;
  return false;
}

// ---- value splitting
export function splitSpaces(v) {
  const out = []; let cur = '', depth = 0, quote = 0;
  for (let i = 0; i < v.length; i++) {
    const c = v[i];
    if (quote) { cur += c; if (c === quote) quote = 0; else if (c === '\\') { cur += v[++i] || ''; } continue; }
    if (c === '"' || c === "'") { quote = c; cur += c; continue; }
    if (c === '(') depth++; if (c === ')') depth--;
    if (/\s/.test(c) && depth === 0) { if (cur) out.push(cur); cur = ''; continue; }
    cur += c;
  }
  if (cur) out.push(cur);
  return out;
}
export function splitCommas(v) {
  const out = []; let cur = '', depth = 0, quote = 0;
  for (let i = 0; i < v.length; i++) {
    const c = v[i];
    if (quote) { cur += c; if (c === quote) quote = 0; continue; }
    if (c === '"' || c === "'") quote = c;
    if (c === '(') depth++; if (c === ')') depth--;
    if (c === ',' && depth === 0) { out.push(cur.trim()); cur = ''; continue; }
    cur += c;
  }
  if (cur.trim()) out.push(cur.trim());
  return out;
}
// "/" split for font and border-radius
function splitSlash(v) { const out = []; let cur = '', depth = 0; for (const c of v) { if (c === '(') depth++; if (c === ')') depth--; if (c === '/' && depth === 0) { out.push(cur.trim()); cur = ''; continue; } cur += c; } out.push(cur.trim()); return out; }

// ---- shorthands
const SIDES = ['top', 'right', 'bottom', 'left'];
function four(v) { const p = splitSpaces(v); if (!p.length) return null; const [a, b = a, c = a, d = b] = p; return [a, b, c, d]; }
const BORDER_STYLES = new Set(['none', 'hidden', 'solid', 'dashed', 'dotted', 'double', 'groove', 'ridge', 'inset', 'outset']);
function borderParts(v) {
  let width = null, style = null, color = null;
  for (const p of splitSpaces(v)) {
    if (BORDER_STYLES.has(p)) style = p;
    else if (/^(\d|\.|-)|^(thin|medium|thick)$/.test(p) && !/^(rgb|hsl|#)/.test(p) && width === null && !isColorWord(p)) width = p;
    else color = p;
  }
  return { width: width ?? 'medium', style: style ?? 'none', color: color ?? 'currentcolor' };
}
export const SHORTHANDS = new Map([
  ['margin', SIDES.map(s => 'margin-' + s)], ['padding', SIDES.map(s => 'padding-' + s)],
  ['inset', SIDES], ['border-width', SIDES.map(s => `border-${s}-width`)], ['border-style', SIDES.map(s => `border-${s}-style`)], ['border-color', SIDES.map(s => `border-${s}-color`)],
  ['border', SIDES.flatMap(s => [`border-${s}-width`, `border-${s}-style`, `border-${s}-color`])],
  ['border-top', ['border-top-width', 'border-top-style', 'border-top-color']], ['border-right', ['border-right-width', 'border-right-style', 'border-right-color']],
  ['border-bottom', ['border-bottom-width', 'border-bottom-style', 'border-bottom-color']], ['border-left', ['border-left-width', 'border-left-style', 'border-left-color']],
  ['border-radius', ['border-top-left-radius', 'border-top-right-radius', 'border-bottom-right-radius', 'border-bottom-left-radius']],
  ['outline', ['outline-width', 'outline-style', 'outline-color']],
  ['background', ['background-color', 'background-image', 'background-repeat', 'background-position', 'background-size', 'background-clip', 'background-origin', 'background-attachment']],
  ['font', ['font-style', 'font-variant', 'font-weight', 'font-stretch', 'font-size', 'line-height', 'font-family']],
  ['flex', ['flex-grow', 'flex-shrink', 'flex-basis']], ['flex-flow', ['flex-direction', 'flex-wrap']],
  ['gap', ['row-gap', 'column-gap']], ['grid-gap', ['row-gap', 'column-gap']],
  ['overflow', ['overflow-x', 'overflow-y']], ['place-items', ['align-items', 'justify-items']], ['place-content', ['align-content', 'justify-content']], ['place-self', ['align-self', 'justify-self']],
  ['text-decoration', ['text-decoration-line', 'text-decoration-style', 'text-decoration-color', 'text-decoration-thickness']],
  ['transition', ['transition-property', 'transition-duration', 'transition-timing-function', 'transition-delay']],
  ['animation', ['animation-name', 'animation-duration', 'animation-timing-function', 'animation-delay', 'animation-iteration-count', 'animation-direction', 'animation-fill-mode', 'animation-play-state']],
  ['list-style', ['list-style-type', 'list-style-position', 'list-style-image']],
  ['columns', ['column-width', 'column-count']], ['grid-area', ['grid-row-start', 'grid-column-start', 'grid-row-end', 'grid-column-end']],
  ['grid-row', ['grid-row-start', 'grid-row-end']], ['grid-column', ['grid-column-start', 'grid-column-end']],
  ['grid-template', ['grid-template-rows', 'grid-template-columns', 'grid-template-areas']], ['grid', ['grid-template-rows', 'grid-template-columns', 'grid-template-areas', 'grid-auto-rows', 'grid-auto-columns', 'grid-auto-flow']],
  ['mask', ['mask-image', 'mask-size', 'mask-position', 'mask-repeat']], ['-webkit-mask', ['-webkit-mask-image', '-webkit-mask-size', '-webkit-mask-position', '-webkit-mask-repeat']],
  ['inset-inline', ['left', 'right']], ['inset-block', ['top', 'bottom']], ['margin-inline', ['margin-left', 'margin-right']], ['margin-block', ['margin-top', 'margin-bottom']],
  ['padding-inline', ['padding-left', 'padding-right']], ['padding-block', ['padding-top', 'padding-bottom']],
  ['border-inline', ['border-left-width', 'border-left-style', 'border-left-color', 'border-right-width', 'border-right-style', 'border-right-color']],
  ['border-block', ['border-top-width', 'border-top-style', 'border-top-color', 'border-bottom-width', 'border-bottom-style', 'border-bottom-color']],
  ['scroll-margin', SIDES.map(s => 'scroll-margin-' + s)], ['scroll-padding', SIDES.map(s => 'scroll-padding-' + s)],
]);
const CSS_WIDE = new Set(['inherit', 'initial', 'unset', 'revert', 'revert-layer']);
// logical properties, left to right writing: their physical names
export const LOGICAL = { 'margin-inline-start': 'margin-left', 'margin-inline-end': 'margin-right', 'margin-block-start': 'margin-top', 'margin-block-end': 'margin-bottom', 'padding-inline-start': 'padding-left', 'padding-inline-end': 'padding-right', 'padding-block-start': 'padding-top', 'padding-block-end': 'padding-bottom', 'inset-inline-start': 'left', 'inset-inline-end': 'right', 'inset-block-start': 'top', 'inset-block-end': 'bottom', 'inline-size': 'width', 'block-size': 'height', 'min-inline-size': 'min-width', 'max-inline-size': 'max-width', 'min-block-size': 'min-height', 'max-block-size': 'max-height', 'border-inline-start-width': 'border-left-width', 'border-inline-end-width': 'border-right-width', 'border-inline-start-style': 'border-left-style', 'border-inline-end-style': 'border-right-style', 'border-inline-start-color': 'border-left-color', 'border-inline-end-color': 'border-right-color', 'border-block-start-width': 'border-top-width', 'border-block-end-width': 'border-bottom-width', 'border-block-start-style': 'border-top-style', 'border-block-end-style': 'border-bottom-style', 'border-block-start-color': 'border-top-color', 'border-block-end-color': 'border-bottom-color', 'border-start-start-radius': 'border-top-left-radius', 'border-start-end-radius': 'border-top-right-radius', 'border-end-start-radius': 'border-bottom-left-radius', 'border-end-end-radius': 'border-bottom-right-radius', 'overflow-inline': 'overflow-x', 'overflow-block': 'overflow-y', 'scroll-margin-inline-start': 'scroll-margin-left', 'scroll-margin-inline-end': 'scroll-margin-right', 'scroll-padding-inline-start': 'scroll-padding-left', 'scroll-padding-inline-end': 'scroll-padding-right' };
const LOGICAL_SHORT = { 'border-inline-start': 'border-left', 'border-inline-end': 'border-right', 'border-block-start': 'border-top', 'border-block-end': 'border-bottom' };
// a shorthand into its longhands; null when not a shorthand
export const PENDING = '\u0001';
export function expandShorthand(k, v) {
  if (LOGICAL[k]) return [[LOGICAL[k], v]];
  if (LOGICAL_SHORT[k]) return expandShorthand(LOGICAL_SHORT[k], v);
  const longs = SHORTHANDS.get(k);
  if (!longs) return null;
  // a var() in a shorthand is split once substituted (style raw)
  if (v.includes('var(')) return longs.map(l => [l, PENDING + k + PENDING + v]);
  if (v === '' || CSS_WIDE.has(v)) return longs.map(l => [l, v]);
  switch (k) {
  case 'margin': case 'padding': case 'inset': case 'border-width': case 'border-style': case 'border-color': case 'scroll-margin': case 'scroll-padding': {
    const f = four(v); if (!f) return longs.map(l => [l, '']); return longs.map((l, i) => [l, f[i]]);
  }
  case 'border-radius': {
    const [h, vv] = splitSlash(v); const a = four(h); const b = vv ? four(vv) : a; if (!a) return longs.map(l => [l, '']);
    return longs.map((l, i) => [l, b && b[i] !== a[i] ? `${a[i]} ${b[i]}` : a[i]]);
  }
  case 'border': { const b = borderParts(v); return SIDES.flatMap(s => [[`border-${s}-width`, b.width], [`border-${s}-style`, b.style], [`border-${s}-color`, b.color]]); }
  case 'border-top': case 'border-right': case 'border-bottom': case 'border-left': { const b = borderParts(v); return [[`${k}-width`, b.width], [`${k}-style`, b.style], [`${k}-color`, b.color]]; }
  case 'border-inline': { const b = borderParts(v); return ['left', 'right'].flatMap(s => [[`border-${s}-width`, b.width], [`border-${s}-style`, b.style], [`border-${s}-color`, b.color]]); }
  case 'border-block': { const b = borderParts(v); return ['top', 'bottom'].flatMap(s => [[`border-${s}-width`, b.width], [`border-${s}-style`, b.style], [`border-${s}-color`, b.color]]); }
  case 'outline': { const b = borderParts(v); return [['outline-width', b.width], ['outline-style', b.style], ['outline-color', b.color]]; }
  case 'background': return expandBackground(v);
  case 'font': return expandFont(v);
  case 'flex': {
    const p = splitSpaces(v);
    if (p[0] === 'none') return [['flex-grow', '0'], ['flex-shrink', '0'], ['flex-basis', 'auto']];
    if (p[0] === 'auto') return [['flex-grow', p[1] || '1'], ['flex-shrink', '1'], ['flex-basis', 'auto']];
    let grow = '1', shrink = '1', basis = '0%';
    const nums = p.filter(x => /^[\d.]+$/.test(x)), rest = p.filter(x => !/^[\d.]+$/.test(x));
    if (nums[0] !== undefined) grow = nums[0]; if (nums[1] !== undefined) shrink = nums[1];
    if (rest[0] !== undefined) basis = rest[0]; else if (nums.length === 1 && p.length === 1) basis = '0%';
    return [['flex-grow', grow], ['flex-shrink', shrink], ['flex-basis', basis]];
  }
  case 'flex-flow': { const p = splitSpaces(v); let dir = 'row', wrap = 'nowrap'; for (const x of p) { if (/wrap/.test(x)) wrap = x; else dir = x; } return [['flex-direction', dir], ['flex-wrap', wrap]]; }
  case 'gap': case 'grid-gap': { const p = splitSpaces(v); return [['row-gap', p[0]], ['column-gap', p[1] ?? p[0]]]; }
  case 'overflow': { const p = splitSpaces(v); return [['overflow-x', p[0]], ['overflow-y', p[1] ?? p[0]]]; }
  case 'place-items': case 'place-content': case 'place-self': { const p = splitSpaces(v); return [[longs[0], p[0]], [longs[1], p[1] ?? p[0]]]; }
  case 'inset-inline': case 'inset-block': case 'margin-inline': case 'margin-block': case 'padding-inline': case 'padding-block': { const p = splitSpaces(v); return [[longs[0], p[0]], [longs[1], p[1] ?? p[0]]]; }
  case 'text-decoration': {
    let line = [], style = 'solid', color = 'currentcolor', thick = 'auto';
    for (const x of splitSpaces(v)) { if (/^(none|underline|overline|line-through|blink)$/.test(x)) line.push(x); else if (/^(solid|double|dotted|dashed|wavy)$/.test(x)) style = x; else if (/^(\d|auto|from-font)/.test(x)) thick = x; else color = x; }
    return [['text-decoration-line', line.length ? line.join(' ') : 'none'], ['text-decoration-style', style], ['text-decoration-color', color], ['text-decoration-thickness', thick]];
  }
  case 'transition': return expandList(v, ['transition-property', 'transition-duration', 'transition-timing-function', 'transition-delay'], parseTransition);
  case 'animation': return expandList(v, longs, parseAnimation);
  case 'list-style': { let t = 'disc', pos = 'outside', img = 'none'; for (const x of splitSpaces(v)) { if (x === 'inside' || x === 'outside') pos = x; else if (x.startsWith('url(')) img = x; else t = x; } return [['list-style-type', t], ['list-style-position', pos], ['list-style-image', img]]; }
  case 'columns': { const p = splitSpaces(v); return [['column-width', p[0] || 'auto'], ['column-count', p[1] || 'auto']]; }
  case 'grid-row': case 'grid-column': { const [a, b] = splitSlash(v); return [[longs[0], a], [longs[1], b ?? (isIdent(a) ? a : 'auto')]]; }
  case 'grid-area': {
    // an omitted line repeats a named one, else auto
    const p = splitSlash(v);
    const rs = p[0], cs = p[1] ?? (isIdent(rs) ? rs : 'auto'), re = p[2] ?? (isIdent(rs) ? rs : 'auto'), ce = p[3] ?? (isIdent(cs) ? cs : 'auto');
    return [[longs[0], rs], [longs[1], cs], [longs[2], re], [longs[3], ce]];
  }
  case 'grid-template': case 'grid': { const [rows, cols] = splitSlash(v); return [['grid-template-rows', rows || 'none'], ['grid-template-columns', cols || 'none'], ['grid-template-areas', 'none']]; }
  case 'mask': case '-webkit-mask': { const pre = k === 'mask' ? 'mask-' : '-webkit-mask-'; let img = 'none', size = 'auto', pos = '0% 0%', rep = 'repeat'; for (const x of splitSpaces(v)) { if (/^(url|linear-gradient|radial-gradient|-webkit-)/.test(x)) img = x; else if (/^(no-repeat|repeat|repeat-x|repeat-y|space|round)$/.test(x)) rep = x; else if (/^(contain|cover|\d)/.test(x) && img !== 'none') size = x; else pos = x; } return [[pre + 'image', img], [pre + 'size', size], [pre + 'position', pos], [pre + 'repeat', rep]]; }
  }
  return longs.map(l => [l, v]);
}
function isIdent(v) { return /^[a-zA-Z_][\w-]*$/.test(v) && v !== 'auto' && !v.startsWith('span'); }
function expandList(v, longs, parseOne) {
  const items = splitCommas(v).map(parseOne);
  return longs.map((l, i) => [l, items.map(it => it[i]).join(', ')]);
}
function parseTransition(s) {
  let prop = 'all', dur = '0s', fn = 'ease', delay = '0s', seenTime = false;
  for (const x of splitSpaces(s)) {
    if (/^-?[\d.]+m?s$/.test(x)) { if (!seenTime) { dur = x; seenTime = true; } else delay = x; }
    else if (/^(ease|linear|ease-in|ease-out|ease-in-out|step-start|step-end|steps\(|cubic-bezier\()/.test(x)) fn = x;
    else prop = x;
  }
  return [prop, dur, fn, delay];
}
function parseAnimation(s) {
  let name = 'none', dur = '0s', fn = 'ease', delay = '0s', count = '1', dir = 'normal', fill = 'none', state = 'running', seenTime = false;
  for (const x of splitSpaces(s)) {
    if (/^-?[\d.]+m?s$/.test(x)) { if (!seenTime) { dur = x; seenTime = true; } else delay = x; }
    else if (/^(ease|linear|ease-in|ease-out|ease-in-out|step-start|step-end|steps\(|cubic-bezier\()/.test(x)) fn = x;
    else if (/^(infinite|[\d.]+)$/.test(x)) count = x;
    else if (/^(normal|reverse|alternate|alternate-reverse)$/.test(x)) dir = x;
    else if (/^(forwards|backwards|both)$/.test(x) || (x === 'none' && name !== 'none')) fill = x;
    else if (/^(running|paused)$/.test(x)) state = x;
    else name = x;
  }
  return [name, dur, fn, delay, count, dir, fill, state];
}
function expandBackground(v) {
  const layers = splitCommas(v);
  const color = [], image = [], repeat = [], position = [], size = [], clip = [], origin = [], attach = [];
  let finalColor = 'transparent';
  for (const layer of layers) {
    let img = 'none', rep = 'repeat', pos = '0% 0%', sz = 'auto', cl = 'border-box', or = 'padding-box', at = 'scroll';
    const [main, after] = splitSlash(layer);
    const posParts = [];
    for (const x of splitSpaces(main)) {
      if (/^(url\(|linear-gradient|radial-gradient|conic-gradient|repeating-|-webkit-gradient|image-set|-webkit-image-set)/.test(x)) img = x;
      else if (/^(no-repeat|repeat|repeat-x|repeat-y|space|round)$/.test(x)) rep = x;
      else if (/^(border-box|padding-box|content-box|text)$/.test(x)) { or = x; cl = x; }
      else if (/^(scroll|fixed|local)$/.test(x)) at = x;
      else if (/^(left|right|top|bottom|center|-?[\d.]+(px|%|em|rem|vh|vw)?|calc\()/.test(x) && !isColorWord(x)) posParts.push(x);
      else finalColor = x;
    }
    if (posParts.length) pos = posParts.join(' ');
    if (after) sz = after;
    image.push(img); repeat.push(rep); position.push(pos); size.push(sz); clip.push(cl); origin.push(or); attach.push(at);
  }
  return [['background-color', finalColor], ['background-image', image.join(', ')], ['background-repeat', repeat.join(', ')], ['background-position', position.join(', ')], ['background-size', size.join(', ')], ['background-clip', clip.join(', ')], ['background-origin', origin.join(', ')], ['background-attachment', attach.join(', ')]];
}
function expandFont(v) {
  const sys = { caption: '13px system-ui', icon: '13px system-ui', menu: '13px system-ui', 'message-box': '13px system-ui', 'small-caption': '13px system-ui', 'status-bar': '13px system-ui' };
  if (sys[v]) v = sys[v];
  let style = 'normal', variant = 'normal', weight = 'normal', stretch = 'normal', size = 'medium', lh = 'normal', family = '';
  const parts = splitSpaces(v);
  let i = 0;
  for (; i < parts.length; i++) {
    const x = parts[i];
    if (x === 'italic' || x === 'oblique') style = x;
    else if (x === 'small-caps') variant = x;
    else if (/^(bold|bolder|lighter|[1-9]00)$/.test(x)) weight = x;
    else if (/^(ultra-condensed|extra-condensed|condensed|semi-condensed|semi-expanded|expanded|extra-expanded|ultra-expanded)$/.test(x)) stretch = x;
    else if (x === 'normal') continue;
    else break;
  }
  if (i < parts.length) {
    const [s, l] = splitSlash(parts[i]); size = s; if (l) lh = l;
    i++;
    if (i < parts.length && parts[i].startsWith('/')) { lh = parts[i].slice(1) || parts[++i]; i++; }
  }
  family = parts.slice(i).join(' ');
  return [['font-style', style], ['font-variant', variant], ['font-weight', weight], ['font-stretch', stretch], ['font-size', size], ['line-height', lh], ['font-family', family]];
}

// ---- colors
export const NAMED_COLORS = { transparent: [0, 0, 0, 0], black: [0, 0, 0, 1], white: [255, 255, 255, 1], red: [255, 0, 0, 1], green: [0, 128, 0, 1], blue: [0, 0, 255, 1], yellow: [255, 255, 0, 1], gray: [128, 128, 128, 1], grey: [128, 128, 128, 1], silver: [192, 192, 192, 1], orange: [255, 165, 0, 1], purple: [128, 0, 128, 1], cyan: [0, 255, 255, 1], aqua: [0, 255, 255, 1], magenta: [255, 0, 255, 1], fuchsia: [255, 0, 255, 1], lime: [0, 255, 0, 1], navy: [0, 0, 128, 1], teal: [0, 128, 128, 1], maroon: [128, 0, 0, 1], olive: [128, 128, 0, 1], pink: [255, 192, 203, 1], brown: [165, 42, 42, 1], gold: [255, 215, 0, 1], darkgray: [169, 169, 169, 1], darkgrey: [169, 169, 169, 1], lightgray: [211, 211, 211, 1], lightgrey: [211, 211, 211, 1], dimgray: [105, 105, 105, 1], dimgrey: [105, 105, 105, 1], whitesmoke: [245, 245, 245, 1], gainsboro: [220, 220, 220, 1], lightblue: [173, 216, 230, 1], skyblue: [135, 206, 235, 1], steelblue: [70, 130, 180, 1], dodgerblue: [30, 144, 255, 1], royalblue: [65, 105, 225, 1], tomato: [255, 99, 71, 1], coral: [255, 127, 80, 1], salmon: [250, 128, 114, 1], crimson: [220, 20, 60, 1], firebrick: [178, 34, 34, 1], darkred: [139, 0, 0, 1], orangered: [255, 69, 0, 1], darkorange: [255, 140, 0, 1], khaki: [240, 230, 140, 1], lightyellow: [255, 255, 224, 1], ivory: [255, 255, 240, 1], beige: [245, 245, 220, 1], wheat: [245, 222, 179, 1], tan: [210, 180, 140, 1], chocolate: [210, 105, 30, 1], sienna: [160, 82, 45, 1], darkgreen: [0, 100, 0, 1], forestgreen: [34, 139, 34, 1], seagreen: [46, 139, 87, 1], limegreen: [50, 205, 50, 1], springgreen: [0, 255, 127, 1], lightgreen: [144, 238, 144, 1], yellowgreen: [154, 205, 50, 1], darkslategray: [47, 79, 79, 1], slategray: [112, 128, 144, 1], lightslategray: [119, 136, 153, 1], midnightblue: [25, 25, 112, 1], darkblue: [0, 0, 139, 1], mediumblue: [0, 0, 205, 1], indigo: [75, 0, 130, 1], violet: [238, 130, 238, 1], orchid: [218, 112, 214, 1], plum: [221, 160, 221, 1], lavender: [230, 230, 250, 1], thistle: [216, 191, 216, 1], turquoise: [64, 224, 208, 1], darkcyan: [0, 139, 139, 1], cadetblue: [95, 158, 160, 1], aliceblue: [240, 248, 255, 1], azure: [240, 255, 255, 1], honeydew: [240, 255, 240, 1], mintcream: [245, 255, 250, 1], snow: [255, 250, 250, 1], linen: [250, 240, 230, 1], oldlace: [253, 245, 230, 1], seashell: [255, 245, 238, 1], cornsilk: [255, 248, 220, 1], lemonchiffon: [255, 250, 205, 1], moccasin: [255, 228, 181, 1], bisque: [255, 228, 196, 1], peachpuff: [255, 218, 185, 1], mistyrose: [255, 228, 225, 1], hotpink: [255, 105, 180, 1], deeppink: [255, 20, 147, 1], mediumpurple: [147, 112, 219, 1], rebeccapurple: [102, 51, 153, 1], slateblue: [106, 90, 205, 1], darkslateblue: [72, 61, 139, 1], mediumslateblue: [123, 104, 238, 1], blueviolet: [138, 43, 226, 1], darkviolet: [148, 0, 211, 1], darkmagenta: [139, 0, 139, 1], greenyellow: [173, 255, 47, 1], chartreuse: [127, 255, 0, 1], lawngreen: [124, 252, 0, 1], palegreen: [152, 251, 152, 1], mediumseagreen: [60, 179, 113, 1], darkseagreen: [143, 188, 143, 1], olivedrab: [107, 142, 35, 1], darkolivegreen: [85, 107, 47, 1], aquamarine: [127, 255, 212, 1], mediumaquamarine: [102, 205, 170, 1], lightseagreen: [32, 178, 170, 1], darkturquoise: [0, 206, 209, 1], mediumturquoise: [72, 209, 204, 1], paleturquoise: [175, 238, 238, 1], powderblue: [176, 224, 230, 1], lightskyblue: [135, 206, 250, 1], deepskyblue: [0, 191, 255, 1], cornflowerblue: [100, 149, 237, 1], lightsteelblue: [176, 196, 222, 1], lightcoral: [240, 128, 128, 1], indianred: [205, 92, 92, 1], rosybrown: [188, 143, 143, 1], darksalmon: [233, 150, 122, 1], lightsalmon: [255, 160, 122, 1], sandybrown: [244, 164, 96, 1], burlywood: [222, 184, 135, 1], goldenrod: [218, 165, 32, 1], darkgoldenrod: [184, 134, 11, 1], peru: [205, 133, 63, 1], saddlebrown: [139, 69, 19, 1], darkkhaki: [189, 183, 107, 1], palegoldenrod: [238, 232, 170, 1], lightcyan: [224, 255, 255, 1], lightpink: [255, 182, 193, 1], palevioletred: [219, 112, 147, 1], mediumvioletred: [199, 21, 133, 1], mediumorchid: [186, 85, 211, 1], darkorchid: [153, 50, 204, 1], navajowhite: [255, 222, 173, 1], blanchedalmond: [255, 235, 205, 1], antiquewhite: [250, 235, 215, 1], papayawhip: [255, 239, 213, 1], lavenderblush: [255, 240, 245, 1], floralwhite: [255, 250, 240, 1], ghostwhite: [248, 248, 255, 1], mediumspringgreen: [0, 250, 154, 1], lightgoldenrodyellow: [250, 250, 210, 1] };
export function isColorWord(x) { return x in NAMED_COLORS || x === 'currentcolor' || /^#|^rgba?\(|^hsla?\(|^color-mix\(|^color\(|^hwb\(|^lab\(|^lch\(|^oklab\(|^oklch\(/.test(x); }
// a color as [r, g, b, a] 0..255 / 0..1; null when it is not one
export function parseColor(v, current) {
  if (!v) return null;
  v = v.trim();
  const lower = v.toLowerCase();
  if (lower === 'currentcolor') return current || [0, 0, 0, 1];
  if (lower in NAMED_COLORS) return NAMED_COLORS[lower].slice();
  if (lower[0] === '#') {
    const h = lower.slice(1);
    if (h.length === 3 || h.length === 4) { const n = h.split('').map(c => parseInt(c + c, 16)); return [n[0], n[1], n[2], h.length === 4 ? n[3] / 255 : 1]; }
    if (h.length === 6 || h.length === 8) { const n = [0, 2, 4, 6].map(i => parseInt(h.slice(i, i + 2), 16)); return [n[0], n[1], n[2], h.length === 8 ? n[3] / 255 : 1]; }
    return null;
  }
  let m = lower.match(/^rgba?\((.*)\)$/);
  if (m) {
    const p = m[1].includes(',') ? m[1].split(',').map(s => s.trim()) : m[1].replace('/', ' ').split(/\s+/).filter(Boolean);
    const c = i => { const s = p[i]; if (s === undefined) return 0; if (s === 'none') return 0; return s.endsWith('%') ? parseFloat(s) * 2.55 : parseFloat(s); };
    const a = p[3] === undefined ? 1 : (p[3].endsWith('%') ? parseFloat(p[3]) / 100 : parseFloat(p[3]));
    return [c(0), c(1), c(2), isNaN(a) ? 1 : a];
  }
  m = lower.match(/^hsla?\((.*)\)$/);
  if (m) {
    const p = m[1].includes(',') ? m[1].split(',').map(s => s.trim()) : m[1].replace('/', ' ').split(/\s+/).filter(Boolean);
    const h = parseFloat(p[0]) || 0, s = (parseFloat(p[1]) || 0) / 100, l = (parseFloat(p[2]) || 0) / 100;
    const a = p[3] === undefined ? 1 : (p[3].endsWith('%') ? parseFloat(p[3]) / 100 : parseFloat(p[3]));
    const [r, g, b] = hslToRgb(h, s, l);
    return [r, g, b, isNaN(a) ? 1 : a];
  }
  m = lower.match(/^color-mix\(\s*in\s+[a-z-]+\s*,(.*)\)$/);
  if (m) {
    const parts = splitCommas(m[1]);
    const pa = parts[0].match(/^(.*?)\s+([\d.]+)%$/), pb = parts[1] && parts[1].match(/^(.*?)\s+([\d.]+)%$/);
    const ca = parseColor(pa ? pa[1] : parts[0], current), cb = parseColor(pb ? pb[1] : parts[1], current);
    if (!ca || !cb) return ca || cb;
    let wa = pa ? parseFloat(pa[2]) / 100 : (pb ? 1 - parseFloat(pb[2]) / 100 : 0.5), wb = pb ? parseFloat(pb[2]) / 100 : 1 - wa;
    const t = wa + wb; if (t > 0) { wa /= t; wb /= t; }
    const a = ca[3] * wa + cb[3] * wb;
    if (a === 0) return [0, 0, 0, 0];
    return [(ca[0] * ca[3] * wa + cb[0] * cb[3] * wb) / a, (ca[1] * ca[3] * wa + cb[1] * cb[3] * wb) / a, (ca[2] * ca[3] * wa + cb[2] * cb[3] * wb) / a, a];
  }
  m = lower.match(/^color\(\s*srgb\s+(.*)\)$/);
  if (m) { const p = m[1].replace('/', ' ').split(/\s+/).filter(Boolean); return [parseFloat(p[0]) * 255, parseFloat(p[1]) * 255, parseFloat(p[2]) * 255, p[3] === undefined ? 1 : parseFloat(p[3])]; }
  return null;
}
function hslToRgb(h, s, l) {
  h = ((h % 360) + 360) % 360 / 360;
  if (s === 0) return [l * 255, l * 255, l * 255];
  const q = l < 0.5 ? l * (1 + s) : l + s - l * s, p = 2 * l - q;
  const f = t => { if (t < 0) t += 1; if (t > 1) t -= 1; if (t < 1 / 6) return p + (q - p) * 6 * t; if (t < 1 / 2) return q; if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6; return p; };
  return [f(h + 1 / 3) * 255, f(h) * 255, f(h - 1 / 3) * 255];
}
export function colorToCss(c) { return c[3] >= 1 ? `rgb(${Math.round(c[0])}, ${Math.round(c[1])}, ${Math.round(c[2])})` : `rgba(${Math.round(c[0])}, ${Math.round(c[1])}, ${Math.round(c[2])}, ${+c[3].toFixed(3)})`; }

// ---- lengths. ctx: { em, rem, vw, vh, pct (the % base), root }
export function parseLength(v, ctx, pctBase) {
  if (v == null) return null;
  if (typeof v === 'number') return v;
  v = String(v).trim();
  if (v === '' || v === 'auto' || v === 'none') return null;
  if (v.startsWith('calc(') || v.startsWith('min(') || v.startsWith('max(') || v.startsWith('clamp(')) return calc(v, ctx, pctBase);
  const m = v.match(/^(-?[\d.]+(?:e-?\d+)?)([a-z%]*)$/i);
  if (!m) return null;
  const n = parseFloat(m[1]), u = m[2].toLowerCase();
  switch (u) {
  case '': case 'px': return n;
  case '%': return pctBase == null ? null : n * pctBase / 100;
  case 'em': return n * (ctx.em || 16);
  case 'rem': return n * (ctx.rem || 16);
  case 'vw': return n * (ctx.vw || 0) / 100;
  case 'vh': return n * (ctx.vh || 0) / 100;
  case 'vmin': return n * Math.min(ctx.vw || 0, ctx.vh || 0) / 100;
  case 'vmax': return n * Math.max(ctx.vw || 0, ctx.vh || 0) / 100;
  case 'ch': return n * (ctx.ch || (ctx.em || 16) * 0.5);
  case 'ex': return n * (ctx.em || 16) * 0.5;
  case 'pt': return n * 4 / 3;
  case 'pc': return n * 16;
  case 'in': return n * 96;
  case 'cm': return n * 96 / 2.54;
  case 'mm': return n * 96 / 25.4;
  case 'q': return n * 96 / 101.6;
  case 'lh': return n * (ctx.lh || (ctx.em || 16) * 1.2);
  case 'dvh': case 'svh': case 'lvh': return n * (ctx.vh || 0) / 100;
  case 'dvw': case 'svw': case 'lvw': return n * (ctx.vw || 0) / 100;
  }
  return null;
}
// calc(): numbers with units, + - * /, nesting, min/max/clamp
function calc(v, ctx, pctBase) {
  const toks = v.match(/[\d.]+(?:e-?\d+)?[a-z%]*|[-+*/(),]|min|max|clamp|calc|var/gi) || [];
  let i = 0;
  const value = t => { const n = parseLength(t, ctx, pctBase); return n == null ? (parseFloat(t) || 0) : n; };
  function expr() { let a = term(); while (toks[i] === '+' || toks[i] === '-') { const op = toks[i++]; const b = term(); a = op === '+' ? a + b : a - b; } return a; }
  function term() { let a = unary(); while (toks[i] === '*' || toks[i] === '/') { const op = toks[i++]; const b = unary(); a = op === '*' ? a * b : (b ? a / b : 0); } return a; }
  function unary() { if (toks[i] === '-') { i++; return -unary(); } if (toks[i] === '+') { i++; return unary(); } return atom(); }
  function atom() {
    const t = toks[i++];
    if (t === undefined) return 0;
    const l = t.toLowerCase();
    if (l === 'calc' || l === '(') { if (l === 'calc') i++; const r = expr(); if (toks[i] === ')') i++; return r; }
    if (l === 'min' || l === 'max' || l === 'clamp') {
      i++; const args = [expr()]; while (toks[i] === ',') { i++; args.push(expr()); } if (toks[i] === ')') i++;
      if (l === 'min') return Math.min(...args); if (l === 'max') return Math.max(...args);
      return Math.max(args[0], Math.min(args[1], args[2]));
    }
    return value(t);
  }
  return expr();
}
export function parseTime(v) { if (!v) return 0; const m = String(v).trim().match(/^([\d.]+)(ms|s)?$/); if (!m) return 0; return m[2] === 's' ? parseFloat(m[1]) * 1000 : parseFloat(m[1]); }

// ---- stylesheets
export class StyleRule { constructor(selectors, decls, order) { this.selectors = selectors; this.decls = decls; this.order = order; this.layer = 0; } }
let ruleOrder = 0;
// text -> { rules: [StyleRule], fontFaces: [], keyframes: Map, imports: [] }
export function parseStylesheet(text, media) {
  const sheet = { rules: [], fontFaces: [], keyframes: new Map(), imports: [], text };
  parseBlock(stripComments(text), sheet, media);
  return sheet;
}
function stripComments(s) { return s.replace(/\/\*[\s\S]*?\*\//g, ''); }
// one { } block body: rules and at-rules
function parseBlock(text, sheet, media, nestingParents) {
  // inside a rule, an at-rule's body is that rule's body
  if (nestingParents) { parseRuleBody(text, nestingParents, sheet, media); return; }
  let i = 0; const n = text.length;
  while (i < n) {
    while (i < n && /\s/.test(text[i])) i++;
    if (i >= n) break;
    if (text[i] === '@') {
      const end = findAtEnd(text, i);
      const head = text.slice(i, end).trim();
      if (text[end] === ';') { handleAtStatement(head, sheet); i = end + 1; continue; }
      const close = matchBrace(text, end);
      const body = text.slice(end + 1, close);
      handleAtBlock(head, body, sheet, media, nestingParents);
      i = close + 1;
      continue;
    }
    const open = text.indexOf('{', i);
    if (open < 0) break;
    const close = matchBrace(text, open);
    const prelude = text.slice(i, open).trim();
    const body = text.slice(open + 1, close);
    if (media && !media.matches) { i = close + 1; continue; }
    parseRuleBody(body, parseSelectorList(prelude, null), sheet, media);
    i = close + 1;
  }
}
// a rule's body: its declarations, then each nested rule, recursively
function parseRuleBody(body, selectors, sheet, media) {
  if (media && !media.matches) return;
  const { decls, nested } = splitNested(body);
  if (selectors.length && decls.length) sheet.rules.push(new StyleRule(selectors, expandDecls(decls), ruleOrder++));
  for (const [nprelude, nbody] of nested) {
    if (nprelude.startsWith('@')) { handleAtBlock(nprelude, nbody, sheet, media, selectors); continue; }
    parseRuleBody(nbody, parseSelectorList(nprelude, selectors), sheet, media);
  }
}
// a rule's declarations with every shorthand's longhands after it
export function expandDecls(decls) {
  const out = [];
  for (const d of decls) {
    out.push(d);
    const longs = expandShorthand(d[0], d[1]);
    if (longs) for (const [k, v] of longs) out.push([k, v, d[2]]);
  }
  return out;
}
function findAtEnd(text, i) { let depth = 0; for (let j = i; j < text.length; j++) { const c = text[j]; if (c === '(') depth++; else if (c === ')') depth--; else if (depth === 0 && (c === ';' || c === '{')) return j; } return text.length; }
function matchBrace(text, open) {
  let depth = 0, quote = 0;
  for (let j = open; j < text.length; j++) {
    const c = text[j];
    if (quote) { if (c === quote) quote = 0; else if (c === '\\') j++; continue; }
    if (c === '"' || c === "'") quote = c;
    else if (c === '{') depth++;
    else if (c === '}') { depth--; if (depth === 0) return j; }
  }
  return text.length;
}
// a rule body: declarations, and nested rules (& or @media)
function splitNested(body) {
  const nested = [];
  if (!body.includes('{')) return { decls: parseDeclarations(body), nested };
  let plain = '', i = 0, seg = 0, depth = 0, quote = 0;
  while (i < body.length) {
    const c = body[i];
    if (quote) { if (c === '\\') i++; else if (c === quote) quote = 0; i++; continue; }
    if (c === '"' || c === "'") quote = c;
    else if (c === '(') depth++;
    else if (c === ')') depth--;
    // a prelude starts after the last ; or nested block
    else if (depth === 0 && c === ';') { plain += body.slice(seg, i + 1); seg = i + 1; }
    else if (depth === 0 && c === '{') {
      const close = matchBrace(body, i);
      nested.push([body.slice(seg, i).trim(), body.slice(i + 1, close)]);
      i = close + 1; seg = i; continue;
    }
    i++;
  }
  plain += body.slice(seg);
  return { decls: parseDeclarations(plain), nested };
}
function handleAtStatement(head, sheet) {
  const m = head.match(/^@import\s+(?:url\()?["']?([^"')]+)["']?\)?(.*)$/);
  if (m) sheet.imports.push({ url: m[1], media: m[2].trim() });
}
function handleAtBlock(head, body, sheet, media, parents) {
  const m = head.match(/^@([a-z-]+)\s*(.*)$/i);
  if (!m) return;
  const kind = m[1].toLowerCase(), params = m[2].trim();
  switch (kind) {
  case 'media': { const mq = { text: params, matches: evalMedia(params) && (!media || media.matches) }; parseBlock(body, sheet, mq, parents); break; }
  case 'supports': { if (evalSupports(params)) parseBlock(body, sheet, media, parents); break; }
  case 'layer': case 'scope': case 'container': case 'starting-style': parseBlock(body, sheet, media, parents); break;
  case 'font-face': { const d = new Map(parseDeclarations(body).map(([k, v]) => [k, v])); sheet.fontFaces.push(d); break; }
  case 'keyframes': case '-webkit-keyframes': {
    const frames = [];
    let i = 0;
    while (i < body.length) {
      const open = body.indexOf('{', i); if (open < 0) break;
      const close = matchBrace(body, open);
      const sel = body.slice(i, open).trim().split(',').map(s => s.trim()).map(s => s === 'from' ? 0 : s === 'to' ? 1 : parseFloat(s) / 100);
      const decls = parseDeclarations(body.slice(open + 1, close));
      for (const off of sel) frames.push({ offset: off, decls });
      i = close + 1;
    }
    sheet.keyframes.set(params, frames.sort((a, b) => a.offset - b.offset));
    break;
  }
  case 'page': case 'counter-style': case 'property': case 'font-feature-values': case 'namespace': case 'charset': break;
  }
}
export const mediaEnv = { width: 1280, height: 800, dpr: 1, dark: true, reducedMotion: false, hover: true, pointer: 'fine', highContrast: false, forcedColors: false };
export function evalMedia(q) {
  q = q.trim().toLowerCase();
  if (!q || q === 'all' || q === 'screen') return true;
  if (q === 'print') return false;
  return splitCommas(q).some(part => evalMediaAnd(part));
}
function evalMediaAnd(q) {
  let neg = false;
  if (q.startsWith('not ')) { neg = true; q = q.slice(4); }
  if (q.startsWith('only ')) q = q.slice(5);
  const terms = q.split(/\s+and\s+/);
  const r = terms.every(t => {
    t = t.trim();
    if (t === 'all' || t === 'screen') return true;
    if (t === 'print' || t === 'speech') return false;
    const m = t.match(/^\(\s*([a-z-]+)\s*(?::\s*(.*?))?\s*\)$/);
    if (!m) { const r = t.match(/^\(\s*(?:width|height)\s*([<>]=?)\s*([\d.]+)px\s*\)$/); if (r) { const v = t.includes('width') ? mediaEnv.width : mediaEnv.height; const n = parseFloat(r[2]); return r[1] === '<' ? v < n : r[1] === '<=' ? v <= n : r[1] === '>' ? v > n : v >= n; } return false; }
    const f = m[1], v = m[2];
    switch (f) {
    case 'prefers-color-scheme': return v === (mediaEnv.dark ? 'dark' : 'light');
    case 'prefers-reduced-motion': return (v === 'reduce') === mediaEnv.reducedMotion;
    case 'prefers-contrast': return v === 'more' ? mediaEnv.highContrast : v === 'no-preference' ? !mediaEnv.highContrast : false;
    case 'forced-colors': return (v === 'active') === mediaEnv.forcedColors;
    case 'hover': return (v === 'hover') === mediaEnv.hover;
    case 'any-hover': return (v === 'hover') === mediaEnv.hover;
    case 'pointer': case 'any-pointer': return v === mediaEnv.pointer;
    case 'min-width': return mediaEnv.width >= parseFloat(v);
    case 'max-width': return mediaEnv.width <= parseFloat(v);
    case 'min-height': return mediaEnv.height >= parseFloat(v);
    case 'max-height': return mediaEnv.height <= parseFloat(v);
    case 'width': return v === undefined || mediaEnv.width === parseFloat(v);
    case 'height': return v === undefined || mediaEnv.height === parseFloat(v);
    case 'orientation': return v === (mediaEnv.width >= mediaEnv.height ? 'landscape' : 'portrait');
    case 'min-resolution': return mediaEnv.dpr * 96 >= parseFloat(v);
    case '-webkit-min-device-pixel-ratio': return mediaEnv.dpr >= parseFloat(v);
    case 'min-device-pixel-ratio': return mediaEnv.dpr >= parseFloat(v);
    case 'resolution': return true;
    case 'display-mode': return v === 'standalone';
    case 'prefers-reduced-transparency': return v === 'no-preference';
    case 'color': case 'color-gamut': case 'dynamic-range': return v === undefined || v === 'srgb' || v === 'standard';
    case 'scripting': return v === 'enabled';
    case 'update': return v === 'fast';
    case 'inverted-colors': return v === 'none';
    }
    return false;
  });
  return neg ? !r : r;
}
function evalSupports(q) {
  q = q.trim();
  if (q.startsWith('not ')) return !evalSupports(q.slice(4));
  if (/\s+or\s+/.test(q)) return q.split(/\s+or\s+/).some(evalSupports);
  if (/\s+and\s+/.test(q)) return q.split(/\s+and\s+/).every(evalSupports);
  if (q.startsWith('(') && q.endsWith(')')) q = q.slice(1, -1).trim();
  if (q.startsWith('selector(')) return true;
  const m = q.match(/^([a-z-]+)\s*:\s*(.*)$/i);
  if (!m) return true;
  const prop = m[1].toLowerCase(), val = m[2].trim().toLowerCase();
  if (prop === 'display' && (val === 'grid' || val === 'subgrid' || val.includes('masonry'))) return val === 'grid';
  if (prop === 'container-type' || prop === 'anchor-name' || prop === 'view-transition-name') return false;
  return true;
}

// ---- selectors
// a complex selector: [{ compound, combinator }] left to right; matched right to left
export function parseSelectorList(text, parents) {
  const out = [];
  for (let s of splitCommas(text)) {
    s = s.trim();
    if (!s) continue;
    if (parents && parents.length) {
      if (s.includes('&')) { for (const p of parents) out.push(...parseSelectorList(s.replace(/&/g, p.text), null)); continue; }
      for (const p of parents) out.push(...parseSelectorList(p.text + ' ' + s, null));
      continue;
    }
    const sel = parseComplex(s);
    if (sel) out.push(sel);
  }
  return out;
}
function parseComplex(s) {
  const parts = []; let i = 0, depth = 0, cur = '', pendingComb = null;
  const pushCompound = () => { if (cur.trim()) { parts.push({ compound: parseCompound(cur.trim()), comb: pendingComb }); cur = ''; pendingComb = null; } };
  while (i < s.length) {
    const c = s[i];
    if (c === '(' || c === '[') depth++;
    if (c === ')' || c === ']') depth--;
    if (depth === 0 && (c === '>' || c === '+' || c === '~')) { pushCompound(); pendingComb = c; i++; continue; }
    if (depth === 0 && /\s/.test(c)) {
      let j = i; while (j < s.length && /\s/.test(s[j])) j++;
      if (cur.trim() && j < s.length && !'>+~'.includes(s[j])) { pushCompound(); pendingComb = ' '; }
      i = j; continue;
    }
    cur += c; i++;
  }
  pushCompound();
  if (!parts.length) return null;
  let spec = [0, 0, 0];
  for (const p of parts) { const sp = p.compound.spec; spec = [spec[0] + sp[0], spec[1] + sp[1], spec[2] + sp[2]]; }
  // keys an ancestor of a matching element must carry: every
  // compound left of the last, but one that names a sibling
  const anc = [];
  for (let i = 0; i < parts.length - 1; i++) {
    if (parts[i + 1].comb === '+' || parts[i + 1].comb === '~') continue;
    const c = parts[i].compound;
    if (c.id) anc.push('#' + c.id);
    for (const x of c.classes) anc.push('.' + x);
    if (c.tag) anc.push(c.tag);
  }
  return { parts, spec: spec[0] * 1000000 + spec[1] * 1000 + spec[2], text: s, pseudoEl: parts[parts.length - 1].compound.pseudoEl, anc: anc.length ? anc : null };
}
function parseCompound(s) {
  const c = { tag: null, id: null, classes: [], attrs: [], pseudos: [], pseudoEl: null, spec: [0, 0, 0] };
  let i = 0;
  const ident = () => { let j = i; while (j < s.length && /[\w-]|\\|[^\x00-\x7f]/.test(s[j])) { if (s[j] === '\\') j++; j++; } const r = s.slice(i, j).replace(/\\(.)/g, '$1'); i = j; return r; };
  while (i < s.length) {
    const ch = s[i];
    if (ch === '*') { i++; continue; }
    if (ch === '#') { i++; c.id = ident(); c.spec[0]++; continue; }
    if (ch === '.') { i++; c.classes.push(ident()); c.spec[1]++; continue; }
    if (ch === '[') {
      const end = s.indexOf(']', i); const body = s.slice(i + 1, end).trim(); i = end + 1;
      const m = body.match(/^([\w-]+)\s*(?:([~|^$*]?=)\s*(?:"([^"]*)"|'([^']*)'|([^\s\]]+))\s*([is])?)?$/i);
      if (m) c.attrs.push({ name: m[1].toLowerCase(), op: m[2] || null, value: m[3] ?? m[4] ?? m[5] ?? null, ci: m[6] === 'i' });
      c.spec[1]++; continue;
    }
    if (ch === ':') {
      if (s[i + 1] === ':') { i += 2; c.pseudoEl = ident(); if (s[i] === '(') { i = skipParens(s, i); } continue; }
      i++;
      const name = ident().toLowerCase();
      let arg = null;
      if (s[i] === '(') { const end = skipParens(s, i); arg = s.slice(i + 1, end - 1); i = end; }
      if (name === 'before' || name === 'after' || name === 'placeholder' || name === 'selection' || name === 'first-line' || name === 'first-letter') { c.pseudoEl = name; continue; }
      const p = { name, arg };
      if (name === 'not' || name === 'is' || name === 'where' || name === 'has' || name === 'matches' || name === '-webkit-any') {
        p.list = parseSelectorList(arg, null);
        if (name !== 'where') { let best = [0, 0, 0]; for (const l of p.list) { const sp = specParts(l.spec); if (l.spec > best[0] * 1000000 + best[1] * 1000 + best[2]) best = sp; } c.spec = c.spec.map((v, k) => v + best[k]); }
      } else if (name.startsWith('nth-')) { p.nth = parseNth(arg || ''); c.spec[1]++; }
      else c.spec[1]++;
      c.pseudos.push(p);
      continue;
    }
    const t = ident();
    if (!t) { i++; continue; }
    c.tag = t.toLowerCase(); c.spec[2]++;
  }
  return c;
}
function specParts(n) { return [Math.floor(n / 1000000), Math.floor(n / 1000) % 1000, n % 1000]; }
function skipParens(s, i) { let d = 0; for (; i < s.length; i++) { if (s[i] === '(') d++; else if (s[i] === ')') { d--; if (d === 0) return i + 1; } } return i; }
function parseNth(a) {
  a = a.trim().toLowerCase();
  let ofSel = null;
  const of = a.match(/^(.*?)\s+of\s+(.*)$/);
  if (of) { a = of[1]; ofSel = parseSelectorList(of[2], null); }
  if (a === 'odd') return { a: 2, b: 1, of: ofSel }; if (a === 'even') return { a: 2, b: 0, of: ofSel };
  const m = a.replace(/\s/g, '').match(/^([+-]?\d*)n([+-]\d+)?$|^([+-]?\d+)$/);
  if (!m) return { a: 0, b: 0, of: ofSel };
  if (m[3] !== undefined) return { a: 0, b: parseInt(m[3]), of: ofSel };
  const A = m[1] === '' || m[1] === '+' ? 1 : m[1] === '-' ? -1 : parseInt(m[1]);
  return { a: A, b: m[2] ? parseInt(m[2]) : 0, of: ofSel };
}
function nthMatch(nth, idx) { if (nth.a === 0) return idx === nth.b; const k = (idx - nth.b) / nth.a; return k >= 0 && Number.isInteger(k); }

// matching. pseudo-class state lives on the element: _pseudo (Set)
export function matchesComplex(el, sel, scope, ancKeys) {
  const parts = sel.parts;
  if (ancKeys && sel.anc) for (const k of sel.anc) if (!ancKeys.has(k)) return false;
  return matchFrom(el, parts, parts.length - 1, scope);
}
// the ids, classes and tags of el's ancestors (shadow hosts too)
export function ancestorKeys(el) {
  const set = new Set();
  for (let a = el.parentElement || (el.parentNode && el.parentNode.host) || null; a; a = a.parentElement || (a.parentNode && a.parentNode.host) || null) {
    set.add(a.localName);
    const id = a._attrs.get('id'); if (id) set.add('#' + id);
    const cls = a._classes(); if (cls) for (const c of cls) set.add('.' + c);
  }
  return set;
}
function matchFrom(el, parts, k, scope) {
  if (!matchCompound(el, parts[k].compound, scope)) return false;
  if (k === 0) return true;
  const comb = parts[k].comb;
  if (comb === '>') { const p = el.parentElement; return !!p && matchFrom(p, parts, k - 1, scope); }
  if (comb === '+') { const p = el.previousElementSibling; return !!p && matchFrom(p, parts, k - 1, scope); }
  if (comb === '~') { for (let p = el.previousElementSibling; p; p = p.previousElementSibling) if (matchFrom(p, parts, k - 1, scope)) return true; return false; }
  for (let p = el.parentElement; p; p = p.parentElement) if (matchFrom(p, parts, k - 1, scope)) return true;
  return false;
}
export function matchCompound(el, c, scope) {
  if (c.tag && c.tag !== el.localName && !(el.namespaceURI === 'http://www.w3.org/2000/svg' && c.tag === el.localName.toLowerCase())) return false;
  if (c.id !== null && el.getAttribute('id') !== c.id) return false;
  if (c.classes.length) { const set = el._classes(); if (!set) return false; for (const x of c.classes) if (!set.has(x)) return false; }
  for (const a of c.attrs) {
    let v = el.getAttribute(a.name);
    if (v === null) return false;
    if (a.op === null) continue;
    let want = a.value;
    if (a.ci) { v = v.toLowerCase(); want = want.toLowerCase(); }
    switch (a.op) {
    case '=': if (v !== want) return false; break;
    case '~=': if (!v.split(/\s+/).includes(want)) return false; break;
    case '|=': if (v !== want && !v.startsWith(want + '-')) return false; break;
    case '^=': if (!want || !v.startsWith(want)) return false; break;
    case '$=': if (!want || !v.endsWith(want)) return false; break;
    case '*=': if (!want || !v.includes(want)) return false; break;
    }
  }
  for (const p of c.pseudos) if (!matchPseudo(el, p, scope)) return false;
  return true;
}
function matchPseudo(el, p, scope) {
  switch (p.name) {
  case 'not': return !p.list.some(s => matchesComplex(el, s, scope));
  case 'is': case 'matches': case '-webkit-any': case 'where': return p.list.some(s => matchesComplex(el, s, scope));
  case 'has': return hasMatch(el, p.list);
  case 'hover': return el._pseudo.has('hover');
  case 'active': return el._pseudo.has('active');
  case 'focus': return el._pseudo.has('focus');
  case 'focus-visible': return el._pseudo.has('focus') && el._pseudo.has('focus-visible');
  case 'focus-within': return el._pseudo.has('focus-within');
  case 'checked': return el._pseudo.has('checked') || (el.localName === 'input' && (el.getAttribute('type') === 'checkbox' || el.getAttribute('type') === 'radio') && el.checked) || (el.localName === 'option' && el.selected);
  case 'disabled': return el.hasAttribute('disabled') || (!!el.closest && !!el.closest('fieldset[disabled]'));
  case 'enabled': return !el.hasAttribute('disabled') && FORMISH.has(el.localName);
  case 'required': return el.hasAttribute('required'); case 'optional': return !el.hasAttribute('required') && FORMISH.has(el.localName);
  case 'read-only': return !(el.localName === 'input' || el.localName === 'textarea' || el.isContentEditable) || el.hasAttribute('readonly');
  case 'read-write': return (el.localName === 'input' || el.localName === 'textarea' || el.isContentEditable) && !el.hasAttribute('readonly');
  case 'placeholder-shown': return (el.localName === 'input' || el.localName === 'textarea') && !el.value && el.hasAttribute('placeholder');
  case 'invalid': return false; case 'valid': return true; case 'indeterminate': return !!el.indeterminate;
  case 'empty': return el.childNodes.every(n => n.nodeType === 8);
  case 'root': return el === el.ownerDocument.documentElement;
  case 'scope': return scope ? el === scope : el === el.ownerDocument.documentElement;
  case 'first-child': return !el.previousElementSibling;
  case 'last-child': return !el.nextElementSibling;
  case 'only-child': return !el.previousElementSibling && !el.nextElementSibling;
  case 'first-of-type': { for (let s = el.previousElementSibling; s; s = s.previousElementSibling) if (s.localName === el.localName) return false; return true; }
  case 'last-of-type': { for (let s = el.nextElementSibling; s; s = s.nextElementSibling) if (s.localName === el.localName) return false; return true; }
  case 'only-of-type': return matchPseudo(el, { name: 'first-of-type' }) && matchPseudo(el, { name: 'last-of-type' });
  case 'nth-child': case 'nth-last-child': case 'nth-of-type': case 'nth-last-of-type': {
    const ofType = p.name.includes('of-type');
    const last = p.name.includes('last');
    let idx = 1;
    const test = s => (ofType ? s.localName === el.localName : true) && (p.nth.of ? p.nth.of.some(sel => matchesComplex(s, sel, scope)) : true);
    for (let s = last ? el.nextElementSibling : el.previousElementSibling; s; s = last ? s.nextElementSibling : s.previousElementSibling) if (test(s)) idx++;
    return nthMatch(p.nth, idx);
  }
  case 'link': case 'any-link': return el.localName === 'a' && el.hasAttribute('href');
  case 'visited': return false; case 'target': return false; case 'defined': return true;
  case 'lang': return (el.closest && el.closest('[lang]') || { getAttribute: () => 'en' }).getAttribute('lang').startsWith(p.arg || '');
  case 'dir': return (el.getAttribute('dir') || 'ltr') === p.arg;
  case 'host': case 'host-context': return false;
  case 'fullscreen': return false; case 'modal': return false; case 'popover-open': return false;
  case 'autofill': case '-webkit-autofill': return false;
  case 'default': return false;
  }
  return false;
}
const FORMISH = new Set(['input', 'textarea', 'select', 'button', 'option', 'fieldset', 'optgroup']);
function hasMatch(el, list) {
  const walk = n => { for (const c of n.childNodes) { if (c.nodeType === 1) { if (list.some(s => matchesRelative(c, s, el))) return true; if (walk(c)) return true; } } return false; };
  return walk(el);
}
// :has(> a) style selectors are matched against el as the scope
function matchesRelative(cand, sel, scope) {
  const parts = sel.parts;
  if (!matchFrom(cand, parts, parts.length - 1, scope)) return false;
  // the leftmost compound's combinator binds to the scope
  const first = parts[0];
  if (!first.comb) return scope.contains(cand) && cand !== scope;
  return true;
}
