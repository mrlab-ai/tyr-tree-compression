/*
 * Copyright (C) 2025-2026 Dominik Drexler
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TYR_SRC_FORMALISM_PLANNING_INVARIANTS_UTILS_HPP_
#define TYR_SRC_FORMALISM_PLANNING_INVARIANTS_UTILS_HPP_

#include "tyr/formalism/planning/invariants/atom.hpp"
#include "tyr/formalism/planning/invariants/invariant.hpp"
#include "tyr/formalism/planning/repository.hpp"

namespace tyr::formalism::planning::invariant
{

inline const TempAtom* find_part(const Invariant& inv, PredicateView<FluentTag> predicate)
{
    const auto it = std::find_if(inv.atoms.begin(), inv.atoms.end(), [&](const auto& atom) { return atom.predicate == predicate; });

    return (it == inv.atoms.end()) ? nullptr : &*it;
}

}

#endif
