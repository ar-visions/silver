import sys, struct, zipfile
# methods whose code loads a string holding the text
apk, text = sys.argv[1], sys.argv[2]
z = zipfile.ZipFile(apk)
for nm in sorted(n for n in z.namelist() if n.startswith('classes') and n.endswith('.dex')):
    d = z.read(nm)
    u16=lambda o: struct.unpack_from('<H',d,o)[0]; u32=lambda o: struct.unpack_from('<I',d,o)[0]
    def uleb(o):
        r=sh=0
        while True:
            b=d[o]; o+=1; r|=(b&0x7f)<<sh; sh+=7
            if not b&0x80: return r,o
    sn,so,tn,to,pn,po,fn,fo,mn,mo,cn,co = [u32(0x38+4*i) for i in range(12)]
    def string(i):
        p=u32(so+4*i); l,p=uleb(p); e=d.index(b'\0',p); return d[p:e].decode('utf8','replace')
    hits={i for i in range(sn) if text in string(i)}
    if not hits: continue
    for ci in range(cn):
        b=co+32*ci; cdo=u32(b+24)
        if not cdo: continue
        p=cdo; a,p=uleb(p); bb,p=uleb(p); dm,p=uleb(p); vm,p=uleb(p)
        for _ in range(a+bb): _,p=uleb(p); _,p=uleb(p)
        idx=0
        for k in range(dm+vm):
            if k==dm: idx=0
            di,p=uleb(p); idx+=di; _,p=uleb(p); code,p=uleb(p)
            if not code: continue
            ins=u32(code+12); q=code+16; end=q+2*ins
            while q+4<=end:
                w=u16(q)
                if w&0xff==0x1a and u16(q+2) in hits:
                    c=u16(mo+8*idx); print(nm, string(u32(to+4*c)), string(u32(mo+8*idx+4)), string(u16(q+2))); break
                q+=2
