Gem::Specification.new do |s|
  s.name = "ruby-taglib2"
  s.version = "1.01"
 
  s.required_rubygems_version = nil if s.respond_to? :required_rubygems_version=
  s.authors = ["Neil Stevens", "Saimon Moore"]
  s.cert_chain = nil
  s.date = "2009-11-19"
  s.email = "neil@hakubi.us"
  s.extensions = ["ext/extconf.rb"]
  s.files = ["README", "ext/extconf.rb", "ext/ruby-taglib2.cpp", "lib/ruby_taglib2.rb"]
  s.has_rdoc = false
  s.homepage = "http://www.hakubi.us/ruby-taglib"
  s.require_paths = ["lib"]
  s.required_ruby_version = Gem::Requirement.new("> 0.0.0")
  s.requirements = ["Ruby bindings for Taglib's C Library"]
  s.description = "Ruby bindings for Taglib's C Library"
  s.summary = "ruby-taglib2 is a compiled extension to ruby that provides access to the TagLib library"
 
  if s.respond_to? :specification_version then
    current_version = Gem::Specification::CURRENT_SPECIFICATION_VERSION
    s.specification_version = 1
 
    if current_version >= 3 then
    else
    end
  else
  end
end