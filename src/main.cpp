/**
 * @file main.cpp
 * @brief Application entry point. Dispatches to either the interactive menu
 *        or the batch mode based on the command-line arguments.
 *
 * Batch invocation (§4.1 of the handout):
 *     myProg -b ranges.txt registers.txt allocation.txt
 *
 * Anything else (no arguments, or arguments without -b) starts the
 * interactive text menu.
 */
#include "menu.h"

#include <iostream>
#include <string>

namespace {

void printUsage(std::ostream& os, const char* progName) {
    os << "Usage:\n"
       << "  " << progName << "                                  # interactive menu\n"
       << "  " << progName << " -b ranges.txt registers.txt allocation.txt\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc >= 2 && std::string(argv[1]) == "-b") {
        if (argc != 5) {
            std::cerr << "Error: batch mode expects exactly 3 file arguments.\n";
            printUsage(std::cerr, argv[0]);
            return 2;
        }
        return runBatchMode(argv[2], argv[3], argv[4]);
    }
    if (argc >= 2) {
        // Unknown flag — show usage but still drop into the menu so the
        // executable remains demo-friendly.
        std::cerr << "Note: unrecognised argument \"" << argv[1]
                  << "\", starting interactive menu.\n";
        printUsage(std::cerr, argv[0]);
    }
    runInteractiveMenu();
    return 0;
}
