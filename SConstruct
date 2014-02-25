import sys
import os
import multiprocessing
import SCons.Util

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

def CheckPkgMinVersion(context, name, version):
    context.Message('Checking %s-%s or greater... ' % (name, version))
    ret = context.TryAction('pkg-config --atleast-version \'%s\' \'%s\'' % (version, name))[0]
    context.Result(ret)
    return ret

## Configuration

opts = Variables('Local.sc')

opts.AddVariables(
    ("CC", "C Compiler"),
    ("CXX", "C++ Compiler"),
    ("AS", "Assembler"),
    ("LINK", "Linker"),
    ("NUMCPUS", "Number of CPUs to use for build (0 means auto)", 0, None, int),
    EnumVariable("BUILDTYPE", "Build type", "RELEASE", ["RELEASE", "DEBUG", "PERF"]),
    BoolVariable("VERBOSE", "Show full build information", 0),
    BoolVariable("WITH_FUSE", "Include FUSE file system", 1),
    BoolVariable("WITH_HTTPD", "Include HTTPD server", 0),
    BoolVariable("WITH_ORILOCAL", "Include Ori checkout CLI", 0),
    BoolVariable("WITH_MDNS", "Include Zeroconf (through DNS-SD) support", 0),
    BoolVariable("WITH_GPROF", "Include gprof profiling", 0),
    BoolVariable("WITH_GOOGLEHEAP", "Link to Google Heap Cheker", 0),
    BoolVariable("WITH_GOOGLEPROF", "Link to Google CPU Profiler", 0),
    BoolVariable("WITH_TSAN", "Enable Clang Race Detector", 0),
    BoolVariable("WITH_ASAN", "Enable Clang AddressSanitizer", 0),
    BoolVariable("WITH_LIBS3", "Include support for Amazon S3", 0),
    BoolVariable("BUILD_BINARIES", "Build binaries", 1),
    BoolVariable("CROSSCOMPILE", "Cross compile", 0),
    BoolVariable("USE_FAKES3", "Send S3 requests to fakes3 instead of Amazon", 0),
    EnumVariable("HASH_ALGO", "Hash algorithm", "SHA256", ["SHA256"]),
    EnumVariable("COMPRESSION_ALGO", "Compression algorithm", "FASTLZ", ["LZMA", "FASTLZ", "SNAPPY", "NONE"]),
    EnumVariable("CHUNKING_ALGO", "Chunking algorithm", "RK", ["RK", "FIXED"]),
    PathVariable("PREFIX", "Installation target directory", "/usr/local", PathVariable.PathAccept),
    PathVariable("DESTDIR", "The root directory to install into. Useful mainly for binary package building", "", PathVariable.PathAccept),
)

env = Environment(options = opts,
                  tools = ['default'],
                  ENV = os.environ)
Help(opts.GenerateHelpText(env))

# Copy environment variables
if os.environ.has_key('CC'):
    env["CC"] = os.getenv('CC')
if os.environ.has_key('CXX'):
    env["CXX"] = os.getenv('CXX')
if os.environ.has_key('AS'):
    env["AS"] = os.getenv('AS')
if os.environ.has_key('LD'):
    env["LINK"] = os.getenv('LD')
if os.environ.has_key('CFLAGS'):
    env.Append(CCFLAGS = SCons.Util.CLVar(os.environ['CFLAGS']))
if os.environ.has_key('CPPFLAGS'):
    env.Append(CPPFLAGS = SCons.Util.CLVar(os.environ['CPPFLAGS']))
if os.environ.has_key('CXXFLAGS'):
    env.Append(CXXFLAGS = SCons.Util.CLVar(os.environ['CXXFLAGS']))
if os.environ.has_key('LDFLAGS'):
    env.Append(LINKFLAGS = SCons.Util.CLVar(os.environ['LDFLAGS']))

# Windows Configuration Changes
if sys.platform == "win32":
    env["WITH_FUSE"] = False
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
else:
    print "Error unsupported hash algorithm"
    sys.exit(-1)

if env["COMPRESSION_ALGO"] == "LZMA":
    env.Append(CPPFLAGS = [ "-DORI_USE_LZMA" ])
elif env["COMPRESSION_ALGO"] == "FASTLZ":
    env.Append(CPPFLAGS = [ "-DORI_USE_FASTLZ" ])
elif env["COMPRESSION_ALGO"] == "SNAPPY":
    env.Append(CPPFLAGS = [ "-DORI_USE_SNAPPY" ])
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

if not env["WITH_MDNS"]:
    env.Append(CPPFLAGS = [ "-DWITHOUT_MDNS" ])

if env["WITH_LIBS3"]:
    env.Append(CPPFLAGS = [ "-DWITH_LIBS3" ])

if env["USE_FAKES3"]:
    env.Append(CPPDEFINES = ['USE_FAKES3'])

if env["WITH_GPROF"]:
    env.Append(CPPFLAGS = [ "-pg" ])
    env.Append(LINKFLAGS = [ "-pg" ])

if env["BUILDTYPE"] == "DEBUG":
    env.Append(CPPFLAGS = [ "-g", "-DDEBUG", "-DORI_DEBUG", "-Wall", 
        "-Wno-deprecated-declarations" ])
    env.Append(LINKFLAGS = [ "-g", "-rdynamic" ])
elif env["BUILDTYPE"] == "PERF":
    env.Append(CPPFLAGS = [ "-g", "-DNDEBUG", "-DORI_PERF", "-Wall", "-O2"])
    env.Append(LDFLAGS = [ "-g", "-rdynamic" ])
elif env["BUILDTYPE"] == "RELEASE":
    env.Append(CPPFLAGS = ["-DNDEBUG", "-DORI_RELEASE", "-Wall", "-O2"])
else:
    print "Error BUILDTYPE must be RELEASE or DEBUG"
    sys.exit(-1)

try:
    hf = open(".git/HEAD", 'r')
    head = hf.read()
    if head.startswith("ref: "):
        if head.endswith("\n"):
            head = head[0:-1]
        with open(".git/" + head[5:]) as bf:
            branch = bf.read()
            if branch.endswith("\n"):
                branch = branch[0:-1]
            env.Append(CPPFLAGS = [ "-DGIT_VERSION=\\\"" + branch + "\\\""])
except IOError:
    pass

if not env["VERBOSE"]:
    env["CCCOMSTR"] = "Compiling $SOURCE"
    env["CXXCOMSTR"] = "Compiling $SOURCE"
    env["SHCCCOMSTR"] = "Compiling $SOURCE"
    env["SHCXXCOMSTR"] = "Compiling $SOURCE"
    env["ARCOMSTR"] = "Creating library $TARGET"
    env["RANLIBCOMSTR"] = "Indexing library $TARGET"
    env["LINKCOMSTR"] = "Linking $TARGET"

def GetNumCPUs(env):
    if env["NUMCPUS"] > 0:
        return int(env["NUMCPUS"])
    return 2*multiprocessing.cpu_count()

env.SetOption('num_jobs', GetNumCPUs(env))

# Modify CPPPATH and LIBPATH
if sys.platform != "darwin" and sys.platform != "win32" and not env["CROSSCOMPILE"]:
    env.Append(CPPFLAGS = "-D_FILE_OFFSET_BITS=64")
    env.Append(LIBPATH = [ "/usr/local/lib/event2" ])

if sys.platform != "win32" and not env["CROSSCOMPILE"]:
    env.Append(CPPPATH = [ "/usr/local/include" ])
    env.Append(LIBPATH = [ "$LIBPATH", "/usr/local/lib" ])

# FreeBSD requires libexecinfo
# Linux and darwin have the header
# NetBSD and Windows do not
if sys.platform.startswith("freebsd"):
    env.Append(LIBS = ['execinfo'])
    env.Append(CPPFLAGS = "-DHAVE_EXECINFO")
elif sys.platform == "linux2" or sys.platform == "darwin":
    env.Append(CPPFLAGS = "-DHAVE_EXECINFO")

if sys.platform == "win32":
    env.Append(LIBPATH=['#../boost_1_53_0\stage\lib'],
               CPPPATH=['#../boost_1_53_0'])
    env.Append(LIBPATH=['#../libevent-2.0.21-stable'],
               CPPPATH=['#../libevent-2.0.21-stable\include'])

# XXX: Hack to support clang static analyzer
def CheckFailed():
    if os.getenv('CCC_ANALYZER_OUTPUT_FORMAT') != None:
        return
    Exit(1)

# Configuration
conf = env.Configure(custom_tests = { 'CheckPkgConfig' : CheckPkgConfig,
                                      'CheckPkg' : CheckPkg,
                                      'CheckPkgMinVersion' : CheckPkgMinVersion })

if not conf.CheckCC():
    print 'Your C compiler and/or environment is incorrectly configured.'
    CheckFailed()

if not conf.CheckCXX():
    print 'Your C++ compiler and/or environment is incorrectly configured.'
    CheckFailed()

if (sys.platform == "win32") or env["CROSSCOMPILE"]:
    env["HAS_PKGCONFIG"] = False
else:
    env["HAS_PKGCONFIG"] = True
    if not conf.CheckPkgConfig():
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
    CheckFailed()

if not conf.CheckCXXHeader('boost/uuid/uuid.hpp'):
    print 'Boost UUID headers are missing!'
    Exit(1)

if not conf.CheckCXXHeader('boost/bind.hpp'):
    print 'Boost bind headers are missing!'
    Exit(1)

if not conf.CheckCXXHeader('boost/date_time/posix_time/posix_time.hpp'):
    print 'Boost posix_time headers are missing!'
    Exit(1)

if sys.platform.startswith("freebsd"):
    if not conf.CheckLib('execinfo'):
        print 'FreeBSD requires libexecinfo to build.'
        Exit(1)

check_uuid_h = conf.CheckCHeader('uuid.h')
check_uuid_uuid_h = conf.CheckCHeader('uuid/uuid.h')
if check_uuid_h:
    env.Append(CPPFLAGS = "-DHAVE_UUID_H")
elif check_uuid_uuid_h:
    env.Append(CPPFLAGS = "-DHAVE_UUID_UUID_H")
else:
    print 'Supported UUID header is missing!'
    Exit(1)

if env["COMPRESSION_ALGO"] == "LZMA":
    if not conf.CheckLibWithHeader('lzma',
                                   'lzma.h',
                                   'C',
                                   'lzma_version_string();'):
        print 'Please install liblzma'
        Exit(1)

if env["WITH_FUSE"]:
    if env["HAS_PKGCONFIG"] and not conf.CheckPkg('fuse'):
        print 'FUSE is not registered in pkg-config'
        Exit(1)

if env["HAS_PKGCONFIG"]:
    if not conf.CheckPkg('libevent'):
        print 'libevent is not registered in pkg-config'
        Exit(1)
    if not conf.CheckPkgMinVersion("libevent", "2.0"):
        print 'libevent version 2.0 or above required'
        Exit(1)
    env.ParseConfig('pkg-config --libs --cflags libevent')

has_event = conf.CheckLibWithHeader('', 'event2/event.h', 'C', 'event_init();')
if not (has_event or (env["CROSSCOMPILE"])):
    print 'Cannot link test binary with libevent 2.0+'
    Exit(1)

if (env["WITH_MDNS"]) and (sys.platform != "darwin"):
    if not conf.CheckLibWithHeader('dns_sd','dns_sd.h','C'):
	print 'Please install libdns_sd'
	Exit(1)

if env["HAS_PKGCONFIG"]:
    if not conf.CheckPkg("openssl"):
        print 'openssl is not registered in pkg-config'
        Exit(1)
    if not conf.CheckPkgMinVersion("openssl", "1.0.0"):
        print 'openssl version 1.0.0 or above required'
        Exit(1)
    env.ParseConfig('pkg-config --libs --cflags openssl')

conf.Finish()

Export('env')

# Set compile options for binaries
env.Append(CPPPATH = ['#public', '#.'])
env.Append(LIBS = ["diffmerge", "z"], LIBPATH = ['#build/libdiffmerge'])
env.Append(LIBS = ["ori"], LIBPATH = ['#build/libori'])
env.Append(LIBS = ["oriutil"], LIBPATH = ['#build/liboriutil'])

if sys.platform != "win32" and sys.platform != "darwin":
    env.Append(CPPFLAGS = ['-pthread'])
    env.Append(LIBS = ["pthread"])

# Optional Components
if env["WITH_LIBS3"]:
    env.Append(CPPPATH = '#libs3-2.0/inc')
    SConscript('libs3-2.0/SConscript', variant_dir='build/libs3-2.0')
if env["COMPRESSION_ALGO"] == "SNAPPY":
    env.Append(CPPPATH = ['#snappy-1.0.5'])
    env.Append(LIBS = ["snappy"], LIBPATH = ['#build/snappy-1.0.5'])
    SConscript('snappy-1.0.5/SConscript', variant_dir='build/snappy-1.0.5')
if env["COMPRESSION_ALGO"] == "FASTLZ":
    env.Append(CPPPATH = ['#libfastlz'])
    env.Append(LIBS = ["fastlz"], LIBPATH = ['#build/libfastlz'])
    SConscript('libfastlz/SConscript', variant_dir='build/libfastlz')

# Debugging Tools
if env["WITH_GOOGLEHEAP"]:
    env.Append(LIBS = ["tcmalloc"])
if env["WITH_GOOGLEPROF"]:
    env.Append(LIBS = ["profiler"])
if env["WITH_TSAN"]:
    env.Append(CPPFLAGS = ["-fsanitize=thread", "-fPIE"])
    env.Append(LINKFLAGS = ["-fsanitize=thread", "-pie"])
if env["WITH_ASAN"]:
    env.Append(CPPFLAGS = ["-fsanitize=address"])
    env.Append(LINKFLAGS = ["-fsanitize=address"])
if env["WITH_TSAN"] and env["WITH_ASAN"]:
    print "Cannot set both WITH_TSAN and WITH_ASAN!"
    sys.exit(-1)

# libori
SConscript('libdiffmerge/SConscript', variant_dir='build/libdiffmerge')
SConscript('libori/SConscript', variant_dir='build/libori')
SConscript('liboriutil/SConscript', variant_dir='build/liboriutil')

# Ori Utilities
if env["BUILD_BINARIES"]:
    SConscript('ori/SConscript', variant_dir='build/ori')
    SConscript('oridbg/SConscript', variant_dir='build/oridbg')
    SConscript('orisync/SConscript', variant_dir='build/orisync')
    if env["WITH_LIBS3"]:
        SConscript('oris3/SConscript', variant_dir='build/oris3')
    if env["WITH_FUSE"]:
        SConscript('orifs/SConscript', variant_dir='build/orifs')
    if env["WITH_HTTPD"]:
        SConscript('ori_httpd/SConscript', variant_dir='build/ori_httpd')
    if env["WITH_ORILOCAL"]:
        SConscript('orilocal/SConscript', variant_dir='build/orilocal')

# Install Targets
if env["WITH_FUSE"]:
    env.Install('$DESTDIR$PREFIX/bin','build/orifs/orifs')
env.Install('$DESTDIR$PREFIX/bin','build/ori/ori')
env.Install('$DESTDIR$PREFIX/bin','build/oridbg/oridbg')
env.Install('$DESTDIR$PREFIX/bin','build/orisync/orisync')
if env["WITH_LIBS3"]:
    env.Install('$DESTDIR$PREFIX/bin','build/ori/oris3')
if env["WITH_HTTPD"]:
    env.Install('$DESTDIR$PREFIX/bin','build/ori_httpd/ori_httpd')
if env["WITH_ORILOCAL"]:
    env.Install('$DESTDIR$PREFIX/bin','build/orilocal/orilocal')

env.Install('$DESTDIR$PREFIX/share/man/man1','docs/ori.1')
env.Install('$DESTDIR$PREFIX/share/man/man1','docs/orifs.1')
env.Install('$DESTDIR$PREFIX/share/man/man1','docs/orisync.1')
env.Install('$DESTDIR$PREFIX/share/man/man1','docs/oridbg.1')

env.Alias('install','$DESTDIR$PREFIX')

