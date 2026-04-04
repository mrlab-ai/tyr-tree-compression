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

#ifndef TYR_FORMALISM_PLANNING_ATOM_HPP_
#define TYR_FORMALISM_PLANNING_ATOM_HPP_

#include "tyr/common/comparators.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/invariants/term.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/unification/structure_traits.hpp"
#include "tyr/formalism/unification/structure_traits_impl.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <variant>
#include <vector>

namespace tyr::formalism::planning::invariant
{
struct TempAtom
{
    PredicateView<FluentTag> predicate;
    std::vector<Data<Term>> terms;

    auto identifying_members() const noexcept { return std::tie(predicate, terms); }

    friend bool operator==(const TempAtom& lhs, const TempAtom& rhs) { return lhs.identifying_members() == rhs.identifying_members(); }

    friend bool operator<(const TempAtom& lhs, const TempAtom& rhs) { return lhs.identifying_members() < rhs.identifying_members(); }
};

using TempAtomList = std::vector<TempAtom>;

inline TempAtom make_temp_atom(AtomView<FluentTag> element)
{
    auto terms = std::vector<Data<Term>> {};
    terms.reserve(element.get_terms().size());

    for (const auto term : element.get_terms())
        terms.push_back(make_temp_term(term));

    return TempAtom {
        .predicate = element.get_predicate(),
        .terms = std::move(terms),
    };
}

inline TempAtom make_temp_effect_atom(AtomView<FluentTag> element, size_t num_action_variables)
{
    auto terms = std::vector<Data<Term>> {};
    terms.reserve(element.get_terms().size());

    for (const auto term : element.get_terms())
        terms.push_back(make_temp_effect_term(term, num_action_variables));

    return TempAtom {
        .predicate = element.get_predicate(),
        .terms = std::move(terms),
    };
}

}

namespace tyr::formalism::unification
{

template<>
struct structure_traits<tyr::formalism::planning::invariant::TempAtom>
{
    using T = tyr::formalism::planning::invariant::TempAtom;

    template<typename F>
    static bool zip_terms(const T& lhs, const T& rhs, F&& f)
    {
        if (lhs.predicate != rhs.predicate)
            return false;

        if (lhs.terms.size() != rhs.terms.size())
            return false;

        for (size_t i = 0; i < lhs.terms.size(); ++i)
        {
            if (!structure_traits<Data<Term>>::zip_terms(lhs.terms[i], rhs.terms[i], f))
                return false;
        }

        return true;
    }

    template<typename F>
    static T transform_terms(const T& value, F&& f)
    {
        auto terms = std::vector<Data<Term>> {};
        terms.reserve(value.terms.size());

        for (const auto& term : value.terms)
            terms.push_back(structure_traits<Data<Term>>::transform_terms(term, f));

        return T {
            .predicate = value.predicate,
            .terms = std::move(terms),
        };
    }
};

}  // namespace tyr::formalism::unification

#endif
