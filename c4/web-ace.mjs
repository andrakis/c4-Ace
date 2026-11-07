// web-ace.mjs - The Ace of Penguins on C4IX in the browser, with a real mouse:
// a game takes the display from the desktop, a click turns a card, a drag
// carries one and a bad drop puts it back, h shows the help, q gives the
// display back to the desktop. Then -window, in the Terminal it was started
// from. Over CDP, in a tab of its own in one of the user's Chromes (c4's
// libjs/tests/cdp.mjs), against a page served from c4's tree:
//
//   node libjs/serve.mjs PORT --isolate &
//   node games/ace/c4/web-ace.mjs http://127.0.0.1:9224 https://PORT.code.home.stargazer.onl OUTDIR

import { writeFileSync, mkdirSync } from 'node:fs';
import { Browser } from '../../../libjs/tests/cdp.mjs';

const [cdp = 'http://127.0.0.1:9224', base = 'http://127.0.0.1:8472', outdir = '/tmp/web-ace'] = process.argv.slice(2);
mkdirSync(outdir, { recursive: true });
let fail = 0;
const ok = (what, cond, extra = '') => {
  console.log(`web-ace: ${what} ${cond ? 'OK' : 'FAILED'}${!cond && extra ? '\n  ' + String(extra).slice(-200) : ''}`);
  if (!cond) fail = 1;
  return cond;
};
const sleep = ms => new Promise(r => setTimeout(r, ms));

const b = await Browser.connect(cdp);
console.log(`web-ace: ${b.version}`);
const tab = await b.newTab(false);
try {
  const shown = 'window.c4m.display.shownText';
  const shownHas = (s, t = 20000) => tab.waitFor(`${shown}.some(t => ${s instanceof RegExp ? `${s}.test(t.s)` : `t.s.trimEnd() === ${JSON.stringify(s)}`})`, t);
  const find = async s => tab.eval(`${shown}.filter(t => ${s instanceof RegExp ? `${s}.test(t.s)` : `t.s.trimEnd() === ${JSON.stringify(s)}`})`);
  const at = async (x, y) => {
    const w = await tab.eval("document.getElementById('display').width"), h = await tab.eval("document.getElementById('display').height");
    return tab.eval(`(() => { const r = document.getElementById('display').getBoundingClientRect();
      return { x: r.left + r.width * ${x / w}, y: r.top + r.height * ${y / h} }; })()`);
  };
  const clickAt = async (x, y) => {
    const p = await at(x, y);
    await tab.mouse('mouseMoved', p.x, p.y);
    await tab.mouse('mousePressed', p.x, p.y, 'left', 1);
    await tab.mouse('mouseReleased', p.x, p.y, 'left', 0);
    await sleep(900);    // not a double click, if another comes here
  };
  // a pixel of the canvas: [r, g, b]
  const pixel = (x, y) => tab.eval(`(() => { const c = document.getElementById('display');
    return [...c.getContext('2d').getImageData(${x}, ${y}, 1, 1).data].slice(0, 3); })()`);
  const isWhite = p => p[0] > 240 && p[1] > 240 && p[2] > 240;
  const isTable = p => p[0] === 0 && p[1] === 102 && p[2] === 0;
  const pixelIs = async (x, y, test, t = 10000) => {
    const end = Date.now() + t;
    let p;
    while (Date.now() < end) { p = await pixel(x, y); if (test(p)) return true; await sleep(100); }
    return false;
  };
  const size = () => tab.eval("(() => { const c = document.getElementById('display'); return [c.width, c.height]; })()");
  const shot = async name => { writeFileSync(`${outdir}/${name}.png`, await tab.screenshot()); };
  const key = async (k, code) => {
    const vk = k === 'Escape' ? 27 : k.toUpperCase().charCodeAt(0);
    await tab.send('Input.dispatchKeyEvent', { type: k.length === 1 ? 'keyDown' : 'rawKeyDown', key: k, code, windowsVirtualKeyCode: vk, text: k.length === 1 ? k : undefined });
    await sleep(60);
    await tab.send('Input.dispatchKeyEvent', { type: 'keyUp', key: k, code, windowsVirtualKeyCode: vk });
    await sleep(200);
  };

  await tab.goto(`${base}/libjs/web/?persist=reset`);
  const up = await tab.waitFor('window.c4m && window.c4m.display', 90000);
  ok('the page is the page, not a sign-in', up, await tab.eval('location.href'));
  await tab.eval('window.c4m.display.recordText = true');
  ok('the C4IX desktop boots, a Terminal open', up && await shownHas(/^Terminal - task/, 180000));
  await sleep(1500);
  const [W0, H0] = await size();
  let termX, termY;
  {
    const t = (await find(/^Terminal - task/)).filter(t => t.y < H0 - 30).pop();
    termX = t.x + 5;
    termY = t.y + 5;
    await clickAt(termX, termY);
  }
  await tab.type('solitaire -seed 1\n');

  // M=6, W=73, H=97: the deck at (6,6), the discards beside it, the first
  // column's top card at (6,109)
  ok('solitaire takes the display, at its own size', await tab.waitFor(`document.getElementById('display').width === 559`, 60000), await size());
  ok('the table is drawn', await pixelIs(300, 480, isTable, 20000), await pixel(300, 480));
  await shot('1-splash');
  await sleep(500);
  await clickAt(300, 480);          // the splash goes
  await shot('2-table');
  // the discards: upstream's auto-play has already turned one by now, after
  // the splash's click. A checksum of what is there tells one from the next.
  const region = (x, y, w, h) => tab.eval(`(() => { const c = document.getElementById('display');
    const d = c.getContext('2d').getImageData(${x}, ${y}, ${w}, ${h}).data;
    let s = 0; for (let i = 0; i < d.length; i++) s = (s * 31 + d[i]) | 0; return s; })()`);
  const changes = async (x, y, w, h, was, t = 10000) => {
    const end = Date.now() + t;
    while (Date.now() < end) { if (await region(x, y, w, h) !== was) return true; await sleep(100); }
    return false;
  };
  const was = await region(85, 6, 73, 97);
  await clickAt(42, 54);            // the deck
  ok('a click on the deck turns a card', await changes(85, 6, 73, 97, was), was);
  await shot('3-deck-clicked');

  // a card carried over the empty table, which it cannot be dropped on: the
  // no-drop picture follows the mouse, and the card stays where it was
  {
    const p = await at(42, 150), q = await at(300, 450);
    ok('the table is empty where the card is going', isTable(await pixel(300, 450)), await pixel(300, 450));
    await tab.mouse('mouseMoved', p.x, p.y);
    await tab.mouse('mousePressed', p.x, p.y, 'left', 1);
    for (let k = 1; k <= 6; k++) { await tab.mouse('mouseMoved', p.x + (q.x - p.x) * k / 6, p.y + (q.y - p.y) * k / 6, 'left', 1); await sleep(60); }
    ok('a drag shows under the mouse', await pixelIs(300, 450, c => !isTable(c)), await pixel(300, 450));
    await shot('4-dragging');
    await tab.mouse('mouseReleased', q.x, q.y, 'left', 0);
    ok('a bad drop leaves nothing there', await pixelIs(300, 450, isTable), await pixel(300, 450));
    ok('and the card where it came from', await pixelIs(42, 150, isWhite), await pixel(42, 150));
    await sleep(900);
    await shot('5-dropped');
  }
  // seed 1 deals the two of spades on column 3 and the three of hearts on
  // column 4: a legal move, which fans it below the three
  {
    ok('below the three of hearts, the table', isTable(await pixel(279, 258)), await pixel(279, 258));
    const p = await at(200, 190), q = await at(279, 190);
    await tab.mouse('mouseMoved', p.x, p.y);
    await tab.mouse('mousePressed', p.x, p.y, 'left', 1);
    for (let k = 1; k <= 6; k++) { await tab.mouse('mouseMoved', p.x + (q.x - p.x) * k / 6, p.y + (q.y - p.y) * k / 6, 'left', 1); await sleep(60); }
    await tab.mouse('mouseReleased', q.x, q.y, 'left', 0);
    ok('the two of spades goes on the three of hearts', await pixelIs(279, 258, isWhite), await pixel(279, 258));
    await sleep(900);
    await shot('5b-moved');
  }

  await key('h', 'KeyH');
  await sleep(800);
  await shot('6-help');
  await key('Escape', 'Escape');
  await sleep(800);
  await key('q', 'KeyQ');
  ok('q gives the display back', await tab.waitFor(`document.getElementById('display').width === ${W0}`, 30000), await size());
  await tab.eval('window.c4m.display.shownText = []');
  ok('the desktop draws itself again', await shownHas(/^Terminal - task/, 30000));
  await sleep(1500);
  await shot('7-desktop-again');

  // in the window it was started from
  {
    const t = (await find(/^Terminal - task/)).filter(t => t.y < H0 - 30).pop();
    await clickAt(t.x + 5, t.y + 5);
    await tab.type('solitaire -seed 1 -window\n');
    await sleep(6000);
    await shot('8-window');
    ok('-window leaves the display to the desktop', (await size())[0] === W0, await size());
    // the window's canvas starts at about (105, 65) of the desktop, and the
    // 559-wide table is in its middle: the deck is near (55, 55) of that,
    // its discards to the right
    const wx = 105 + 40;
    await clickAt(wx + 300, 65 + 370);    // the splash goes
    await sleep(1000);
    const was2 = await region(wx + 90, 65 + 15, 60, 80);
    await clickAt(wx + 45, 65 + 55);
    ok('a click in the window turns a card', await changes(wx + 90, 65 + 15, 60, 80, was2), was2);
    await shot('8b-window-clicked');
    await key('q', 'KeyQ');
    await sleep(1500);
    await shot('9-window-closed');
  }

  // every other game: it takes the display, a click takes the splash off,
  // and q gives the display back
  for (const g of ['canfield', 'freecell', 'golf', 'mastermind', 'merlin', 'minesweeper', 'pegged', 'spider', 'taipei', 'thornq']) {
    // the Terminal's title, where it was: nothing redraws it to ask
    await sleep(500);
    await clickAt(termX, termY);
    await tab.type(`${g} -seed 1\n`);
    const took = await tab.waitFor(`document.getElementById('display').width !== ${W0}`, 60000);
    await sleep(2500);
    await shot(`g-${g}-splash`);
    const [w, h] = await size();
    await clickAt(w >> 1, h >> 1);
    await sleep(800);
    await shot(`g-${g}`);
    await key('q', 'KeyQ');
    const back = await tab.waitFor(`document.getElementById('display').width === ${W0}`, 30000);
    ok(`${g} takes the display, ${w}x${h}, and gives it back`, took && back, await size());
    await sleep(1000);
  }
  ok('no errors on the page', !tab.errors.length, tab.errors.join('\n'));

  // under grin, C4DOS's graphical shell, in a window of grin's
  {
    await tab.goto(`${base}/libjs/web/?system=c4dos&persist=reset`);
    const up2 = await tab.waitFor('window.c4m && document.getElementById("terminal").innerText.includes("Starting grin")', 90000);
    await tab.eval('window.c4m.display.recordText = true');
    await sleep(500);
    await clickAt(620, 300);
    ok('the page boots into grin', up2 && await shownHas('Program Manager', 60000));
    const clickT = async (t, dbl) => {
      await clickAt0(t.x + 5, t.y + 5);
      if (dbl) await clickAt0(t.x + 5, t.y + 5);
      await sleep(400);
    };
    const clickAt0 = async (x, y) => {
      const p = await at(x, y);
      await tab.mouse('mouseMoved', p.x, p.y);
      await tab.mouse('mousePressed', p.x, p.y, 'left', 1);
      await tab.mouse('mouseReleased', p.x, p.y, 'left', 0);
      await sleep(60);
    };
    await clickT((await find('Program Manager')).pop());
    await clickT((await find('Main')).sort((a, c) => a.x - c.x)[0]);
    const all = await find('DOS Prompt');
    await clickT(all.filter(t => t.x > 140)[0] || all[all.length - 1], true);
    ok('a DOS Prompt in grin', await shownHas('DOS Prompt 1', 20000));
    await tab.type('CD \\GAMES\\ACE\n');
    await sleep(800);
    await shot('grin-1-prompt');
    await tab.type('solitaire -seed 1\n');
    ok('solitaire opens a window of grin\'s', await shownHas('solitaire', 30000));
    await sleep(3000);
    await shot('grin-2-window');
    const title = (await find('solitaire')).pop();
    console.log(`web-ace: grin's title at ${title.x},${title.y}, display ${await size()}`);
    // the title is centred over the frame, which is the table's size and a
    // menu's height below it: the deck is near (55, 55) of it, the discards
    // to the right
    const x0 = title.x + 27 - (559 >> 1), y0 = title.y + 54;
    await clickAt(x0 + 300, y0 + 370);      // the splash goes
    await sleep(1500);
    await shot('grin-3-table');
    const was3 = await region(x0 + 100, y0 + 15, 70, 80);
    await clickAt(x0 + 55, y0 + 55);
    ok('a click in grin\'s window turns a card', await changes(x0 + 100, y0 + 15, 70, 80, was3), was3);
    await shot('grin-4-clicked');
    await key('q', 'KeyQ');
    await sleep(2000);
    await shot('grin-5-closed');
    ok('q closes it', await tab.waitFor(`!${shown}.some(t => t.s.trimEnd() === 'solitaire')`, 15000));
    ok('no errors on the page under grin', !tab.errors.length, tab.errors.join('\n'));
  }
} finally {
  await tab.close();
  b.disconnect();
}
console.log(`web-ace: ${fail ? 'FAILED' : 'all OK'}`);
process.exit(fail);
