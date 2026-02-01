#include <import>

vector vector_with_path(vector a, path file_path);

void Buffer_init(Buffer b) {
    vector    data = vector_with_path(new(vector), b->uri);
    b->data = data;
}

Node Model_find(Model a, cstr name) {
    each (a->nodes, Node, node)
        if (cmp(node->name, name) == 0)
            return node;
    return (Node)null;
}

Node Model_parent(Model a, Node n) {
    num node_index = index_of(a->nodes, n);
    verify(node_index >= 0, "invalid node memory");
    each (a->nodes->origin, Node, node) {
        for (num i = 0, ln = len(node->children); i < ln; i++) {
            i64 id = 0;//get(node->children, i);
            if (node_index == id)
                return node;
        }
    }
    return (Node)null;
}

int Model_index_of(Model a, cstr name) {
    int index = 0;
    each (a->nodes, Node, node) {
        if (cmp(node->name, name) == 0)
            return index;
        index++;
    }
    return -1;
}

Node Model_index_string(Model a, string name) {
    return find(a, cstring(name));
}

/// builds Transform, separate from Model/Skin/Node and contains usable glm types
Transform Model_node_transform(Model a, JData joints, mat4f parent_mat, int node_index, Transform parent) {
    Node node = a->nodes->origin[node_index];

    node->processed = true;
    Transform transform;
    if (node->joint_index >= 0) {
        transform->jdata     = joints;

        mat4f i = mat4f((floats)null);
        vec3f v = vec3f(1, 1, 1);
        mat4f rotation_m4 = mat4f(&node->rotation); // calls the quaternion constructor on our struct
        /// vectors have no special field on the end so we have to invoke their entire function names (not using macro or function table; people like this better)
        transform->local = mat4f((floats)null);
        transform->local = mat4f_translate(&transform->local, &node->translation);
        transform->local = mat4f_mul      (&transform->local, &rotation_m4);
        transform->local = mat4f_scale    (&transform->local, &node->scale);
        transform->local_default = transform->local;
        transform->iparent   = parent->istate;
        transform->istate    = node->joint_index;
        mat4f state_mat = mat4f_mul(&parent_mat, &transform->local_default);
        mat4f* jstates = data(joints->states);
        jstates[transform->istate] = state_mat;
        each (node->children, object, p_node_index) {
            int node_index = *(int*)p_node_index;
            /// ch is referenced from the ops below, when called here (not released)
            Transform ch = node_transform(a, joints, state_mat, node_index, transform);
            if (ch) {
                Node n = a->nodes->origin[node_index];
                verify(n->joint_index == ch->istate, "joint index mismatch");
                push(transform->ichildren, &ch->istate); /// there are cases where a referenced node is not part of the joints array; we dont add those.
            }
        }
        push(joints->transforms, transform);
    }
    return transform;
}

JData Model_joints(Model a, Node node) {
    if (node->mx_joints)
        return node->mx_joints;

    JData joints;
    mat4f ident = mat4f_ident();
    if (node->skin != -1) {
        Skin skin         = a->skins->origin[node->skin];
        map  all_children = map();
        each (skin->joints, object, p_node_index) {
            int node_index = *(int*)p_node_index;
            Node node = a->nodes->origin[node_index];
            each (node->children, object, i) {
                verify(!contains(all_children, i), "already contained");
                set(all_children, i, _bool(true));
            }
        }

        int j_index = 0;
        values (skin->joints, int, node_index)
            ((Node)a->nodes->origin[node_index])->joint_index = j_index++;

        int n = len(skin->joints);
        joints->transforms = array(n); /// we are appending, so dont set size (just verify)
        joints->states     = vector(n);
        
        for (int i = 0; i < n; i++)
            push(joints->states, &ident);
        
        /// for each root joint, resolve the local and global matrices
        values (skin->joints, int, node_index) {
            if (contains(all_children, &node_index)) // fix
                continue;
            
            node_transform(a, joints, ident, node_index, null);
        }
    } else {
        int n = len(joints->states);
        for (int i = 0; i < n; i++)
            push(joints->states, &ident);
    }

    /// adding transforms twice
    verify(len(joints->states) == len(joints->transforms), "length mismatch");
    node->mx_joints = joints;
    return joints;
}

/// Data is Always upper-case, ...sometimes lower
void Accessor_init(Accessor a) {
    a->stride      = vcount(a) * component_size(a);
    a->total_bytes = a->stride * vcount(a);
    verify(a->count, "count is required");
}

u64 Accessor_vcount(Accessor a) {
    u64 vsize = 0;
    switch (a->type) {
        case CompoundType_SCALAR: vsize = 1; break;
        case CompoundType_VEC2:   vsize = 2; break;
        case CompoundType_VEC3:   vsize = 3; break;
        case CompoundType_VEC4:   vsize = 4; break;
        case CompoundType_MAT2:   vsize = 4; break;
        case CompoundType_MAT3:   vsize = 9; break;
        case CompoundType_MAT4:   vsize = 16; break;
        default: fault("invalid CompoundType2");
    }
    return vsize;
};

u64 Accessor_component_size(Accessor a) {
    u64 scalar_sz = 0;
    switch (a->componentType) {
        case ComponentType_BYTE:           scalar_sz = sizeof(u8); break;
        case ComponentType_UNSIGNED_BYTE:  scalar_sz = sizeof(u8); break;
        case ComponentType_SHORT:          scalar_sz = sizeof(u16); break;
        case ComponentType_UNSIGNED_SHORT: scalar_sz = sizeof(u16); break;
        case ComponentType_UNSIGNED_INT:   scalar_sz = sizeof(u32); break;
        case ComponentType_FLOAT:          scalar_sz = sizeof(float); break;
        default: fault("invalid ComponentType");
    }
    return scalar_sz;
};

Au_t Accessor_member_type(Accessor a) {
    switch (a->componentType) {
        case ComponentType_BYTE:
        case ComponentType_UNSIGNED_BYTE:
            switch (a->type) {
                case CompoundType_SCALAR: return typeid(i8);
                //case CompoundType_VEC2:   return typeid(vec2i8);
                //case CompoundType_VEC3:   return typeid(vec3i8);
                case CompoundType_VEC4:   return typeid(rgba8);
                default: break;
            }
            break;
        case ComponentType_SHORT:
        case ComponentType_UNSIGNED_SHORT:
            switch (a->type) {
                case CompoundType_SCALAR: return typeid(i16);
                case CompoundType_VEC4:   return typeid(rgba16);
                default:
                    break;
            }
            break;
        case ComponentType_UNSIGNED_INT:
            switch (a->type) {
                case CompoundType_SCALAR: return typeid(i32);
                default: break;
            }
            break;
        case ComponentType_FLOAT:
            switch (a->type) {
                case CompoundType_SCALAR: return typeid(f32);
                case CompoundType_VEC2:   return typeid(vec2f);
                case CompoundType_VEC3:   return typeid(vec3f);
                case CompoundType_VEC4:   return typeid(vec4f);
                //case CompoundType_MAT2:   return typeid(mat2f); 
                //case CompoundType_MAT3:   return typeid(mat3f); 
                case CompoundType_MAT4:   return typeid(mat4f); 
                default: fault("invalid CompoundType");
            }
            break;
        default:
            break;
    }
    fault("invalid CompoundType1: %o", e_str(CompoundType, a->type));
    return 0;
};

void Transform_multiply(Transform a, mat4f m) {
    a->local = mat4f_mul(&a->local, &m);
    Transform_propagate(a);
}

void Transform_set(Transform a, mat4f m) {
    a->local = m;
    Transform_propagate(a);
}

void Transform_set_default(Transform a) {
    a->local = a->local_default;
    Transform_propagate(a);
}

void Transform_operator__assign_mul(Transform a, mat4f m) {
    multiply(a, m);
}

bool Transformer_cast_bool(Transform a) {
    return a->jdata != null;
}

void Transform_propagate(Transform a) {
    mat4f ident = mat4f_ident(); /// iparent's istate will always == iparent
    mat4f m = (a->iparent != -1) ? *(mat4f*)get(a->jdata->states, a->iparent) : ident;
    mat4f* jstates = data(a->jdata->states);
    jstates[a->istate] = mat4f_mul(&m, &a->local);
    each (a->ichildren, i64*, i)
        Transform_propagate(a->jdata->transforms->origin[*i]);
}

Primitive Node_primitive(Node a, Model mdl, cstr name) {
    Mesh m = get(mdl->meshes, a->mesh);
    return m ? primitive(m, name) : null;
}

Primitive Mesh_primitive(Mesh a, cstr name) {
    each (a->primitives, Primitive, p) {
        if (cmp(p->name, name) == 0)
            return p;
    }
    return null;
}


define_enum(ComponentType)
define_enum(CompoundType)
define_enum(TargetType)
define_enum(Mode)
define_enum(Interpolation)

define_class(Sampler,       Au)
define_class(ChannelTarget, Au)
define_class(Channel,       Au)
define_class(Animation,     Au)
define_class(SparseInfo,    Au)
define_class(Sparse,        Au)
define_class(Accessor,      Au)
define_class(BufferView,    Au)
define_class(Skin,          Au)
define_class(JData,         Au)
define_class(Transform,     Au)
define_class(Node,          Au)
define_class(Primitive,     Au)
define_class(MeshExtras,    Au)
define_class(Mesh,          Au)
define_class(Scene,         Au)
define_class(AssetDesc,     Au)
define_class(Buffer,        Au)
define_class(Model,         Au)

define_class(pbrMetallicRoughness, Au)
define_class(TextureInfo, Au)
define_class(Material, Au)
