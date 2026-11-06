srcs = [
	'com/google/android/gms/maps/internal/CreatorImpl.java',
	'com/google/android/gms/providerinstaller/ProviderInstallerImpl.java',
]
java_args = [
	'-bootclasspath', bootclasspath,
	'-source', '1.8', '-target', '1.8',
	'-encoding', 'UTF-8',
	'-Xlint:-deprecation', # we implement deprecated APIs
	'-h', join_paths(dir_base, 'src/api-impl-jni/generated_headers')
]

ant = find_program('ant', required: false)

if ant.found()
	gstub_jar = custom_target('gstub',
							  output: 'gstub.jar',
							  command: [ant,
										'-buildfile', files('build.xml'),
										'-Dbasedir=' + builddir_base,
										'-Dsrcdir=' + meson.current_source_dir(),
										'-Dincludes=' + ','.join(srcs),
										'-Djarfile=@OUTPUT@',
										'-Dbuild=@PRIVATE_DIR@',
										'-Djavaargs=' + ' '.join(java_args + ['-classpath', hax_jar.full_path()]),
							  ],
							  depends: [hax_jar],
							  depend_files: srcs + ['build.xml'],
	)
else
	# as hax: a list file, the first jar on the classpath
	gstub_list = join_paths(meson.current_build_dir(), 'gstub_srcs.txt')
	gstub_abs = []
	foreach s : srcs
		gstub_abs += join_paths(meson.current_source_dir(), s)
	endforeach
	run_command(find_program('python3'), '-c',
	            'import sys; open(sys.argv[1], "w").write("\\n".join(sys.argv[2:]) + "\\n")',
	            gstub_list, gstub_abs, check: true)
	gstub_jar = custom_target('gstub',
	                          output: 'gstub.jar',
	                          command: [files('../api-impl/javac_list.sh'), '@OUTPUT@', '@PRIVATE_DIR@', gstub_list]
	                                   + java_args + ['-classpath', hax_jar.full_path()],
	                          depends: [hax_jar],
	                          depend_files: srcs,
	)
endif