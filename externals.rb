MxxRu::arch_externals :libmosquitto do |e|
  e.url 'https://github.com/eao197/mosquitto/archive/v1.4.8-p1.tar.gz'

  e.map_dir 'lib' => 'dev/libmosquitto'
  e.map_file 'config.h' => 'dev/libmosquitto/*'
end

MxxRu::arch_externals :libmosquitto_mxxru do |e|
  e.url 'https://github.com/Stiffstream/libmosquitto_mxxru/archive/v.1.1.0.tar.gz'

  e.map_file 'dev/libmosquitto/prj.rb' => 'dev/libmosquitto/*'
end

MxxRu::arch_externals :fmt do |e|
  e.url 'https://github.com/fmtlib/fmt/archive/4.1.0.zip'

  e.map_dir 'fmt' => 'dev/fmt'
end

MxxRu::arch_externals :fmtlib_mxxru do |e|
  e.url 'https://github.com/Stiffstream/fmtlib_mxxru/archive/fmt-4.1.0.tar.gz'

  e.map_dir 'dev/fmt_mxxru' => 'dev'
end

MxxRu::arch_externals :spdlog do |e|
  e.url 'https://github.com/gabime/spdlog/archive/v0.16.3.zip'

  e.map_dir 'include' => 'dev/spdlog'
end

MxxRu::arch_externals :spdlog_mxxru do |e|
  e.url 'https://github.com/Stiffstream/spdlog_mxxru/archive/v.1.2.1.tar.gz'

  e.map_dir 'dev/spdlog_mxxru' => 'dev'
end

MxxRu::arch_externals :so5 do |e|
  e.url 'https://sourceforge.net/projects/sobjectizer/files/sobjectizer/SObjectizer%20Core%20v.5.5/so-5.5.24.4.tar.xz'

  e.map_dir 'dev/so_5' => 'dev'
  e.map_dir 'dev/timertt' => 'dev'
end

MxxRu::arch_externals :catch do |e|
  e.url 'https://github.com/catchorg/Catch2/archive/v2.2.2.tar.gz'

  e.map_file 'single_include/catch.hpp' => 'dev/catch/*'
end

# vim:ts=2:sts=2:sw=2:expandtab

