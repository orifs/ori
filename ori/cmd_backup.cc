#include <iostream>

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

    BackupService::sp backup;
    if (config.get<std::string>("backup.method") == "s3") {
        backup.reset(new S3BackupService(
                config.get<std::string>("s3.accessKeyID"),
                config.get<std::string>("s3.secretAccessKey"),
                config.get<std::string>("s3.bucketName")
                ));
    }
    else {
        fprintf(stderr, "Backup method %s not supported\n",
                config.get<std::string>("backup.method").c_str());
        return 1;
    }

    // TODO: testing
    std::cout << "Has file10: " << backup->hasKey("file10.tst") << std::endl;
    std::cout << "Has file11: " << backup->hasKey("file11.tst") << std::endl;

    backup->putObj("file10.tst",
            repository.getObject(
                ObjectHash::fromHex("d85426bf322eb68897f8fafc1e399d8ccf69b37a69e043108204fe8424294d07")));
    std::cout << "file10:\n" << backup->getObj("file10.tst")->getPayload() <<
        std::endl;

    backup->putFile("metadata", ".ori/metadata");

    return 0;
}
