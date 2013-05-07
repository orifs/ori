import sys
import os
import multiprocessing

## Helper Functions

def CheckPkgConfig(context):
    context.Message('Checking for pkg-config... ')
    ret = context.TryAction('pkg-config --version')[0]
    context.Result(ret)
    return ret

def CheckPkg(context, name):
    context.Message('Checking for %s... ' % name)
    ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
    context.Result(ret)
    return ret

## Configuration

opts = Variables('Local.sc')

opts.AddVariables(
    ("CC", "C Compiler"),
    ("CXX", "C++ Compiler"),
    ("AS", "Assembler"),
    ("LINK", "Linker"),
    ("BUILDTYPE", "Build type (RELEASE, DEBUG, or PERF)", "RELEASE"),
    ("VERBOSE", "Show full build information (0 or 1)", "0"),
    ("NUMCPUS", "Number of CPUs to use for build (0 means auto).", "0"),
    ("WITH_FUSE", "Include FUSE file system (0 or 1).", "1"),
    ("WITH_HTTPD", "Include HTTPD server (0 or 1).", "1"),
    ("WITH_MDNS", "Include Zeroconf (through DNS-SD) support (0 or 1).", "0"),
    ("WITH_GPROF", "Include gprof profiling (0 or 1).", "0"),
    ("WITH_GOOGLEHEAP", "Link to Google Heap Cheker.", "0"),
    ("WITH_GOOGLEPROF", "Link to Google CPU Profiler.", "0"),
    ("WITH_TSAN", "Enable Clang Race Detector.", "0"),
    ("WITH_ASAN", "Enable Clang AddressSanitizer.", "0"),
    ("WITH_LIBS3", "Include support for Amazon S3 (0 or 1).", "0"),
    ("USE_FAKES3", "Send S3 requests to fakes3 instead of Amazon (0 or 1).",
        "0"),
    ("HASH_ALGO", "Hash algorithm (SHA256 or SKEIN).", "SHA256"),
    ("COMPRESSION_ALGO", "Compression algorithm (LZMA; FASTLZ; SNAPPY; NONE).",
        "FASTLZ"),
    ("CHUNKING_ALGO", "Chunking algorithm (RK; FIXED).", "RK"),
    ("PREFIX", "Installation target directory.", "/usr/local/bin/")
)

env = Environment(options = opts,
                  tools = ['default'],
                  ENV = os.environ)
Help(opts.GenerateHelpText(env))

# Copy environment variables
if os.getenv('CC') != None:
    env["CC"] = os.getenv('CC')
if os.getenv('CXX') != None:
    env["CXX"] = os.getenv('CXX')
if os.getenv('AS') != None:
    env["AS"] = os.getenv('AS')
if os.getenv('LD') != None:
    env["LINK"] = os.getenv('LD')

# Windows Configuration Changes
if sys.platform == "win32":
    env["WITH_FUSE"] = "0"
    env.Append(CPPFLAGS = ['-DWIN32'])

#env.Append(CPPFLAGS = [ "-Wall", "-Wformat=2", "-Wextra", "-Wwrite-strings",
#                        "-Wno-unused-parameter", "-Wmissing-format-attribute",
#                        "-Werror" ])
#env.Append(CFLAGS = [ "-Wmissing-prototypes", "-Wmissing-declarations",
#                      "-Wshadow", "-Wbad-function-cast", "-Werror" ])
#env.Append(CXXFLAGS = [ "-Wno-non-template-friend", "-Woverloaded-virtual",
#                        "-Wcast-qual", "-Wcast-align", "-Wconversion",
#                        "-Weffc++", "-std=c++0x", "-Werror" ])

if env["HASH_ALGO"] == "SHA256":
    env.Append(CPPFLAGS = [ "-DORI_USE_SHA256" ])
elif env["HASH_ALGO"] == "SKEIN":
    env.Append(CPPFLAGS = [ "-DORI_USE_SKEIN" ])
else:
    print "Error unsupported hash algorithm"
    sys.exit(-1)

if env["COMPRESSION_ALGO"] == "LZMA":
    env.Append(CPPFLAGS = [ "-DORI_USE_LZMA", "-DENABLE_COMPRESSION" ])
elif env["COMPRESSION_ALGO"] == "FASTLZ":
    env.Append(CPPFLAGS = [ "-DORI_USE_FASTLZ", "-DENABLE_COMPRESSION" ])
elif env["COMPRESSION_ALGO"] == "SNAPPY":
    env.Append(CPPFLAGS = [ "-DORI_USE_SNAPPY", "-DENABLE_COMPRESSION" ])
elif env["COMPRESSION_ALGO"] == "NONE":
    print "Building without compression"
else:
    print "Error unsupported compression algorithm"
    sys.exit(-1)

if env["CHUNKING_ALGO"] == "RK":
    env.Append(CPPFLAGS = [ "-DORI_USE_RK" ])
elif env["CHUNKING_ALGO"] == "FIXED":
    env.Append(CPPFLAGS = [ "-DORI_USE_FIXED" ])
else:
    print "Error unsupported chunking algorithm"
    sys.exit(-1)

if env["WITH_MDNS"] != "1":
    env.Append(CPPFLAGS = [ "-DWITHOUT_MDNS" ])

if env["WITH_LIBS3"] == "1":
    env.Append(CPPFLAGS = [ "-DWITH_LIBS3" ])

if env["USE_FAKES3"] == "1":
    env.Append(CPPDEFINES = ['USE_FAKES3'])

if env["WITH_GPROF"] == "1":
    env.Append(CPPFLAGS = [ "-pg" ])
    env.Append(LINKFLAGS = [ "-pg" ])

if env["BUILDTYPE"] == "DEBUG":
    env.Append(CPPFLAGS = [ "-g", "-DDEBUG", "-Wall",
	"-Wno-deprecated-declarations" ])
    env.Append(LINKFLAGS = [ "-g" ])
elif env["BUILDTYPE"] == "PERF":
    env.Append(CPPFLAGS = [ "-g", "-DNDEBUG", "-Wall", "-O0"])
    env.Append(LDFLAGS = [ "-g" ])
elif env["BUILDTYPE"] == "RELEASE":
    env.Append(CPPFLAGS = ["-DNDEBUG", "-Wall", "-O2"])
else:
    print "Error BUILDTYPE must be RELEASE or DEBUG"
    sys.exit(-1)

if env["VERBOSE"] == "0":
    env["CCCOMSTR"] = "Compiling $SOURCE"
    env["CXXCOMSTR"] = "Compiling $SOURCE"
    env["SHCCCOMSTR"] = "Compiling $SOURCE"
    env["SHCXXCOMSTR"] = "Compiling $SOURCE"
    env["ARCOMSTR"] = "Creating library $TARGET"
    env["RANLIBCOMSTR"] = "Indexing library $TARGET"
    env["LINKCOMSTR"] = "Linking $TARGET"

def GetNumCPUs(env):
    if env["NUMCPUS"] != "0":
        return int(env["NUMCPUS"])
    return 2*multiprocessing.cpu_count()

env.SetOption('num_jobs', GetNumCPUs(env))

# Modify CPPPATH and LIBPATH
if sys.platform != "darwin" and sys.platform != "win32":
    env.Append(CPPFLAGS = "-D_FILE_OFFSET_BITS=64")
    env.Append(LIBPATH = [ "/usr/local/lib/event2" ])

if sys.platform != "win32":
    env.Append(CPPPATH = [ "/usr/local/include" ])
    env.Append(LIBPATH = [ "$LIBPATH", "/usr/local/lib" ])

# FreeBSD requires libexecinfo
# Linux and darwin have the header
# NetBSD and Windows do not
if sys.platform == "freebsd9" or sys.platform == "freebsd8":
    env.Append(LIBS = ['execinfo'])
    env.Append(CPPFLAGS = "-DHAVE_EXECINFO")
elif sys.platform == "linux2" or sys.platform == "darwin":
    env.Append(CPPFLAGS = "-DHAVE_EXECINFO")

if sys.platform == "darwin":
    # OpenSSL needs to be overridden on OS X
    env.Append(LIBPATH=['/usr/local/Cellar/openssl/1.0.1c/lib'],
               CPPPATH=['/usr/local/Cellar/openssl/1.0.1c/include'])

if sys.platform == "win32":
    env.Append(LIBPATH=['#../boost_1_53_0\stage\lib'],
               CPPPATH=['#../boost_1_53_0'])
    env.Append(LIBPATH=['#../libevent-2.0.21-stable'],
               CPPPATH=['#../libevent-2.0.21-stable\include'])

# Configuration
conf = env.Configure(custom_tests = { 'CheckPkgConfig' : CheckPkgConfig,
                                      'CheckPkg' : CheckPkg })

if not conf.CheckCC():
    print 'Your C compiler and/or environment is incorrectly configured.'
    Exit(1)

if not conf.CheckCXX():
    print 'Your C++ compiler and/or environment is incorrectly configured.'
    Exit(1)

if (sys.platform != "win32") and not conf.CheckPkgConfig():
    print 'pkg-config not found!'
    Exit(1)

#
#env.AppendUnique(CXXFLAGS = ['-std=c++11'])
#if not conf.CheckCXX():
#    env['CXXFLAGS'].remove('-std=c++11')
#
#env.AppendUnique(CXXFLAGS = ['-std=c++0x'])
#if not conf.CheckCXX():
#    env['CXXFLAGS'].remove('-std=c++0x')
#

if conf.CheckCXXHeader('unordered_map'):
    env.Append(CPPFLAGS = "-DHAVE_CXX11")
elif conf.CheckCXXHeader('tr1/unordered_map'):
    env.Append(CPPFLAGS = "-DHAVE_CXXTR1")
else:
    print 'Either C++11, C++0x, or C++ TR1 must be present!'
    Exit(1)

if not conf.CheckCXXHeader('boost/uuid/uuid.hpp'):
    print 'Boost UUID headers are missing!'
    Exit(1)

if not conf.CheckCXXHeader('boost/bind.hpp'):
    print 'Boost bind headers are missing!'
    Exit(1)

if not conf.CheckCXXHeader('boost/date_time/posix_time/posix_time.hpp'):
    print 'Boost posix_time headers are missing!'
    Exit(1)

if sys.platform == "freebsd9" or sys.platform == "freebsd8":
    if not conf.CheckLib('execinfo'):
        print 'FreeBSD requires libexecinfo to build.'
        Exit(1)

if env["COMPRESSION_ALGO"] == "LZMA":
    if not conf.CheckLibWithHeader('lzma',
                                   'lzma.h',
                                   'C',
                                   'lzma_version_string();'):
        print 'Please install liblzma'
        Exit(1)

if sys.platform != "win32" and not conf.CheckPkg('fuse'):
    print 'FUSE is not registered in pkg-config'
    Exit(1)

if sys.platform != "win32":
    if not conf.CheckPkg('libevent'):
        print 'libevent is not registered in pkg-config'
        Exit(1)
    env.ParseConfig('pkg-config --libs --cflags libevent')

# Library is libevent.so or libevent-2.0.so depending on the system
has_event2 = conf.CheckLibWithHeader('event-2.0', 'event2/event.h', 'C', 
'event_init();')
has_event = conf.CheckLibWithHeader('event', 'event2/event.h', 'C', 
'event_init();')
if not (has_event or has_event2):
    print 'Please install libevent 2.0+'
    Exit(1)

if (env["WITH_MDNS"] == "1") and (sys.platform != "darwin"):
    if not conf.CheckLibWithHeader('dns_sd','dns_sd.h','C'):
	print 'Please install libdns_sd'
	Exit(1)

# Test for recent OpenSSL

conf.Finish()

Export('env')

# libori
if env["WITH_LIBS3"] == "1":
    env.Append(CPPPATH = '#libs3-2.0/inc')

SConscript('libdiffmerge/SConscript', variant_dir='build/libdiffmerge')
SConscript('libori/SConscript', variant_dir='build/libori')
SConscript('liboriutil/SConscript', variant_dir='build/liboriutil')

# Set compile options for binaries
env.Append(LIBS = ["diffmerge", "z"],
           LIBPATH = ['#build/libdiffmerge'])

env.Append(LIBS = ["ori"],
           CPPPATH = ['#public'],
           LIBPATH = ['#build/libori'])

env.Append(LIBS = ["oriutil"],
           CPPPATH = ['#public'],
           LIBPATH = ['#build/liboriutil'])

if env["WITH_LIBS3"] == "1":
    SConscript('libs3-2.0/SConscript', variant_dir='build/libs3-2.0')

if env["COMPRESSION_ALGO"] == "SNAPPY":
    env.Append(CPPPATH = ['#snappy-1.0.5'])
    env.Append(LIBS = ["snappy"], LIBPATH = ['#build/snappy-1.0.5'])
    SConscript('snappy-1.0.5/SConscript', variant_dir='build/snappy-1.0.5')
if env["COMPRESSION_ALGO"] == "FASTLZ":
    env.Append(CPPPATH = ['#libfastlz'])
    env.Append(LIBS = ["fastlz"], LIBPATH = ['#build/libfastlz'])
    SConscript('libfastlz/SConscript', variant_dir='build/libfastlz')
if env["HASH_ALGO"] == "SKEIN":
    env.Append(CPPPATH = ['#libskein'])
    env.Append(LIBS = ["skein"], LIBPATH = ['#build/libskein'])
    SConscript('libskein/SConscript', variant_dir='build/libskein')

# Debugging Tools
if env["WITH_GOOGLEHEAP"] == "1":
    env.Append(LIBS = ["tcmalloc"])
if env["WITH_GOOGLEPROF"] == "1":
    env.Append(LIBS = ["profiler"])
if env["WITH_TSAN"] == "1":
    env.Append(CPPFLAGS = ["-fsanitize=thread"])
    env.Append(LINKFLAGS = ["-fsanitize=thread"])
if env["WITH_ASAN"] == "1":
    env.Append(CPPFLAGS = ["-fsanitize=address"])
    env.Append(LINKFLAGS = ["-fsanitize=address"])
if env["WITH_TSAN"] == "1" and env["WITH_ASAN"] == "1":
    print "Cannot set both WITH_TSAN and WITH_ASAN!"
    sys.exit(-1)

# Installation Targets

# Ori Utilities
SConscript('ori/SConscript', variant_dir='build/ori')
SConscript('orisync/SConscript', variant_dir='build/orisync')
if env["WITH_LIBS3"] == "1":
    SConscript('oris3/SConscript', variant_dir='build/oris3')
if env["WITH_FUSE"] == "1":
    SConscript('orifs/SConscript', variant_dir='build/orifs')
if env["WITH_HTTPD"] == "1":
    SConscript('ori_httpd/SConscript', variant_dir='build/ori_httpd')

# Install Targets
if env["WITH_FUSE"] == "1":
    env.Install('$PREFIX/bin','build/orifs/orifs')
env.Install('$PREFIX/bin','build/ori/ori')
env.Install('$PREFIX/bin','build/orisync/orisync')
if env["WITH_LIBS3"] == "1":
    env.Install('$PREFIX/bin','build/ori/oris3')
if env["WITH_HTTPD"] == "1":
    env.Install('$PREFIX/bin','build/ori_httpd/ori_httpd')
env.Install('$PREFIX/share/man/man1','docs/ori.1')
env.Install('$PREFIX/share/man/man1','docs/orifs.1')
env.Alias('install','$PREFIX')

