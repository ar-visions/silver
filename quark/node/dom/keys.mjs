// GLFW key codes (as trinity reports them) to DOM key, code, keyCode
const NAMED = { 256: ['Escape', 'Escape', 27], 257: ['Enter', 'Enter', 13], 258: ['Tab', 'Tab', 9], 259: ['Backspace', 'Backspace', 8], 260: ['Insert', 'Insert', 45], 261: ['Delete', 'Delete', 46], 262: ['ArrowRight', 'ArrowRight', 39], 263: ['ArrowLeft', 'ArrowLeft', 37], 264: ['ArrowDown', 'ArrowDown', 40], 265: ['ArrowUp', 'ArrowUp', 38], 266: ['PageUp', 'PageUp', 33], 267: ['PageDown', 'PageDown', 34], 268: ['Home', 'Home', 36], 269: ['End', 'End', 35], 280: ['CapsLock', 'CapsLock', 20], 281: ['ScrollLock', 'ScrollLock', 145], 282: ['NumLock', 'NumLock', 144], 283: ['PrintScreen', 'PrintScreen', 44], 284: ['Pause', 'Pause', 19], 330: ['.', 'NumpadDecimal', 110], 331: ['/', 'NumpadDivide', 111], 332: ['*', 'NumpadMultiply', 106], 333: ['-', 'NumpadSubtract', 109], 334: ['+', 'NumpadAdd', 107], 335: ['Enter', 'NumpadEnter', 13], 336: ['=', 'NumpadEqual', 187], 340: ['Shift', 'ShiftLeft', 16], 341: ['Control', 'ControlLeft', 17], 342: ['Alt', 'AltLeft', 18], 343: ['Meta', 'MetaLeft', 91], 344: ['Shift', 'ShiftRight', 16], 345: ['Control', 'ControlRight', 17], 346: ['Alt', 'AltRight', 18], 347: ['Meta', 'MetaRight', 92], 348: ['ContextMenu', 'ContextMenu', 93] };
const PUNCT = { 32: ['Space', 32, ' '], 39: ['Quote', 222, "'", '"'], 44: ['Comma', 188, ',', '<'], 45: ['Minus', 189, '-', '_'], 46: ['Period', 190, '.', '>'], 47: ['Slash', 191, '/', '?'], 59: ['Semicolon', 186, ';', ':'], 61: ['Equal', 187, '=', '+'], 91: ['BracketLeft', 219, '[', '{'], 92: ['Backslash', 220, '\\', '|'], 93: ['BracketRight', 221, ']', '}'], 96: ['Backquote', 192, '`', '~'], 161: ['IntlBackslash', 226, '<', '>'] };
const DIGIT_SHIFT = ')!@#$%^&*(';
export function keyInfo(code, mods) {
  const shift = !!(mods & 1);
  if (code >= 65 && code <= 90) { const l = String.fromCharCode(code); return { key: shift ? l : l.toLowerCase(), code: 'Key' + l, keyCode: code }; }
  if (code >= 48 && code <= 57) { const d = code - 48; return { key: shift ? DIGIT_SHIFT[d] : String(d), code: 'Digit' + d, keyCode: code }; }
  if (code >= 290 && code <= 314) { const n = code - 289; return { key: 'F' + n, code: 'F' + n, keyCode: 111 + n }; }
  if (code >= 320 && code <= 329) { const n = code - 320; return { key: String(n), code: 'Numpad' + n, keyCode: 96 + n, location: 3 }; }
  if (NAMED[code]) { const [key, c, kc] = NAMED[code]; return { key, code: c, keyCode: kc, location: c.endsWith('Right') ? 2 : c.endsWith('Left') ? 1 : c.startsWith('Numpad') ? 3 : 0 }; }
  if (PUNCT[code]) { const [c, kc, k, ks] = PUNCT[code]; return { key: shift && ks ? ks : k, code: c, keyCode: kc }; }
  return { key: 'Unidentified', code: '', keyCode: 0 };
}
