#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "localrepo.h"
#include "backup.h"

extern LocalRepo repository;

int
cmd_backup(int argc, const char *argv[])
{
    if (argc < 2) {
        printf("Usage: ori backup configfile.ini\n");
        return 1;
    }

    std::string configFileName = argv[1];
    boost::property_tree::ptree config;
    boost::property_tree::ini_parser::read_ini(configFileName, config);

    BackupService *backup = NULL;
    if (config.get<std::string>("backup.method") == "s3") {
        return 0;
    }
    else {
        fprintf(stderr, "Backup method %s not supported\n",
                config.get<std::string>("backup.method").c_str());
        return 1;
    }
}
