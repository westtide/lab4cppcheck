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

#ifndef CPPCHECK_UNINITTRACE_H
#define CPPCHECK_UNINITTRACE_H

#include <string>

class Token;
class TokenList;
class Variable;

namespace UninitTrace {
bool enabled();
bool matchVar(const std::string &varName, const std::string &memberName);
void log(const TokenList *tokenList,
         const char *phase,
         const char *event,
         const Token *tok,
         const Variable *var,
         const std::string &membervar,
         const char *stateBefore,
         const char *stateAfter,
         const char *reason,
         const char *scopeName);
}

#endif // CPPCHECK_UNINITTRACE_H
