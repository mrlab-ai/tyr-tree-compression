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

#ifndef TYR_FORMALISM_PLANNING_INVARIANTS_ACTION_HPP_
#define TYR_FORMALISM_PLANNING_INVARIANTS_ACTION_HPP_

#include "tyr/common/comparators.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/grounder.hpp"
#include "tyr/formalism/planning/invariants/effect.hpp"
#include "tyr/formalism/planning/invariants/literal.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/unification/structure_traits.hpp"
#include "tyr/formalism/unification/structure_traits_impl.hpp"

namespace tyr::formalism::planning::invariant
{

struct TempAction
{
    size_t num_variables;
    TempLiteralList precondition;
    TempEffectList effects;
};

using TempActionList = std::vector<TempAction>;

inline TempAction make_temp_action(ActionView op)
{
    TempAction result {};

    result.num_variables = op.get_arity();

    for (const auto lit : op.get_condition().get_literals<FluentTag>())
        result.precondition.push_back(make_temp_literal(lit));

    result.effects.reserve(op.get_effects().size());
    for (const auto cond_eff : op.get_effects())
        result.effects.push_back(make_temp_effect(cond_eff, op.get_arity()));

    return result;
}

inline TempActionList make_temp_actions(ActionListView ops)
{
    auto result = TempActionList {};
    result.reserve(ops.size());

    for (const auto op : ops)
        result.push_back(make_temp_action(op));

    return result;
}

}

namespace tyr::formalism::unification
{

template<>
struct structure_traits<tyr::formalism::planning::invariant::TempAction>
{
    using T = tyr::formalism::planning::invariant::TempAction;
    using Literal = tyr::formalism::planning::invariant::TempLiteral;
    using Effect = tyr::formalism::planning::invariant::TempEffect;

    template<typename F>
    static bool zip_terms(const T& lhs, const T& rhs, F&& f)
    {
        if (lhs.num_variables != rhs.num_variables)
            return false;

        if (!detail::zip_vector_terms<Literal>(lhs.precondition, rhs.precondition, f))
            return false;
        if (!detail::zip_vector_terms<Effect>(lhs.effects, rhs.effects, f))
            return false;

        return true;
    }

    template<typename F>
    static T transform_terms(const T& value, F&& f)
    {
        return T {
            .num_variables = value.num_variables,
            .precondition = detail::transform_vector_terms<Literal>(value.precondition, f),
            .effects = detail::transform_vector_terms<Effect>(value.effects, f),
        };
    }
};

}  // namespace tyr::formalism::unification

#endif
