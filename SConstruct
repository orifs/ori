import sys
import os
import multiprocessing

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

# Add pkg-config options (TODO)
if sys.platform != "win32":
    env.ParseConfig('pkg-config --libs --cflags libevent')

if sys.platform == "freebsd9" or sys.platform == "freebsd8":
    env.Append(LIBS = ['execinfo'])

if sys.platform == "darwin":
    # OpenSSL needs to be overridden on OS X
    env.Append(LIBPATH=['/usr/local/Cellar/openssl/1.0.1c/lib'],
               CPPPATH=['/usr/local/Cellar/openssl/1.0.1c/include'])

# Configuration
conf = env.Configure()

if env["COMPRESSION_ALGO"] == "LZMA":
    if not conf.CheckLibWithHeader('lzma',
                                   'lzma.h',
                                   'C',
                                   'lzma_version_string();'):
        print 'Please install liblzma'
        Exit(1)

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

conf.Finish()

Export('env')

# libori
if env["WITH_LIBS3"] == "1":
    env.Append(CPPPATH = '#libs3-2.0/inc')

SConscript('libori/SConscript', variant_dir='build/libori')

# Set compile options for binaries
env.Append(LIBS = ["ori"],
           CPPPATH = ['#public'],
           LIBPATH = ['#build/libori'])

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

# Installation Targets

# Ori Utilities
SConscript('ori/SConscript', variant_dir='build/ori')
if env["WITH_LIBS3"] == "1":
    SConscript('oris3/SConscript', variant_dir='build/oris3')
if env["WITH_FUSE"] == "1":
    SConscript('mount_ori/SConscript', variant_dir='build/mount_ori')
    SConscript('mount_oring/SConscript', variant_dir='build/mount_oring')
if env["WITH_HTTPD"] == "1":
    SConscript('ori_httpd/SConscript', variant_dir='build/ori_httpd')

if env["WITH_FUSE"] == "1":
    env.Install('$PREFIX','build/mount_ori/mount_ori')
env.Install('$PREFIX','build/ori/ori')
if env["WITH_LIBS3"] == "1":
    env.Install('$PREFIX','build/ori/oris3')
if env["WITH_HTTPD"] == "1":
    env.Install('$PREFIX','build/ori_httpd/ori_httpd')
env.Alias('install','$PREFIX')

