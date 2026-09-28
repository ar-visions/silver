sources = files(
  'mbedtls-module.c',
  'gtlsbackend-mbedtls.c',
  'gtlscertificate-mbedtls.c',
  'gtlsconnection-mbedtls.c',
  'gtlsclientconnection-mbedtls.c',
  'gtlsserverconnection-mbedtls.c',
  'gtlsdatabase-mbedtls.c',
  'gtlsfiledatabase-mbedtls.c',
)

deps = [
  gio_dep,
  glib_dep,
  gmodule_dep,
  gobject_dep,
  mbedtls_dep,
  tlsbase_dep,
]

giomodules += 'giombedtls'

module = shared_module(
  'giombedtls',
  sources: sources,
  include_directories: [top_inc],
  dependencies: deps,
  link_args: module_ldflags,
  link_depends: symbol_map,
  name_suffix: module_suffix,
  install: build_shared,
  install_dir: gio_module_dir,
)
