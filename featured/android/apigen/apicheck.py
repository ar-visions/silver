import sys,struct,zipfile
def load(d):
    u16=lambda o: struct.unpack_from('<H',d,o)[0]; u32=lambda o: struct.unpack_from('<I',d,o)[0]
    def uleb(o):
        r=sh=0
        while True:
            b=d[o];o+=1;r|=(b&127)<<sh;sh+=7
            if b<128:return r,o
    strs=[]
    for i in range(u32(0x38)):
        l,p=uleb(u32(u32(0x3c)+4*i)); e=d.index(b'\0',p); strs.append(d[p:e].decode('utf8','replace'))
    types=[strs[u32(u32(0x44)+4*i)] for i in range(u32(0x40))]
    po=u32(0x4c); protos=[]
    for i in range(u32(0x48)):
        sh,rt,pa=struct.unpack_from('<III',d,po+12*i)
        ps=[]
        if pa:
            for k in range(u32(pa)): ps.append(types[u16(pa+4+2*k)])
        protos.append('('+''.join(ps)+')'+types[rt])
    mo=u32(0x5c); meths=[]
    for i in range(u32(0x58)):
        c,p,n=struct.unpack_from('<HHI',d,mo+8*i); meths.append((types[c],strs[n],protos[p]))
    fo=u32(0x54); fields=[]
    for i in range(u32(0x50)):
        c,t,n=struct.unpack_from('<HHI',d,fo+8*i); fields.append((types[c],strs[n]))
    classes={}
    for c in range(u32(0x60)):
        co=u32(0x64)+32*c; cls=types[u32(co)]; sup=u32(co+8); ifo=u32(co+12)
        ifs=[types[u16(ifo+4+2*k)] for k in range(u32(ifo))] if ifo else []
        cdo=u32(co+24); ms=set(); fs=set()
        if cdo:
            o=cdo; sf,o=uleb(o); inf,o=uleb(o); dm,o=uleb(o); vm,o=uleb(o)
            fi=0
            for _ in range(sf+inf):
                x,o=uleb(o); fi+=x if _ not in (0,sf) else x; 
            # recompute fields properly
            o=cdo; sf,o=uleb(o); inf,o=uleb(o); dm,o=uleb(o); vm,o=uleb(o)
            for cnt in (sf,inf):
                fi=0
                for _ in range(cnt):
                    x,o=uleb(o); fi+=x; _a,o=uleb(o); fs.add(fields[fi][1])
            for cnt in (dm,vm):
                mi=0
                for _ in range(cnt):
                    x,o=uleb(o); mi+=x; _a,o=uleb(o); _b,o=uleb(o); ms.add(meths[mi][1]+meths[mi][2])
        classes[cls]=(types[sup] if sup!=0xffffffff else None, ifs, ms, fs)
    return meths,fields,classes
lib={}
for p in sys.argv[1].split(','):
    z=zipfile.ZipFile(p)
    for n in z.namelist():
        if n.endswith('.dex'): lib.update(load(z.read(n))[2])
app={}; refs=set(); frefs=set()
z=zipfile.ZipFile(sys.argv[2])
for n in z.namelist():
    if n.startswith('classes') and n.endswith('.dex'):
        m,f,c=load(z.read(n)); app.update(c); refs.update(m); frefs.update(f)
def has(cls,key,isf):
    seen=set(); st=[cls]
    while st:
        c=st.pop()
        if not c or c in seen: continue
        seen.add(c)
        if c not in lib and c not in app: continue
        sup,ifs,ms,fs=lib.get(c) or app.get(c)
        if key in (fs if isf else ms): return True
        st.append(sup); st.extend(ifs)
    return False
pre=('Landroid/','Ljava/','Ljavax/','Lorg/json/','Lorg/xml','Ldalvik/')
missc=set(); out=[]
# what app classes extend and implement is referenced too
for c,(sup,ifs,ms,fs) in app.items():
    for t in [sup]+list(ifs):
        if t and t.startswith(pre) and t not in app and t not in lib: missc.add(t)
for c,nm,pr in sorted(refs):
    if not c.startswith(pre) or c in app: continue
    if c.startswith('['): continue
    if c not in lib: missc.add(c); continue
    if not has(c,nm+pr,False): out.append(c+'->'+nm+pr)
for c,nm in sorted(frefs):
    if not c.startswith(pre) or c in app or c not in lib: continue
    if not has(c,nm,True): out.append(c+'.'+nm)
print('MISSING CLASSES',len(missc)); print('\n'.join(sorted(missc)))
print('MISSING MEMBERS',len(out)); print('\n'.join(out))
