import sys, struct, zipfile
# a class's super, interfaces, fields and method signatures
apk, cls = sys.argv[1], sys.argv[2]
z = zipfile.ZipFile(apk)
want = 'L' + cls.replace('.', '/') + ';'
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
    typ=lambda i: string(u32(to+4*i))
    def proto(i):
        b=po+12*i; r=typ(u32(b+4)); pl=u32(b+8); a=[]
        if pl:
            for k in range(u32(pl)): a.append(typ(u16(pl+4+2*k)))
        return '(' + ', '.join(a) + ') -> ' + r
    for ci in range(cn):
        b=co+32*ci
        if typ(u32(b))!=want: continue
        print(nm, 'flags 0x%x' % u32(b+4), 'super', typ(u32(b+8)) if u32(b+8)!=0xffffffff else '-')
        cdo=u32(b+24)
        if not cdo: continue
        p=cdo; ns,p=uleb(p); ni,p=uleb(p); dm,p=uleb(p); vm,p=uleb(p)
        idx=0
        for k in range(ns+ni):
            if k==ns: idx=0
            di,p=uleb(p); idx+=di; fl,p=uleb(p)
            print('  field 0x%x %s %s' % (fl, string(u32(fo+8*idx+4)), typ(u16(fo+8*idx+2))))
        idx=0
        for k in range(dm+vm):
            if k==dm: idx=0
            di,p=uleb(p); idx+=di; fl,p=uleb(p); _,p=uleb(p)
            print('  method 0x%x %s %s' % (fl, string(u32(mo+8*idx+4)), proto(u16(mo+8*idx+2))))
