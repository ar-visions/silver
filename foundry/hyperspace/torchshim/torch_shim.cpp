// extern-C shim over libtorch TorchScript inference (see torch_shim.h)
// build: ./build-shim.sh (links the pip-installed libtorch, rpath'd)
#include <torch/script.h>
#include <cstring>

extern "C" {

void* ts_load(const char* path) {
    try {
        auto* m = new torch::jit::script::Module(torch::jit::load(path));
        m->eval();
        return m;
    } catch (const std::exception& e) {
        fprintf(stderr, "ts_load: %s\n", e.what());
        return nullptr;
    }
}

static int run(torch::jit::script::Module* m,
               std::vector<torch::jit::IValue>& iv,
               float* out, int out_cap) {
    try {
        torch::NoGradGuard ng;
        auto r = m->forward(iv).toTensor().contiguous();
        int n = (int)std::min<int64_t>(r.numel(), out_cap);
        memcpy(out, r.data_ptr<float>(), n * sizeof(float));
        return n;
    } catch (const std::exception& e) {
        fprintf(stderr, "ts_forward: %s\n", e.what());
        return -1;
    }
}

int ts_forward1(void* model, const float* img, int S,
                float* out, int out_cap) {
    auto* m = (torch::jit::script::Module*)model;
    std::vector<torch::jit::IValue> iv;
    iv.push_back(torch::from_blob((void*)img, {1, 1, S, S}, torch::kFloat32));
    return run(m, iv, out, out_cap);
}

int ts_forward4(void* model, const float* l, const float* r,
                const float* f, const float* aux, int S, int aux_n,
                float* out, int out_cap) {
    auto* m = (torch::jit::script::Module*)model;
    std::vector<torch::jit::IValue> iv;
    iv.push_back(torch::from_blob((void*)l,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)r,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)f,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)aux, {1, aux_n},   torch::kFloat32));
    return run(m, iv, out, out_cap);
}

int ts_read_cols(void* model, const float* img, int h, int w,
                 float* out, int out_cap) {
    // spectral reader: (1,1,h,w) -> per-column [token, prob, sect]
    auto* m = (torch::jit::script::Module*)model;
    std::vector<torch::jit::IValue> iv;
    iv.push_back(torch::from_blob((void*)img, {1, 1, h, w}, torch::kFloat32));
    try {
        torch::NoGradGuard ng;
        auto r   = m->forward(iv).toTuple();
        auto ch  = r->elements()[0].toTensor()[0];        // NCLS, T
        auto se  = r->elements()[1].toTensor()[0][0];     // T
        auto pr  = torch::softmax(ch, 0);
        auto mx  = pr.max(0);
        auto ids = std::get<1>(mx).contiguous();
        auto pb  = std::get<0>(mx).contiguous();
        auto sp  = torch::sigmoid(se).contiguous();
        int T = (int)ids.size(0);
        int n = std::min(T, out_cap / 3);
        auto ia = ids.accessor<int64_t, 1>();
        auto pa = pb.accessor<float, 1>();
        auto sa = sp.accessor<float, 1>();
        for (int i = 0; i < n; i++) {
            out[i * 3]     = (float)ia[i];
            out[i * 3 + 1] = pa[i];
            out[i * 3 + 2] = sa[i];
        }
        return n;
    } catch (const std::exception& e) {
        fprintf(stderr, "ts_read_cols: %s\n", e.what());
        return -1;
    }
}

int ts_forward2i(void* model, const float* a, const float* b,
                 const float* aux, int S, int aux_n,
                 float* out, int out_cap) {
    auto* m = (torch::jit::script::Module*)model;
    std::vector<torch::jit::IValue> iv;
    iv.push_back(torch::from_blob((void*)a,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)b,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)aux, {1, aux_n},   torch::kFloat32));
    return run(m, iv, out, out_cap);
}

int ts_forward6i(void* model, const float* a, const float* b,
                 const float* c, const float* d, const float* e,
                 const float* f, const float* aux, int S, int aux_n,
                 float* out, int out_cap) {
    auto* m = (torch::jit::script::Module*)model;
    std::vector<torch::jit::IValue> iv;
    iv.push_back(torch::from_blob((void*)a,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)b,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)c,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)d,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)e,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)f,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)aux, {1, aux_n},   torch::kFloat32));
    return run(m, iv, out, out_cap);
}

int ts_forward5(void* model, const float* l, const float* r,
                const float* f, const float* w, const float* aux,
                int S, int aux_n, float* out, int out_cap) {
    auto* m = (torch::jit::script::Module*)model;
    std::vector<torch::jit::IValue> iv;
    iv.push_back(torch::from_blob((void*)l,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)r,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)f,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)w,   {1, 1, S, S}, torch::kFloat32));
    iv.push_back(torch::from_blob((void*)aux, {1, aux_n},   torch::kFloat32));
    return run(m, iv, out, out_cap);
}

}
