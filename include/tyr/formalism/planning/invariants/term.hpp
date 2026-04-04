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

#ifndef TYR_FORMALISM_PLANNING_INVARIANTS_TERM_HPP_
#define TYR_FORMALISM_PLANNING_INVARIANTS_TERM_HPP_

#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/unification/structure_traits.hpp"
#include "tyr/formalism/unification/structure_traits_impl.hpp"

namespace tyr::formalism::planning::invariant
{

inline Data<Term> make_temp_term(TermView element) { return element.get_data(); }

inline Data<Term> make_temp_effect_term(TermView element, size_t /*num_action_variables*/) { return element.get_data(); }

}

namespace tyr::formalism::unification
{

template<>
struct structure_traits<Data<Term>>
{
    template<typename F>
    static bool zip_terms(const Data<Term>& lhs, const Data<Term>& rhs, F&& f)
    {
        return f(lhs, rhs);
    }

    template<typename F>
    static Data<Term> transform_terms(const Data<Term>& value, F&& f)
    {
        return f(value);
    }
};

}  // namespace tyr::formalism::unification

#endif
