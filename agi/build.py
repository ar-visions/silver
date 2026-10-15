# agi seed: tokenizer and request / response pairs from the
# authored books in ~/.cache/agi; each pair's authors are kept
# beside it (provenance), never trained on
import os, re, struct, random, json, collections
from tokenizers import Tokenizer, models, trainers, pre_tokenizers, decoders

DATA = os.path.expanduser('~/.cache/agi')
OUT = os.path.join(DATA, 'seed')
VOCAB = 8192
os.makedirs(OUT, exist_ok=True)
rnd = random.Random(1)

corpus = open(os.path.join(DATA, 'corpus.u8'), 'rb').read()
raw = open(os.path.join(DATA, 'docs.i64'), 'rb').read()
docs = [struct.unpack_from('<7q', raw, i * 56) for i in range(len(raw) // 56)]
cr = open(os.path.join(DATA, 'credits.i64'), 'rb').read()
credits = struct.unpack('<%dq' % (len(cr) // 8), cr)
names = [l.rstrip('\n').split('\t', 1)[1]
         for l in open(os.path.join(DATA, 'identities.txt'))]


def text_of(d):
    return corpus[d[0]:d[0] + d[1]].decode('utf-8', 'replace')


books = []
for d in docs:
    cs, cn = d[5], d[6]
    who = list(credits[cs:cs + cn])
    books.append({'num': d[3], 'who': who, 'text': text_of(d)})
authors = sorted({a for b in books for a in b['who']})
col = {a: i for i, a in enumerate(authors)}

# names particular to a book: capitalised words used often in it
# and in few other books
word = re.compile(r'(?<![.!?"\'\n] )\b([A-Z][a-z]{2,})\b')
seen = collections.Counter()
per = []
for b in books:
    c = collections.Counter(word.findall(b['text']))
    per.append(c)
    seen.update(set(c))
for b, c in zip(books, per):
    b['names'] = [w for w, n in c.most_common(400)
                  if n >= 6 and seen[w] <= 3][:30]

# the tokenizer: byte level BPE on the books, then the markers
specials = ['<pad>', '<req>', '<resp>', '<end>']
tok = Tokenizer(models.BPE())
tok.pre_tokenizer = pre_tokenizers.ByteLevel(add_prefix_space=False)
tok.decoder = decoders.ByteLevel()
tr = trainers.BpeTrainer(vocab_size=VOCAB, special_tokens=specials,
                         initial_alphabet=pre_tokenizers.ByteLevel.alphabet())
sample = [b['text'][:200000] for b in books]
tok.train_from_iterator(sample, tr)
tok.save(os.path.join(OUT, 'tokenizer.json'))


# a sentence start at or before i
def start_at(t, i):
    dot = max(t.rfind('. ', 0, i), t.rfind('? ', 0, i),
              t.rfind('! ', 0, i))
    return dot + 2 if dot >= 0 and i - dot < 300 else i


def clean(p):
    return re.sub(r'\s+', ' ', p).strip()


# a small transcript from book b: what happened so far, then
# the next few sentences
def scene(b, before, after):
    t = b['text']
    at = start_at(t, rnd.randint(before + 1000, len(t) - after - 1000))
    s = start_at(t, at - before)
    e = at + after
    dot = t.find('. ', e - 120, e + 120)
    if dot >= 0:
        e = dot + 1
    past, nxt = clean(t[s:at]), clean(t[at:e])
    if len(past) < 80 or len(nxt) < 60:
        return None
    seen = [w for w in b['names'] if re.search(r'\b%s\b' % w, past)]
    return past, nxt, seen[:3]


# a request is the story so far and who is in it; the response
# is what happens next; mixed: two books' scenes joined, the
# next part from both
def pair():
    k = rnd.choices([1, 2], [2, 1])[0]
    picks = rnd.sample(range(len(books)), k)
    if len({tuple(books[i]['who']) for i in picks}) < k:
        return None
    pasts, nexts, called = [], [], []
    for i in picks:
        got = scene(books[i], 500 // k, 360 // k)
        if not got:
            return None
        pasts.append(got[0])
        nexts.append((i, got[1]))
        called += got[2]
    ask = 'So far: ' + ' '.join(pasts)
    if called:
        ask += '\nCharacters: ' + ', '.join(called)
    ask += '\nWhat happens next?'
    resp = ' '.join(n for _, n in nexts)
    total = sum(len(n) for _, n in nexts)
    share = collections.Counter()
    for i, n in nexts:
        who = books[i]['who']
        for a in who:
            share[col[a]] += len(n) / total / len(who)
    credit = sorted(share.items(), key=lambda x: -x[1])
    return {'ask': ask, 'resp': resp,
            'authors': [[a, round(w, 4)] for a, w in credit],
            'books': [books[i]['num'] for i, _ in nexts]}


def make(n, path):
    with open(path, 'w') as f:
        made = 0
        while made < n:
            p = pair()
            if p:
                f.write(json.dumps(p) + '\n')
                made += 1


# training pairs (line n is training example n), and requests
# to try
make(300000, os.path.join(OUT, 'train.jsonl'))
make(2000, os.path.join(OUT, 'requests.jsonl'))
json.dump({'authors': [[a, names[a]] for a in authors],
           'vocab': tok.get_vocab_size()},
          open(os.path.join(OUT, 'meta.json'), 'w'))
print(len(books), 'books,', len(authors),
      'authors, vocab', tok.get_vocab_size())
