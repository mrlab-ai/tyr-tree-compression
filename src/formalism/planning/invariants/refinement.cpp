/*
 * Copyright (C) 2025 Dominik Drexler
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

#include "refinement.hpp"

#include "matching.hpp"
#include "normalization.hpp"
#include "proof.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace tyr::formalism::planning::invariant
{
namespace
{

TempAtom make_initial_atom(PredicateView<FluentTag> predicate, size_t counted_position)
{
    const auto arity = static_cast<size_t>(predicate.get_arity());

    auto terms = std::vector<Data<Term>> {};
    terms.reserve(arity);

    if (counted_position == arity)
    {
        for (size_t i = 0; i < arity; ++i)
            terms.emplace_back(Data<Term>(ParameterIndex(i)));
    }
    else
    {
        const auto counted_index = ParameterIndex(arity - 1);

        for (size_t i = 0; i < arity; ++i)
        {
            if (i == counted_position)
            {
                terms.emplace_back(Data<Term>(counted_index));
            }
            else
            {
                const auto rigid_index = (i < counted_position) ? ParameterIndex(i) : ParameterIndex(i - 1);
                terms.emplace_back(Data<Term>(rigid_index));
            }
        }
    }

    return TempAtom {
        .predicate = predicate,
        .terms = std::move(terms),
    };
}

TempAtom make_refinement_atom(PredicateView<FluentTag> predicate, size_t num_rigid_variables, std::optional<size_t> counted_position)
{
    const auto arity = static_cast<size_t>(predicate.get_arity());

    auto terms = std::vector<Data<Term>> {};
    terms.reserve(arity);

    const bool has_counted = counted_position.has_value();
    const auto counted_index = ParameterIndex(num_rigid_variables);

    for (size_t i = 0; i < arity; ++i)
    {
        if (has_counted && i == *counted_position)
        {
            terms.emplace_back(Data<Term>(counted_index));
        }
        else
        {
            size_t rigid_slot = i;
            if (has_counted && i > *counted_position)
                --rigid_slot;

            terms.emplace_back(Data<Term>(ParameterIndex(rigid_slot)));
        }
    }

    return TempAtom {
        .predicate = predicate,
        .terms = std::move(terms),
    };
}

void try_refinement(InvariantList& result,
                    const Invariant& inv,
                    const TempAction& op,
                    const TempEffect& effect,
                    const TempAtom& add_atom,
                    TempAtom phi_prime,
                    size_t num_counted_variables)
{
    if (covers(inv, phi_prime))
        return;

    auto atoms = inv.atoms;
    atoms.push_back(std::move(phi_prime));

    auto refined = Invariant(inv.num_rigid_variables, num_counted_variables, std::move(atoms));

    normalize_invariant(refined);

    // if (!is_well_shaped_invariant(refined))
    //     return;

    if (!is_add_effect_unbalanced(op, effect, add_atom, refined))
        result.push_back(std::move(refined));
}

}  // namespace

InvariantList refine_candidate(const Invariant& inv, const Threat& threat, const TempActionList& ops, PredicateListView<FluentTag> predicates)
{
    InvariantList result;

    const auto& op = ops[threat.op_index];
    const auto& effect = op.effects[threat.effect_index];
    const auto& add_atom = effect.add_effects[threat.add_index];

    for (const auto predicate : predicates)
    {
        const auto arity = static_cast<size_t>(predicate.get_arity());

        if (arity == inv.num_rigid_variables)
        {
            try_refinement(result,
                           inv,
                           op,
                           effect,
                           add_atom,
                           make_refinement_atom(predicate, inv.num_rigid_variables, std::nullopt),
                           inv.num_counted_variables);
        }

        if (arity == inv.num_rigid_variables + 1)
        {
            for (size_t counted_position = 0; counted_position < arity; ++counted_position)
            {
                try_refinement(result,
                               inv,
                               op,
                               effect,
                               add_atom,
                               make_refinement_atom(predicate, inv.num_rigid_variables, counted_position),
                               std::max<size_t>(inv.num_counted_variables, 1));
            }
        }
    }

    return result;
}

InvariantList make_initial_candidates(PredicateListView<FluentTag> predicates)
{
    auto result = InvariantList {};

    for (const auto predicate : predicates)
    {
        const auto arity = static_cast<size_t>(predicate.get_arity());

        result.emplace_back(arity, 0, TempAtomList { make_initial_atom(predicate, arity) });

        for (size_t counted_position = 0; counted_position < arity; ++counted_position)
            result.emplace_back(arity - 1, 1, TempAtomList { make_initial_atom(predicate, counted_position) });
    }

    return result;
}

}  // namespace tyr::formalism::planning::invariant