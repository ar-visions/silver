# agi seed: the backbone, author modules and router trained from
# random weights on request / response pairs
#   python3 train.py [steps] [--omit a,b,c]
# loss: the response's next words, plus the router's weights
# against the pair's author shares. Half the steps run the author
# modules on the true authors (each module learns from its own
# books), half on the router's weights (as when writing).
# --omit leaves those authors' pairs out: a model to validate
# the attribution against, saved to model-omit-a-b-c
import os, sys, json, math, time, random
import torch
from tokenizers import Tokenizer
from model import model_new

SEED = os.path.expanduser('~/.cache/agi/seed')
CTX = 512
BATCH = 48
# a batch runs in parts, their gradients added (GPU memory)
PARTS = 2
LR = 1e-3
AUTHOR_WEIGHT = 1.0

tok = Tokenizer.from_file(os.path.join(SEED, 'tokenizer.json'))
sp = {t: tok.token_to_id(t) for t in ['<pad>', '<req>', '<resp>',
                                      '<end>']}
meta = json.load(open(os.path.join(SEED, 'meta.json')))
nauth = len(meta['authors'])


# a pair's tokens: the request (its end kept, at most half the
# window), then the response and end, cut to fit
def encode(p):
    words = tok.encode(p['ask']).ids[-(CTX // 2 - 2):]
    ask = [sp['<req>']] + words + [sp['<resp>']]
    out = tok.encode(p['resp']).ids + [sp['<end>']]
    return ask, out[:CTX - len(ask)]


def load(name):
    return [json.loads(l) for l in open(os.path.join(SEED, name))]


# ids; labels: the response's tokens at the place before each;
# shares: each row's authors [rows, authors]
def batch_of(rows):
    ids = torch.full((len(rows), CTX), sp['<pad>'])
    lab = torch.full((len(rows), CTX), -100)
    share = torch.zeros(len(rows), nauth)
    for i, p in enumerate(rows):
        ask, out = encode(p)
        seq = ask + out
        ids[i, :len(seq)] = torch.tensor(seq)
        lab[i, len(ask) - 1:len(seq) - 1] = torch.tensor(out)
        for a, w in p.get('authors', []):
            share[i, a] = w
    return ids, lab, share


# each row's mean next-word loss over its response: [rows]
def row_losses(logits, lab):
    each = torch.nn.functional.cross_entropy(
        logits.float().transpose(1, 2), lab, ignore_index=-100,
        reduction='none')
    return each.sum(1) / (lab != -100).sum(1).clamp(min=1)


# the router against the true shares, over the response's places
def author_loss(w, lab, share):
    keep = (lab != -100).float()
    lw = torch.log(w.float().clamp(min=1e-9))
    each = -(lw * share[:, None, :]).sum(-1)
    return (each * keep).sum() / keep.sum().clamp(min=1)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    steps = int(args[0]) if args else 12000
    omit = set()
    if '--omit' in sys.argv:
        omit = {int(a) for a in
                sys.argv[sys.argv.index('--omit') + 1].split(',')}
    folder = 'model'
    if omit:
        folder = 'model-omit-' + '-'.join(str(a) for a in sorted(omit))
    dev = 'cuda'
    torch.manual_seed(1)
    train = [p for p in load('train.jsonl')
             if not any(a in omit for a, _ in p['authors'])]
    m = model_new(tok.get_vocab_size(), nauth, sp['<pad>'], CTX).to(dev)
    print('%.1fM parameters (author modules %.1fM), %d pairs, %d '
          'authors%s' % (
              sum(p.numel() for p in m.parameters()) / 1e6,
              sum(p.numel() for p in m.authors.parameters()) / 1e6,
              len(train), nauth,
              ', leaving out %s' % sorted(omit) if omit else ''),
          flush=True)
    opt = torch.optim.AdamW(m.parameters(), lr=LR, betas=(0.9, 0.95),
                            weight_decay=0.1)
    rnd = random.Random(1)
    t0 = time.time()
    for s in range(steps):
        lr = LR * min(1, (s + 1) / 500) * 0.5 * (
            1 + math.cos(math.pi * s / steps))
        for g in opt.param_groups:
            g['lr'] = lr
        rows = rnd.sample(train, BATCH)
        opt.zero_grad(set_to_none=True)
        text = auth = 0.0
        size = BATCH // PARTS
        for at in range(0, BATCH, size):
            ids, lab, share = batch_of(rows[at:at + size])
            ids, lab, share = ids.to(dev), lab.to(dev), share.to(dev)
            m.authors.given = share if s % 2 == 0 else None
            with torch.autocast('cuda', dtype=torch.bfloat16):
                logits, w = m(ids)
                t = row_losses(logits, lab).mean()
                a = author_loss(w, lab, share)
            ((t + AUTHOR_WEIGHT * a) / PARTS).backward()
            text += t.item() / PARTS
            auth += a.item() / PARTS
        torch.nn.utils.clip_grad_norm_(m.parameters(), 1.0)
        opt.step()
        if (s + 1) % 50 == 0:
            print('step %d: text %.3f, authors %.3f, %.0f ms a step' % (
                s + 1, text, auth,
                (time.time() - t0) * 1000 / 50), flush=True)
            t0 = time.time()
        if (s + 1) % 2000 == 0 or s + 1 == steps:
            m.authors.given = None
            m.save(os.path.join(SEED, folder))
            print('saved %s at step %d' % (folder, s + 1), flush=True)
            t0 = time.time()


if __name__ == '__main__':
    main()
