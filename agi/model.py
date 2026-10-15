# agi seed: an OLMo 2 backbone, shared, with a module per author
# (a low-rank adapter on the upper blocks) and a router: the
# context scored against every author vector picks how much of
# each author's module is used. The router's weights are the
# author output; switching modules off measures contributions
import os, math
import torch
import torch.nn as nn
from transformers import Olmo2Config, Olmo2ForCausalLM

WIDE = 512
RANK = 4
DIM = 128
# the router reads the stream after this block; the author
# modules add to the blocks after it
ROUTE_AT = 3


class Authors(nn.Module):
    def __init__(self, n, layers):
        super().__init__()
        self.n = n
        self.vec = nn.Parameter(torch.randn(n, DIM) * 0.02)
        self.proj = nn.Linear(WIDE, DIM, bias=False)
        self.down = nn.ParameterList(
            [nn.Parameter(torch.randn(WIDE, n * RANK) * 0.02)
             for _ in range(layers)])
        self.up = nn.ParameterList(
            [nn.Parameter(torch.zeros(n * RANK, WIDE))
             for _ in range(layers)])
        # set per forward: given weights in place of the router's
        # [rows, n]; and a mask of modules switched off [rows, n]
        self.given = None
        self.off = None
        self.w = None

    # each place's weights over the authors, from the stream so
    # far (a running mean, so a place sees only its past)
    def route(self, h):
        t = torch.arange(1, h.size(1) + 1, device=h.device)
        so_far = h.float().cumsum(1) / t[None, :, None]
        s = self.proj(so_far) @ self.vec.t() / math.sqrt(DIM)
        return torch.softmax(s, -1)

    def add(self, i, h):
        w = self.w
        if self.given is not None:
            w = self.given[:, None, :].expand(-1, h.size(1), -1)
        if self.off is not None:
            w = w * self.off[:, None, :]
        z = (h @ self.down[i].to(h.dtype)).view(
            h.size(0), h.size(1), self.n, RANK)
        z = z * w[..., None].to(h.dtype)
        return z.view(h.size(0), h.size(1), -1) @ self.up[i].to(h.dtype)


class AgiModel(nn.Module):
    def __init__(self, lm, nauth):
        super().__init__()
        self.lm = lm
        blocks = lm.model.layers
        self.authors = Authors(nauth, len(blocks) - ROUTE_AT - 1)
        blocks[ROUTE_AT].register_forward_hook(self.at_route)
        for i in range(ROUTE_AT + 1, len(blocks)):
            blocks[i].register_forward_hook(self.at_block(
                i - ROUTE_AT - 1))

    def at_route(self, mod, inp, out):
        self.authors.w = self.authors.route(out)

    def at_block(self, i):
        def hook(mod, inp, out):
            return out + self.authors.add(i, out)
        return hook

    # logits [rows, places, vocab]; router weights [rows, places, n]
    def forward(self, ids):
        logits = self.lm(input_ids=ids).logits
        return logits, self.authors.w

    def save(self, folder):
        self.lm.save_pretrained(folder)
        torch.save(self.authors.state_dict(),
                   os.path.join(folder, 'authors.pt'))


def model_new(vocab, nauth, pad, ctx):
    c = Olmo2Config(vocab_size=vocab, hidden_size=WIDE,
                    intermediate_size=1376, num_hidden_layers=8,
                    num_attention_heads=8, num_key_value_heads=8,
                    max_position_embeddings=ctx, pad_token_id=pad)
    return AgiModel(Olmo2ForCausalLM(c), nauth)


def model_open(folder, nauth):
    m = AgiModel(Olmo2ForCausalLM.from_pretrained(folder), nauth)
    m.authors.load_state_dict(torch.load(
        os.path.join(folder, 'authors.pt'), map_location='cpu'))
    return m
