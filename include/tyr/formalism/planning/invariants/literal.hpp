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

#ifndef TYR_FORMALISM_PLANNING_INVARIANTS_LITERAL_HPP_
#define TYR_FORMALISM_PLANNING_INVARIANTS_LITERAL_HPP_

#include "tyr/common/comparators.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/invariants/atom.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/unification/structure_traits.hpp"
#include "tyr/formalism/unification/structure_traits_impl.hpp"

#include <vector>

namespace tyr::formalism::planning::invariant
{

struct TempLiteral
{
    TempAtom atom;
    bool polarity;

    auto identifying_members() const noexcept { return std::tie(atom, polarity); }

    friend bool operator==(const TempLiteral& lhs, const TempLiteral& rhs) { return lhs.identifying_members() == rhs.identifying_members(); }

    friend bool operator<(const TempLiteral& lhs, const TempLiteral& rhs) { return lhs.identifying_members() < rhs.identifying_members(); }
};

using TempLiteralList = std::vector<TempLiteral>;

inline TempLiteral make_temp_literal(LiteralView<FluentTag> element)
{
    return TempLiteral {
        .atom = make_temp_atom(element.get_atom()),
        .polarity = element.get_polarity(),
    };
}

inline TempLiteral make_temp_effect_literal(LiteralView<FluentTag> element, size_t num_action_variables)
{
    return TempLiteral {
        .atom = make_temp_effect_atom(element.get_atom(), num_action_variables),
        .polarity = element.get_polarity(),
    };
}

}

namespace tyr::formalism::unification
{

template<>
struct structure_traits<tyr::formalism::planning::invariant::TempLiteral>
{
    using T = tyr::formalism::planning::invariant::TempLiteral;

    template<typename F>
    static bool zip_terms(const T& lhs, const T& rhs, F&& f)
    {
        if (lhs.polarity != rhs.polarity)
            return false;

        return structure_traits<tyr::formalism::planning::invariant::TempAtom>::zip_terms(lhs.atom, rhs.atom, f);
    }

    template<typename F>
    static T transform_terms(const T& value, F&& f)
    {
        return T {
            .atom = structure_traits<tyr::formalism::planning::invariant::TempAtom>::transform_terms(value.atom, f),
            .polarity = value.polarity,
        };
    }
};

}  // namespace tyr::formalism::unification

#endif
