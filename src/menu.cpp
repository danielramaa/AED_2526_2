/**
 * @file menu.cpp
 * @brief Implementation of the interactive menu and batch entry point.
 *
 * The algorithmic stages (web construction, interference graph coloring, ...)
 * are not yet implemented; this file wires up the input/output skeleton on
 * top of the parser so that the rest of the team can plug the algorithms in
 * without touching the I/O layer.
 */
#include "menu.h"

#include "allocator.h"
#include "output.h"
#include "parser.h"
#include "types.h"
#include "web.h"

#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace {

/// In-memory state shared by the interactive menu's options.
struct AppState {
    std::string rangesFile;
    std::string registersFile;
    std::string outputFile;
    std::vector<Variable> variables;
    RegisterConfig config;
    bool variablesLoaded = false;
    bool configLoaded = false;
};

void printHeader(std::ostream& os) {
    os << "\n=== Compiler Register Allocation Tool ===\n";
}

void printSettings(std::ostream& os, const AppState& s) {
    os << "  ranges file   : " << (s.rangesFile.empty()    ? "(unset)" : s.rangesFile)    << "\n"
       << "  registers file: " << (s.registersFile.empty() ? "(unset)" : s.registersFile) << "\n"
       << "  output file   : " << (s.outputFile.empty()    ? "(unset)" : s.outputFile)    << "\n"
       << "  variables loaded: " << (s.variablesLoaded ? "yes" : "no")
       << " | config loaded: "   << (s.configLoaded    ? "yes" : "no") << "\n";
}

void printMenu(std::ostream& os) {
    os << "\n"
       << "  1) Set ranges file\n"
       << "  2) Set registers file\n"
       << "  3) Set output file\n"
       << "  4) Show current settings\n"
       << "  5) Load / parse input files\n"
       << "  6) View parsed data\n"
       << "  7) Run register allocation\n"
       << "  8) Show interference graph (ASCII)\n"
       << "  9) Export interference graph (DOT)\n"
       << "  0) Exit\n"
       << "Choice> ";
}

std::string promptLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cin.clear();
        line.clear();
    }
    return line;
}

void dumpVariables(std::ostream& os, const std::vector<Variable>& vars) {
    os << "Parsed " << vars.size() << " variable(s):\n";
    for (const auto& v : vars) {
        os << "  " << v.name << "  (" << v.ranges.size() << " range(s))\n";
        for (std::size_t i = 0; i < v.ranges.size(); ++i) {
            os << "    range " << i << ":";
            for (const auto& p : v.ranges[i].points) {
                os << " " << p.line;
                if (p.isDef)     os << "+";
                if (p.isLastUse) os << "-";
            }
            os << "\n";
        }
    }
}

void dumpConfig(std::ostream& os, const RegisterConfig& c) {
    os << "Register configuration:\n"
       << "  registers: " << c.numRegisters << "\n"
       << "  algorithm: " << algorithmKindName(c.algorithm);
    if (c.hasParam) os << ", " << c.algorithmParam;
    os << "\n";
}

bool loadInputs(AppState& s, std::ostream& err) {
    if (s.rangesFile.empty()) {
        err << "Error: ranges file is not set.\n";
        return false;
    }
    if (s.registersFile.empty()) {
        err << "Error: registers file is not set.\n";
        return false;
    }
    std::string errMsg;
    s.variables.clear();
    if (!parseRanges(s.rangesFile, s.variables, errMsg)) {
        err << "Error parsing ranges file: " << errMsg << "\n";
        s.variablesLoaded = false;
        return false;
    }
    s.variablesLoaded = true;

    s.config = RegisterConfig{};
    if (!parseRegisters(s.registersFile, s.config, errMsg)) {
        err << "Error parsing registers file: " << errMsg << "\n";
        s.configLoaded = false;
        return false;
    }
    s.configLoaded = true;
    return true;
}

void dumpAllocationSummary(std::ostream& os, const AllocationResult& res) {
    os << "Allocation: " << (res.feasible ? "FEASIBLE" : "INFEASIBLE")
       << " (" << res.message << ")\n"
       << "  webs:      " << res.webs.size() << "\n"
       << "  registers: " << res.registersUsed << "\n";
    for (const auto& a : res.assignments) {
        if (a.registerId < 0) os << "    M  -> web" << a.webId << "\n";
        else                  os << "    r" << a.registerId << " -> web" << a.webId << "\n";
    }
}

/**
 * Build the InputData, dispatch to the right algorithm, write the output
 * file and report a summary to @p err. Returns the exit-code-style result
 * (0 success, 1 infeasible / I/O error).
 */
int runAllocation(const AppState& s, std::ostream& err) {
    if (!s.variablesLoaded || !s.configLoaded) {
        err << "Error: load the input files first (option 5).\n";
        return 1;
    }
    InputData input;
    input.variables = s.variables;
    input.config = s.config;

    AllocationResult result = allocate(input);

    if (!s.outputFile.empty()) {
        if (!writeAllocationToFile(s.outputFile, result)) {
            err << "Error: could not open output file " << s.outputFile << "\n";
            return 1;
        }
    } else {
        // No output file configured in interactive mode: still show the
        // formatted output so the user can inspect it.
        writeAllocation(std::cout, result);
    }
    dumpAllocationSummary(err, result);

    if (!result.feasible) {
        // Per §3.5, when allocation is infeasible we must also issue a
        // warning to the console.
        err << "Warning: register allocation infeasible — webs spilled to memory.\n";
        return 1;
    }
    return 0;
}

void handleInteractiveChoice(AppState& s, const std::string& choice, bool& shouldExit) {
    if      (choice == "1") s.rangesFile    = promptLine("Path to ranges file: ");
    else if (choice == "2") s.registersFile = promptLine("Path to registers file: ");
    else if (choice == "3") s.outputFile    = promptLine("Path to output file: ");
    else if (choice == "4") printSettings(std::cout, s);
    else if (choice == "5") {
        if (loadInputs(s, std::cerr)) {
            std::cout << "Inputs parsed successfully.\n";
        }
    }
    else if (choice == "6") {
        if (!s.variablesLoaded || !s.configLoaded) {
            std::cout << "Inputs are not loaded yet (use option 5).\n";
        } else {
            dumpVariables(std::cout, s.variables);
            dumpConfig(std::cout, s.config);
        }
    }
    else if (choice == "7") runAllocation(s, std::cerr);
    else if (choice == "8") {
        if (!s.variablesLoaded) {
            std::cout << "Inputs are not loaded yet (use option 5).\n";
        } else {
            auto webs = buildWebs(s.variables);
            std::cout << "\nInterference graph (ASCII):\n";
            printGraphAscii(std::cout, webs);
        }
    }
    else if (choice == "9") {
        if (!s.variablesLoaded) {
            std::cout << "Inputs are not loaded yet (use option 5).\n";
        } else {
            std::string path = promptLine("DOT output file (e.g. graph.dot): ");
            if (path.empty()) {
                std::cout << "No file given; aborting export.\n";
            } else {
                std::ofstream out(path);
                if (!out) {
                    std::cerr << "Error: could not open " << path << "\n";
                } else {
                    auto webs = buildWebs(s.variables);
                    writeGraphDot(out, webs);
                    std::cout << "DOT written to " << path << "\n"
                              << "Render with: dot -Tpng " << path
                              << " -o " << path << ".png\n";
                }
            }
        }
    }
    else if (choice == "0") shouldExit = true;
    else std::cout << "Unknown choice: \"" << choice << "\"\n";
}

} // namespace

void runInteractiveMenu() {
    AppState state;
    printHeader(std::cout);
    std::cout << "Interactive mode. Type 0 to exit.\n";

    bool done = false;
    while (!done) {
        printMenu(std::cout);
        std::string choice;
        if (!std::getline(std::cin, choice)) break; // EOF
        // Strip leading whitespace.
        std::size_t i = 0;
        while (i < choice.size() && std::isspace(static_cast<unsigned char>(choice[i]))) ++i;
        choice.erase(0, i);
        if (choice.empty()) continue;
        handleInteractiveChoice(state, choice, done);
    }
}

int runBatchMode(const std::string& rangesFile,
                 const std::string& registersFile,
                 const std::string& outputFile) {
    AppState state;
    state.rangesFile    = rangesFile;
    state.registersFile = registersFile;
    state.outputFile    = outputFile;
    if (!loadInputs(state, std::cerr)) return 1;
    return runAllocation(state, std::cerr);
}
