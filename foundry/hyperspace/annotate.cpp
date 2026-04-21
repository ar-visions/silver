#include <ux/app.hpp>
#include <math/math.hpp>
#include <media/video.hpp>
#include <media/camera.hpp>

using namespace ion;

/// the head visor model is pretty basic to describe:
struct Head {
    float     width    =   0.15f; /// width  in meters -- ear canal to ear canal
    float     height   =   0.15f; /// height in meters
    float     depth    =   0.15f; /// depth  in meters -- can keep this locked to the width of the face
    float     eye_x    =   0.22f; /// distance scale between eyes, relative to width; should range from 0.35 to 0.62 (definitely!)
    float     eye_y    =  -0.10f; /// eye_y ratio, on orientation of head on the front plane; -11% height from median; should range from -30% to -5%
    float     eye_z    =   0.00f; /// z offset from frontal plane in units of face width
    float     eye_w    =   0.20f; /// width of eye segment from edge to edge
    float     ear_x    =   0.00f; /// ear_x offset from centroid of side (ear should be in the middle of the head on side plane)
    float     ear_y    =   0.00f; /// ear_y position (same relative ratio from head plane center on y; this is on the side, not front
    float     nose_y   =   0.00f; /// nose relative from median y of head; no sense of x offset here as if anything
    float     nose_z   =   0.15f; /// nose tip z position; in face width scale 
    vec3f pos      = { 0.0f, 0.0f, 0.5f }; /// center of head in xyz cartesian coords
    glm::quat orient   = { 1.0f, 0.0f, 0.0f, 0.0f }; /// rotation stored in quaternion form
    map   tags;

    properties meta() {
        return {
            {"width",     width},
            {"height",    height},
            {"depth",     depth},
            {"eye_x",     eye_x},
            {"eye_y",     eye_y},
            {"eye_z",     eye_z},
            {"eye_w",     eye_w},
            {"ear_x",     ear_x},
            {"ear_y",     ear_y},
            {"nose_y",    nose_y},
            {"nose_z",    nose_z},
            {"pos",       pos},
            {"orient",    orient},
            {"tags",      tags}
        };
    }
};


/// should be self contained and contain all methods for this feature
struct Cursor {
    /// the types we support; a Cursor type of none or !none may change the user interface.
    enums(Type, none, none, rubiks, cubidi);

    /// set centroid and scale on the coordinates, the active range we use;
    /// the model domain should be expanded by 20% when we infer this;
    /// then we can scale it 80% after

    /// it may be a bit useful to make the constraint a bit of a radial scale;
    /// on a cartesian coords as given by model

};

/// JFM
// feature Cursor;
// app     Cursor;

/// buttons inside here
struct Navigator:Element {
    enums(Nav, annotate,
        annotate, record, cursor_config); /// cursor selection (if any) 

    struct props {
        Array<Nav> buttons;

        properties meta() {
            return {
                prop { "buttons", buttons  }
            };
        }
    };

    component(Navigator, Element, props);

    void on_select(event e) {
        printf("selected\n");
    }

    node update() {
        return node::each<Nav>(state->buttons, [&](Nav &button_type) -> node {
            symbol s_type = button_type.symbol();
            return Button {
                { "id",         s_type },
                { "group",      "tab" },
                { "behavior",   Button::Behavior::radio },
                { "on-select",  callback(this, &Navigator::on_select) }
            };
        });
    }
};

struct Seekbar:Element {
    struct props {
        rgbad shadow_color          = rgbad {0,0,0,0.5};
        rgbad frame_color           = rgbad {1,1,1,0.1};
        rgbad frame_second_color    = rgbad {1,1,1,0.3};
        rgbad timeline_seek_color   = rgbad {1,1,1,1.0};
        rgbad timeline_border_color = rgbad {1,1,1,0.3};
        i64   frame_hover           = -1;
        properties meta() {
            return {
                {"shadow-color",            shadow_color},
                {"frame-color",             frame_color},
                {"frame-second-color",      frame_second_color},
                {"timeline-border-color",   timeline_border_color},
                {"timeline-seek-color",     timeline_seek_color},
            };
        }
    };
    component(Seekbar, Element, props);

    void   on_play_pause(event e);
    node   update();
    i64    frame_at_cursor(vec2d cursor);
    double offset_from_frame(int frame_id);
    void   down();
    void   move();
    void   up();
    void   draw(Canvas &canvas);
};

struct VideoViewer:Element {
    struct props {
        float       angle;
        float       z_near, z_far;
        callback    clicked;
        vec2d       last_xy;
        bool        swirl;
        vec2f   sz;
        m44f        model;
        m44f        view;
        m44f        proj;
        vec3f   start_cursor;
        vec3f   start_pos;
        glm::quat   start_orient;
        float       scroll_scale = 0.005f;

        properties meta() {
            return {
                prop { "clicked", clicked }
            };
        }
    };

    component(VideoViewer, Element, props);

    void down() {
        Head *head = context<Head>("head");
        state->last_xy        = Element::data->cursor;
        state->start_cursor   = vec3f(Element::data->cursor.x, Element::data->cursor.y, 0.0);
        state->start_pos      = head->pos;
        state->start_orient   = head->orient;

        // Convert to NDC
        vec2f ndc;
        ndc.x =        (2.0f * state->last_xy.x) / state->sz.x - 1.0f;
        ndc.y = 1.0f - (2.0f * state->last_xy.y) / state->sz.y;

        vec4f rayClip = vec4f(ndc.x, ndc.y, -1.0f, 1.0f);
        vec4f rayEye  = glm::inverse(state->proj) * rayClip;
        rayEye = vec4f(rayEye.x, rayEye.y, -1.0f, 0.0f);

        vec3f rayWor = glm::normalize(vec3f(glm::inverse(state->view) * rayEye));

        double dist = glm::distance(
            vec3f(rayWor.x, rayWor.y, 0.0),
            vec3f(head->pos.x, head->pos.y, 0.0)
        );
        
        // A simple way to check if the click is outside the cube
        state->swirl = dist > head->width * 1.0;
    }

    vec3f forward() {
        m44f      &v = state->view;
        return -glm::normalize(vec3f(v[0][2], v[1][2], v[2][2]));
    }

    vec3f to_world(float x, float y, float reference_z, const m44f      &viewMatrix, const m44f      &projectionMatrix, float screenWidth, float screenHeight) {
        // Convert to normalized device coordinates
        float xNDC = (2.0f * x) / screenWidth - 1.0f;
        float yNDC = 1.0f - (2.0f * y) / screenHeight;
        float zNDC = 2.0f * reference_z - 1.0f; // Convert the reference_z to NDC

        vec4f clipSpacePos = vec4f(xNDC, yNDC, zNDC, 1.0f);

        // Convert from clip space to eye space
        vec4f eyeSpacePos = glm::inverse(projectionMatrix) * clipSpacePos;

        // Convert from eye space to world space
        vec4f worldSpacePos = glm::inverse(viewMatrix) * eyeSpacePos;

        return vec3f(worldSpacePos) / worldSpacePos.w;
    }

    void scroll(real x, real y) {
        Head *head = context<Head>("head");
        head->pos.z += y * state->scroll_scale;
    }

    /// mouse move event
    void move() {
        Head *head = context<Head>("head");
        if (!Element::data->active)
            return;
        
        vec2d diff = Element::data->cursor - state->last_xy;
        state->last_xy = Element::data->cursor;

        const float sensitivity = 0.2f; // Sensitivity factor

        // Convert pixel difference to angles (in radians)
        float ax = glm::radians(diff.y * sensitivity); // Vertical   movement for X-axis rotation
        float ay = glm::radians(diff.x * sensitivity); // Horizontal movement for Y-axis rotation

        auto cd = node::data;
        vec3f drag_pos = vec3f(Element::data->cursor.x, Element::data->cursor.y, 0.0f);
        vec3f drag_vec = state->start_cursor - drag_pos;
        drag_vec.y = -drag_vec.y;

        if (cd->composer->shift) {
            float z  = head->pos.z;
            float zv = 1.0f - (head->pos.z - state->z_near) / (state->z_far - state->z_near);

            vec3f cursor    = vec3f(Element::data->cursor.x, Element::data->cursor.y, 0.0f);
            vec3f p0        = to_world(state->start_cursor.x, state->start_cursor.y, zv, state->view, state->proj, state->sz.x, state->sz.y);
            vec3f p1        = to_world(cursor.x, cursor.y, zv, state->view, state->proj, state->sz.x, state->sz.y);
            
            vec3f pd = p1 - p0;
            printf("pd = %.2f %.2f %.2f\n", pd.x, pd.y, pd.z);
            printf("cursor = %.2f %.2f %.2f\n", cursor.x, cursor.y, cursor.z);

            head->pos     = state->start_pos + (p1 - p0);
            head->pos.z   = z;
        } else {
            if (state->swirl) {
                head->orient = head->orient * glm::angleAxis(-ax, vec3f(0.0f, 0.0f, 1.0f));
            } else {
                // Calculate the rotation axis and angle from the mouse drag
                vec3f view_dir = forward();
                vec3f r_axis   = glm::normalize(glm::cross(drag_vec, view_dir));
                float     r_amount = glm::length(drag_vec) / 100.0f; // Adjust sensitivity
                head->orient = state->start_orient * glm::angleAxis(r_amount, r_axis);
            }
        }
    }

    void draw(Canvas& canvas);
};

/// its a button and it controls the main menu ops
struct MainMenu:Element {
    struct props {
        bool sample;
    };

    component(MainMenu, Element, props);

    void on_click(event e) {
        printf("main menu\n");
    }

    node update() {
        return Button {
            { "id",         "main-menu" },
            { "on-click",    callback(this, &MainMenu::on_click) }
        };
    }
};

/// this should perform operation with alterations to tags, and subsequent style change
/// need syntax for remaining units in coord, and it can be a % of that; i suppose % can always do this?
/// insight: this is probably preferred since it can be reduced to be the same function it was with some heuristics
/// 

/// will be controlled in css; it holds onto 
struct Page:Element {
    struct props {
        bool selected;
        properties meta() {
            return {
                {"selected", selected}
            };
        }
    };
    component(Page, Element, props);
};

struct Ribbon:Element {
    struct props {
        map<Element> content; // headers need only an 'id'-header, their selected/unselected state tag, content would have 'id'-content, selected/unselected state
        str          selected; // we set this, its not exposed
        callback     header_click;
        str          first_id;

        properties meta() {
            return {
                {"content",  content},
                {"selected", selected},
                {"header-click", header_click}
            };
        }
    };

    void select(str id) {
        printf("selected %s\n", id.cs());
        state->selected = id; /// event->target->select(id);
    }

    component(Ribbon, Element, props);

    void mounted() {
        for (auto &f: state->content) {
            state->selected = f.key.hold();
            break;
        }
    }

    node update() {
        node *n_first = node::data->mounts->count(state->first_id) ? node::data->mounts[state->first_id] : null;
        num   len = state->content.len(), header_h = 0, total_h = Element::data->bounds.h;
        if (n_first)
            header_h = (*(Element*)n_first)->bounds.h;

        state->first_id = null; /// prevent issue if this is removed
        
        node res = node::each<str, Element>(state->content, [&](str &id, Element &e) -> node {
            str  header_id = fmt {"{0}-header",  {id}};
            str content_id = fmt {"{0}-content", {id}};
            bool  selected = id == state->selected;
            if (!state->first_id) {
                state->first_id = header_id;
            }
            ///
            return Array<node> {
                Button {
                    { "id",         header_id }, /// css can do the rest
                    { "behavior",   Button::Behavior::radio },
                    { "content",    header_id },
                    { "selected",   selected },
                    { "on-change",  callback([&, id](event e) {
                        // call update
                        printf("id = %s\n", id.cs());
                        state->selected = id;
                        event ev { this };
                        if (state->header_click)
                            state->header_click(ev);
                    })}
                },
                Page {
                    { "id",        content_id },
                    { "selected",  selected }
                }
            };
        });
        return res;
    }
};

struct Content:Element {
    struct props {
        int sample;
        properties meta() {
            return {
            };
        }
    };
    component(Content, Element, props);
    node update();
};

struct Profile:Element {
    struct props {
        int sample;
    };

    component(Profile, Element, props);

    /// how do we get notified when the subject has changed?
    /// we need to be able to annotate multiples, perhaps.. gen1 shouldnt have it, though
    void mounted() {
    }
    
    void draw(Canvas& canvas) {
        /// lookup profile and paint it on top of a cm-grid pattern
        ion::rect &bounds = Element::data->bounds;
        Element::draw(canvas);
        ion::rect area = { 0, 0, bounds.w, bounds.h };
        canvas.color((cstr)"#f00");
        canvas.fill(area);
    }
};

struct Annotate:Element {
    struct props {
        Head        head;
        image       current_image;
        bool        live = false;
        MStream     cam;
        Video       video;
        int         frame_id; /// needs to inter-operate with pts

        properties meta() {
            return {
                prop { "head",  head },
                //prop { "video", video } /// we share video and our head model with components; using context we dont need to pass them around, store them separate.  its better this way
            };
        }
    };

    component(Annotate, Element, props);
        
    void mounted() {
        if (state->live) {
            state->cam = camera(
                { StreamType::Audio,
                  StreamType::Video,
                  StreamType::Image }, /// ::Image resolves the Image from the encoded Video data
                { Media::PCM, Media::PCMf32, Media::YUY2, Media::NV12, Media::MJPEG },
                "Logi", "PnP", 640, 360
            );
            state->cam.listen({ this, &Annotate::on_frame });
            state->video = Video(640, 360, 30, 48000, "test.mp4");
        } else {
            state->video = Video(ion::path("sample.mp4"));
            state->video.play_state(true);
        }
    }

    void on_frame(Frame &frame) {
        //state->current_image = frame.image;
        if (state->video) {
            if (state->frame_id < 30 * 10) {
                state->frame_id++;
                state->video.write_frame(frame);
                if (state->frame_id == 30 * 10) {
                    state->video.stop();
                    state->cam.cancel();
                }
            }
        }
    }

    node update() {
        state->current_image = state->video.get_current_image();
        return Array<node> {
            MainMenu {
                { "id", "main-menu" }
            },
            Navigator {
                { "id", "navigator" },
                { "buttons", Array<Navigator::Nav> {
                    Navigator::Nav("annotate"),
                    Navigator::Nav("record"),
                    Navigator::Nav("cursor-config") } }
            },
            Content {
                { "id", "content" }
            },
            Ribbon {
                { "id", "ribbon" },
                {  "content", map <Element> {
                    /// middle of forehead, down the nose (tip used for), to the top of top lip
                    {"side-profile",    Profile {{"id", "side-profile"}}},
                    /// shape of bottom of nose, where it starts from head, to tip
                    /// it must be half face-based, and must mirror
                    /// it may also extend to ear
                    {"forward-profile", Profile {{"id", "forward-profile"}}} /// will fetch from Annotation model through Context
                } }
            }
        };
    }
};

node Content::update() {
    Annotate *a = grab<Annotate>();
    return Array<node> {
        VideoViewer {
            { "id", "video-viewer" }
        },
        Seekbar {
            {"id", "seekbar" }
        }
    };
}

void Seekbar::on_play_pause(event e) {
    Annotate *a = grab<Annotate>();
    Button   *b = (Button*)e->target;
    a->state->video.play_state(bool(b->node::data->value));
    printf("on_play_pause\n");
}

node Seekbar::update() {
    Annotate *a = grab<Annotate>();
    return Button {
        { "id",       "play-pause" },
        { "behavior",  Button::Behavior(Button::Behavior::toggle) },
        { "value",     bool(a->state->video.get_play_state()) },
        { "on-change", callback(this, &Seekbar::on_play_pause) }
    };
}

i64 Seekbar::frame_at_cursor(vec2d cursor) {
    auto   a               = grab<Annotate>();
    i64    frame           = a->state->video.current_frame(); // we need the currently seeked frame to know the shift of the spectrum
    i64    frame_count     = a->state->video.frame_count();
    image  spec            = a->state->video.audio_spectrum();
    double s_width         = double(spec.width());
    double x_offset        = 0;
    i64    frame_at_origin = 0;
    i64    frames_in_view  = 0;

    if (s_width < data->bounds.w) {
        frames_in_view  = frame_count;
    } else {
        x_offset        = offset_from_frame(frame);
        frame_at_origin = -x_offset / s_width * frame_count;
        frames_in_view  = data->bounds.w / s_width * frame_count;
    }
    double f_cursor     = cursor.x / data->bounds.w;
    return frame_at_origin + frames_in_view * f_cursor;
}

void Seekbar::down() {
    i64 frame = frame_at_cursor(data->cursor);
    auto   a  = grab<Annotate>();
    a->state->video.seek_frame(frame);
}

void Seekbar::move() {
    state->frame_hover = frame_at_cursor(data->cursor);
    printf("frame_hover = %f\n", state->frame_hover / 30.0);
}

void Seekbar::up() {
    printf("up\n");
}

double Seekbar::offset_from_frame(int frame_id) {
    /// frame_id 0   = 0
    /// frame_id max = -spec.width() + bounds.w
    Annotate *a = grab<Annotate>();
    image  spec = a->state->video.audio_spectrum();
    double w    = spec.width();
    double from = 0;
    double to   = -w + data->bounds.w;
    
    i64 frame_count = a->state->video.frame_count();
    double fscale = double(frame_id) / (frame_count - 1);
    return from * (1.0 - fscale) + to * fscale;
}

void Seekbar::draw(Canvas &canvas) {
    auto   a          = grab<Annotate>();
    i64    frame      = a->state->video.current_frame();
    i64    frame_count = a->state->video.frame_count();
    image  spec       = a->state->video.audio_spectrum();
    int    hz         = a->state->video.frame_rate();
    double s_width    = double(spec.width());
    double timeline_h = 16;
    bool   stretch    = false;
    ion::rect  bounds { 0, 0, double(spec.width()), data->bounds.h - timeline_h };
  //ion::font font { 10 };

    if (bounds.w < data->bounds.w) {
        bounds.w = data->bounds.w;
        stretch  = true;
    }

    double x_offset = stretch ? 0.0 : offset_from_frame(frame); /// a function of the seek position

    /// draw ruler
    ion::rect border_ruler = { 0, timeline_h - 1, data->bounds.w, 1 };
    canvas.color(state->timeline_border_color);
    canvas.fill(border_ruler);
    border_ruler.y = 0;
    canvas.fill(border_ruler);
    canvas.font(data->font); /// can do this prior to draw

    for (int f = 1; f < frame_count; f++) {
        int h = 3;
        rgbad c = state->frame_color;
        if (f % hz == 0) {
            h = 6; // draw label
            c = state->frame_second_color;
            double tw = 16;
            str label = fmt {"{0}s", {int(f / hz)}};
            ion::rect textr = {
                x_offset + double(f) / frame_count * bounds.w - tw / 2, double(1 + h + 1),
                tw, double(timeline_h - (1 + h + 1) - 1) };
            canvas.color(rgbad { 0.5, 0.8, 1.0, 0.6 });
            canvas.text(label, textr, { 0.5, 0.5 }, { 0, 0 }, false, null);
        }
        canvas.color(c);
        ion::rect tk = { x_offset + double(f) / frame_count * bounds.w, 1, 1, double(h) };
        canvas.fill(tk);
    }

    /// draw spectrograph
    canvas.image(spec, bounds, alignment(), vec2d { x_offset, timeline_h }, false); /// change: default alignment does not perform bounds scaling

    for (int i = 0; i < 2; i++) {
        if (i == 0 && !Element::data->hover)
            continue;

        float f;
        if (i == 0) {
            /// we want to know where the hover lands in the viewable window.  it may not be visible where its < 0 or > 1
            i64    frame_at_origin = 0;
            i64    frames_in_view  = 0;
            if (s_width < data->bounds.w) {
                frames_in_view  = frame_count;
            } else {
                frame_at_origin = -x_offset / s_width * frame_count;
                frames_in_view  = data->bounds.w / s_width * frame_count;
            }
            state->frame_hover = frame_at_cursor(data->cursor);
            f = double(state->frame_hover - frame_at_origin) / frames_in_view;
        } else {
            f = frame / double(frame_count);
        }

        if (f < 0 || f > 1) continue;

        /// we're centered on an interpolation of 0...1 to 0...bounds
        /// cant use the same factor on the hover frame
        ion::rect  r = { f * (data->bounds.w - 1), 0, 3, data->bounds.h };

        canvas.opacity(i == 0 ? 0.3 : 1.0);
        canvas.color(state->shadow_color);
        canvas.fill(r);
        r.x += 1;
        r.w  = 1;

        graphics::shape arrow;
        arrow.move(vec2d { 0.5 + r.x - 5, 0 });
        arrow.line(vec2d { 0.5 + r.x + 5, 0 });
        arrow.line(vec2d { 0.5 + r.x + 0, 7 });

        canvas.color(state->timeline_seek_color);
        canvas.fill(arrow);
        canvas.fill(r);
    }

    Element::draw(canvas);
}

void VideoViewer::draw(Canvas& canvas) {
    /// the base method calculates all of the rectangular regions; its done in draw because canvas context is needed for measurement
    Element::draw(canvas);

    Head *head = context<Head>("head");
    float w = head->width  / 2.0f;
    float h = head->height / 2.0f;
    float d = head->depth  / 2.0f;

    // test code:
    //glm::quat additional_rotation = glm::angleAxis(radians(1.0f) / 10.0f, vec3f(0.0f, 1.0f, 0.0f));
    //head->orient = head->orient * additional_rotation;

    Array<vec3f> face_box = {
        vec3f(-w, -h, -d), vec3f( w, -h, -d), // EF
        vec3f( w, -h, -d), vec3f( w,  h, -d), // FG
        vec3f( w,  h, -d), vec3f(-w,  h, -d), // GH
        vec3f(-w,  h, -d), vec3f(-w, -h, -d)  // HE
    };

    vec3f eye = vec3f(0.0f, 0.0f, 0.0f);
    
    state->z_near = 0.0575f / 2.0f * sin(radians(45.0f));
    state->z_far  = 10.0f;

    double cw = Element::data->bounds.w;
    double ch = Element::data->bounds.h;
    vec2f sz    = { cw, ch };
    m44f      proj  = glm::perspective(glm::radians(70.0f), sz.x / sz.y, state->z_near, state->z_far);
    proj[1][1] *= -1;

    state->sz = sz;

    m44f      view  = glm::lookAt(eye, vec3f(0.0f, 0.0f, 1.0f), vec3f(0.0f, 1.0f, 0.0f));
    m44f      model = glm::translate(m44f(1.0f), head->pos) * glm::toMat4(head->orient); // glm::rotate(m44f(1.0f), angle, vec3f(0.0f, 1.0f, 0.0f));

    static rgbad white = { 1.0, 1.0, 1.0, 1.0 };
    static rgbad red   = { 1.0, 0.0, 0.0, 1.0 };
    static rgbad green = { 0.0, 1.0, 0.0, 1.0 };
    static rgbad blue  = { 0.0, 0.0, 1.0, 1.0 };

    canvas.save();

    ion::rect     bounds { 0.0, 0.0, sz.x, sz.y };
    vec2d     offset { 0.0, 0.0 };
    alignment align  { 0.5, 0.5 };

    canvas.color(Element::data->drawings[operation::fill].color);
    canvas.fill(bounds);

    Annotate *a = grab<Annotate>();
    if (a->state->current_image) {
        canvas.image(a->state->current_image, bounds, align, offset);  
    }

    canvas.projection(model, view, proj);
    canvas.outline_sz(2);
    for (size_t i = 0; i < 4; i++)
        canvas.line(face_box[i * 2 + 0], face_box[i * 2 + 1]);
    
    state->model = model;
    state->view  = view;
    state->proj  = proj;
    
    /// draw eyes
    float fw    = head->width;
    float fh    = head->height;
    float eye_w = fw * head->eye_w;
    float eye_x = fw * head->eye_x;
    float eye_y = fw * head->eye_y;
    float eye_z = fw * head->eye_z; /// frontal plane is the eye plane as annotated; useful to have a z offset
    
    float nose_x = 0.0f;
    float nose_y = fh * head->nose_y;
    float nose_z = fw * head->nose_z;
    float nose_h = fh * 0.02f;

    float ear_x  = fw * head->ear_x;
    float ear_y  = fh * head->ear_y;
    float ear_h  = fh * 0.02f; /// should be a circle or a square, not a line segment
    canvas.outline_sz(1);

    /// we want to replace this with a silohette on 2 axis
    /// thats far easier to scale and line up
    /// its not a chore to manage these points with a profile view.
    /// its literally a profile that we measure with the model, associated to the subject in annotations
    /// the idea of making planes is not good
    /// top part: middle of forehead (not visible with hair, but measurable by human; basically lower middle of ballcap or something)
    /// bottom: top of the upper lip is probably good
    /// ability to copy and paste profiles is a good feature, from file to file

    Array<vec3f> features = {
        vec3f(-eye_x - eye_w / 2, eye_y, -d + eye_z),
        vec3f(-eye_x + eye_w / 2, eye_y, -d + eye_z),

        vec3f( eye_x - eye_w / 2, eye_y, -d + eye_z),
        vec3f( eye_x + eye_w / 2, eye_y, -d + eye_z),

        vec3f( nose_x, nose_y,          -d - nose_z),
        vec3f( nose_x, nose_y + nose_h, -d - nose_z),

        vec3f(-w, ear_y,         d * ear_x),
        vec3f(-w, ear_y + ear_h, d * ear_x), /// just for noticable length

        vec3f(+w, ear_y,         d * ear_x),
        vec3f(+w, ear_y + ear_h, d * ear_x) /// just for noticable length
    };

    for (size_t i = 0; i < 10; i += 2)
        canvas.line(features[i + 0], features[i + 1]);
    
    canvas.restore();
}

int main(int argc, char *argv[]) {
    map defs  {{ "debug", uri { null }}};
    map config { args::parse(argc, argv, defs) };
    if    (!config) return args::defaults(defs);
    ///
    return App(config, [](App &app) -> node {
        return Annotate {
            { "id", "main" }
        };
    });
}
