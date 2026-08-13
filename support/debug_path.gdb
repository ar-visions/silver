set breakpoint pending on
set pagination off

# 1. e_create when target is path
break aether.c:3584
cond $bpnum mdl && mdl->au && mdl->au->ident && strcmp(mdl->au->ident, "path") == 0
commands
  printf "== e_create target=path input=%s loaded=%d no_build=%d\n", input ? input->au->ident : "null", input ? input->loaded : -1, a->no_build
  bt 3
  continue
end

# 2. e_operand when building string for path chain
break aether.c:1776
cond $bpnum src_model && src_model->au && strcmp(src_model->au->ident, "string") == 0
commands
  printf "== e_operand string target, isa(op)=%s no_build=%d\n", ((struct _Au_t*)((struct _Au*)op - 1)->type)->ident, a->no_build
  bt 3
  continue
end

# 3. e_interpolate entry
break aether.c:1298
commands
  printf "== e_interpolate str='%s' no_build=%d\n", str->chars, a->no_build
  bt 3
  continue
end

# 4. e_fn_call for path_with_string
break aether.c:2270
cond $bpnum fn && fn->au && fn->au->ident && strcmp(fn->au->ident, "with_string") == 0 && fn->au->context && strcmp(fn->au->context->ident, "path") == 0
commands
  printf "== e_fn_call path.with_string no_build=%d\n", a->no_build
  bt 3
  continue
end

# 5. convertible when from=string/symbol to=path or from=symbol to=string
break aether.c:2701
cond $bpnum ma && mb && ((mb->au->ident && strcmp(mb->au->ident, "path") == 0) || (ma->au->ident && strcmp(ma->au->ident, "symbol") == 0 && mb->au->ident && strcmp(mb->au->ident, "string") == 0))
commands
  printf "== convertible %s -> %s\n", ma->au->ident, mb->au->ident
  bt 3
  continue
end

# 6. e_init for path alloc
break aether.c:3217
cond $bpnum alloc && alloc->au && strcmp(alloc->au->ident, "path") == 0
commands
  printf "== e_init path ctr=%p ctr_input=%p no_build=%d\n", ctr, ctr_input, a->no_build
  if ctr_input
    printf "   ctr_input type=%s loaded=%d\n", ctr_input->au->ident, ctr_input->loaded
  end
  bt 3
  continue
end

run
quit
