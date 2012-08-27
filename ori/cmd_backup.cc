#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "localrepo.h"
#include "backup.h"

extern LocalRepo repository;

#define ORI_BACKUP "ori_backup"

std::string
hashKey(const ObjectHash &hash) {
    return std::string(ORI_BACKUP "/obj/") + hash.hex();
}

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
#ifdef USE_FAKES3
        if (argc >= 3) {
            ((S3BackupService*)backup.get())->setHostname(argv[2]);
        }
#endif
    }
    else {
        fprintf(stderr, "Backup method %s not supported\n",
                config.get<std::string>("backup.method").c_str());
        return 1;
    }

    // Do an incremental backup ("push")
    std::string bsCommitIDs;
    std::vector<Commit> pushedCommits;
    if (backup->hasKey(ORI_BACKUP "/meta/commitIDs")) {
        bool success = backup->getData(ORI_BACKUP "/meta/commitIDs", bsCommitIDs);
        if (!success) {
            return 1;
        }

        strstream ss(bsCommitIDs);
        std::set<ObjectHash> commitIDs;
        while (!ss.ended()) {
            ObjectHash hash;
            ss.readHash(hash);
            commitIDs.insert(hash);
        }

        std::vector<Commit> allCommits = repository.listCommits();
        for (size_t i = 0; i < allCommits.size(); i++) {
            if (commitIDs.find(allCommits[i].hash()) == commitIDs.end()) {
                pushedCommits.push_back(allCommits[i]);
            }
        }
    }
    else {
        // Push everything
        pushedCommits = repository.listCommits();
    }

    strwstream commitIdStream(bsCommitIDs);

    std::deque<ObjectHash> toPush;
    for (size_t i = 0; i < pushedCommits.size(); i++) {
        printf("Backing up commit %s\n", pushedCommits[i].hash().hex().c_str());
        toPush.push_back(pushedCommits[i].hash());

        while (toPush.size() > 0) {
            ObjectHash hash = toPush.front();
            toPush.pop_front();

            Object::sp obj = repository.getObject(hash);
            ObjectInfo::Type type = obj->getInfo().type;

            // Add more objects
            if (type == ObjectInfo::Commit) {
                if (repository.getMetadata().getMeta(hash, "status") == "purged")
                    continue;

                Commit c = repository.getCommit(hash);
                //if (!backup->hasKey(hashKey(c.getTree())))
                toPush.push_back(c.getTree());
            }
            else if (type == ObjectInfo::Tree) {
                Tree t = repository.getTree(hash);
                for (std::map<std::string, TreeEntry>::iterator it = t.tree.begin();
                        it != t.tree.end();
                        it++) {
                    const TreeEntry &te = (*it).second;
                    if (te.type != TreeEntry::Blob ||
                            !backup->hasKey(hashKey(te.hash)))
                        toPush.push_back(te.hash);
                }
            }
            else if (type == ObjectInfo::LargeBlob) {
                LargeBlob lb(&repository);
                lb.fromBlob(repository.getPayload(hash));
                for (std::map<uint64_t, LBlobEntry>::iterator it = lb.parts.begin();
                        it != lb.parts.end();
                        it++) {
                    const LBlobEntry &lbe = (*it).second;
                    if (!backup->hasKey(hashKey(lbe.hash)))
                        toPush.push_back(lbe.hash);
                }
            }

            // Push to backup
            if (!backup->putObj(hashKey(hash), obj)) {
                printf("Error putting %s\n", hash.hex().c_str());
            }
        }

        // Update commit IDs
        commitIdStream.writeHash(pushedCommits[i].hash());
        backup->putData(ORI_BACKUP "/meta/commitIDs", commitIdStream.str());
    }

    // Push metadata
    backup->putFile(ORI_BACKUP "/meta/metadata", ".ori/metadata");

    return 0;
}
