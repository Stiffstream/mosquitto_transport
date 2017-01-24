require 'rubygems'

gem 'Mxx_ru', '>= 1.3.0'

require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

  target '_test_dummy_decoder'

  required_prj 'mosquitto_transport/prj.rb'

  cpp_source 'main.cpp'

}

