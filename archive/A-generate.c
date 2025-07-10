#include <import>
#include <llama.h>
#include <poll.h>

void llama_logger(enum ggml_log_level level, const char * text, void * user_data) {
    print("llama: %s", text);
}

static struct llama_context* ctx;
static struct llama_model*   model;
static struct llama_sampler* smpl;
static struct llama_vocab*   vocab;

static string generate(string prompt) {
    string     response        =  string(alloc, 1024);
    const bool is_first        =  llama_get_kv_cache_used_cells(ctx) == 0;
    const int  n_prompt_tokens = -llama_tokenize(vocab, cstring(prompt), len(prompt), NULL, 0, is_first, true);
    vector     prompt_tokens   =  vector(alloc, n_prompt_tokens, type, typeid(i32));

    if (llama_tokenize(vocab, cstring(prompt), len(prompt),
            vdata(prompt_tokens), n_prompt_tokens, is_first, true) < 0) {
        fault("failed to tokenize the prompt");
    }

    // prepare a batch for the prompt
    llama_batch batch = llama_batch_get_one(vdata(prompt_tokens), n_prompt_tokens);
    llama_token new_token_id;
    for (;;) {
        int n_ctx      = llama_n_ctx(ctx);
        int n_ctx_used = llama_get_kv_cache_used_cells(ctx);

        verify (n_ctx_used + batch.n_tokens <= n_ctx, "context size exceeded");
        verify (llama_decode(ctx, batch) == 0, "failed to decode");

        // sample the next token
        new_token_id = llama_sampler_sample(smpl, ctx, -1);

        // is it an end of generation?
        if (llama_vocab_is_eog(vocab, new_token_id))
            break;

        // convert the token to a string, print it and add it to the response
        char buf[256];
        int  n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
        verify (n >= 0, "token to words failure");
        string words = string(alloc, n, ref_length, n, chars, buf);

        put("%o", words);
        concat(response, words);

        // prepare the next batch with the sampled token
        batch = llama_batch_get_one(&new_token_id, 1);
    }
    print("");
    return response;
};

int main(int n_args, cstr* v_args) {
    A_start(v_args);
  //string a          = string("Audrey");
    symbol model_file = "bartowski_Meta-Llama-3-8B-Instruct-GGUF_Meta-Llama-3-8B-Instruct-Q4_K_M.gguf"; /// form this from two parts
    path   model_path = form(path, "%s/.cache/llama.cpp/%s", getenv("HOME"), model_file);
    int    ngl        = 100;
    int    n_ctx      = 2048;

    //llama_log_set(llama_logger, null);
    llama_backend_init();

    // initialize the model
    struct llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = ngl;
    model_params.use_mmap = true;
    model_params.use_mlock = true;

    model = llama_model_load_from_file(cstring(model_path), model_params);
    vocab = (struct llama_vocab*)llama_model_get_vocab(model);

    // initialize the context
    struct llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx   = n_ctx;
    ctx_params.n_batch = n_ctx;

    ctx = llama_init_from_model(model, ctx_params);
    verify (ctx, "failed to create the llama_context");

    // initialize the sampler
    smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // helper function to evaluate a prompt and generate a response
    vector messages  = vector(alloc, 32, type, typeid(message)); /// array of chat messages
    i64    bsize     = llama_n_ctx(ctx);
    vector formatted = vector(alloc, bsize, type, typeid(i8));
    int    prev_len  = 0;
    file   f_stdin   = file(id, stdin, text_mode, true);

    struct pollfd fds = (struct pollfd) {
        .fd = STDIN_FILENO,
        .events = POLLIN
    };

    while (true) {
        // get user input
        put("\033[32m> \033[0m");
        symbol  tmpl    = llama_model_chat_template(model, null);
        string  query   = null;
        int     line_count = 1;
        for (;;) {
            string  line = read(f_stdin, typeid(string));
            if (!line)  break;
            if (!query) query = string("this is my query (with line numbers for reference):\n");
            line = mid(line, 0, len(line) - 1);
            string annot = form(string, "%o : line #%i\n", line, line_count);
            printf("annot = %s", annot->chars);
            concat(query, annot);
            /// we need to check and see if there is more available on stdin, and then call read again!
            /// we will concat(query, line) for each the user pastes in from console
            if (poll(&fds, 1, 100) != 1)
                break;
            line_count++;
        }
        if (!query)
            break;
        print("now i am going to send to the bot...");
        message req = message(role, "user", content, (cstr)query->chars);
        push(messages, req);

        int new_len = llama_chat_apply_template(tmpl, vdata(messages), len(messages), true, vdata(formatted), len(formatted));
        if (new_len > (int)len(formatted)) {
            resize(formatted, new_len); // this may not copy what we need
            new_len = llama_chat_apply_template(tmpl, vdata(messages), len(messages), true, vdata(formatted), len(formatted));
        }
        verify (new_len >= 0, "failed to apply the chat template");
        i8*    f_data = vdata(formatted);
        string prompt = string(chars, (cstr)&f_data[prev_len], ref_length, new_len - prev_len);

        // generate a response
        printf("\033[33m");
        string response = generate(prompt);
        printf("\n\033[0m");
        message res = message(role, "assistant", content, (cstr)response->chars);

        // add response
        push(messages, res);
        prev_len = llama_chat_apply_template(tmpl, vdata(messages), len(messages), false, null, 0);
        verify(prev_len >= 0, "failed to apply the chat template");
    }
    llama_sampler_free(smpl);
    llama_free(ctx);
    llama_model_free(model);
    llama_log_set(llama_logger, null);
    return 0;
}