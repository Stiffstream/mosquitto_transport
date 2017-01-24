require 'rubygems'

gem 'Mxx_ru', '>= 1.3.0'

require 'mxx_ru/binary_unittest'

path = 'test/topic_name_splitter'

MxxRu::setup_target(
  MxxRu::BinaryUnittestTarget.new(
    "#{path}/prj.ut.rb",
    "#{path}/prj.rb" ) )

