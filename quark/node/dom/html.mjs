// HTML into the tree: a tokenizer and a tree builder with the
// implied closes pages rely on (p, li, option, table rows, head/body)
import { VOID, SVG_NS, HTML_NS } from './node.mjs';
const RAW_TEXT = new Set(['script', 'style', 'textarea', 'title', 'xmp', 'noscript']);
const ENTITIES = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'", nbsp: ' ', copy: '©', reg: '®', trade: '™', hellip: '…', mdash: '—', ndash: '–', lsquo: '‘', rsquo: '’', ldquo: '“', rdquo: '”', bull: '•', middot: '·', times: '×', divide: '÷', laquo: '«', raquo: '»', deg: '°', plusmn: '±', euro: '€', pound: '£', yen: '¥', cent: '¢', sect: '§', para: '¶', larr: '←', rarr: '→', uarr: '↑', darr: '↓', harr: '↔', ensp: ' ', emsp: ' ', thinsp: ' ', zwnj: '‌', zwj: '‍', shy: '­', iexcl: '¡', iquest: '¿', frac12: '½', frac14: '¼', frac34: '¾', micro: 'µ', check: '✓', cross: '✗', star: '☆', hearts: '♥', infin: '∞', ne: '≠', le: '≤', ge: '≥', minus: '−', lowast: '∗', radic: '√', prop: '∝', ang: '∠', and: '∧', or: '∨', cap: '∩', cup: '∪', int: '∫', there4: '∴', sim: '∼', cong: '≅', asymp: '≈', equiv: '≡', sub: '⊂', sup: '⊃', nsub: '⊄', sube: '⊆', supe: '⊇', oplus: '⊕', otimes: '⊗', perp: '⊥', sdot: '⋅', alpha: 'α', beta: 'β', gamma: 'γ', delta: 'δ', epsilon: 'ε', lambda: 'λ', mu: 'μ', pi: 'π', sigma: 'σ', omega: 'ω', Delta: 'Δ', Omega: 'Ω', Sigma: 'Σ', Pi: 'Π', uuml: 'ü', ouml: 'ö', auml: 'ä', Uuml: 'Ü', Ouml: 'Ö', Auml: 'Ä', szlig: 'ß', eacute: 'é', egrave: 'è', ecirc: 'ê', agrave: 'à', aacute: 'á', acirc: 'â', ccedil: 'ç', ntilde: 'ñ', oacute: 'ó', iacute: 'í', uacute: 'ú', Eacute: 'É', nbsp2: ' ', tab: '\t', newline: '\n', excl: '!', num: '#', dollar: '$', percnt: '%', lpar: '(', rpar: ')', ast: '*', plus: '+', comma: ',', period: '.', sol: '/', colon: ':', semi: ';', equals: '=', quest: '?', commat: '@', lsqb: '[', bsol: '\\', rsqb: ']', lowbar: '_', grave: '`', lcub: '{', verbar: '|', rcub: '}', tilde: '~', Tab: '\t', NewLine: '\n' };
export function decodeEntities(s) {
  if (s.indexOf('&') < 0) return s;
  return s.replace(/&(#x[0-9a-fA-F]+|#\d+|[a-zA-Z][a-zA-Z0-9]*);?/g, (m, e) => {
    if (e[0] === '#') { const n = e[1] === 'x' || e[1] === 'X' ? parseInt(e.slice(2), 16) : parseInt(e.slice(1)); return isNaN(n) ? m : String.fromCodePoint(n > 0x10ffff ? 0xfffd : n || 0xfffd); }
    return ENTITIES[e] !== undefined ? ENTITIES[e] : m;
  });
}
const CLOSE_P = new Set(['address', 'article', 'aside', 'blockquote', 'details', 'div', 'dl', 'fieldset', 'figcaption', 'figure', 'footer', 'form', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'header', 'hgroup', 'hr', 'main', 'menu', 'nav', 'ol', 'p', 'pre', 'section', 'table', 'ul']);
const IMPLIED_END = { li: ['li'], dt: ['dt', 'dd'], dd: ['dt', 'dd'], option: ['option', 'optgroup'], optgroup: ['optgroup'], tr: ['tr', 'td', 'th'], td: ['td', 'th'], th: ['td', 'th'], thead: ['tbody', 'tfoot', 'thead'], tbody: ['tbody', 'tfoot', 'thead'], tfoot: ['tbody', 'tfoot', 'thead'], rt: ['rt', 'rp'], rp: ['rt', 'rp'] };
const SCOPE_STOP = new Set(['html', 'table', 'td', 'th', 'caption', 'template', 'ul', 'ol', 'div', 'section', 'article']);

export function parseHTML(doc, text, context) {
  const root = context ? doc.createDocumentFragment() : doc;
  const stack = [root];
  let i = 0; const n = text.length;
  const top = () => stack[stack.length - 1];
  const topName = () => { const t = top(); return t.localName || ''; };
  const inSvg = () => stack.some(e => e.namespaceURI === SVG_NS);
  let inHead = !context, head = null, body = null;
  const findOpen = name => { for (let k = stack.length - 1; k > 0; k--) { if (stack[k].localName === name) return k; if (!context && SCOPE_STOP.has(stack[k].localName) && stack[k].localName !== name) { /* keep looking across for html-level tags */ } } return -1; };
  function appendText(s) {
    if (!s) return;
    if (!context && top() === doc) { if (!s.trim()) return; ensureBody(); }
    if (!context && (top() === doc.documentElement || top() === head) && !s.trim()) return;
    if (!context && top() === doc.documentElement) ensureBody();
    const t = top();
    const last = t.childNodes[t.childNodes.length - 1];
    if (last && last.nodeType === 3) last._data += s; else t.appendChild(doc.createTextNode(s));
  }
  function ensureHtml() { if (context) return top(); if (!doc.documentElement) { const h = doc._makeElement('html'); doc.appendChild(h); stack.push(h); } return doc.documentElement; }
  function ensureBody() {
    if (context) return;
    ensureHtml();
    if (!body) {
      if (head && stack.includes(head)) stack.length = stack.indexOf(head);
      if (!stack.includes(doc.documentElement)) stack.push(doc.documentElement);
      body = doc._makeElement('body'); doc.documentElement.appendChild(body); stack.push(body); inHead = false;
    } else if (!stack.includes(body)) { stack.length = 1; stack.push(doc.documentElement, body); }
  }
  function openElement(name, attrs, selfClose) {
    if (!context) {
      if (name === 'html') { const h = ensureHtml(); for (const [k, v] of attrs) if (!h.hasAttribute(k)) h.setAttribute(k, v); return; }
      if (name === 'head') { ensureHtml(); if (!head) { head = doc._makeElement('head'); doc.documentElement.appendChild(head); } stack.length = 2; stack.push(head); inHead = true; return; }
      if (name === 'body') { ensureBody(); for (const [k, v] of attrs) if (!body.hasAttribute(k)) body.setAttribute(k, v); return; }
      if (inHead && !['meta', 'link', 'title', 'style', 'script', 'base', 'noscript', 'template'].includes(name)) { ensureBody(); }
      else if (!body && !head && ['meta', 'link', 'title', 'style', 'script', 'base'].includes(name)) { ensureHtml(); head = doc._makeElement('head'); doc.documentElement.appendChild(head); stack.length = 2; stack.push(head); inHead = true; }
      else if (!body && !inHead) ensureBody();
    }
    // implied ends
    if (name === 'p' || CLOSE_P.has(name)) { const k = stack.lastIndexOf(stack.find(e => e.localName === 'p')); if (k > 0 && !stack.slice(k + 1).some(e => SCOPE_STOP.has(e.localName))) stack.length = k; }
    const imp = IMPLIED_END[name];
    if (imp) { for (let k = stack.length - 1; k > 0; k--) { const l = stack[k].localName; if (imp.includes(l)) { stack.length = k; break; } if (SCOPE_STOP.has(l) || l === 'select' || l === 'dl') break; } }
    if ((name === 'td' || name === 'th' || name === 'tr') && !context) {
      // cells land in a row, rows in a tbody
      if (name !== 'tr' && topName() !== 'tr') { if (findOpen('table') >= 0) openElement('tr', [], false); }
      if (name === 'tr' && topName() !== 'tbody' && topName() !== 'thead' && topName() !== 'tfoot' && topName() === 'table') openElement('tbody', [], false);
    }
    const svg = name === 'svg' || inSvg();
    const el = doc._makeElement(svg ? name : name.toLowerCase(), svg ? SVG_NS : HTML_NS);
    for (const [k, v] of attrs) el._attrs.set(k, v);
    top().appendChild(el);
    if (el.localName === 'template' && !svg) { el._content = doc.createDocumentFragment(); }
    if (!(VOID.has(name) && !svg) && !(selfClose && svg)) stack.push(el);
  }
  function closeElement(name) {
    if (!context && (name === 'html' || name === 'body' || name === 'head')) { if (name === 'head') { inHead = false; stack.length = 2; } return; }
    for (let k = stack.length - 1; k > 0; k--) {
      if (stack[k].localName === name) { stack.length = k; return; }
      if (name === 'p' && k === stack.length - 1 && false) return;
    }
    if (name === 'p') { openElement('p', [], false); stack.pop(); }
  }
  while (i < n) {
    const lt = text.indexOf('<', i);
    if (lt < 0) { appendText(decodeEntities(text.slice(i))); break; }
    if (lt > i) appendText(decodeEntities(text.slice(i, lt)));
    i = lt;
    if (text.startsWith('<!--', i)) { const end = text.indexOf('-->', i + 4); const data = text.slice(i + 4, end < 0 ? n : end); if (top() !== doc || context) top().appendChild(doc.createComment(data)); i = end < 0 ? n : end + 3; continue; }
    if (text.startsWith('<!', i) || text.startsWith('<?', i)) { const end = text.indexOf('>', i); const body = text.slice(i + 2, end < 0 ? n : end); if (/^doctype/i.test(body) && !context) doc._setDoctype(body.replace(/^doctype\s*/i, '').split(/\s/)[0] || 'html'); i = end < 0 ? n : end + 1; continue; }
    if (text[i + 1] === '/') {
      const end = text.indexOf('>', i); const name = text.slice(i + 2, end < 0 ? n : end).trim().toLowerCase().split(/\s/)[0];
      closeElement(inSvg() ? text.slice(i + 2, end).trim().split(/\s/)[0] : name);
      i = end < 0 ? n : end + 1; continue;
    }
    // a start tag
    let j = i + 1;
    const nm = text.slice(j).match(/^[a-zA-Z][^\s/>]*/);
    if (!nm) { appendText('<'); i++; continue; }
    const rawName = nm[0]; j += rawName.length;
    const attrs = [];
    let selfClose = false;
    for (;;) {
      while (j < n && /\s/.test(text[j])) j++;
      if (j >= n) break;
      if (text[j] === '>') { j++; break; }
      if (text[j] === '/' ) { if (text[j + 1] === '>') { selfClose = true; j += 2; break; } j++; continue; }
      const am = text.slice(j).match(/^[^\s=\/>]+/);
      if (!am) { j++; continue; }
      let aname = am[0]; j += aname.length;
      while (j < n && /\s/.test(text[j])) j++;
      let val = '';
      if (text[j] === '=') {
        j++; while (j < n && /\s/.test(text[j])) j++;
        const q = text[j];
        if (q === '"' || q === "'") { const e = text.indexOf(q, j + 1); val = text.slice(j + 1, e < 0 ? n : e); j = e < 0 ? n : e + 1; }
        else { const vm = text.slice(j).match(/^[^\s>]*/); val = vm[0]; j += val.length; }
      }
      attrs.push([inSvg() || rawName === 'svg' ? aname : aname.toLowerCase(), decodeEntities(val)]);
    }
    const name = (inSvg() || rawName === 'svg') ? rawName : rawName.toLowerCase();
    i = j;
    openElement(name, attrs, selfClose);
    const lname = name.toLowerCase();
    if (RAW_TEXT.has(lname) && !selfClose && !inSvg()) {
      const close = new RegExp(`</${lname}\\s*>`, 'ig'); close.lastIndex = i;
      const m = close.exec(text);
      const end = m ? m.index : n;
      let raw = text.slice(i, end);
      if (lname === 'textarea' || lname === 'title') raw = decodeEntities(raw);
      const el = top();
      if (raw) el.appendChild(doc.createTextNode(raw));
      stack.pop();
      i = m ? m.index + m[0].length : n;
    }
  }
  if (!context) { ensureHtml(); if (!body) ensureBody(); }
  return root;
}
