#include <WGPU.h>
#include <dawn/webgpu.h>
#include <dawn/dawn_proc_table.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

none app_onAdapterRequestEnded(WGPURequestAdapterStatus status, WGPUAdapter adapter, symbol message, handle userdata);

bool app_a_context_fn(app self, i64 i);

i64 app_run(app self);

/* class-declaration: app_a_context_fn_ctx */
#define app_a_context_fn_ctx_schema(X,Y,Z) \
	i_public(X,Y,Z, i32, ctx)\
	i_construct(X,Y,Z, i32)\

declare_class(app_a_context_fn_ctx)

app_a_context_fn_ctx app_a_context_fn_ctx_with_i32(app_a_context_fn_ctx self, i32 ctx);

none app_onAdapterRequestEnded(WGPURequestAdapterStatus status, WGPUAdapter adapter, symbol message, handle userdata) {
	handle* u = (handle*)&userdata;
	handle something = userdata;
	*u = something;
	return;
}

bool app_a_context_fn(app self, i64 i) {
	if (i == 0) {
		return false;
	}
	return true;
}

i64 app_run(app self) {
	fn lambda = A_lambda(self, A_member(isa(self), A_TYPE_IMETHOD | A_TYPE_SMETHOD, "a_context_fn"), ctr(app_a_context_fn_ctx, i32, (i32)1));
}

define_class(app)

app_a_context_fn_ctx app_a_context_fn_ctx_with_i32(app_a_context_fn_ctx self, i32 ctx) {
	self->ctx = ctx;
}

define_class(app_a_context_fn_ctx)


int main(int argc, char* argv[]) {
    A_finish_types();
    // todo: parse args and set in app class
    app main = new(app);
    return (int)call(main, run);
}