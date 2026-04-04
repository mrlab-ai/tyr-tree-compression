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

#ifndef TYR_FORMALISM_PLANNING_INVARIANTS_INVARIANT_HPP_
#define TYR_FORMALISM_PLANNING_INVARIANTS_INVARIANT_HPP_

#include "tyr/common/comparators.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/grounder.hpp"
#include "tyr/formalism/planning/invariants/atom.hpp"
#include "tyr/formalism/planning/repository.hpp"

namespace tyr::formalism::planning::invariant
{

struct Invariant
{
    size_t num_rigid_variables = 0;
    size_t num_counted_variables = 0;
    TempAtomList atoms;
    UnorderedSet<PredicateView<FluentTag>> predicates;

    Invariant() = default;

    Invariant(size_t num_rigid_variables_, size_t num_counted_variables_, TempAtomList atoms_) :
        num_rigid_variables(num_rigid_variables_),
        num_counted_variables(num_counted_variables_),
        atoms(std::move(atoms_)),
        predicates()
    {
        canonicalize();
    }

    void canonicalize()
    {
        std::sort(atoms.begin(), atoms.end());
        atoms.erase(std::unique(atoms.begin(), atoms.end()), atoms.end());

        predicates.clear();
        for (const auto& atom : atoms)
            predicates.insert(atom.predicate);
    }

    auto identifying_members() const noexcept { return std::tie(num_rigid_variables, num_counted_variables, atoms); }

    friend bool operator==(const Invariant& lhs, const Invariant& rhs) { return lhs.identifying_members() == rhs.identifying_members(); }

    friend bool operator<(const Invariant& lhs, const Invariant& rhs) { return lhs.identifying_members() < rhs.identifying_members(); }
};

using InvariantList = std::vector<Invariant>;

}

namespace tyr::formalism::unification
{

template<>
struct structure_traits<tyr::formalism::planning::invariant::Invariant>
{
    using T = tyr::formalism::planning::invariant::Invariant;
    using Atom = tyr::formalism::planning::invariant::TempAtom;

    template<typename F>
    static bool zip_terms(const T& lhs, const T& rhs, F&& f)
    {
        if (lhs.num_rigid_variables != rhs.num_rigid_variables)
            return false;
        if (lhs.num_counted_variables != rhs.num_counted_variables)
            return false;

        return detail::zip_vector_terms<Atom>(lhs.atoms, rhs.atoms, f);
    }

    template<typename F>
    static T transform_terms(const T& value, F&& f)
    {
        T result;
        result.num_rigid_variables = value.num_rigid_variables;
        result.num_counted_variables = value.num_counted_variables;
        result.atoms = detail::transform_vector_terms<Atom>(value.atoms, f);

        result.predicates.clear();
        for (const auto& atom : result.atoms)
            result.predicates.insert(atom.predicate);

        return result;
    }
};

}  // namespace tyr::formalism::unification

#endif
