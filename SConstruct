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
env.Append(CPPFLAGS = "-D_FILE_OFFSET_BITS=64")
env.Append(CPPPATH = [ "/usr/local/include" ])
env.Append(LIBPATH = [ "$LIBPATH", "/usr/local/lib", "/usr/local/lib/event2" ])

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

Export('env')

if env["WITH_FUSE"] == "1":
    SConscript('mount_ori/SConscript', variant_dir='build/mount_ori')
SConscript('libori/SConscript', variant_dir='build/libori')
SConscript('ori/SConscript', variant_dir='build/ori')

env.Install('$PREFIX','build/ori/ori')
env.Alias('install','$PREFIX')

