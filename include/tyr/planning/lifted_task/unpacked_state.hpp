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

#ifndef TYR_PLANNING_LIFTED_TASK_UNPACKED_STATE_HPP_
#define TYR_PLANNING_LIFTED_TASK_UNPACKED_STATE_HPP_

#include "tyr/common/config.hpp"
#include "tyr/common/dynamic_bitset.hpp"
#include "tyr/common/vector.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/lifted_task/state_iterators.hpp"
#include "tyr/planning/state_index.hpp"
#include "tyr/planning/state_iterators.hpp"
#include "tyr/planning/unpacked_state.hpp"

#include <boost/dynamic_bitset.hpp>
#include <vector>

namespace tyr::planning
{
template<>
class UnpackedState<LiftedTask>
{
public:
    using TaskType = LiftedTask;

    UnpackedState() = default;

    Index<State<LiftedTask>> get_index() const;
    void set(Index<State<LiftedTask>> index);

    void clear();
    void clear_unextended_part();
    void clear_extended_part();
    void assign_unextended_part(const UnpackedState<LiftedTask>& other);

    /**
     * UnpackedStateConcept
     */

    formalism::planning::FDRValue get(Index<formalism::planning::FDRVariable<formalism::FluentTag>> index) const;
    void set(Data<formalism::planning::FDRFact<formalism::FluentTag>> fact);
    float_t get(Index<formalism::planning::GroundFunctionTerm<formalism::FluentTag>> index) const;
    void set(Index<formalism::planning::GroundFunctionTerm<formalism::FluentTag>> index, float_t value);
    bool test(Index<formalism::planning::GroundAtom<formalism::DerivedTag>> index) const;
    void set(Index<formalism::planning::GroundAtom<formalism::DerivedTag>> index);

    formalism::planning::FDRValue get(formalism::planning::FDRVariableView<formalism::FluentTag> view) const;
    void set(formalism::planning::FDRFactView<formalism::FluentTag> view);
    float_t get(formalism::planning::GroundFunctionTermView<formalism::FluentTag> view) const;
    void set(formalism::planning::GroundFunctionTermView<formalism::FluentTag> view, float_t value);
    bool test(formalism::planning::GroundAtomView<formalism::DerivedTag> view) const;
    void set(formalism::planning::GroundAtomView<formalism::DerivedTag> view);

    auto get_fluent_facts() const noexcept;
    auto get_derived_atoms() const noexcept;
    auto get_fluent_fterm_values() const noexcept;

    auto get_fluent_facts_view(const formalism::planning::Repository& repository) const noexcept;
    auto get_derived_atoms_view(const formalism::planning::Repository& repository) const noexcept;
    auto get_fluent_fterm_values_view(const formalism::planning::Repository& repository) const noexcept;

    /**
     * For LiftedTask
     */

    template<formalism::FactKind T>
    boost::dynamic_bitset<>& get_atoms() noexcept;

    template<formalism::FactKind T>
    const boost::dynamic_bitset<>& get_atoms() const noexcept;

    std::vector<float_t>& get_numeric_variables() noexcept;
    const std::vector<float_t>& get_numeric_variables() const noexcept;

private:
    Index<State<LiftedTask>> m_index;
    boost::dynamic_bitset<> m_fluent_atoms;
    boost::dynamic_bitset<> m_derived_atoms;
    std::vector<float_t> m_numeric_variables;
};

static_assert(UnpackedStateConcept<UnpackedState<LiftedTask>, LiftedTask>);

/**
 * Implementations
 */

inline Index<State<LiftedTask>> UnpackedState<LiftedTask>::get_index() const { return m_index; }

inline void UnpackedState<LiftedTask>::set(Index<State<LiftedTask>> index) { m_index = index; }

// Fluent facts
inline formalism::planning::FDRValue UnpackedState<LiftedTask>::get(Index<formalism::planning::FDRVariable<formalism::FluentTag>> index) const
{
    return formalism::planning::FDRValue(tyr::test(uint_t(index), m_fluent_atoms));
}

inline void UnpackedState<LiftedTask>::set(Data<formalism::planning::FDRFact<formalism::FluentTag>> fact)
{
    assert(uint_t(fact.value) < 2);  // can only handle binary using bitsets
    tyr::set(uint_t(fact.variable), bool(uint_t(fact.value)), m_fluent_atoms);
}

// Fluent numeric variables
inline float_t UnpackedState<LiftedTask>::get(Index<formalism::planning::GroundFunctionTerm<formalism::FluentTag>> index) const
{
    return tyr::get(uint_t(index), m_numeric_variables, std::numeric_limits<float_t>::quiet_NaN());
}

inline void UnpackedState<LiftedTask>::set(Index<formalism::planning::GroundFunctionTerm<formalism::FluentTag>> index, float_t value)
{
    tyr::set(uint_t(index), value, m_numeric_variables, std::numeric_limits<float_t>::quiet_NaN());
}

// Derived atoms
inline bool UnpackedState<LiftedTask>::test(Index<formalism::planning::GroundAtom<formalism::DerivedTag>> index) const
{
    return tyr::test(uint_t(index), m_derived_atoms);
}

inline void UnpackedState<LiftedTask>::set(Index<formalism::planning::GroundAtom<formalism::DerivedTag>> index)
{
    tyr::set(uint_t(index), true, m_derived_atoms);
}

// Fluent facts
inline formalism::planning::FDRValue UnpackedState<LiftedTask>::get(formalism::planning::FDRVariableView<formalism::FluentTag> view) const
{
    return get(view.get_index());
}

inline void UnpackedState<LiftedTask>::set(formalism::planning::FDRFactView<formalism::FluentTag> view) { set(view.get_data()); }

// Fluent numeric variables
inline float_t UnpackedState<LiftedTask>::get(formalism::planning::GroundFunctionTermView<formalism::FluentTag> view) const { return get(view.get_index()); }

inline void UnpackedState<LiftedTask>::set(formalism::planning::GroundFunctionTermView<formalism::FluentTag> view, float_t value)
{
    set(view.get_index(), value);
}

// Derived atoms
inline bool UnpackedState<LiftedTask>::test(formalism::planning::GroundAtomView<formalism::DerivedTag> view) const { return test(view.get_index()); }

inline void UnpackedState<LiftedTask>::set(formalism::planning::GroundAtomView<formalism::DerivedTag> view) { set(view.get_index()); }

inline void UnpackedState<LiftedTask>::clear()
{
    clear_unextended_part();
    clear_extended_part();
}

inline void UnpackedState<LiftedTask>::clear_unextended_part()
{
    m_fluent_atoms.clear();
    m_numeric_variables.clear();
}

inline void UnpackedState<LiftedTask>::clear_extended_part() { m_derived_atoms.clear(); }

inline void UnpackedState<LiftedTask>::assign_unextended_part(const UnpackedState<LiftedTask>& other)
{
    m_fluent_atoms = other.m_fluent_atoms;
    m_numeric_variables = other.m_numeric_variables;
}

inline auto UnpackedState<LiftedTask>::get_fluent_facts() const noexcept
{
    return FDRFactRange<LiftedTask, formalism::FluentTag>(get_atoms<formalism::FluentTag>());
}

inline auto UnpackedState<LiftedTask>::get_derived_atoms() const noexcept { return AtomRange<formalism::DerivedTag>(get_atoms<formalism::DerivedTag>()); }

inline auto UnpackedState<LiftedTask>::get_fluent_fterm_values() const noexcept
{
    return FunctionTermValueRange<formalism::FluentTag>(get_numeric_variables());
}

inline auto UnpackedState<LiftedTask>::get_fluent_facts_view(const formalism::planning::Repository& repository_) const noexcept
{
    return get_fluent_facts() | std::views::transform([repository = &repository_](auto id) { return make_view(id, *repository); });
}

inline auto UnpackedState<LiftedTask>::get_derived_atoms_view(const formalism::planning::Repository& repository_) const noexcept
{
    return get_derived_atoms() | std::views::transform([repository = &repository_](auto id) { return make_view(id, *repository); });
}

inline auto UnpackedState<LiftedTask>::get_fluent_fterm_values_view(const formalism::planning::Repository& repository_) const noexcept
{
    return get_fluent_fterm_values()
           | std::views::transform([repository = &repository_](auto&& pair) { return std::make_pair(make_view(pair.first, *repository), pair.second); });
}

/**
 * For lifted task
 */

template<formalism::FactKind T>
inline boost::dynamic_bitset<>& UnpackedState<LiftedTask>::get_atoms() noexcept
{
    if constexpr (std::same_as<T, formalism::FluentTag>)
        return m_fluent_atoms;
    else if constexpr (std::same_as<T, formalism::DerivedTag>)
        return m_derived_atoms;
    else
        static_assert(dependent_false<T>::value, "Missing case");
}

template<formalism::FactKind T>
inline const boost::dynamic_bitset<>& UnpackedState<LiftedTask>::get_atoms() const noexcept
{
    if constexpr (std::same_as<T, formalism::FluentTag>)
        return m_fluent_atoms;
    else if constexpr (std::same_as<T, formalism::DerivedTag>)
        return m_derived_atoms;
    else
        static_assert(dependent_false<T>::value, "Missing case");
}

inline std::vector<float_t>& UnpackedState<LiftedTask>::get_numeric_variables() noexcept { return m_numeric_variables; }

inline const std::vector<float_t>& UnpackedState<LiftedTask>::get_numeric_variables() const noexcept { return m_numeric_variables; }
}

#endif
