// names Au macroizes that must be released before the next header sees them.
// deliberately has no include guard: every module header re-macroizes, so this
// is included again after each batch of them.

// method-name macros: every class defines these
#undef init
#undef dealloc

#ifdef __cplusplus
// keywords and std names. the msvc stl refuses outright: xkeycheck.h #errors
// on a macroized keyword, and <utility> needs forward for std::forward
#undef true
#undef false
#undef forward
#endif
