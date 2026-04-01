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

#include "initial_state.hpp"

#include "matching.hpp"

namespace tyr::formalism::planning::invariant
{
namespace
{

TempAtom make_temp_ground_atom(GroundAtomView<FluentTag> element)
{
    auto terms = std::vector<Data<Term>> {};
    const auto objects = element.get_row().get_objects();
    terms.reserve(objects.size());

    for (const auto object : objects)
        terms.emplace_back(Data<Term>(object.get_index()));

    return TempAtom {
        .predicate = element.get_predicate(),
        .terms = std::move(terms),
    };
}

bool rigid_bindings_compatible(const Invariant& inv, const InvariantSubstitution& lhs, const InvariantSubstitution& rhs)
{
    for (size_t p = 0; p < inv.num_rigid_variables; ++p)
    {
        const auto& lhs_binding = lhs.get(ParameterIndex(p));
        const auto& rhs_binding = rhs.get(ParameterIndex(p));

        if (lhs_binding.has_value() && rhs_binding.has_value() && *lhs_binding != *rhs_binding)
            return false;
    }

    return true;
}

struct InitialAtomMatch
{
    TempAtom atom;
    std::vector<InvariantSubstitution> substitutions;
};

std::optional<InitialAtomMatch> collect_initial_atom_matches(const Invariant& inv, GroundAtomView<FluentTag> atom_view)
{
    auto atom = make_temp_ground_atom(atom_view);
    auto substitutions = std::vector<InvariantSubstitution> {};

    for (const auto& pattern : inv.atoms)
    {
        auto sigma = match_invariant_against_ground_atom(inv, pattern, atom);
        if (sigma.has_value())
            substitutions.push_back(std::move(*sigma));
    }

    if (substitutions.empty())
        return std::nullopt;

    return InitialAtomMatch {
        .atom = std::move(atom),
        .substitutions = std::move(substitutions),
    };
}

bool initial_atoms_conflict(const Invariant& inv, const InitialAtomMatch& lhs, const InitialAtomMatch& rhs)
{
    for (const auto& sigma_lhs : lhs.substitutions)
    {
        for (const auto& sigma_rhs : rhs.substitutions)
        {
            if (rigid_bindings_compatible(inv, sigma_lhs, sigma_rhs))
                return true;
        }
    }

    return false;
}

}  // namespace

bool holds_in_initial_state(const Invariant& inv, GroundAtomListView<FluentTag> initial_atoms)
{
    auto covered_atoms = std::vector<InitialAtomMatch> {};
    covered_atoms.reserve(initial_atoms.size());

    for (const auto atom_view : initial_atoms)
    {
        auto match = collect_initial_atom_matches(inv, atom_view);
        if (match.has_value())
            covered_atoms.push_back(std::move(*match));
    }

    for (size_t i = 0; i < covered_atoms.size(); ++i)
    {
        for (size_t j = i + 1; j < covered_atoms.size(); ++j)
        {
            if (initial_atoms_conflict(inv, covered_atoms[i], covered_atoms[j]))
                return false;
        }
    }

    return true;
}

}  // namespace tyr::formalism::planning::invariant