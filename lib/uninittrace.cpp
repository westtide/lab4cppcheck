/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2025 Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "uninittrace.h"

#include "symboldatabase.h"
#include "token.h"
#include "tokenlist.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace {
const char kTraceEnv[] = "CPPCHECK_UNINIT_TRACE";
const char kTraceVarEnv[] = "CPPCHECK_UNINIT_TRACE_VAR";

bool traceEnvEnabled()
{
    const char *value = std::getenv(kTraceEnv);
    if (!value || value[0] == '\0')
        return false;
    return std::string(value) != "0";
}

const std::string &traceVarFilter()
{
    static const std::string filter = []() {
        const char *value = std::getenv(kTraceVarEnv);
        return value ? std::string(value) : std::string();
    }();
    return filter;
}
}

bool UninitTrace::enabled()
{
    static const bool enabledCached = traceEnvEnabled();
    return enabledCached;
}

bool UninitTrace::matchVar(const std::string &varName, const std::string &memberName)
{
    const std::string &filter = traceVarFilter();
    if (filter.empty())
        return true;
    if (varName == filter)
        return true;
    if (!memberName.empty() && varName + "." + memberName == filter)
        return true;
    return false;
}

void UninitTrace::log(const TokenList *tokenList,
                      const char *phase,
                      const char *event,
                      const Token *tok,
                      const Variable *var,
                      const std::string &membervar,
                      const char *stateBefore,
                      const char *stateAfter,
                      const char *reason,
                      const char *scopeName)
{
    if (!enabled())
        return;

    std::string varName = "-";
    if (var && var->nameToken())
        varName = var->nameToken()->str();

    if (!matchVar(varName, membervar))
        return;

    std::string fullVar = varName;
    if (!membervar.empty())
        fullVar += "." + membervar;

    std::string loc = "unknown";
    if (tokenList && tok)
        loc = tokenList->fileLine(tok);

    std::ostringstream out;
    out << "phase=" << (phase ? phase : "-")
        << " event=" << (event ? event : "-")
        << " var=" << fullVar
        << " varid=";

    if (var)
        out << var->declarationId();
    else
        out << "-";

    out << " loc=" << loc;

    if (stateBefore && stateBefore[0] != '\0')
        out << " state_before=" << stateBefore;
    if (stateAfter && stateAfter[0] != '\0')
        out << " state_after=" << stateAfter;
    if (reason && reason[0] != '\0')
        out << " reason=" << reason;
    if (scopeName && scopeName[0] != '\0')
        out << " scope=" << scopeName;

    out << '\n';
    std::fputs(out.str().c_str(), stderr);
}
