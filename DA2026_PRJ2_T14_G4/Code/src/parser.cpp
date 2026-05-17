/**
 * @file parser.cpp
 * @brief Implementation of the live-ranges and register-configuration parsers.
 */
#include "parser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {

/// Trim leading/trailing ASCII whitespace from @p s in place.
void trim(std::string& s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
}

/// True when @p s is empty or starts (after trimming) with `#`.
bool isCommentOrBlank(const std::string& s) {
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        return c == '#';
    }
    return true;
}

/**
 * Parse a single `tok` of the form `<digits>[+][-]` (markers may appear in
 * either order, although the spec only shows the trailing `+` / `-` form).
 * Returns false if the token is malformed.
 */
bool parseToken(const std::string& tok, ProgramPoint& out) {
    if (tok.empty()) return false;
    out = ProgramPoint{};

    std::size_t i = 0;
    // optional leading sign? No — line numbers are non-negative.
    std::string digits;
    while (i < tok.size() && std::isdigit(static_cast<unsigned char>(tok[i]))) {
        digits.push_back(tok[i++]);
    }
    if (digits.empty()) return false;

    // Trailing markers: '+' and/or '-' in any combination.
    for (; i < tok.size(); ++i) {
        if (tok[i] == '+')       out.isDef = true;
        else if (tok[i] == '-')  out.isLastUse = true;
        else return false;
    }

    try {
        out.line = std::stoi(digits);
    } catch (...) {
        return false;
    }
    if (out.line < 0) return false;
    return true;
}

}
bool parseRanges(const std::string& path,
                 std::vector<Variable>& variables,
                 std::string& error) {
    variables.clear();
    error.clear();

    std::ifstream in(path);
    if (!in) {
        error = "could not open ranges file: " + path;
        return false;
    }

    // Maintain insertion order while also offering O(1) lookup by name.
    std::unordered_map<std::string, std::size_t> indexByName;

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (isCommentOrBlank(line)) continue;

        // Strip an optional trailing inline comment introduced by `#`.
        auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);

        auto colon = line.find(':');
        if (colon == std::string::npos) {
            std::ostringstream oss;
            oss << "line " << lineNo << ": missing ':' separator";
            error = oss.str();
            return false;
        }

        std::string name = line.substr(0, colon);
        std::string rest = line.substr(colon + 1);
        trim(name);
        if (name.empty()) {
            std::ostringstream oss;
            oss << "line " << lineNo << ": empty variable name";
            error = oss.str();
            return false;
        }

        LiveRange range;
        std::string tok;
        std::istringstream iss(rest);
        while (std::getline(iss, tok, ',')) {
            trim(tok);
            if (tok.empty()) continue;
            ProgramPoint pp;
            if (!parseToken(tok, pp)) {
                std::ostringstream oss;
                oss << "line " << lineNo << ": malformed token \"" << tok << "\"";
                error = oss.str();
                return false;
            }
            range.points.push_back(pp);
        }

        if (range.points.empty()) {
            std::ostringstream oss;
            oss << "line " << lineNo << ": variable \"" << name
                << "\" has no program points";
            error = oss.str();
            return false;
        }

        auto it = indexByName.find(name);
        if (it == indexByName.end()) {
            indexByName.emplace(name, variables.size());
            variables.push_back(Variable{name, {std::move(range)}});
        } else {
            variables[it->second].ranges.push_back(std::move(range));
        }
    }

    if (variables.empty()) {
        error = "ranges file contained no variables";
        return false;
    }
    return true;
}

namespace {

/// Lowercase ASCII copy of @p s (used for case-insensitive algorithm names).
std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool parseAlgorithm(const std::string& value,
                    AlgorithmKind& kind,
                    int& param,
                    bool& hasParam,
                    std::string& error) {
    // value is everything to the right of `algorithm:`, possibly comma-separated.
    auto comma = value.find(',');
    std::string nameRaw = (comma == std::string::npos) ? value : value.substr(0, comma);
    std::string paramRaw = (comma == std::string::npos) ? std::string() : value.substr(comma + 1);
    trim(nameRaw);
    trim(paramRaw);

    std::string name = toLower(nameRaw);
    if      (name == "basic")     kind = AlgorithmKind::BASIC;
    else if (name == "spilling")  kind = AlgorithmKind::SPILLING;
    else if (name == "splitting") kind = AlgorithmKind::SPLITTING;
    else if (name == "free")      kind = AlgorithmKind::FREE;
    else {
        error = "unknown algorithm: \"" + nameRaw + "\"";
        return false;
    }

    hasParam = !paramRaw.empty();
    if (hasParam) {
        try {
            std::size_t consumed = 0;
            param = std::stoi(paramRaw, &consumed);
            if (consumed != paramRaw.size() || param < 0) {
                error = "invalid algorithm parameter: \"" + paramRaw + "\"";
                return false;
            }
        } catch (...) {
            error = "invalid algorithm parameter: \"" + paramRaw + "\"";
            return false;
        }
    } else {
        param = 0;
    }
    return true;
}

} 

bool parseRegisters(const std::string& path,
                    RegisterConfig& config,
                    std::string& error) {
    error.clear();
    config = RegisterConfig{};

    std::ifstream in(path);
    if (!in) {
        error = "could not open registers file: " + path;
        return false;
    }

    bool sawRegisters = false;
    bool sawAlgorithm = false;

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (isCommentOrBlank(line)) continue;

        auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);

        auto colon = line.find(':');
        if (colon == std::string::npos) {
            std::ostringstream oss;
            oss << "line " << lineNo << ": missing ':' separator";
            error = oss.str();
            return false;
        }

        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        trim(key);
        trim(value);
        std::string keyLc = toLower(key);

        if (keyLc == "registers") {
            try {
                std::size_t consumed = 0;
                int n = std::stoi(value, &consumed);
                if (consumed != value.size() || n <= 0) {
                    std::ostringstream oss;
                    oss << "line " << lineNo << ": invalid register count \""
                        << value << "\"";
                    error = oss.str();
                    return false;
                }
                config.numRegisters = n;
                sawRegisters = true;
            } catch (...) {
                std::ostringstream oss;
                oss << "line " << lineNo << ": invalid register count \""
                    << value << "\"";
                error = oss.str();
                return false;
            }
        } else if (keyLc == "algorithm") {
            std::string subErr;
            if (!parseAlgorithm(value, config.algorithm, config.algorithmParam,
                                config.hasParam, subErr)) {
                std::ostringstream oss;
                oss << "line " << lineNo << ": " << subErr;
                error = oss.str();
                return false;
            }
            sawAlgorithm = true;
        } else {
            std::ostringstream oss;
            oss << "line " << lineNo << ": unknown key \"" << key << "\"";
            error = oss.str();
            return false;
        }
    }

    if (!sawRegisters) { error = "missing 'registers:' entry"; return false; }
    if (!sawAlgorithm) { error = "missing 'algorithm:' entry"; return false; }

    // Semantic check: spilling/splitting must provide a parameter.
    if ((config.algorithm == AlgorithmKind::SPILLING ||
         config.algorithm == AlgorithmKind::SPLITTING) && !config.hasParam) {
        error = "algorithm \"" + algorithmKindName(config.algorithm) +
                "\" requires a numeric parameter (e.g. \"" +
                algorithmKindName(config.algorithm) + ", 2\")";
        return false;
    }
    return true;
}
