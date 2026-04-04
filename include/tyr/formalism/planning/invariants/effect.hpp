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

#ifndef TYR_FORMALISM_PLANNING_INVARIANTS_EFFECT_HPP_
#define TYR_FORMALISM_PLANNING_INVARIANTS_EFFECT_HPP_

#include "tyr/common/comparators.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/invariants/literal.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/unification/structure_traits.hpp"
#include "tyr/formalism/unification/structure_traits_impl.hpp"

namespace tyr::formalism::planning::invariant
{

struct TempEffect
{
    size_t num_action_variables;
    size_t num_effect_variables;
    TempLiteralList condition;
    TempAtomList add_effects;
    TempAtomList del_effects;
};

using TempEffectList = std::vector<TempEffect>;

inline TempEffect make_temp_effect(ConditionalEffectView element, size_t action_arity)
{
    TempEffect result {};

    result.num_action_variables = action_arity;
    result.num_effect_variables = element.get_arity();

    for (const auto lit : element.get_condition().get_literals<FluentTag>())
        result.condition.push_back(make_temp_effect_literal(lit, action_arity));

    for (const auto lit : element.get_effect().get_literals())
    {
        if (lit.get_polarity())
            result.add_effects.push_back(make_temp_effect_atom(lit.get_atom(), action_arity));
        else
            result.del_effects.push_back(make_temp_effect_atom(lit.get_atom(), action_arity));
    }

    return result;
}

}

namespace tyr::formalism::unification
{

template<>
struct structure_traits<tyr::formalism::planning::invariant::TempEffect>
{
    using T = tyr::formalism::planning::invariant::TempEffect;
    using Atom = tyr::formalism::planning::invariant::TempAtom;
    using Literal = tyr::formalism::planning::invariant::TempLiteral;

    template<typename F>
    static bool zip_terms(const T& lhs, const T& rhs, F&& f)
    {
        if (lhs.num_action_variables != rhs.num_action_variables)
            return false;
        if (lhs.num_effect_variables != rhs.num_effect_variables)
            return false;

        if (!detail::zip_vector_terms<Literal>(lhs.condition, rhs.condition, f))
            return false;
        if (!detail::zip_vector_terms<Atom>(lhs.add_effects, rhs.add_effects, f))
            return false;
        if (!detail::zip_vector_terms<Atom>(lhs.del_effects, rhs.del_effects, f))
            return false;

        return true;
    }

    template<typename F>
    static T transform_terms(const T& value, F&& f)
    {
        return T {
            .num_action_variables = value.num_action_variables,
            .num_effect_variables = value.num_effect_variables,
            .condition = detail::transform_vector_terms<Literal>(value.condition, f),
            .add_effects = detail::transform_vector_terms<Atom>(value.add_effects, f),
            .del_effects = detail::transform_vector_terms<Atom>(value.del_effects, f),
        };
    }
};

}  // namespace tyr::formalism::unification

#endif
