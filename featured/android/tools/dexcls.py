import sys, struct, zipfile
apk, cls = sys.argv[1], sys.argv[2]
z = zipfile.ZipFile(apk)
for nm in sorted(n for n in z.namelist() if n.startswith('classes') and n.endswith('.dex')):
    d = z.read(nm)
    u16=lambda o: struct.unpack_from('<H',d,o)[0]; u32=lambda o: struct.unpack_from('<I',d,o)[0]
    def uleb(o):
        r=sh=0
        while True:
            b=d[o]; o+=1; r|=(b&0x7f)<<sh; sh+=7
            if not b&0x80: return r,o
    sn,so,tn,to,pn,po,fn,fo,mn,mo,cn,co=[u32(0x38+4*i) for i in range(12)]
    def string(i):
        p=u32(so+4*i); l,p=uleb(p); e=d.index(b'\0',p); return d[p:e].decode('utf8','replace')
    def typ(i): return string(u32(to+4*i))
    want='L'+cls.replace('.','/')+';'
    for ci in range(cn):
        b=co+32*ci
        if typ(u32(b))!=want: continue
        sup=u32(b+8); print(nm, want, 'extends', typ(sup))
        ifo=u32(b+12)
        if ifo:
            n=u32(ifo); print(' implements', [typ(u16(ifo+4+2*k)) for k in range(n)])
        cd=u32(b+24)
        if cd:
            sf,inf,dm,vm=[0]*4; o=cd
            sf,o=uleb(o); inf,o=uleb(o); dm,o=uleb(o); vm,o=uleb(o)
            for _ in range(sf+inf): _,o=uleb(o); _,o=uleb(o)
            names=[]
            for cnt in (dm,vm):
                idx=0
                for _ in range(cnt):
                    dd,o=uleb(o); idx+=dd; _,o=uleb(o); _,o=uleb(o)
                    names.append(string(u32(mo+8*idx+4)))
            print(' methods', names)
