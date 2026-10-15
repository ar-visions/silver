# agi seed: which authors contributed to a response
#   python3 attribute.py ask "the story so far" [characters]
#   python3 attribute.py try [n]     (n requests from requests.jsonl)
#   python3 attribute.py validate model-omit-a-b [n]
# the response is written, then held fixed and run again with
# author modules switched off; the drop in its log probability
# is a module's contribution, shared out by sampled Shapley
# values over the authors the router weighted most
import os, sys, json, random
import numpy as np
import torch
from train import SEED, CTX, tok, sp, nauth, meta, load, batch_of
from model import model_open

dev = 'cuda' if torch.cuda.is_available() else 'cpu'
names = [n for _, n in meta['authors']]
CANDIDATES = 12
ORDERS = 24
BATCH = 48


def open_at(folder):
    return model_open(os.path.join(SEED, folder), nauth).to(dev).eval()


def request(so_far, characters=''):
    ask = 'So far: ' + so_far
    if characters:
        ask += '\nCharacters: ' + characters
    return ask + '\nWhat happens next?'


@torch.no_grad()
def write(m, ask, heat=0.8, most=200):
    m.authors.given = m.authors.off = None
    words = tok.encode(ask).ids[-(CTX // 2 - 2):]
    x = torch.tensor([[sp['<req>']] + words + [sp['<resp>']]],
                     device=dev)
    out = []
    for _ in range(most):
        nxt = m(x)[0][0, -1].float()
        t = int(torch.multinomial(torch.softmax(nxt / heat, -1), 1))
        if t == sp['<end>']:
            break
        out.append(t)
        x = torch.cat([x, torch.tensor([[t]], device=dev)], 1)
    return tok.decode(out)


# the response's log probability under each mask of modules on
# (1) or off (0): [masks]; and the router's mean weights over the
# response, with every module on
@torch.no_grad()
def logps(m, ask, resp, masks):
    ids, lab, _ = batch_of([{'ask': ask, 'resp': resp}])
    ids, lab = ids.to(dev), lab.to(dev)
    out = []
    route = None
    for at in range(0, len(masks), BATCH):
        part = masks[at:at + BATCH].to(dev)
        n = part.size(0)
        m.authors.given = None
        m.authors.off = part
        logits, w = m(ids.expand(n, -1))
        lg = torch.log_softmax(logits.float(), -1)
        keep = lab[0] != -100
        tgt = lab[0][keep]
        got = lg[:, keep, :].gather(
            2, tgt[None, :, None].expand(n, -1, 1))[..., 0]
        out.append(got.sum(1))
        if route is None:
            route = w[0][keep].float().mean(0)
    m.authors.off = None
    return torch.cat(out), route


def contributions(m, ask, resp):
    full, route = logps(m, ask, resp, torch.ones(1, nauth))
    cands = torch.argsort(route, descending=True)[:CANDIDATES].tolist()
    # Shapley: the candidates switched on in random orders, the
    # rest of the modules left on throughout
    rnd = random.Random(1)
    masks, steps = [], []
    for _ in range(ORDERS):
        order = cands[:]
        rnd.shuffle(order)
        mask = torch.ones(nauth)
        mask[cands] = 0
        masks.append(mask.clone())
        for a in order:
            mask[a] = 1
            masks.append(mask.clone())
            steps.append(a)
    none = torch.zeros(1, nauth)
    vals, _ = logps(m, ask, resp, torch.stack(masks + [none[0]]))
    alone = vals[-1].item()
    # each order: all candidates off, then one on at a time; an
    # author's gain is the change its switching on made
    phi = {a: 0.0 for a in cands}
    for o in range(ORDERS):
        base = o * (CANDIDATES + 1)
        for k in range(CANDIDATES):
            a = steps[o * CANDIDATES + k]
            phi[a] += (vals[base + k + 1] - vals[base + k]).item()
    for a in phi:
        phi[a] /= ORDERS
    return full.item(), alone, route, phi


def show(m, ask, resp):
    full, alone, route, phi = contributions(m, ask, resp)
    print(resp.strip())
    pos = sum(v for v in phi.values() if v > 0)
    print('-- share of measured positive contribution (log '
          'probability gained by each author module):')
    for a, v in sorted(phi.items(), key=lambda x: -x[1]):
        if v <= 0:
            continue
        print('   %5.1f%%  %s  (%+.3f)' % (100 * v / pos, names[a], v))
    neg = [(a, v) for a, v in phi.items() if v < 0]
    if neg:
        neg.sort(key=lambda x: x[1])
        print('-- against:', ', '.join(
            '%s (%+.3f)' % (names[a], v) for a, v in neg))
    print('-- the response\'s log probability: %.2f with every module, '
          '%.2f with the shared backbone alone' % (full, alone))
    print('-- the router\'s author output:', ', '.join(
        '%s %.0f%%' % (names[a], 100 * route[a].item())
        for a in torch.argsort(route, descending=True)[:5].tolist()))


# leave-out check: the drop from switching the omitted authors'
# modules off should match the drop under the model trained
# without them
def validate(m, folder, n):
    omit = [int(a) for a in folder.split('model-omit-')[1].split('-')]
    other = open_at(folder)
    off = torch.ones(1, nauth)
    off[0, omit] = 0
    said, lost = [], []
    for p in load('requests.jsonl')[:n]:
        resp = write(m, p['ask'])
        if not resp.strip():
            continue
        both, _ = logps(m, p['ask'], resp,
                        torch.cat([torch.ones(1, nauth), off]))
        theirs, _ = logps(other, p['ask'], resp, torch.ones(1, nauth))
        said.append((both[0] - both[1]).item())
        lost.append((both[0] - theirs[0]).item())
    said, lost = np.array(said), np.array(lost)
    print('%d responses, left out %s' % (
        len(said), ', '.join(names[o] for o in omit)))
    print('correlation of the predicted drop (their modules off) with '
          'the drop under the model trained without them: %.3f'
          % np.corrcoef(said, lost)[0, 1])
    hi = said > np.percentile(said, 80)
    print('log probability lost without them: responses credited '
          'to them most %.3f, the rest %.3f' % (
              lost[hi].mean(), lost[~hi].mean()))


if __name__ == '__main__':
    word = sys.argv[1] if len(sys.argv) > 1 else 'try'
    m = open_at('model')
    if word == 'ask':
        ask = request(sys.argv[2], sys.argv[3] if len(sys.argv) > 3
                      else '')
        show(m, ask, write(m, ask))
    elif word == 'validate':
        validate(m, sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3
                 else 200)
    else:
        n = int(sys.argv[2]) if len(sys.argv) > 2 else 3
        for p in load('requests.jsonl')[:n]:
            print('== request:', p['ask'][:200].replace('\n', ' '))
            show(m, p['ask'], write(m, p['ask']))
            print('   (the request was cut from: ' + ', '.join(
                names[a] for a, _ in p['authors']) + ')\n')
