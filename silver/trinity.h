#ifndef TRINITY_H
#define TRINITY_H

#include <A>

#define MAX_VERTEX_SIZE 64

// Enums
#define ClearFlags_meta(X,Y) \
    enum_value(X,Y, Undefined) \
    enum_value(X,Y, Color) \
    enum_value(X,Y, Depth) \
    enum_value(X,Y, Stencil)
declare_enum(ClearFlags)

#define Key_meta(X,Y) \
    enum_value(X,Y, Undefined) \
    enum_value(X,Y, Space) \
    // ... (other key definitions)
declare_enum(Key)

#define PolygonMode_meta(X,Y) \
    enum_value(X,Y, undefined) \
    enum_value(X,Y, tri) \
    enum_value(X,Y, quad) \
    enum_value(X,Y, wire) \
    enum_value(X,Y, mixed) \
    enum_value(X,Y, ngon)
declare_enum(PolygonMode)

#define AssetType_meta(X,Y) \
    enum_value(X,Y, undefined) \
    enum_value(X,Y, color) \
    enum_value(X,Y, normal) \
    enum_value(X,Y, material) \
    enum_value(X,Y, reflect) \
    enum_value(X,Y, env) \
    enum_value(X,Y, attachment) \
    enum_value(X,Y, depth_stencil) \
    enum_value(X,Y, multisample)
declare_enum(AssetType)

#define Sampling_meta(X,Y) \
    enum_value(X,Y, undefined) \
    enum_value(X,Y, nearest) \
    enum_value(X,Y, linear) \
    enum_value(X,Y, aniso)
declare_enum(Sampling)

// Structs
#define HumanVertex_meta(X,Y,Z) \
    i_public(X,Y,Z, vec3f, pos) \
    i_public(X,Y,Z, vec3f, normal) \
    i_public(X,Y,Z, vec2f, uv0) \
    i_public(X,Y,Z, vec2f, uv1) \
    i_public(X,Y,Z, vec4f, tangent) \
    i_public(X,Y,Z, f32,   joints0[4]) \
    i_public(X,Y,Z, f32,   joints1[4]) \
    i_public(X,Y,Z, f32,   weights0[4]) \
    i_public(X,Y,Z, f32,   weights1[4])
declare_class(HumanVertex)

// Forward declarations
declare_class(Mesh)
declare_class(Texture)
declare_class(Device)
declare_class(Pipeline)
declare_class(Model)

#define ShaderModule_meta(X,Y) \
    enum_value(X,Y, undefined) \
    enum_value(X,Y, vertex) \
    enum_value(X,Y, fragment) \
    enum_value(X,Y, compute)
declare_enum(ShaderModule)

typedef void (*GraphicsGen)(Mesh* mesh, array images);

#define GraphicsData_meta(X,Y,Z) \
    i_public(X,Y,Z, string,      name) \
    i_public(X,Y,Z, string,      shader) \
    i_public(X,Y,Z, AType,       vtype) \
    i_public(X,Y,Z, GraphicsGen, gen) \
    i_public(X,Y,Z, array,       bindings)
declare_class(GraphicsData)

declare_class(Graphics)

#define Group_meta(X,Y,Z) \
    i_public(X,Y,Z, A,        parent) \
    i_public(X,Y,Z, Mesh,     mesh) \
    i_public(X,Y,Z, gltf_Joints, joints) \
    i_public(X,Y,Z, array,    children) \
    i_public(X,Y,Z, Pipeline, pipeline)
declare_class(Group)

declare_class(Object)

typedef array Bones;

// Function declarations
#define Device_meta(X,Y,Z) \
    i_method(X,Y,Z, handle,  get_handle) \
    i_method(X,Y,Z, none,    get_dpi, f32*, f32*) \
    i_method(X,Y,Z, Texture, create_texture, vec2i, AssetType)
declare_class(Device)

#define Texture_meta(X,Y,Z) \
    s_method(X,Y,Z, Texture, load, Device, symbol, AssetType) \
    i_method(X,Y,Z, none,    set_content, A) \
    i_method(X,Y,Z, none,    resize, vec2i) \
    i_method(X,Y,Z, vec2i,   size) \
    i_method(X,Y,Z, handle,  get_handle)
declare_class(Texture)

declare_class(Pipeline)

#define Model_meta(X,Y,Z) \
    i_construct(X,Y,Z, Device, symbol, array) \
    i_method(X,Y,Z, Group,    load_group, string) \
    i_method(X,Y,Z, Pipeline, get_pipeline, string) \
    i_method(X,Y,Z, Object,   instance)
declare_class(Model)

#define Window_meta(X,Y,Z) \
    s_method(X,Y,Z, Window, create, string, vec2i) \
    i_method(X,Y,Z, none,   set_visibility, bool) \
    i_method(X,Y,Z, none,   close) \
    i_method(X,Y,Z, string, title) \
    i_method(X,Y,Z, none,   set_title, string) \
    i_method(X,Y,Z, none,   run) \
    i_method(X,Y,Z, handle, get_handle) \
    i_method(X,Y,Z, bool,   process) \
    i_method(X,Y,Z, handle, get_user_data) \
    i_method(X,Y,Z, none,   set_user_data, handle) \
    i_method(X,Y,Z, vec2i,  size) \
    i_method(X,Y,Z, f32,    aspect) \
    i_method(X,Y,Z, Device, device)
declare_class(Window)

#endif // TRINITY_H