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
