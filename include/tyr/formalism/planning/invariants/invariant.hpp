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

#ifndef TYR_FORMALISM_PLANNING_INVARIANTS_INVARIANT_HPP_
#define TYR_FORMALISM_PLANNING_INVARIANTS_INVARIANT_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/grounder.hpp"
#include "tyr/formalism/planning/planning_task.hpp"
#include "tyr/formalism/planning/repository.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <unordered_set>
#include <variant>
#include <vector>

namespace tyr::formalism::planning::invariant
{
struct Invariant
{
    VariableViewList variables;
    size_t quantified_arity;
    AtomViewList<FluentTag> atoms;
    UnorderedSet<PredicateView<FluentTag>> predicates;
};

using InvariantList = std::vector<Invariant>;

// Fig 7
bool covers(const Invariant& inv, AtomView<FluentTag> element)
{
    for (const auto atom : inv.atoms)
    {
        // TODO
        // if the counted variables in ϕ (those not in V )
        // can be renamed so that ϕ = ψ:
        // return true.
    }

    return false;
}

bool is_operator_too_heavy(const ActionView& op, const Invariant& inv)
{
    // TODO:
    // Let o′ be a copy of o.
    // Duplicate all (non-trivially) quantified effects of o′ .
    // Assign unique names to all quantified variables in effects of o′ .

    for (const auto conj_eff_lhs : op.get_effects())
    {
        for (const auto conj_eff_rhs : op.get_effects())
        {
            for (const auto lit_lhs : conj_eff_lhs.get_effect().get_literals())
            {
                if (!lit_lhs.get_polarity())
                    continue;

                if (!inv.predicates.contains(lit_lhs.get_atom().get_predicate()))
                    continue;

                for (const auto lit_rhs : conj_eff_rhs.get_effect().get_literals())
                {
                    if (!lit_rhs.get_polarity())
                        continue;

                    if (!inv.predicates.contains(lit_rhs.get_atom().get_predicate()))
                        continue;

                    // TODO:
                    // if the parameters of operator o′ can be renamed so that
                    // (e.atom 6= e′ .atom and
                    // covers(V , Φ, e.atom) and covers(V , Φ, e′ .atom) and
                    // o′ .precond ∧ e.cond ∧ e′ .cond ∧ ¬e.atom ∧ ¬e′ .atom
                    // is satisfiable):
                    // return true. { The operator is too heavy. }
                }
            }
        }
    }

    return false;  // { The operator is not too heavy. }
}

bool is_add_effect_unbalanced(const ActionView& op, AtomView<FluentTag> e, const Invariant& inv)
{
    for (const auto conj_eff : op.get_effects())
    {
        // TODO:
        // Let o′ be a copy of o where the parameters are minimally renamed so that covers(V , Φ, e.atom) is true.

        for (const auto lit : conj_eff.get_effect().get_literals())
        {
            if (lit.get_polarity())
                continue;

            // TODO:
            // if the quantified variables of e′ can be renamed so that
            // (e.atom 6= e′ .atom and covers(V , Φ, e′ .atom) and
            // o′ .precond ∧ e.cond ∧ ¬e.atom |= e′ .cond ∧ e′ .atom):
            // return false. { e′ balances e. }
        }
    }

    return true;  // { The add effect is unbalanced. }
}

bool prove_invariant(const Invariant& inv, const ActionListView& ops)
{
    for (const auto op : ops)
    {
        if (is_operator_too_heavy(op, inv))
            return false;  // { Reject the candidate. }

        for (const auto conj_eff : op.get_effects())
        {
            for (const auto lit : conj_eff.get_effect().get_literals())
            {
                if (!lit.get_polarity())
                    continue;

                if (is_add_effect_unbalanced(op, lit.get_atom(), inv))
                    return false;  // { Reject the candidate. }
            }
        }
    }

    return true;  // { Accept the candidate. }
}

// Fig 8
void refine_candidate(const Invariant& inv, const ActionListView& ops)
{
    // TODO
    // Select some schematic operator o and add effect e such that
    // is-add-effect-unbalanced(o, e, V , Φ) returns true.
    // for each atom ϕ′ over variables from V and at most one other variable
    // for which covers(V , Φ, ϕ′ ) is not true:
    // Φ′ := Φ ∪ {ϕ′ }
    // Simplify Φ′ by removing atoms from Φ that are covered by ϕ′ .
    // (These cannot contribute to the weight of an instance of hV, Φ′i.)
    // Simplify Φ′ by removing unused parameters.
    // if not is-add-effect-unbalanced(o, e, V , Φ′ ):
    // Add hV, Φ′i to the set of invariant candidates.
}

// Fig 15 and 17
std::vector<GroundAtomViewList<FluentTag>> compute_mutex_groups(const InvariantList& invs, TaskView task);
}

#endif
