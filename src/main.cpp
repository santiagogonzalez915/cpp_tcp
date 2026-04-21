#include "nb/emulator.hpp"
#include "nb/runtime_manager.hpp"
#include "nb/simulator.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <string>

namespace {
void usage() {
    std::cerr <<
        "Usage: netstack simulate <num nodes> <topo file> [command file] "
        "[timescale]\n"
        "   or: netstack emulate <trawler host> <trawler port> <local port> "
        "[command file]\n"
        "Arguments in <> are required and arguments in [] are optional.\n"
        "Command file is a file with commands for a node.\n"
        "Topo file is the topology file to use. It may also contain commands "
        "for a node.\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Missing arguments\n";
        usage();
        return 1;
    }

    std::string mode = argv[1];
    try {
        std::unique_ptr<nb::RuntimeManager> manager;
        std::string noFile = "-";

        if (mode == "simulate") {
            int numNodes = std::stoi(argv[2]);
            std::string topoFile = argv[3];
            auto sim = std::make_unique<nb::Simulator>(numNodes, topoFile);
            if (argc >= 5 && argv[4] != noFile) {
                if (sim->setCommandFile(argv[4]) != 0) {
                    std::cerr << "Could not open command file " << argv[4]
                              << std::endl;
                    return 1;
                }
            }
            if (argc >= 6) {
                sim->setTimescale(std::stod(argv[5]));
            }
            manager = std::move(sim);
        } else if (mode == "emulate") {
            if (argc < 5) {
                std::cerr << "Missing arguments to emulator\n";
                usage();
                return 1;
            }
            std::string trawlerHost = argv[2];
            int trawlerPort = std::stoi(argv[3]);
            int localUdp = std::stoi(argv[4]);
            auto emu = std::make_unique<nb::Emulator>(trawlerHost, trawlerPort,
                                                      localUdp);
            if (argc >= 6 && argv[5] != noFile) {
                if (emu->setCommandFile(argv[5]) != 0) {
                    std::cerr << "Could not open command file " << argv[5]
                              << std::endl;
                    return 1;
                }
            }
            manager = std::move(emu);
        } else {
            std::cerr << "Unknown mode: " << mode << "\n";
            usage();
            return 1;
        }
        manager->start();
    } catch (const std::exception& e) {
        std::cerr << "Exception occurred in netstack: " << e.what()
                  << std::endl;
        return 1;
    }
    return 0;
}
