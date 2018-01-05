
class Surface {
    override String to_string(C);
    override void free(C);
    int divisible(int n, int d);
    uint framebuffer(C);
    C cache_fetch(C, int w, int h, enum SurfaceType type);
    void cache_update(C);
    C alloc(C, int w, int h, int stride, enum SurfaceType type);
    void texture_clamp(C, bool);
    C resample(C, int w, int h, bool flip);
    uint8 * image_from_file(char *filename, int *w, int *h, int *c, int *stride);
    C image_with_bytes(C, uint8 *bytes, int length, bool store);
    bool write(C, char *filename);
    bool write_callback(C, void(*callback_func)(void *, void *, int), void *context);
    int channels(C);
    int stride(C);
    void update_rgba(C, int w, int h, RGBA *rgba, int stride, bool store);
    void yuvp(C, int w, int h, uint8 *y_plane, int y_stride,
        uint8 *u_plane, int u_stride, uint8 *v_plane, int v_stride);
    void update_yuvp(C, int w, int h,
        uint8 *y_plane, int y_stride, uint8 *u_plane, int u_stride, uint8 *v_plane, int v_stride);
    C create_yuvp(Gfx *gfx, int w, int h,
        uint8 *y_plane, int y_stride, uint8 *u_plane, int u_stride, uint8 *v_plane, int v_stride);
    GLenum gl_type(C);
    void readable(C, bool readable);
    int read(C);
    bool resize(C, int w, int h);
    void linear(C, bool linear);
    C new_gray(C, int w, int h, uint8 *bytes, int stride, bool store);
    C new_rgba(C, int w, int h, RGBA *bytes, int stride, bool store);
    C new_rgba_empty(C, int w, int h);
    void dest(C);
    void src(C, float x, float y);
    private Gfx gfx;
    private uint tx;
    private int w;
    private int h;
    private bool image;
    private uint fb;
    private uint ppo;
    private int ppo_len;
    private uint8 * bytes;
    private int stride;
    private enum SurfaceType type;
    private long cached_at;
}

class Clip {
	private Surface surface;
	private Surface last_surface;
	private GfxRect rect;
	private float2 scale;
	private float2 offset;
	private float u[4];
}
