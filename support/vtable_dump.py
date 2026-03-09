import lldb, sys

# usage: script exec(open('/path/to/silver/support/vtable_dump.py').read())
#
# dumps the vtable (ft) for a Silver type info global.
# change SYMBOL and SLOTS below, or pass args via lldb variable.
#
# the layout is:
#   [0..155]  Au object header (156 bytes)
#   [156..355] Au_t fields (200 bytes to ft)
#   [356..]   ft function pointer slots

SYMBOL = "ai_conv_i"
SLOTS  = 62

e = lldb.SBError()
v = lldb.target.EvaluateExpression(
    "(long long*)((char*)&%s + 356)" % SYMBOL)
base = v.GetValueAsUnsigned()
if base == 0:
    print("error: could not resolve &%s (is the process running?)" % SYMBOL)
else:
    print("vtable for %s  (ft at 0x%x)" % (SYMBOL, base))
    print("-" * 60)
    for i in range(SLOTS):
        addr = lldb.process.ReadPointerFromMemory(base + i * 8, e)
        s = lldb.target.ResolveLoadAddress(addr).symbol
        n = s.name if s else "(null)"
        print("[%2d] 0x%016x  %s" % (i, addr, n))
