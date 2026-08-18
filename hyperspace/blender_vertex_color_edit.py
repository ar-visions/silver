bl_info = {
    "name": "Edit Mode Vertex Color",
    "author": "claude + kalen",
    "version": (1, 0),
    "blender": (3, 6, 0),
    "location": "3D View > N sidebar > Vertex Color (in Edit Mode)",
    "description": "See and set the active color attribute on the edit-mode selection",
    "category": "Mesh",
}

import bpy


def _active_attr(me):
    # blender has TWO actives: the properties-list highlight (.active)
    # and the paint active (.active_color). the list is what the user
    # sees selected — follow it, fall back to the paint active
    ca = me.color_attributes.active
    if ca is None:
        ca = me.color_attributes.active_color
    return ca


def _selected_vert_indices(me):
    return [v.index for v in me.vertices if v.select]


class MESH_OT_vcp_apply(bpy.types.Operator):
    """Set the selected vertices to the panel color on the active color attribute"""
    bl_idname = "mesh.vcp_apply"
    bl_label = "Apply Color to Selected"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'MESH' and context.object.mode == 'EDIT'

    def execute(self, context):
        obj = context.object
        # edit-mode mesh data is stale; hop through object mode to sync
        bpy.ops.object.mode_set(mode='OBJECT')
        me = obj.data
        ca = _active_attr(me)
        if ca is None:
            bpy.ops.object.mode_set(mode='EDIT')
            self.report({'ERROR'}, "no active color attribute")
            return {'CANCELLED'}
        col = context.scene.vcp_color
        rgba = (col[0], col[1], col[2], 1.0)
        sel = set(_selected_vert_indices(me))
        if not sel:
            bpy.ops.object.mode_set(mode='EDIT')
            self.report({'WARNING'}, "no vertices selected — nothing written")
            return {'CANCELLED'}
        n = 0
        if ca.domain == 'POINT':
            for i in sel:
                ca.data[i].color_srgb = rgba
                n += 1
        else:  # CORNER: every loop that touches a selected vertex
            for loop in me.loops:
                if loop.vertex_index in sel:
                    ca.data[loop.index].color_srgb = rgba
                    n += 1
        bpy.ops.object.mode_set(mode='EDIT')
        self.report({'INFO'}, f"{ca.name}: {n} written")
        return {'FINISHED'}


class MESH_OT_vcp_sample(bpy.types.Operator):
    """Read the active color attribute from the selection into the panel color"""
    bl_idname = "mesh.vcp_sample"
    bl_label = "Sample from Selected"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'MESH' and context.object.mode == 'EDIT'

    def execute(self, context):
        obj = context.object
        bpy.ops.object.mode_set(mode='OBJECT')
        me = obj.data
        ca = _active_attr(me)
        sel = _selected_vert_indices(me)
        if ca is None or not sel:
            bpy.ops.object.mode_set(mode='EDIT')
            self.report({'ERROR'}, "no active color attribute or nothing selected")
            return {'CANCELLED'}
        if ca.domain == 'POINT':
            c = ca.data[sel[0]].color_srgb
        else:
            c = (1.0, 1.0, 1.0, 1.0)
            for loop in me.loops:
                if loop.vertex_index == sel[0]:
                    c = ca.data[loop.index].color_srgb
                    break
        context.scene.vcp_color = (c[0], c[1], c[2])
        bpy.ops.object.mode_set(mode='EDIT')
        self.report({'INFO'}, f"{ca.name}: r={c[0]:.3f} g={c[1]:.3f} b={c[2]:.3f}")
        return {'FINISHED'}


class MESH_OT_vcp_mirror(bpy.types.Operator):
    """Mirror painted vertex colors across X: wherever one side of a
    mirrored vertex pair has paint (blue > 0.5) and the other doesn't,
    copy it over. Applies to every _V_ color attribute."""
    bl_idname = "mesh.vcp_mirror"
    bl_label = "Mirror _V_ Colors Across X"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'MESH'

    def execute(self, context):
        obj = context.object
        mode0 = obj.mode
        if mode0 != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        me = obj.data
        # mirror pair lookup by rounded rest position
        pos = {}
        for v in me.vertices:
            pos[(round(v.co.x, 5), round(v.co.y, 5), round(v.co.z, 5))] = v.index
        pair = {}
        for v in me.vertices:
            m = pos.get((round(-v.co.x, 5), round(v.co.y, 5), round(v.co.z, 5)))
            if m is not None and m != v.index:
                pair[v.index] = m
        # vert -> loops map for corner-domain attributes
        vloops = {}
        for loop in me.loops:
            vloops.setdefault(loop.vertex_index, []).append(loop.index)
        total = 0
        for ca in me.color_attributes:
            if not ca.name.startswith('_V_'):
                continue
            def vert_col(i):
                if ca.domain == 'POINT':
                    return ca.data[i].color_srgb
                ls = vloops.get(i)
                return ca.data[ls[0]].color_srgb if ls else None
            def set_vert(i, c):
                if ca.domain == 'POINT':
                    ca.data[i].color_srgb = c
                else:
                    for li in vloops.get(i, []):
                        ca.data[li].color_srgb = c
            n = 0
            for a, b in pair.items():
                if a > b:
                    continue  # each pair once
                cA, cB = vert_col(a), vert_col(b)
                if cA is None or cB is None:
                    continue
                pa, pb = cA[2] > 0.5, cB[2] > 0.5
                if pa and not pb:
                    set_vert(b, tuple(cA))
                    n += 1
                elif pb and not pa:
                    set_vert(a, tuple(cB))
                    n += 1
            if n:
                print(f"vcp_mirror {ca.name}: {n} verts copied")
                total += n
        if mode0 != 'OBJECT':
            bpy.ops.object.mode_set(mode=mode0)
        self.report({'INFO'}, f"mirrored {total} vertex colors")
        return {'FINISHED'}


class VIEW3D_OT_vcp_view(bpy.types.Operator):
    """Toggle the viewport between normal shading and flat vertex-color display"""
    bl_idname = "view3d.vcp_view"
    bl_label = "Toggle Color View"

    def execute(self, context):
        sh = context.space_data.shading
        if sh.color_type == 'VERTEX':
            sh.color_type = 'MATERIAL'
            sh.light = 'STUDIO'
        else:
            sh.type = 'SOLID'
            sh.color_type = 'VERTEX'
            sh.light = 'FLAT'
        return {'FINISHED'}


class VIEW3D_PT_vcp(bpy.types.Panel):
    bl_label = "Vertex Color"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Vertex Color"
    bl_context = "mesh_edit"

    def draw(self, context):
        lay = self.layout
        me = context.object.data
        ca = me.color_attributes.active_color
        lay.label(text=f"attribute: {ca.name if ca else 'NONE'}")
        lay.prop_search(me.color_attributes, "active_color_name", me, "color_attributes", text="")
        lay.prop(context.scene, "vcp_color", text="")
        row = lay.row(align=True)
        row.operator("mesh.vcp_sample", text="Sample")
        row.operator("mesh.vcp_apply", text="Apply")
        lay.operator("view3d.vcp_view", text="Toggle Color View")
        lay.operator("mesh.vcp_mirror", text="Mirror _V_ Colors X")
        col = context.scene.vcp_color
        lay.label(text=f"r {col[0]:.3f}  g {col[1]:.3f}  b {col[2]:.3f}")


classes = (MESH_OT_vcp_apply, MESH_OT_vcp_sample, MESH_OT_vcp_mirror,
           VIEW3D_OT_vcp_view, VIEW3D_PT_vcp)


def register():
    bpy.types.Scene.vcp_color = bpy.props.FloatVectorProperty(
        name="Vertex Color", subtype='COLOR', size=3,
        min=0.0, max=1.0, default=(0.5, 0.5, 1.0))
    for c in classes:
        bpy.utils.register_class(c)


def unregister():
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
    del bpy.types.Scene.vcp_color


if __name__ == "__main__":
    register()
