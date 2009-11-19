require 'mkmf'

# We need C++, not C, because Taglib is in C++
CONFIG['CC'] = 'g++'
CONFIG['CPP'].sub!(CONFIG['CPP'], 'g++ -E')
CONFIG['LDSHARED'].sub!(CONFIG['CC'], 'g++')

RbConfig::CONFIG['CC'] = 'g++'
RbConfig::CONFIG['CC'] = 'g++ -E'

# taglib-config does not implement libs-only-l, so we copy and paste pkg_config
# from mkmf then remove the call to taglib-config --libs-only-l
def pkg_config(pkg)
  if pkgconfig = with_config("#{pkg}-config") and find_executable0(pkgconfig)
    # iff package specific config command is given
    get = proc {|opt| `#{pkgconfig} --#{opt}`.chomp}
  elsif ($PKGCONFIG ||= 
         (pkgconfig = with_config("pkg-config", ("pkg-config" unless CROSS_COMPILING))) &&
         find_executable0(pkgconfig) && pkgconfig) and
      system("#{$PKGCONFIG} --exists #{pkg}")
    # default to pkg-config command
    get = proc {|opt| `#{$PKGCONFIG} --#{opt} #{pkg}`.chomp}
  elsif find_executable0(pkgconfig = "#{pkg}-config")
    # default to package specific config command, as a last resort.
    get = proc {|opt| `#{pkgconfig} --#{opt}`.chomp}
  end
  if get
    cflags = get['cflags']
    ldflags = get['libs']
    libs = ''#get['libs-only-l']
    ldflags = (Shellwords.shellwords(ldflags) - Shellwords.shellwords(libs)).quote.join(" ")
    $CFLAGS += " " << cflags
    $LDFLAGS += " " << ldflags
    $libs += " " << libs
    Logging::message "package configuration for %s\n", pkg
    Logging::message "cflags: %s\nldflags: %s\nlibs: %s\n\n",
                     cflags, ldflags, libs
    [cflags, ldflags, libs]
  else
    Logging::message "package configuration for %s is not found\n", pkg
    nil
  end
end

pkg_config('taglib')

testCode = <<-EOD
#include <tag.h>
int main(int, char **)
{
	return 0;
}
EOD

if try_compile(testCode)
	create_makefile('taglib2')
else
	puts <<-EOD
Taglib not found.  Please ensure taglib-config is in your PATH.
	EOD
end
