/**
 * @file menu.h
 * @brief Two top-level entry points of the tool: an interactive text menu and
 *        a non-interactive batch mode driven by command-line arguments.
 */
#ifndef DA_PRJ2_MENU_H
#define DA_PRJ2_MENU_H

#include <string>

/**
 * @brief Runs the interactive command-line menu until the user chooses to
 *        exit. State (selected file paths, parsed data) is kept inside the
 *        function across menu iterations.
 */
void runInteractiveMenu();

/**
 * @brief Runs the tool in batch mode.
 *
 * Matches the invocation described.
 * `myProg -b ranges.txt registers.txt allocation.txt`.
 *
 * Parses both inputs, attempts the allocation requested by the
 * registers file, writes the result (or an `M:`-only output) to
 * @p outputFile, and emits error / status messages to stderr.
 *
 * @return process exit code: 0 on success, non-zero on failure.
 */
int runBatchMode(const std::string& rangesFile,
                 const std::string& registersFile,
                 const std::string& outputFile);

#endif // DA_PRJ2_MENU_H
