#!/usr/bin/env python3
# polaris stage 3 — train the embedder ("sync a local model to our system").
#
# takes a SMALL base sentence-encoder and fine-tunes it on OUR corpus so the
# vector space is organized by what our code means, then freezes it + emits the
# region index polaris loads. contrastive fine-tune: pull each synthetic
# dictation query toward the annotation of the region it refers to; everything
# else in the batch is a negative (MultipleNegativesRankingLoss / in-batch).
#
# input  : annotated.jsonl  (stage 2 output) — one region per line:
#   { "source","remote","path","name","start","end",
#     "note":    "<name-stripped description of what the region does>",
#     "queries": ["where do we ...", "the part that ...", ...] }   # synthetic asks
# output : <model_out>/                      the fine-tuned encoder (local, frozen)
#          index.jsonl                        one region per line + its note vector
#
# v1 is annotation-only: we embed `note`, search by dictation. no var-name
# binding, no symbol sidecar. the model never sees identifiers.
#
# deps: sentence-transformers (pulls torch + transformers). this will not train
# on an 8GB laptop in any reasonable time — run it on a box with a GPU. the
# script is the deliverable; the hardware is someone else's problem.

import os, sys, json, argparse, random

def load(path):
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line:
                rows.append(json.loads(line))
    return rows

def build_examples(rows, ST):
    # one (query -> note) positive per synthetic query. regions with no queries
    # fall back to (note -> note) so they still land in the space (weak signal).
    ex = []
    for r in rows:
        note = r.get("note", "").strip()
        if not note:
            continue
        qs = r.get("queries") or []
        if qs:
            for q in qs:
                ex.append(ST.InputExample(texts=[q.strip(), note]))
        else:
            ex.append(ST.InputExample(texts=[note, note]))
    return ex

def holdout(rows, frac, seed=42):
    # pull aside some (query, region-index) pairs for a recall@k sanity check.
    rng = random.Random(seed)
    pairs = []
    for i, r in enumerate(rows):
        for q in (r.get("queries") or []):
            pairs.append((q.strip(), i))
    rng.shuffle(pairs)
    cut = int(len(pairs) * frac)
    return pairs[:cut]

def recall_at_k(model, rows, held, ks=(1, 5, 10)):
    import numpy as np
    notes = [r.get("note", "") for r in rows]
    note_vecs = model.encode(notes, normalize_embeddings=True, show_progress_bar=False)
    qs   = [q for q, _ in held]
    gold = [i for _, i in held]
    qvecs = model.encode(qs, normalize_embeddings=True, show_progress_bar=False)
    sims = qvecs @ note_vecs.T                      # cosine (normalized)
    ranked = (-sims).argsort(axis=1)
    out = {}
    for k in ks:
        hit = sum(1 for row, g in zip(ranked, gold) if g in row[:k])
        out[k] = hit / max(len(gold), 1)
    return out

def export_index(model, rows, out_path):
    notes = [r.get("note", "") for r in rows]
    vecs = model.encode(notes, normalize_embeddings=True, show_progress_bar=True)
    dim = int(vecs.shape[1])
    with open(out_path, "w") as f:
        for r, v in zip(rows, vecs):
            f.write(json.dumps({
                "source": r.get("source"), "remote": r.get("remote", True),
                "path":   r.get("path"),   "name":   r.get("name"),
                "start":  r.get("start"),  "end":    r.get("end"),
                "dim":    dim,
                "vec":    [round(float(x), 6) for x in v],
            }) + "\n")
    return dim, len(rows)

def main():
    ap = argparse.ArgumentParser(description="polaris embedder fine-tune")
    ap.add_argument("--in",    dest="inp",  default="foundry/polaris/corpus/annotated.jsonl")
    ap.add_argument("--base",  default="BAAI/bge-small-en-v1.5", help="33M, 384-dim")
    ap.add_argument("--out",   default="foundry/polaris/model")
    ap.add_argument("--index", default="foundry/polaris/corpus/index.jsonl")
    ap.add_argument("--epochs", type=int, default=2)
    ap.add_argument("--batch",  type=int, default=64)
    ap.add_argument("--eval-frac", type=float, default=0.1)
    args = ap.parse_args()

    try:
        import sentence_transformers as ST
        from sentence_transformers import losses
        from torch.utils.data import DataLoader
    except Exception as e:
        sys.exit(f"need sentence-transformers (+torch) to train: {e}\n"
                 f"  pip install sentence-transformers")

    if not os.path.exists(args.inp):
        sys.exit(f"no annotated corpus at {args.inp} — run stage 2 (annotate) first.")

    rows = load(args.inp)
    print(f"loaded {len(rows)} annotated regions")
    examples = build_examples(rows, ST)
    print(f"built {len(examples)} training pairs")

    held = holdout(rows, args.eval_frac)
    print(f"held out {len(held)} (query, region) pairs for recall@k")

    model  = ST.SentenceTransformer(args.base)
    loader = DataLoader(examples, shuffle=True, batch_size=args.batch)
    loss   = losses.MultipleNegativesRankingLoss(model)

    warmup = int(len(loader) * args.epochs * 0.1)
    model.fit(train_objectives=[(loader, loss)], epochs=args.epochs,
              warmup_steps=warmup, show_progress_bar=True)

    os.makedirs(args.out, exist_ok=True)
    model.save(args.out)
    print(f"saved fine-tuned encoder -> {args.out}")

    if held:
        r = recall_at_k(model, rows, held)
        print("recall@k:", "  ".join(f"@{k}={v:.3f}" for k, v in r.items()))

    dim, n = export_index(model, rows, args.index)
    print(f"wrote {args.index}: {n} regions x {dim}-dim vectors")

if __name__ == "__main__":
    main()
