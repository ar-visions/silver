#include <import>

void test_fdeep(float* input, float* result);

symbol char_from_pixel(u8 gray) {
    if (gray < 32)  return "  ";
    if (gray < 48)  return " ░";
    if (gray < 64)  return "░░";
    if (gray < 72)  return "░▒";
    if (gray < 96)  return "▒▒";
    if (gray < 112) return "▒▓";
    if (gray < 128) return "▓▓";
    return "█";
}

tensor base_labels(map data) {
    map   annots    = get(data, string("labels"));
    array eye_left  = get(annots, string("eye-left"));
    array eye_right = get(annots, string("eye-right"));
    i64   l_x       = *(f64*)get(eye_left,  0);
    i64   l_y       = *(f64*)get(eye_left,  1);
    i64   r_x       = *(f64*)get(eye_right, 0);
    i64   r_y       = *(f64*)get(eye_right, 1);
    f64   x         = (l_x + r_x) / 2.0;
    f64   y         = (l_y + r_y) / 2.0;
    array scale     = get(annots, string("scale"));
    f64   s         = *(f64*)get(scale, 0);
    tensor labels   = tensor(shape, shape_new(3, 0));
    f32*   ldata    = labels->realized;
    ldata[0] = (f32)x;
    ldata[1] = (f32)y;
    ldata[2] = (f32)s;
    return labels;
}

// we want all of vision training in here, and that includes whatever augmenting we are doing
// entire training stack here, and not arb delegations to script
// cv runs in C++ and we may use that quite simply with A-type being a universal object model
// will be important to get a better 'dataset' prototype than what we have in python

int main(int argc, symbol args[]) {
    startup(args);

    path   f      = form(path, "models/vision_base.json");
    keras  k      = read(f, typeid(keras));
    image  input  = image("images/f6_center-offset-4248_0.7211_0.0951.png");
    image  resized = resize(input, 32, 32);
    path   a      = path("images/f6_center-offset-4248_0.7211_0.0951.json");
    map    annots = read(a, typeid(map));
    map    training = map();

    // print input image 
    i8* data = data(resized);
    int h = resized->height;
    int w = resized->width;
    for (int y = 0; y < h; y++) {
        i8* scan = &data[w * y];
        for (int x = 0; x < w; x++) {
            put("%s", char_from_pixel(scan[x]));
        }
        put("\n");
    }

    png(resized, form(path, "images/resized.png"));
    tensor tensor_resized = tensor(resized);
    set(training, tensor_resized, base_labels(annots));
    train(k, 22 * 32, training, 0.01);

    tensor output = forward(k, tensor_resized);
    f32*   out    = output->realized;
    print("keras model %o, output = %.2f, %.2f, %.2f\n", k->ident, out[0], out[1], out[2]);
    return 0;
}