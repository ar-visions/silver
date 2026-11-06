srcs = [
	'com/google/android/gms/ads/identifier/AdvertisingIdClient.java',
	'com/google/android/gms/ads/measurement/DynamiteMeasurementManager.java',
	'com/google/android/gms/semanticlocation/internal/ISemanticLocationService.java',
]
java_args = [
	'-bootclasspath', bootclasspath,
	'-source', '1.8', '-target', '1.8',
	'-encoding', 'UTF-8',
	'-Xlint:-deprecation', # we implement deprecated APIs
	# microG's own types to compile against, not in the jar
	'-sourcepath', join_paths(meson.current_source_dir(), '../ghax-stubs'),
	'-implicit:none',
	'-h', join_paths(dir_base, 'src/api-impl-jni/generated_headers')
]

ant = find_program('ant', required: false)

if ant.found()
	ghax_jar = custom_target('ghax',
							 output: 'ghax.jar',
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
	ghax_list = join_paths(meson.current_build_dir(), 'ghax_srcs.txt')
	ghax_abs = []
	foreach s : srcs
		ghax_abs += join_paths(meson.current_source_dir(), s)
	endforeach
	run_command(find_program('python3'), '-c',
	            'import sys; open(sys.argv[1], "w").write("\\n".join(sys.argv[2:]) + "\\n")',
	            ghax_list, ghax_abs, check: true)
	ghax_jar = custom_target('ghax',
	                         output: 'ghax.jar',
	                         command: [files('../api-impl/javac_list.sh'), '@OUTPUT@', '@PRIVATE_DIR@', ghax_list]
	                                  + java_args + ['-classpath', hax_jar.full_path()],
	                         depends: [hax_jar],
	                         depend_files: srcs,
	)
endif