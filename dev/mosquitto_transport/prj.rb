require 'rubygems'

gem 'Mxx_ru', '>= 1.3.0'

require 'mxx_ru/cpp'

MxxRu::Cpp::lib_target {

  target 'lib/mosquitto_transport'

  required_prj 'libmosquitto/prj.rb'
  required_prj 'spdlog_mxxru/prj.rb'
  required_prj 'so_5/prj_s.rb'
  required_prj 'fmt_mxxru/prj.rb'

  cpp_source 'initializer.cpp'
  cpp_source 'pub.cpp'
  cpp_source 'a_transport_manager.cpp'
}

