#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target( MxxRu::BUILD_ROOT ) {

	toolset.force_cpp14
	global_include_path "."

	if 'gcc' == toolset.name || 'clang' == toolset.name
		global_linker_option '-pthread'
	end

	if 'gcc' == toolset.name
		global_compiler_option '-Wextra'
		global_compiler_option '-Wall'
	end

	# If there is local options file then use it.
	if FileTest.exist?( "local-build.rb" )
		required_prj "local-build.rb"
	else
		default_runtime_mode( MxxRu::Cpp::RUNTIME_RELEASE )
		MxxRu::enable_show_brief
		global_obj_placement MxxRu::Cpp::RuntimeSubdirObjPlacement.new(
			'target/' + toolset.name )
	end

	ENV[ 'LD_LIBRARY_PATH' ] = mxx_obj_placement.get_dll('.', self, self)

	required_prj 'test/topic_name_splitter/prj.ut.rb'
	required_prj 'test/subscription_map/prj.ut.rb'

	required_prj 'test/simple_start_stop/prj.rb'
	required_prj 'test/simple_subscribe/prj.rb'
	required_prj 'test/dummy_decoder/prj.rb'
}
