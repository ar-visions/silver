
#include <import>
#include <math.h>

typedef struct Uniforms {
    Orbiter orbiter;
    Rays  rays;
} Uniforms;

mat4f blast_matrix() {
    mat4f m;
    for (int i = 0; i < 16; i++)
        m.m[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    return m;
}

void orbiter_frame(pipeline p, Uniforms* u) {
    vec3f   eye         = vec3f   (0.0f,  3.5f, -2.0f);
    vec3f   target      = vec3f   (0.0f,  0.0f,  0.0f);
    vec3f   up          = vec3f   (0.0f, -1.0f,  0.0f);

    static float sequence = 0;
    sequence += 0.000f;

    static float m_sequence = 0;
    m_sequence += 0.00022f;

    vec4f v              = vec4f(0.0f, 1.0f, 0.0f, radians(sequence));
    quatf q              = quatf(&v);
    Orbiter orb          = u->orbiter;
    orb->pos             = vec4f(&eye);
    orb->model           = mat4f_rotate     (&u->orbiter->model, &q);
    orb->proj            = mat4f_perspective(radians(40.0f), 1.0f, 0.1f, 100.0f);
    orb->view            = mat4f_look_at    (&eye, &target, &up);
    orb->moment          = m_sequence;
    orb->moment_angle    = 0.5f;
    orb->moment_amount   = 10.0f;
    orb->pos_radius      = vec4f(0.92, 0.0, -0.09, 0.5f);
    orb->normal_falloff  = vec4f(0.0,  1.0, 0.0, 0.8f);
    orb->color_intensity = vec4f(0.6,  0.3, 1.0, 4.0f);

    if (orb->moment_angle > M_PI) orb->moment_angle = orb->moment_angle - M_PI;
    
    /*
    vec4f v2 = vec4f(0.0f, 1.0f, 0.0f, radians(sequence));
    quatf q2 = quatf(&v2);
    u->rays->model  = mat4f_ident      ();
    u->rays->model  = mat4f_rotate     (&u->rays->model, &q2);
  //u->rays->model  = mat4f_translate  (&u->rays->model, &(vec3f){0, 0, -3});
    u->rays->view   = mat4f_look_at    (&eye, &target, &up);
    u->rays->proj   = u->pbr->proj;

    // i am overriding the mvp this way.. the above mats render orbiter just fine, and orbiter is the size and position of this plane. so, no clue!
    u->rays->model = u->pbr->model;
    u->rays->view  = u->pbr->view;
    u->rays->proj  = u->pbr->proj;

    //u->rays->model = blast_matrix();
    //u->rays->view  = mat4f_ident();
    //u->rays->proj  = mat4f_ortho(-1, 1, -1, 1, -1, 1);
    */
}
/*
void isolate(trinity t) {
    int    size     = 256;
    int    wsize    = sizeof(struct _window);
    int    bsize    = sizeof(struct _Basic);
    window w        = window(t, t, format, Pixel_rgbaf32, backbuffer, true, width, size, height, size);
    Model  data     = read   (path("models/env.gltf"), typeid(Model));
    Basic    e      = Basic  (t, t, name, string("basic"));
    image  environment = image(uri, form(path, "images/forest.exr"), surface, Surface_color);
    model  env      = model  (t, t, w, w, id, data, s, e, samplers, a(environment, null));
    array  models   = a      (env, null);
    e->proj         = mat4f_perspective (radians(90.0f), 1.0f, 0.1f, 10.0f);
    e->view         = mat4f_ident();
    e->model        = mat4f_ident();
    print("from orbiter: basic = %p, &basic->proj = %p", e, &e->proj); 

    mat4f* addr = &e->proj;

    int offset = ((char*)&e->model - (char*)e);
    process(w, models, null, null);

    image screen = cast(image, w);
    exr(screen, form(path, "screenshot.exr"));
}
*/

int main(int argc, cstr argv[]) {
    A_start(argv);
    trinity t = trinity();
 
    //isolate(t);
    Uniforms u = {};
    path    gltf        = path    ("models/orbiter2.gltf");
    //path    gltf_march  = path    ("models/ray-march.gltf"); // needs a model matrix to 
    Model   orbiter     = read    (gltf, typeid(Model));
    //Model   march       = read    (gltf_march, typeid(Model));
    
    window  w           = window  (t, t, title, string("orbiter"), width, 800, height, 800 );
    image   environment = image   (uri, form(path, "images/forest.exr"), surface, Surface_environment);

    u.orbiter = Orbiter(t, t, name, string("orbiter"));
    //u.rays  = Rays(t, t, name, string("ray-march"));

    model   agent = model(t, t, w, w, id, orbiter, s, u.orbiter, samplers, a(environment, null));
    //model   ray_march   = model   (t, t, w, w, id, march, s, u.rays);
    push(w, agent);
    //push(w, ray_march);
    return loop(w, orbiter_frame, &u);
}


