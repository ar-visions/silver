# silver changelog

## LLDB debug info member offsets (lldb.c)
- `m->offset` is a runtime field, always 0 at compile time — LLDB showed corrupt values
- fix: use `LLVMOffsetOfElement(target_data, struct_type, m->index)` for actual byte offsets
- purely debug info fix, no runtime impact

## string_with_f64 constructor (Au.c, Au header)
- `string_with_f64(string, f64)` using `snprintf(buf, 64, "%g", value)`
- enables float-to-string interpolation for f64 members
- registered in string_schema as `M(AA,BB, i, ctr, public, f64)`

## fault macro halt fix (aether.c)
- `fault` with `pk->line == 0` printed error but never called `halt()`
- errors were silently continuing past the fault
- fix: added `halt(s, null)` to the `line == 0` branch

## is_required compile-time validation (aether.c)
- `e_init` now validates that `expect` members are provided during construction
- walks context chain checking `is_required` members against provided props
- skips map/array types

## e_convert_or_cast canonical fix (aether.c)
- `is_prim(input)` on unloaded member enodes returned false (got `typeid(enode)`)
- fix: `is_prim(canonical(input))` to get the semantic type

## tensor_init scale overwrite (ai.ag)
- `tensor_init` else branch unconditionally set `scale = 1.0f` when initializer undefined
- clobbered user-provided scale values
- root cause of "scale shows 1" bug — not a compiler conversion issue

## dlopen/dlclose for imported modules (aether.c, Au.c)
- `dlopen` only runs constructors on first load; `module_erase` nulled module from array
- second import: dlopen bumps refcount but constructors don't re-run
- fix: `aether_unload_libs(a)` calls `dlclose` on all lib handles before `module_erase`
- `a->libs` map keys are lib paths, values are raw dlopen handles cast to `(Au)`
- Au entry keyed as `"Au"` with `_bool(true)` — skip via `eq(key, "Au")`

## C import module stomping fix (aclang.cc)
- aclang.cc was setting `->module` on all types parsed from C headers
- `def_pointer` caches pointer types on `ref->ptr` — second import overwrites module
- fix: removed all `->module =` assignments in aclang.cc
- C headers are not modules

## vtable override offset fix (aether.c)
- removed stale `-2` from override index calculation
- Au_t was compacted (shape removed, etc) but offset compensation wasn't updated
- init worked by coincidence (called by name, not vtable dispatch)
- hash exposed the bug since Au_hash dispatches through vtable
- fix: `abi_size/8 + index * ptr` (no more -2)

## implements macro (macros.h)
- added `implements(I, M)` macro (Au) for vtable function pointer comparison
- null guard on context before comparing ft entries
- Au_hash uses direct cast comparison instead (implements doesn't work on Au base)

## tensor hash (ai.ag)
- added `func hash [] -> u64` on tensor class
- forward declared fnv1a_hash, FNV_PRIME, OFFSET_BASIS in silver
- hash dispatches through vtable via Au_hash override

## coverage function names (coverage.c, aether.c, Au.c)
- compile-time: aether collects function names into global `char*[]` array
- `coverage_set_func_name` stores names by timing ID
- `__coverage_register` takes 5th arg for func_names
- runtime prints function name in 22-char column instead of index
- shows alt (full class_method name) when available

## debug_loc_here -> debug_emit rename (aether.c)
- renamed all 9 occurrences

## alias keyword (silver.c)
- `alias name: type` at module level
- registers target etype under alias name via registry
- added to keywords list and `silver_peek_def`

## indexer overload selection (silver.c)
- `find_member` returned first INDEX member; now scans all indexers
- picks specific type match over generic Au fallback
- i32 matches num/i64 indexer via `is_integral` compatibility

## index macro alt name fix (macros.h)
- `m->alt` used `#R` (return type) so both indexers got same alt name
- fixed: uses `emit_idx_symbol` to match actual C function name

## null compare on class types (aether.c)
- `obj != null` was calling `Au_compare()` which crashes on null operand
- now does pointer compare when either side is `LLVMIsNull`

## iterator null guard (aether.c)
- for-in on null array/map crashed (null deref on count/origin)
- added `debug_emit` before collection access
- optional null guard behind `a->iterator_guard` (disabled by default)

## map/array assorted by default (Au.c)
- `map_init` and `array_init` set `assorted = true`
- was a debug-era check blocking polymorphic collections

## AU_TRAIT_IS_DEFAULT (object.h, Au.c)
- trait bit 50 for marking default positional argument receiver
- `is_default` bitfield on Au_t
- `Au_with_cstrs` assigns positional (non -/-- prefixed) args to default member

## Au_with_cstrs positional args and required verification (Au.c)
- positional args (no - prefix) assigned to member with `is_default` trait
- skips argv[0] (executable path)
- after arg processing, walks type chain verifying all `is_required` members are set

## find_member traits parameter (Au.c, Au header, silver.c, aether.c, aether header)
- added `u64 traits` param between member_type and poly
- filters members by `(au->traits & traits) == traits` when traits != 0
- all existing callers pass 0; `AU_TRAIT_IS_DEFAULT` and `AU_TRAIT_ABSTRACT` use it
- replaced manual default-member loop in `Au_with_cstrs` with single `find_member` call

## module path validation (silver.c)
- resolves module to absolute path in `silver_init`
- validates parent dir name matches module stem name

## --asan CLI property (aether, silver.c, bootstrap)
- `bool asan` on aether schema, `--asan` CLI arg
- auto-sets true if silver compiled with ASan
- `make asan` target, `bootstrap.sh --asan`, gen.py `is_asan` flag

## random module (foundry/random/random.ag)
- xoshiro256++ with SplitMix64 seeding
- inline x86 asm rotate (rotl23, rotl45)
- hex float constants compile correctly
