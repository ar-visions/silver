# aeon: a .blend becomes an Aeon project folder
# with bpy: python3.11 aeon/blend.py <file.blend> <out> [refs]
# in blender: blender -b <file.blend> --python aeon/blend.py
#   -- <out> [refs]; refs: blender's points, for aeon's tests
import bpy, sys, os, struct, math
import numpy as np

AXES = 'xyz'
DOMAINS = { 'POINT': 0, 'EDGE': 1, 'FACE': 2, 'CORNER': 3 }
# blender data type: (kind 0 f32 1 i32 2 u8, width, field)
TYPES = {
    'FLOAT':        (0, 1, 'value'),
    'FLOAT2':       (0, 2, 'vector'),
    'FLOAT_VECTOR': (0, 3, 'vector'),
    'FLOAT_COLOR':  (0, 4, 'color'),
    'BYTE_COLOR':   (0, 4, 'color'),
    'INT':          (1, 1, 'value'),
    'INT32_2D':     (1, 2, 'value'),
    'INT8':         (1, 1, 'value'),
    'BOOLEAN':      (2, 1, 'value'),
}
NP = { 0: np.float32, 1: np.int32, 2: np.uint8 }

def name_of(s):
    out = ''.join(c if c.isalnum() else '_' for c in s.lower())
    return out.strip('_') or 'unnamed'

def num(v):
    if isinstance(v, bool): return 'true' if v else 'false'
    if isinstance(v, int): return str(v)
    r = round(float(v), 6)
    if r == int(r): return str(int(r))
    return ('%.6f' % r).rstrip('0')

def vec(v): return '[ ' + ', '.join(num(x) for x in v) + ' ]'

def quote(s): return "'" + s.replace("'", "\\'") + "'"

def words(v): return '[ ' + ', '.join(v) + ' ]'

# the cage: positions, edges, faces, then every named attribute
def write_mesh(me, file):
    nv, ne = len(me.vertices), len(me.edges)
    nf, nc = len(me.polygons), len(me.loops)
    pos = np.zeros(nv * 3, np.float32)
    me.vertices.foreach_get('co', pos)
    ev = np.zeros(ne * 2, np.int32)
    me.edges.foreach_get('vertices', ev)
    fs = np.zeros(nf, np.int32)
    me.polygons.foreach_get('loop_total', fs)
    cv = np.zeros(nc, np.int32)
    me.loops.foreach_get('vertex_index', cv)
    count = { 0: nv, 1: ne, 2: nf, 3: nc }
    attrs = []
    for a in me.attributes:
        if a.name.startswith('.') or a.name == 'position': continue
        if a.data_type not in TYPES or a.domain not in DOMAINS:
            print('aeon: skipped attribute', a.name, a.data_type)
            continue
        kind, width, field = TYPES[a.data_type]
        dom = DOMAINS[a.domain]
        n = count[dom]
        buf = np.zeros(n * width, np.float32 if kind == 0 else
                       (np.int32 if kind == 1 else bool))
        a.data.foreach_get(field, buf)
        attrs.append((a.name, dom, kind, width, buf.astype(NP[kind])))
    with open(file, 'wb') as f:
        f.write(b'AEMS')
        f.write(struct.pack('<5I', 1, nv, ne, nf, nc))
        f.write(pos.tobytes())
        f.write(ev.astype(np.uint32).tobytes())
        f.write(fs.astype(np.uint32).tobytes())
        f.write(cv.astype(np.uint32).tobytes())
        f.write(struct.pack('<I', len(attrs)))
        for name, dom, kind, width, data in attrs:
            b = name.encode()
            f.write(struct.pack('<I', len(b)))
            f.write(b)
            f.write(struct.pack('<3I', dom, kind, width))
            f.write(data.tobytes())
    return nv, nf

def transform(lines, ind, m):
    loc, rot, sc = m.decompose()
    e = rot.to_euler('XYZ')
    lines.append(ind + 'at: ' + vec(loc))
    lines.append(ind + 'turn: ' +
                 vec(round(math.degrees(x), 4) for x in e))
    lines.append(ind + 'size: ' + vec(sc))

def axes_of(flags): return words(AXES[i] for i in range(3) if flags[i])

# a modifier as a node: its settings, never applied
def node(lines, o, m, key, inp, used):
    ind = '\t\t\t'
    lines.append('\t\t' + key + ': ' + {
        'MIRROR': 'Mirror', 'SUBSURF': 'Subdivision',
        'ARRAY': 'Array' }.get(m.type, 'Modifier'))
    lines.append(ind + 'input: ' + inp)
    if not (m.show_viewport and m.show_render):
        lines.append(ind + 'enabled: false')
    if m.type == 'MIRROR':
        lines.append(ind + 'axes: ' + axes_of(m.use_axis))
        if any(m.use_bisect_axis):
            lines.append(ind + 'bisect: ' + axes_of(m.use_bisect_axis))
            lines.append(ind + 'flip: ' +
                         axes_of(m.use_bisect_flip_axis))
        if m.use_clip: lines.append(ind + 'clip: true')
        if m.use_mirror_merge:
            lines.append(ind + 'merge: ' + num(m.merge_threshold))
        if m.mirror_object:
            used.add(m.mirror_object.name)
            lines.append(ind + 'across:')
            transform(lines, ind + '\t', o.matrix_world.inverted()
                      @ m.mirror_object.matrix_world)
    elif m.type == 'SUBSURF':
        lines.append(ind + 'levels: ' + num(m.levels))
        lines.append(ind + 'render_levels: ' + num(m.render_levels))
        if m.subdivision_type != 'CATMULL_CLARK':
            lines.append(ind + 'simple: true')
        lines.append(ind + 'creases: ' + num(m.use_creases))
        lines.append(ind + 'boundary: ' + m.boundary_smooth.lower())
        lines.append(ind + 'uv: ' + m.uv_smooth.lower())
        lines.append(ind + 'limit: ' + num(m.use_limit_surface))
        lines.append(ind + 'quality: ' + num(m.quality))
    elif m.type == 'ARRAY':
        if m.fit_type != 'FIXED_COUNT':
            lines.append(ind + 'fit: ' + m.fit_type.lower())
            lines.append(ind + 'fit_length: ' + num(m.fit_length))
        lines.append(ind + 'count: ' + num(m.count))
        if m.use_relative_offset:
            lines.append(ind + 'relative: ' +
                         vec(m.relative_offset_displace))
        if m.use_constant_offset:
            lines.append(ind + 'constant: ' +
                         vec(m.constant_offset_displace))
        if m.use_object_offset and m.offset_object:
            used.add(m.offset_object.name)
            lines.append(ind + 'step:')
            transform(lines, ind + '\t', o.matrix_world.inverted()
                      @ m.offset_object.matrix_world)
        if m.use_merge_vertices:
            lines.append(ind + 'merge: ' + num(m.merge_threshold))
            lines.append(ind + 'merge_ends: ' +
                         num(m.use_merge_vertices_cap))
    else:
        lines.append(ind + 'kind: ' + m.type.lower())
        for p in m.bl_rna.properties:
            k = p.identifier
            if k in ('rna_type', 'name', 'type') or k.startswith(
                    'show') or k.startswith('is_'): continue
            v = getattr(m, k)
            if isinstance(v, (bool, int, float)):
                lines.append(ind + k + ': ' + num(v))
            elif isinstance(v, str) and v:
                lines.append(ind + k + ': ' + quote(v))
            elif hasattr(v, 'name'):
                lines.append(ind + k + ': ' + quote(v.name))
        print('aeon: no node type yet for', m.type, 'on', o.name)

def socket_key(s): return name_of(s.name)

# a Principled surface: every input not wired, by its own name
def write_material(mat, file):
    lines = [ 'surface: Principled' ]
    if not mat.use_nodes:
        lines.append('\tbase_color: ' + vec(mat.diffuse_color))
        lines.append('\tmetallic: ' + num(mat.metallic))
        lines.append('\troughness: ' + num(mat.roughness))
    else:
        bsdf = [ n for n in mat.node_tree.nodes
                 if n.type == 'BSDF_PRINCIPLED' ]
        others = [ n.type for n in mat.node_tree.nodes
                   if n.type not in ('BSDF_PRINCIPLED',
                                     'OUTPUT_MATERIAL') ]
        if others: print('aeon:', mat.name, 'nodes not read:', others)
        if bsdf:
            b = bsdf[0]
            lines.append('\tdistribution: ' + b.distribution.lower())
            lines.append('\tscattering: ' +
                         b.subsurface_method.lower())
            for s in b.inputs:
                if not s.enabled or s.type in ('SHADER', 'VECTOR'):
                    continue
                if s.is_linked:
                    print('aeon:', mat.name, s.name, 'is wired')
                    continue
                v = s.default_value
                lines.append('\t' + socket_key(s) + ': ' +
                    (vec(v) if hasattr(v, '__len__') else num(v)))
    # blender 4.2 on: dithered (opaque) or blended
    if hasattr(mat, 'surface_render_method'):
        blended = mat.surface_render_method == 'BLENDED'
        lines.append('blend: ' + ('blended' if blended else 'opaque'))
    else:
        lines.append('blend: ' + mat.blend_method.lower())
    with open(file, 'w') as f: f.write('\n'.join(lines) + '\n')

# what follows '--' in blender; else the .blend, opened here
def arguments():
    if '--' in sys.argv:
        return sys.argv[sys.argv.index('--') + 1:]
    bpy.ops.wm.open_mainfile(filepath=sys.argv[1])
    return sys.argv[2:]

def main():
    args = arguments()
    out = args[0]
    os.makedirs(out + '/meshes', exist_ok=True)
    os.makedirs(out + '/materials', exist_ok=True)
    proj = os.path.basename(out.rstrip('/'))
    meshes = {}
    for me in bpy.data.meshes:
        if me.users == 0: continue
        # a mesh one object uses is named by that object
        objs = [ o for o in bpy.data.objects if o.data == me ]
        nm = name_of(objs[0].name if len(objs) == 1 else me.name)
        nv, nf = write_mesh(me, out + '/meshes/' + nm + '.mesh')
        meshes[me.name] = nm
        print('aeon: mesh', nm, nv, 'vertices', nf, 'faces')
    for mat in bpy.data.materials:
        if mat.users == 0: continue
        write_material(mat, out + '/materials/' + name_of(mat.name)
                       + '.agi')
    used = set()
    lines = []
    for o in bpy.data.objects:
        if o.type != 'MESH': continue
        lines.append(name_of(o.name) + ': Object')
        if o.parent:
            lines.append('\tparent: ' + name_of(o.parent.name))
            transform(lines, '\t', o.matrix_parent_inverse
                      @ o.matrix_basis)
        else:
            transform(lines, '\t', o.matrix_world)
        if o.hide_render: lines.append('\tshown: false')
        cols = [ quote(c.name) for c in o.users_collection ]
        lines.append('\tcollections: ' + words(cols))
        lines.append('\tmaterials: ' + words(
            name_of(s.material.name) if s.material else 'none'
            for s in o.material_slots))
        lines.append('\tnodes:')
        lines.append('\t\tcage: Cage')
        lines.append("\t\t\tfile: 'meshes/" + meshes[o.data.name]
                     + ".mesh'")
        inp, taken = 'cage', { 'cage' }
        for m in o.modifiers:
            base = name_of(m.type if m.type != 'SUBSURF'
                           else 'subdivision')
            key, i = base, 2
            while key in taken:
                key = base + str(i)
                i += 1
            taken.add(key)
            node(lines, o, m, key, inp, used)
            inp = key
        lines.append('\tout: ' + inp)
    # an empty that only places an array's step is folded into it
    for o in bpy.data.objects:
        if o.type == 'MESH' or o.name in used: continue
        lines.append(name_of(o.name) + ': ' + o.type.capitalize())
        transform(lines, '\t', o.matrix_world)
        print('aeon: kept', o.type.lower(), o.name)
    with open(out + '/' + proj + '.agi', 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('aeon: wrote', out)
    if len(args) > 1: write_refs(args[1])

# each object as blender makes it: counts, then its points
def write_refs(refs):
    os.makedirs(refs, exist_ok=True)
    dg = bpy.context.evaluated_depsgraph_get()
    for o in bpy.data.objects:
        if o.type != 'MESH': continue
        e = o.evaluated_get(dg).to_mesh()
        p = np.zeros(len(e.vertices) * 3, np.float32)
        e.vertices.foreach_get('co', p)
        with open(refs + '/' + name_of(o.name) + '.ref', 'wb') as f:
            f.write(struct.pack('<2I', len(e.vertices),
                                len(e.polygons)))
            f.write(p.tobytes())
        print('aeon: ref', name_of(o.name), len(e.vertices))

main()
