import lldb

# usage (inside lldb, process paused):
#   (lldb) script exec(open('/src/silver/support/vtable_dump.py').read())
#   (lldb) script dump_vt("vec_vec2f_i")            # dump by <type>_i symbol
#   (lldb) script dump_vt(ptr="ct->type")           # dump from any Au_t pointer expression
#   (lldb) script dump_vt("element_i", slots=60)    # override slot count
#
# offsets are computed from the running process's debug info, so this stays
# in sync with whatever struct _Au / struct _Au_f currently look like.

def dump_vt(symbol=None, ptr=None, slots=40):
    if not symbol and not ptr:
        print("usage: dump_vt(symbol='<name>_i')  or  dump_vt(ptr='<expr>')")
        return

    e        = lldb.SBError()
    # use the selected frame so locals (like aa_t) resolve correctly
    frame    = lldb.debugger.GetSelectedTarget().GetProcess() \
                            .GetSelectedThread().GetSelectedFrame()
    au_sz_v  = frame.EvaluateExpression("(long long)sizeof(struct _Au)")
    ft_off_v = frame.EvaluateExpression("(long long)__builtin_offsetof(struct _Au_f, ft)")
    au_sz    = au_sz_v.GetValueAsSigned()
    ft_off   = ft_off_v.GetValueAsSigned()

    if au_sz <= 0 or ft_off <= 0:
        print("error: could not resolve struct _Au / struct _Au_f offsets from debug info")
        return

    if ptr:
        label = ptr
        expr  = "(long long*)((char*)(%s) + %d)" % (ptr, ft_off)
    else:
        label = "&%s" % symbol
        # _info global layout: [Au header][Au_t fields][ft]
        expr  = "(long long*)((char*)&%s + %d + %d)" % (symbol, au_sz, ft_off)

    v    = frame.EvaluateExpression(expr)
    base = v.GetValueAsUnsigned()
    if base == 0:
        print("error: could not resolve %s (is the process running?)" % label)
        return

    print("vtable for %s" % label)
    print("  sizeof(struct _Au)  = %d" % au_sz)
    print("  offsetof(_Au_f, ft) = %d" % ft_off)
    print("  ft base             = 0x%x (aligned=%s)" % (base, "yes" if base % 8 == 0 else "NO"))
    print("-" * 70)
    for i in range(slots):
        addr = lldb.process.ReadPointerFromMemory(base + i * 8, e)
        s    = lldb.target.ResolveLoadAddress(addr).symbol
        n    = s.name if s else "(null)"
        print("[%2d] 0x%016x  %s" % (i, addr, n))
