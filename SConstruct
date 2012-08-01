import sys
import os

opts = Variables('Local.sc')

opts.AddVariables(
    ("CC", "C Compiler"),
    ("CXX", "C++ Compiler"),
    ("AS", "Assembler"),
    ("LINK", "Linker"),
    ("BUILDTYPE", "Build type (RELEASE or DEBUG)", "RELEASE"),
    ("VERBOSE", "Show full build information (0 or 1)", "0"),
    ("NUMCPUS", "Number of CPUs to use for build (0 means auto).", "0"),
    ("WITH_FUSE", "Include FUSE file system (0 or 1).", "1"),
    ("WITH_HTTPD", "Include HTTPD server (0 or 1).", "1"),
    ("WITH_MDNS", "Include Zeroconf (through DNS-SD) support (0 or 1).", "1"),
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
if sys.platform != "darwin":
    env.Append(CPPFLAGS = "-D_FILE_OFFSET_BITS=64")
    env.Append(LIBPATH = [ "/usr/local/lib/event2" ])

env.Append(CPPPATH = [ "/usr/local/include" ])
env.Append(LIBPATH = [ "$LIBPATH", "/usr/local/lib" ])

if env["WITH_MDNS"] != "1":
    env.Append(CPPFLAGS = [ "-DWITHOUT_MDNS" ])
    
if env["BUILDTYPE"] == "DEBUG":
    env.Append(CPPFLAGS = [ "-g", "-DDEBUG", "-Wall",
	"-Wno-deprecated-declarations" ])
elif env["BUILDTYPE"] == "RELEASE":
    env.Append(CPPFLAGS = "-DNDEBUG")
else:
    print "Error BUILDTYPE must be RELEASE or DEBUG"
    sys.exit(-1)

if env["VERBOSE"] == "0":
    env["CCCOMSTR"] = "Compiling $SOURCE"
    env["CXXCOMSTR"] = "Compiling $SOURCE"
    env["SHCCCOMSTR"] = "Compiling $SOURCE"
    env["SHCXXCOMSTR"] = "Compiling $SOURCE"
    env["ARCOMSTR"] = "Creating library $TARGET"
    env["LINKCOMSTR"] = "Linking $TARGET"

def GetNumCPUs():
    if env["NUMCPUS"] != "0":
        return int(env["NUMCPUS"])
    if os.sysconf_names.has_key("SC_NPROCESSORS_ONLN"):
        cpus = os.sysconf("SC_NPROCESSORS_ONLN")
        if isinstance(cpus, int) and cpus > 0:
            return 2*cpus
        else:
            return 2
    return 2*int(os.popen2("sysctl -n hw.ncpu")[1].read())

env.SetOption('num_jobs', GetNumCPUs())

# Add pkg-config options (TODO)
env.ParseConfig('pkg-config --libs --cflags libevent')

# Configuration
conf = env.Configure()

if not conf.CheckLibWithHeader('lzma','lzma.h','C','lzma_version_string();'):
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

# Targets
Export('env')

if env["WITH_FUSE"] == "1":
    SConscript('mount_ori/SConscript', variant_dir='build/mount_ori')
SConscript('libori/SConscript', variant_dir='build/libori')
SConscript('ori/SConscript', variant_dir='build/ori')
if env["WITH_HTTPD"] == "1":
    SConscript('ori_httpd/SConscript', variant_dir='build/ori_httpd')

env.Install('$PREFIX','build/ori/ori')
env.Alias('install','$PREFIX')

