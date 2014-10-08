Gem::Specification.new do |s|
  s.name = 'satyr'
  s.version = '0.2'
  s.summary = 'Parse uReport bug report format'
  s.description = 'Ruby bindings for satyr, library for working with the uReport problem report format.'
  s.authors = ['Martin Milata']
  s.email = 'mmilata@redhat.com'
  s.files = ['lib/satyr.rb', 'Rakefile']
  s.test_files = ['test/test_satyr.rb', 'test/data/ureport-1']
  s.homepage = 'http://github.com/abrt/satyr'
  s.license = 'GPLv2'
  s.add_runtime_dependency 'ffi'
end
