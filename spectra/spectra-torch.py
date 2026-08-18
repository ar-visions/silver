#!/usr/bin/env python3
# spectra reader — ONE natural pass over the whole spectral stream,
# no crop-and-refeed cascade. a conv net over (1,64,T) keeps full time
# resolution and emits two per-column heads:
#   sect   — fit-to-clip tracking state (speech section boundaries)
#   tokens — CTC WORD-TOKEN logits: the net outputs a vector of word
#            tokens; words detect WITH their column spans in one pass
# the tokenizer is a COMMON HASH the app reproduces exactly: lowercase
# word -> FNV-1a 32 -> % vocab + 1 (0 = CTC blank). token->word text
# for display saves beside the model (read1-vocab.txt)
# inputs are cv_translate's gray pngs: 64 rows 150Hz..8kHz, one column
# per live-stream window (the app's own cadence)
import argparse, os, hashlib
import numpy as np
import torch
import torch.nn as nn


def parse():
    p = argparse.ArgumentParser()
    p.add_argument('--dataset', default='/src/datasets/cv22-spect')
    p.add_argument('--splits',  default='dev')   # comma list pools splits
    p.add_argument('--epochs',  type=int, default=30)
    p.add_argument('--batch',   type=int, default=16)
    p.add_argument('--lr',      type=float, default=3e-4)
    p.add_argument('--wd',      type=float, default=1e-4)
    # the limited set: cap how many clips train at all
    p.add_argument('--limit',   type=int, default=0)
    p.add_argument('--max_t',   type=int, default=512)
    p.add_argument('--vocab',   type=int, default=4096)
    # trunk width: 192 plateaued at ~0.36 eval on 1000 words — the
    # train loss floor said capacity-bound, not lr-bound
    p.add_argument('--width',   type=int, default=384)
    # the LIMITED SET: only the top-N frequent words get their own
    # token; everything else is one <other> class — detected and
    # bounded, just not named. rare words are unlearnable anyway
    p.add_argument('--top_words', type=int, default=1000)
    # sect labels derive from the input itself: column energy through
    # the app's own envelope (fast attack, slow release), thresholded
    p.add_argument('--sect_thr', type=float, default=0.30)
    p.add_argument('--sect_w',   type=float, default=0.3)
    p.add_argument('--align_w',  type=float, default=1.0)
    p.add_argument('--seed',    type=int, default=1)
    p.add_argument('--stats',   action='store_true')
    p.add_argument('--export_ts', action='store_true')
    return p.parse_args()


args = parse()
OTHER = args.vocab + 1                     # rare word: detected, unnamed
NCLS = args.vocab + 2                      # 0 = CTC blank
tok2word = {OTHER: '?'}                    # display text per token


def fnv1a(word):
    h = 2166136261
    for c in word.encode():
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h


def words_of(text):
    keep = "abcdefghijklmnopqrstuvwxyz'"
    out = []
    for w in text.lower().split():
        w = ''.join(c for c in w if c in keep)
        if w:
            out.append(w)
    return out


def encode(words, top):
    toks = []
    for w in words:
        if w in top:
            t = fnv1a(w) % args.vocab + 1
            if t not in tok2word:
                tok2word[t] = w
        else:
            t = OTHER
        toks.append(t)
    return toks


def decode_greedy(ids, min_run=3):
    # a word spans >= min_run columns (~130ms); shorter runs are
    # column-level toggle noise, not words
    out, prev, run = [], 0, 0
    for i in list(ids) + [0]:
        if i == prev:
            run += 1
            continue
        if prev != 0 and run >= min_run:
            out.append(tok2word.get(prev, f'#{prev}'))
        prev, run = i, 1
    return ' '.join(out)


def read_field(text, key):
    for ln in text.splitlines():
        if ln.startswith(key + ':'):
            return ln[len(key) + 1:].strip()
    return ''


def sect_labels(g):
    # g: (64,T) 0..1 — column mean energy through the envelope
    e = g.mean(0)
    ema, out = 0.0, np.zeros(g.shape[1], np.float32)
    for i in range(g.shape[1]):
        a = 0.4 if e[i] > ema else 0.12
        ema = ema * (1 - a) + e[i] * a
        out[i] = 1.0 if ema > args.sect_thr else 0.0
    return out


def load_split(d, split):
    from PIL import Image
    rows, i = [], 0
    while True:
        ap = os.path.join(d, f'{split}_{i}.agi')
        pp = os.path.join(d, f'{split}_{i}.png')
        if not (os.path.exists(ap) and os.path.exists(pp)):
            break
        text = open(ap).read()
        sent = read_field(text, 'sentence')
        g = np.asarray(Image.open(pp), np.float32) / 255.0   # 64,T
        words = words_of(sent)
        i += 1
        if not sent or g.shape[0] != 64:
            continue
        if g.shape[1] > args.max_t or len(words) < 1 or len(words) >= g.shape[1]:
            continue
        nums = [int(v) for v in read_field(text, 'sections').split()
                if v.lstrip('-').isdigit()]
        spans = list(zip(nums[0::2], nums[1::2]))
        wnum = [int(v) for v in read_field(text, 'wspans').split()
                if v.lstrip('-').isdigit()]
        wsp = list(zip(wnum[0::2], wnum[1::2]))
        mks = [int(v) for v in read_field(text, 'marks').split()
               if v.lstrip('-').isdigit()]
        if spans:
            sc = np.zeros(g.shape[1], np.float32)
            for x0, x1 in spans:
                sc[x0:x1] = 1.0
        else:
            sc = sect_labels(g)
        rows.append((g, words, sc, f'{split}_{i-1}', wsp, mks, spans))
        if args.limit and len(rows) >= args.limit:
            break
    return rows


def finalize_rows(raw):
    # two passes: word frequencies first, then tokens — only the top-N
    # words get named tokens, the rest collapse to <other>
    from collections import Counter
    freq = Counter(w for r in raw for w in r[1])
    top = set(w for w, _ in freq.most_common(args.top_words))
    print(f'vocab: {len(freq)} unique words, top {len(top)} named, '
          f'{sum(freq[w] for w in top)}/{sum(freq.values())} tokens covered')
    rows = []
    for g, words, sc, fid, wsp, mks, spans in raw:
        lab = np.array(encode(words, top), np.int64)
        # alignment sources, strongest first: wspans (LibriSpeech MFA
        # word timings — exact start/end per word), human click-marks
        # (annotate mode: n-1 boundaries), then energy sections when
        # their count matches; neither -> CTC discovers it alone
        ali = None
        if len(wsp) == len(lab):
            ali = np.zeros(g.shape[1], np.int64)
            for (x0, x1), t in zip(wsp, lab):
                ali[x0:x1] = t
        elif len(mks) == len(lab) - 1:
            edges = [0] + mks + [g.shape[1]]
            ali = np.zeros(g.shape[1], np.int64)
            for k in range(len(lab)):
                ali[edges[k]:edges[k + 1]] = lab[k]
            if spans:
                ali[:spans[0][0]] = 0
                ali[spans[-1][1]:] = 0
        elif spans and len(spans) == len(lab):
            ali = np.zeros(g.shape[1], np.int64)
            for (x0, x1), t in zip(spans, lab):
                ali[x0:x1] = t
        rows.append((g, lab, sc, fid, ali))
    return rows


class ReadNet(nn.Module):
    # freq collapses 64 -> 4 through strided convs; TIME KEEPS STRIDE 1
    # so every output column maps 1:1 to a stream column (spans are
    # native, nothing needs re-feeding)
    def __init__(self):
        super().__init__()
        W = args.width
        E = W // 6
        self.enc = nn.Sequential(
            nn.Conv2d(1, E, 3, stride=(2, 1), padding=1),
            nn.BatchNorm2d(E), nn.ReLU(),
            nn.Conv2d(E, E * 2, 3, stride=(2, 1), padding=1),
            nn.BatchNorm2d(E * 2), nn.ReLU(),
            nn.Conv2d(E * 2, E * 3, 3, stride=(2, 1), padding=1),
            nn.BatchNorm2d(E * 3), nn.ReLU(),
            nn.Conv2d(E * 3, E * 4, 3, stride=(2, 1), padding=1),
            nn.BatchNorm2d(E * 4), nn.ReLU())
        # dilation widens each column's view to ~±20 columns (~0.9s
        # either side) — a whole word plus its neighbors — while time
        # stays stride 1 so spans keep their 1:1 column mapping
        self.tconv = nn.Sequential(
            nn.Conv1d(E * 4 * 4, W, 5, padding=2),
            nn.BatchNorm1d(W), nn.ReLU(),
            nn.Conv1d(W, W, 5, padding=4, dilation=2),
            nn.BatchNorm1d(W), nn.ReLU(),
            nn.Conv1d(W, W, 5, padding=8, dilation=4),
            nn.BatchNorm1d(W), nn.ReLU())
        self.tokens = nn.Conv1d(W, NCLS, 1)
        self.sect = nn.Conv1d(W, 1, 1)

    def forward(self, x):
        # flatten, not reshape-from-unpacked-shape: tracing bakes
        # unpacked shape ints as constants and the export breaks on
        # any other input length
        z = self.enc(x).flatten(1, 2)
        z = self.tconv(z)
        return self.tokens(z), self.sect(z)


def batchify(rows, idxs):
    T = max(rows[i][0].shape[1] for i in idxs)
    x = np.zeros((len(idxs), 1, 64, T), np.float32)
    s = np.zeros((len(idxs), T), np.float32)
    m = np.zeros((len(idxs), T), np.float32)
    # -100 = no alignment here (CE ignore_index): unaligned clips and
    # padding train the token head through CTC only
    al = np.full((len(idxs), T), -100, np.int64)
    labs, in_len, lab_len = [], [], []
    for k, i in enumerate(idxs):
        g, lab, sc, _, ali = rows[i]
        t = g.shape[1]
        x[k, 0, :, :t] = g
        s[k, :t] = sc
        m[k, :t] = 1.0
        if ali is not None:
            al[k, :t] = ali
        labs.append(lab)
        in_len.append(t)
        lab_len.append(len(lab))
    y = np.concatenate(labs)
    return (torch.from_numpy(x), torch.from_numpy(s), torch.from_numpy(m),
            torch.from_numpy(al), torch.from_numpy(y),
            torch.tensor(in_len), torch.tensor(lab_len))


def cer(a, b):
    # edit distance / reference length
    if not b:
        return 1.0
    dp = list(range(len(b) + 1))
    for i, ca in enumerate(a):
        ndp = [i + 1]
        for j, cb in enumerate(b):
            ndp.append(min(dp[j] + (ca != cb), dp[j + 1] + 1, ndp[-1] + 1))
        dp = ndp
    return dp[-1] / len(b)


def export_ts():
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')
    m = ReadNet()
    m.load_state_dict(torch.load(os.path.join(here, 'read1-torch.pt'),
                                 map_location='cpu'))
    m.eval()
    ts = torch.jit.trace(m, torch.zeros(1, 1, 64, 256))
    ts.save(os.path.join(here, 'read1.ptc'))
    print('export_ts: read1.ptc')


def main():
    if args.export_ts:
        export_ts()
        return
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)
    rows = []
    for s in args.splits.split(','):
        rows += load_split(args.dataset, s.strip())
    if not rows:
        raise SystemExit(f'no clips under {args.dataset} for {args.splits}')
    rows = finalize_rows(rows)
    if args.stats:
        ts = [r[0].shape[1] for r in rows]
        ls = [len(r[1]) for r in rows]
        print(f'{len(rows)} clips  T {min(ts)}..{max(ts)} mean {np.mean(ts):.0f}'
              f'  label {min(ls)}..{max(ls)}')
        return
    evm = np.array([int(hashlib.md5(f'{args.seed}_{r[3]}'.encode()).hexdigest(), 16)
                    % 10 == 0 for r in rows])
    ev = np.where(evm)[0]
    tr = np.where(~evm)[0]
    print(f'read: {len(tr)} train / {len(ev)} eval clips')
    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    nali = sum(1 for r in rows if r[4] is not None)
    print(f'boundaries: {nali}/{len(rows)} clips section-aligned (full supervision)')
    m = ReadNet().to(dev)
    opt = torch.optim.AdamW(m.parameters(), lr=args.lr, weight_decay=args.wd)
    ctc = nn.CTCLoss(blank=0, zero_infinity=True)
    bce = nn.BCEWithLogitsLoss(reduction='none')
    cel = nn.CrossEntropyLoss(ignore_index=-100)
    best, out = None, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')
    os.makedirs(out, exist_ok=True)
    print('epoch      train        eval.cer     eval.sect    sample')
    for epoch in range(args.epochs):
        m.train()
        # length-bucketed batches: neighbors by T pad almost nothing
        # (mixed batches wasted ~a third of the compute); the batch
        # ORDER shuffles per epoch so training stays stochastic
        bysize = sorted(tr, key=lambda i: rows[i][0].shape[1])
        batches = [bysize[b:b + args.batch]
                   for b in range(0, len(bysize), args.batch)]
        np.random.shuffle(batches)
        tot, nb = 0.0, 0
        for idxs in batches:
            x, s, mask, al, y, il, ll = batchify(rows, idxs)
            x, s, mask, al = x.to(dev), s.to(dev), mask.to(dev), al.to(dev)
            ch, se = m(x)
            sl = (bce(se[:, 0], s) * mask).sum() / mask.sum()
            loss = args.sect_w * sl
            # aligned rows get exact per-column supervision; CTC —
            # whose early pull is blank-collapse — applies ONLY to
            # rows with no alignment at all
            has_ali = (al != -100).any(1)
            if bool(has_ali.any()):
                loss = loss + args.align_w * cel(ch, al)
            un = ~has_ali
            if bool(un.any()):
                lp = torch.log_softmax(ch[un], 1).permute(2, 0, 1)
                ys = torch.split(y, ll.tolist())
                yu = torch.cat([ys[k] for k in range(len(ys)) if bool(un[k])])
                loss = loss + ctc(lp, yu.to(dev), il[un].to(dev), ll[un].to(dev))
            opt.zero_grad()
            loss.backward()
            opt.step()
            tot += float(loss.detach())
            nb += 1
        m.eval()
        cers, sacc, sample = [], [], ''
        with torch.no_grad():
            for i in ev:
                g, lab, sc, _, _ali = rows[i]
                x = torch.from_numpy(g[None, None]).to(dev)
                ch, se = m(x)
                ids = ch[0].argmax(0).cpu().numpy()
                hyp = decode_greedy(ids)
                ref = ' '.join(tok2word.get(int(t), f'#{t}') for t in lab)
                cers.append(cer(hyp, ref))
                pr = (torch.sigmoid(se[0, 0]).cpu().numpy() > 0.5)
                sacc.append(float((pr == (sc > 0.5)).mean()))
                if not sample:
                    sample = hyp[:40]
        print(f'{epoch + 1:>2}/{args.epochs:<7}{tot / max(1, nb):<13.6g}'
              f'{np.mean(cers):<13.6g}{np.mean(sacc):<13.6g}{sample}')
        if best is None or np.mean(cers) < best:
            best = np.mean(cers)
            torch.save(m.state_dict(), os.path.join(out, 'read1-torch.pt'))
            with open(os.path.join(out, 'read1-vocab.txt'), 'w') as f:
                for t in sorted(tok2word):
                    f.write(f'{t} {tok2word[t]}\n')
    print(f'best eval cer {best:.4g} -> models/read1-torch.pt')


if __name__ == '__main__':
    main()
