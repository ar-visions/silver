import sys, struct, zipfile
apk, cls, meth = sys.argv[1], sys.argv[2], sys.argv[3]
SZ = {}
def s(a,b,n):
    for i in range(a,b+1): SZ[i]=n
s(0,0xff,1)
for o,n in {2:2,3:3,5:2,6:3,8:2,9:3,0x13:2,0x14:3,0x15:2,0x16:2,0x17:3,0x18:5,0x19:2,0x1a:2,0x1b:3,0x1c:2,0x1f:2,0x20:2,0x22:2,0x23:2,0x24:3,0x25:3,0x26:3,0x29:2,0x2a:3,0x2b:3,0x2c:3}.items(): SZ[o]=n
s(0x2d,0x3d,2); s(0x44,0x72,2); s(0x6e,0x72,3); s(0x74,0x78,3); s(0x90,0xaf,2); s(0xd0,0xe2,2)
SZ[0xfa]=4; SZ[0xfb]=4; SZ[0xfc]=3; SZ[0xfd]=3; SZ[0xfe]=2; SZ[0xff]=2
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
    def typ(i): return string(u32(to+4*i))
    def method(i):
        c=u16(mo+8*i); n=u32(mo+8*i+4); return typ(c)+'->'+string(n)
    def field(i):
        c=u16(fo+8*i); t=u16(fo+8*i+2); n=u32(fo+8*i+4); return typ(c)+'.'+string(n)+':'+typ(t)
    want='L'+cls.replace('.','/')+';'
    for ci in range(cn):
        b=co+32*ci
        if typ(u32(b))!=want: continue
        cdo=u32(b+24); p=cdo
        a,p=uleb(p); bb,p=uleb(p); dm,p=uleb(p); vm,p=uleb(p)
        for _ in range(a+bb): _,p=uleb(p); _,p=uleb(p)
        idx=0
        for k in range(dm+vm):
            if k==dm: idx=0
            di,p=uleb(p); idx+=di; _,p=uleb(p); code,p=uleb(p)
            if string(u32(mo+8*idx+4))!=meth or not code: continue
            print('==', nm, method(idx))
            ins=u32(code+12); q=code+16; end=q+2*ins
            while q<end:
                w=u16(q); op=w&0xff
                if op==0 and w>>8 in (1,2,3): break
                n=SZ[op]; txt='%04x op %02x' % ((q-code-16)//2, op)
                if op in range(0x6e,0x73) or op in range(0x74,0x79): txt+=' invoke '+method(u16(q+2))
                elif op in range(0x52,0x6e): txt+=' field '+field(u16(q+2))
                elif op in (0x1a,): txt+=' "'+string(u16(q+2))[:60]+'"'
                elif op in (0x1c,0x1f,0x20,0x22): txt+=' type '+typ(u16(q+2))
                elif op==0x14: txt+=' const 0x%x' % u32(q+2)
                elif op==0x15: txt+=' const/high16 0x%x' % (u16(q+2)<<16)
                elif op==0x13: txt+=' const/16 %d' % struct.unpack_from('<h',d,q+2)[0]
                elif op==0x12: txt+=' const/4 %d' % ((w>>12) if (w>>12)<8 else (w>>12)-16)
                elif op in range(0x32,0x3e): txt+=' if-> %04x' % ((q-code-16)//2+struct.unpack_from('<h',d,q+2)[0])
                print(txt); q+=2*n
