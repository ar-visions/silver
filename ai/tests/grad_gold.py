# golden files for grad's expects: python3 ai/tests/grad_gold.py <dir>
# then GRAD_GOLD=<dir> silver --test ai (t_grad_lm_ops reads c_ bm_
# xe_ xr_; the older sets came from scripts since lost)
import sys, torch, numpy as np
out = sys.argv[1]
torch.manual_seed(7)

def put(name, t):
    t.detach().float().contiguous().numpy().astype(np.float32).tofile(
        f'{out}/{name}.f32')

# causal attention on [b, c, t], no relative tables, a [b, 1, t] mask
b, c, t, nh = 2, 12, 9, 3
dk = c // nh
q = torch.randn(b, c, t, requires_grad=True)
k = torch.randn(b, c, t, requires_grad=True)
v = torch.randn(b, c, t, requires_grad=True)
m = torch.ones(b, 1, t)
m[1, 0, 7:] = 0.0
qh = q.view(b, nh, dk, t)
kh = k.view(b, nh, dk, t)
vh = v.view(b, nh, dk, t)
s = torch.einsum('bhdi,bhdj->bhij', qh, kh) / dk ** 0.5
mm = m[:, 0, :, None] * m[:, 0, None, :]
causal = torch.tril(torch.ones(t, t))
hide = (mm[:, None] * causal) == 0
s = s.masked_fill(hide, -1e4)
p = torch.softmax(s, dim=-1)
o = torch.einsum('bhij,bhdj->bhdi', p, vh).reshape(b, c, t)
do = torch.randn(b, c, t)
o.backward(do)
for n, x in [('c_q', q), ('c_k', k), ('c_v', v), ('c_m', m), ('c_o', o),
             ('c_do', do), ('c_dq', q.grad), ('c_dk', k.grad),
             ('c_dv', v.grad)]:
    put(n, x)

# batched matmul
a = torch.randn(2, 3, 5, requires_grad=True)
x = torch.randn(2, 5, 4, requires_grad=True)
y = torch.bmm(a, x)
dy = torch.randn(2, 3, 4)
y.backward(dy)
for n, z in [('bm_a', a), ('bm_x', x), ('bm_y', y), ('bm_dy', dy),
             ('bm_da', a.grad), ('bm_dx', x.grad)]:
    put(n, z)

# cross entropy, classes on dim 1 of [b, v, t], one target skipped
xl = torch.randn(2, 7, 6, requires_grad=True)
tg = torch.randint(0, 7, (2, 6))
tg[1, 2] = -1
l = torch.nn.functional.cross_entropy(xl, tg, ignore_index=-1)
l.backward()
put('xe_x', xl)
put('xe_t', tg.float())
put('xe_l', l.reshape(1))
put('xe_dx', xl.grad)

# cross entropy on plain rows [n, v]
xr = torch.randn(5, 7, requires_grad=True)
tr = torch.randint(0, 7, (5,))
tr[3] = -1
lr = torch.nn.functional.cross_entropy(xr, tr, ignore_index=-1)
lr.backward()
put('xr_x', xr)
put('xr_t', tr.float())
put('xr_l', lr.reshape(1))
put('xr_dx', xr.grad)
print('gold written to', out)
