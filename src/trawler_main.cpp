#include "nb/trawler_service.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace {
void usage() {
    std::cerr <<
        "Usage: trawler <port> [topo file]\n"
        "port      : TCP port to listen on for emulated nodes\n"
        "topo file : optional topology script; when omitted all nodes are\n"
        "            neighbors of each other by default.\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Missing arguments" << std::endl;
        usage();
        return 1;
    }
    try {
        int port = std::stoi(argv[1]);
        std::string topoFile;
        if (argc >= 3) topoFile = argv[2];
        bool allToAll = topoFile.empty();
        nb::TrawlerService trawler(port, topoFile, allToAll);
        trawler.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception in trawler: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
