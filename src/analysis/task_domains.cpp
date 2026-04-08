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

#include "tyr/analysis/domains.hpp"
#include "tyr/common/config.hpp"
#include "tyr/common/formatter.hpp"
#include "tyr/common/index_mixins.hpp"
#include "tyr/common/types.hpp"
#include "tyr/common/unordered_set.hpp"
#include "tyr/common/variant.hpp"
#include "tyr/common/vector.hpp"
#include "tyr/formalism/planning/datas.hpp"
#include "tyr/formalism/planning/formatter.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"

#include <algorithm>
#include <assert.h>
#include <deque>
#include <gtl/phmap.hpp>
#include <iostream>
#include <optional>
#include <stddef.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;

namespace tyr::analysis
{
namespace
{

using ActionDomainSet = std::pair<DomainSetList, DomainSetListList>;
using ActionDomainSetList = std::vector<ActionDomainSet>;

VariableDomain to_variable_domain(const DomainSet& set)
{
    auto objects = std::vector<Index<f::Object>>(set.begin(), set.end());
    std::sort(objects.begin(), objects.end());
    return VariableDomain { std::move(objects) };
}

VariableDomainList to_variable_domain_list(const DomainSetList& sets)
{
    auto result = VariableDomainList {};
    result.reserve(sets.size());

    for (const auto& set : sets)
        result.push_back(to_variable_domain(set));

    return result;
}

template<f::FactKind T>
PredicateDomainList<T> to_predicate_domain_list(const DomainSetListList& sets)
{
    auto result = PredicateDomainList<T> {};
    result.reserve(sets.size());

    for (const auto& predicate_variable_domains : sets)
        result.push_back(PredicateDomain<T> { to_variable_domain_list(predicate_variable_domains) });

    return result;
}

template<f::FactKind T>
FunctionDomainList<T> to_function_domain_list(const DomainSetListList& sets)
{
    auto result = FunctionDomainList<T> {};
    result.reserve(sets.size());

    for (const auto& function_variable_domains : sets)
        result.push_back(FunctionDomain<T> { to_variable_domain_list(function_variable_domains) });

    return result;
}

ActionDomainList to_action_domain_list(const ActionDomainSetList& sets)
{
    auto result = ActionDomainList {};
    result.reserve(sets.size());

    for (const auto& [precondition_domains, effect_domain_sets] : sets)
    {
        auto effect_domains = ConditionalEffectDomainList {};
        effect_domains.reserve(effect_domain_sets.size());

        for (const auto& effect_variable_domains : effect_domain_sets)
            effect_domains.push_back(ConditionalEffectDomain { to_variable_domain_list(effect_variable_domains) });

        result.push_back(ActionDomain {
            ConjunctiveConditionDomain { to_variable_domain_list(precondition_domains) },
            std::move(effect_domains),
        });
    }

    return result;
}

AxiomDomainList to_axiom_domain_list(const DomainSetListList& sets)
{
    auto result = AxiomDomainList {};
    result.reserve(sets.size());

    for (const auto& axiom_variable_domains : sets)
        result.push_back(AxiomDomain { ConjunctiveConditionDomain { to_variable_domain_list(axiom_variable_domains) } });

    return result;
}

template<f::FactKind T>
DomainSetListList initialize_predicate_domain_sets(fp::PredicateListView<T> predicates)
{
    auto predicate_domain_sets = DomainSetListList(predicates.size());

    for (const auto predicate : predicates)
        predicate_domain_sets[predicate.get_index().value].resize(predicate.get_arity());

    return predicate_domain_sets;
}

template<f::FactKind T>
void insert_into_predicate_domain_sets(fp::GroundAtomListView<T> atoms, DomainSetListList& predicate_domain_sets)
{
    for (const auto atom : atoms)
    {
        const auto predicate = atom.get_predicate();
        auto pos = size_t { 0 };
        for (const auto object : atom.get_row().get_objects())
            predicate_domain_sets[predicate.get_index().value][pos++].insert(object.get_index());
    }
}

template<f::FactKind T>
DomainSetListList initialize_function_domain_sets(fp::FunctionListView<T> functions)
{
    auto function_domain_sets = DomainSetListList(functions.size());

    for (const auto function : functions)
        function_domain_sets[function.get_index().value].resize(function.get_arity());

    return function_domain_sets;
}

template<f::FactKind T>
void insert_into_function_domain_sets(fp::GroundFunctionTermValueListView<T> fterm_values, DomainSetListList& function_domain_sets)
{
    for (const auto term_value : fterm_values)
    {
        const auto fterm = term_value.get_fterm();
        const auto function = fterm.get_function();
        auto pos = size_t { 0 };
        for (const auto object : fterm.get_row().get_objects())
            function_domain_sets[function.get_index().value][pos++].insert(object.get_index());
    }
}

template<typename Fn>
void for_each_term_with_position(Fn&& fn, fp::TermListView terms)
{
    size_t pos = 0;
    for (const auto term : terms)
    {
        visit([&](auto&& arg) { fn(pos, arg); }, term.get_variant());
        ++pos;
    }
}

/**
 * Policies
 */

struct InsertConstantPolicy
{
    DomainSetListList& static_predicate_domain_sets;
    DomainSetListList& fluent_predicate_domain_sets;
    DomainSetListList& derived_predicate_domain_sets;
    DomainSetListList& static_function_domain_sets;
    DomainSetListList& fluent_function_domain_sets;

    template<typename Element>
    bool should_skip(Element) const
    {
        return false;
    }

    bool should_skip(fp::FunctionView<f::FluentTag>) const { return true; }

    auto& get_domains(fp::PredicateView<f::StaticTag>) { return static_predicate_domain_sets; }

    auto& get_domains(fp::PredicateView<f::FluentTag>) { return fluent_predicate_domain_sets; }

    auto& get_domains(fp::PredicateView<f::DerivedTag>) { return derived_predicate_domain_sets; }

    auto& get_domains(fp::FunctionView<f::StaticTag>) { return static_function_domain_sets; }

    auto& get_domains(fp::FunctionView<f::FluentTag>) { return fluent_function_domain_sets; }

    template<typename Symbol>
    void on_object(size_t pos, fp::ObjectView object, Symbol symbol)
    {
        auto& domain = get_domains(symbol)[symbol.get_index().value][pos];
        domain.insert(object.get_index());
    }

    template<typename Symbol>
    void on_parameter(size_t, f::ParameterIndex, Symbol)
    {
    }
};

struct RestrictPolicy
{
    const DomainSetListList& static_predicate_domain_sets;
    const DomainSetListList& fluent_predicate_domain_sets;
    const DomainSetListList& derived_predicate_domain_sets;
    const DomainSetListList& static_function_domain_sets;
    const DomainSetListList& fluent_function_domain_sets;
    DomainSetList& parameter_domains;

    template<typename Element>
    bool should_skip(Element) const
    {
        return false;
    }

    template<f::FactKind T>
    bool should_skip(fp::LiteralView<T> literal) const
    {
        return !literal.get_polarity();
    }

    bool should_skip(fp::FunctionView<f::FluentTag>) const { return true; }

    const auto& get_domains(fp::PredicateView<f::StaticTag>) const { return static_predicate_domain_sets; }

    const auto& get_domains(fp::PredicateView<f::FluentTag>) const { return fluent_predicate_domain_sets; }

    const auto& get_domains(fp::PredicateView<f::DerivedTag>) const { return derived_predicate_domain_sets; }

    const auto& get_domains(fp::FunctionView<f::StaticTag>) const { return static_function_domain_sets; }

    const auto& get_domains(fp::FunctionView<f::FluentTag>) const { return fluent_function_domain_sets; }

    template<typename Symbol>
    void on_object(size_t, fp::ObjectView, Symbol)
    {
    }

    template<typename Symbol>
    void on_parameter(size_t pos, f::ParameterIndex param, Symbol symbol)
    {
        auto& parameter_domain = parameter_domains[uint_t(param)];
        const auto& symbol_domain = get_domains(symbol)[symbol.get_index().value][pos];
        intersect_inplace(parameter_domain, symbol_domain);
    }
};

struct LiftPolicy
{
    DomainSetListList& static_predicate_domain_sets;
    DomainSetListList& fluent_predicate_domain_sets;
    DomainSetListList& derived_predicate_domain_sets;
    DomainSetListList& static_function_domain_sets;
    DomainSetListList& fluent_function_domain_sets;
    const DomainSetList& parameter_domains;

    template<typename Element>
    bool should_skip(Element) const
    {
        return false;
    }

    bool should_skip(fp::FunctionView<f::StaticTag>) const { return true; }

    auto& get_domains(fp::PredicateView<f::StaticTag>) { return static_predicate_domain_sets; }

    auto& get_domains(fp::PredicateView<f::FluentTag>) { return fluent_predicate_domain_sets; }

    auto& get_domains(fp::PredicateView<f::DerivedTag>) { return derived_predicate_domain_sets; }

    auto& get_domains(fp::FunctionView<f::StaticTag>) { return static_function_domain_sets; }

    auto& get_domains(fp::FunctionView<f::FluentTag>) { return fluent_function_domain_sets; }

    template<typename Symbol>
    void on_object(size_t pos, fp::ObjectView object, Symbol symbol)
    {
        auto& domain = get_domains(symbol)[symbol.get_index().value][pos];
        domain.insert(object.get_index());
    }

    template<typename Symbol>
    void on_parameter(size_t pos, f::ParameterIndex param, Symbol symbol)
    {
        auto& domain = get_domains(symbol)[symbol.get_index().value][pos];
        union_inplace(domain, parameter_domains[uint_t(param)]);
    }
};

/**
 * Policy traversal
 */

template<typename Policy>
void apply_policy(fp::FunctionExpressionView element, Policy& policy);

template<typename Policy>
void apply_policy(float_t, Policy&)
{
}

template<f::OpKind O, typename Policy>
void apply_policy(fp::LiftedUnaryOperatorView<O> element, Policy& policy)
{
    apply_policy(element.get_arg(), policy);
}

template<f::OpKind O, typename Policy>
void apply_policy(fp::LiftedBinaryOperatorView<O> element, Policy& policy)
{
    apply_policy(element.get_lhs(), policy);
    apply_policy(element.get_rhs(), policy);
}

template<f::OpKind O, typename Policy>
void apply_policy(fp::LiftedMultiOperatorView<O> element, Policy& policy)
{
    for (const auto arg : element.get_args())
        apply_policy(arg, policy);
}

template<f::FactKind T, typename Policy>
void apply_policy(fp::AtomView<T> element, Policy& policy)
{
    const auto predicate = element.get_predicate();

    if (policy.should_skip(predicate))
        return;

    for_each_term_with_position(
        [&](size_t pos, auto&& arg)
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, fp::ObjectView>)
            {
                policy.on_object(pos, arg, predicate);
            }
            else if constexpr (std::is_same_v<Alternative, f::ParameterIndex>)
            {
                policy.on_parameter(pos, arg, predicate);
            }
            else
            {
                static_assert(dependent_false<Alternative>::value, "Missing case");
            }
        },
        element.get_terms());
}

template<f::FactKind T, typename Policy>
void apply_policy(fp::LiteralView<T> element, Policy& policy)
{
    if (policy.should_skip(element))
        return;

    apply_policy(element.get_atom(), policy);
}

template<f::FactKind T, typename Policy>
void apply_policy(fp::FunctionTermView<T> element, Policy& policy)
{
    const auto function = element.get_function();

    if (policy.should_skip(function))
        return;

    for_each_term_with_position(
        [&](size_t pos, auto&& arg)
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, fp::ObjectView>)
            {
                policy.on_object(pos, arg, function);
            }
            else if constexpr (std::is_same_v<Alternative, f::ParameterIndex>)
            {
                policy.on_parameter(pos, arg, function);
            }
            else
            {
                static_assert(dependent_false<Alternative>::value, "Missing case");
            }
        },
        element.get_terms());
}

template<fp::NumericEffectOpKind Op, f::FactKind T, typename Policy>
void apply_policy(fp::NumericEffectView<Op, T> element, Policy& policy)
{
    apply_policy(element.get_fterm(), policy);
    apply_policy(element.get_fexpr(), policy);
}

template<typename Policy>
void apply_policy(fp::LiftedArithmeticOperatorView element, Policy& policy)
{
    visit([&](auto&& arg) { apply_policy(arg, policy); }, element.get_variant());
}

template<typename Policy>
void apply_policy(fp::FunctionExpressionView element, Policy& policy)
{
    visit([&](auto&& arg) { apply_policy(arg, policy); }, element.get_variant());
}

template<typename Policy>
void apply_policy(fp::LiftedBooleanOperatorView element, Policy& policy)
{
    visit([&](auto&& arg) { apply_policy(arg, policy); }, element.get_variant());
}

template<f::FactKind T, typename Policy>
void apply_policy(fp::NumericEffectOperatorView<T> element, Policy& policy)
{
    visit([&](auto&& arg) { apply_policy(arg, policy); }, element.get_variant());
}

}  // namespace

TaskVariableDomains compute_variable_domains(fp::TaskView task)
{
    auto objects = std::vector<Index<f::Object>> {};
    for (const auto object : task.get_domain().get_constants())
        objects.push_back(object.get_index());
    for (const auto object : task.get_objects())
        objects.push_back(object.get_index());
    auto universe = DomainSet(objects.begin(), objects.end());

    ///--- Step 1: Initialize static, fluent, and derived predicate parameter domains

    auto static_predicate_domain_sets = initialize_predicate_domain_sets(task.get_domain().get_predicates<f::StaticTag>());
    auto fluent_predicate_domain_sets = initialize_predicate_domain_sets(task.get_domain().get_predicates<f::FluentTag>());

    auto derived_predicate_indices = IndexList<f::Predicate<f::DerivedTag>> {};
    for (const auto predicate : task.get_domain().get_predicates<f::DerivedTag>())
        derived_predicate_indices.push_back(predicate.get_index());
    for (const auto predicate : task.get_derived_predicates())
        derived_predicate_indices.push_back(predicate.get_index());

    auto derived_predicate_domain_sets = initialize_predicate_domain_sets(make_view(derived_predicate_indices, task.get_context()));

    insert_into_predicate_domain_sets(task.get_atoms<f::StaticTag>(), static_predicate_domain_sets);
    insert_into_predicate_domain_sets(task.get_atoms<f::FluentTag>(), fluent_predicate_domain_sets);

    ///--- Step 2: Initialize static and fluent function parameter domains

    auto static_function_domain_sets = initialize_function_domain_sets(task.get_domain().get_functions<f::StaticTag>());
    auto fluent_function_domain_sets = initialize_function_domain_sets(task.get_domain().get_functions<f::FluentTag>());

    insert_into_function_domain_sets(task.get_fterm_values<f::StaticTag>(), static_function_domain_sets);
    insert_into_function_domain_sets(task.get_fterm_values<f::FluentTag>(), fluent_function_domain_sets);

    ///--- Step 2.5: Important not to forget constants in schemas

    {
        auto insert_policy = InsertConstantPolicy {
            static_predicate_domain_sets, fluent_predicate_domain_sets, derived_predicate_domain_sets, static_function_domain_sets, fluent_function_domain_sets,
        };

        for (const auto action : task.get_domain().get_actions())
        {
            for (const auto literal : action.get_condition().get_literals<f::StaticTag>())
                apply_policy(literal.get_atom(), insert_policy);

            for (const auto op : action.get_condition().get_numeric_constraints())
                apply_policy(op, insert_policy);

            for (const auto c_effect : action.get_effects())
            {
                for (const auto literal : c_effect.get_condition().get_literals<f::StaticTag>())
                    apply_policy(literal.get_atom(), insert_policy);

                for (const auto literal : c_effect.get_condition().get_literals<f::FluentTag>())
                    apply_policy(literal.get_atom(), insert_policy);

                for (const auto op : c_effect.get_condition().get_numeric_constraints())
                    apply_policy(op, insert_policy);
            }
        }

        for (const auto axiom : task.get_domain().get_axioms())
        {
            for (const auto literal : axiom.get_body().get_literals<f::StaticTag>())
                apply_policy(literal.get_atom(), insert_policy);

            for (const auto op : axiom.get_body().get_numeric_constraints())
                apply_policy(op, insert_policy);
        }

        for (const auto axiom : task.get_axioms())
        {
            for (const auto literal : axiom.get_body().get_literals<f::StaticTag>())
                apply_policy(literal.get_atom(), insert_policy);

            for (const auto op : axiom.get_body().get_numeric_constraints())
                apply_policy(op, insert_policy);
        }
    }

    ///--- Step 3: Compute action and axiom parameter domains from static information.

    auto action_domain_sets = ActionDomainSetList {};
    {
        for (const auto action : task.get_domain().get_actions())
        {
            auto variables = action.get_variables();
            auto parameter_domains = DomainSetList(variables.size(), universe);

            auto restrict_policy = RestrictPolicy {
                static_predicate_domain_sets, fluent_predicate_domain_sets, derived_predicate_domain_sets,
                static_function_domain_sets,  fluent_function_domain_sets,  parameter_domains,
            };

            for (const auto literal : action.get_condition().get_literals<f::StaticTag>())
                apply_policy(literal, restrict_policy);

            for (const auto op : action.get_condition().get_numeric_constraints())
                apply_policy(op, restrict_policy);

            auto parameter_domains_per_cond_effect = DomainSetListList {};
            parameter_domains_per_cond_effect.reserve(action.get_effects().size());

            for (const auto c_effect : action.get_effects())
            {
                const auto c_variables = c_effect.get_variables();

                auto c_parameter_domains = parameter_domains;
                c_parameter_domains.resize(variables.size() + c_variables.size(), universe);

                auto c_restrict_policy = RestrictPolicy {
                    static_predicate_domain_sets, fluent_predicate_domain_sets, derived_predicate_domain_sets,
                    static_function_domain_sets,  fluent_function_domain_sets,  c_parameter_domains,
                };

                for (const auto literal : c_effect.get_condition().get_literals<f::StaticTag>())
                    apply_policy(literal, c_restrict_policy);

                for (const auto op : c_effect.get_condition().get_numeric_constraints())
                    apply_policy(op, c_restrict_policy);

                parameter_domains_per_cond_effect.push_back(std::move(c_parameter_domains));
            }

            assert(action.get_index().value == action_domain_sets.size());
            action_domain_sets.emplace_back(std::move(parameter_domains), std::move(parameter_domains_per_cond_effect));
        }
    }

    auto axiom_domain_sets = DomainSetListList {};
    {
        for (const auto axiom : task.get_domain().get_axioms())
        {
            auto variables = axiom.get_body().get_variables();
            auto parameter_domains = DomainSetList(variables.size(), universe);

            auto restrict_policy = RestrictPolicy {
                static_predicate_domain_sets, fluent_predicate_domain_sets, derived_predicate_domain_sets,
                static_function_domain_sets,  fluent_function_domain_sets,  parameter_domains,
            };

            for (const auto literal : axiom.get_body().get_literals<f::StaticTag>())
                apply_policy(literal, restrict_policy);

            for (const auto op : axiom.get_body().get_numeric_constraints())
                apply_policy(op, restrict_policy);

            assert(axiom.get_index().value == axiom_domain_sets.size());
            axiom_domain_sets.push_back(std::move(parameter_domains));
        }

        for (const auto axiom : task.get_axioms())
        {
            auto variables = axiom.get_body().get_variables();
            auto parameter_domains = DomainSetList(variables.size(), universe);

            auto restrict_policy = RestrictPolicy {
                static_predicate_domain_sets, fluent_predicate_domain_sets, derived_predicate_domain_sets,
                static_function_domain_sets,  fluent_function_domain_sets,  parameter_domains,
            };

            for (const auto literal : axiom.get_body().get_literals<f::StaticTag>())
                apply_policy(literal, restrict_policy);

            for (const auto op : axiom.get_body().get_numeric_constraints())
                apply_policy(op, restrict_policy);

            assert(axiom.get_index().value == axiom_domain_sets.size());
            axiom_domain_sets.push_back(std::move(parameter_domains));
        }
    }

    ///--- Step 4: Lift predicate/function domains given the variable relationships in actions and axioms.

    for (const auto action : task.get_domain().get_actions())
    {
        auto& [parameter_domains, parameter_domains_per_cond_effect] = action_domain_sets[action.get_index().value];

        auto lift_policy = LiftPolicy {
            static_predicate_domain_sets, fluent_predicate_domain_sets, derived_predicate_domain_sets,
            static_function_domain_sets,  fluent_function_domain_sets,  parameter_domains,
        };

        for (const auto literal : action.get_condition().get_literals<f::StaticTag>())
            apply_policy(literal, lift_policy);

        for (const auto literal : action.get_condition().get_literals<f::FluentTag>())
            apply_policy(literal, lift_policy);

        for (const auto literal : action.get_condition().get_literals<f::DerivedTag>())
            apply_policy(literal, lift_policy);

        for (const auto op : action.get_condition().get_numeric_constraints())
            apply_policy(op, lift_policy);

        for (uint_t i = 0; i < action.get_effects().size(); ++i)
        {
            const auto c_effect = action.get_effects()[i];
            auto& c_parameter_domains = parameter_domains_per_cond_effect[i];

            auto c_lift_policy = LiftPolicy {
                static_predicate_domain_sets, fluent_predicate_domain_sets, derived_predicate_domain_sets,
                static_function_domain_sets,  fluent_function_domain_sets,  c_parameter_domains,
            };

            for (const auto literal : c_effect.get_condition().get_literals<f::StaticTag>())
                apply_policy(literal, c_lift_policy);

            for (const auto literal : c_effect.get_condition().get_literals<f::FluentTag>())
                apply_policy(literal, c_lift_policy);

            for (const auto op : c_effect.get_condition().get_numeric_constraints())
                apply_policy(op, c_lift_policy);

            for (const auto literal : c_effect.get_effect().get_literals())
                apply_policy(literal, c_lift_policy);

            for (const auto op : c_effect.get_effect().get_numeric_effects())
                apply_policy(op, c_lift_policy);
        }
    }

    for (const auto axiom : task.get_domain().get_axioms())
    {
        auto& parameter_domains = axiom_domain_sets[axiom.get_index().value];

        auto lift_policy = LiftPolicy {
            static_predicate_domain_sets, fluent_predicate_domain_sets, derived_predicate_domain_sets,
            static_function_domain_sets,  fluent_function_domain_sets,  parameter_domains,
        };

        for (const auto literal : axiom.get_body().get_literals<f::StaticTag>())
            apply_policy(literal, lift_policy);

        for (const auto literal : axiom.get_body().get_literals<f::FluentTag>())
            apply_policy(literal, lift_policy);

        for (const auto literal : axiom.get_body().get_literals<f::DerivedTag>())
            apply_policy(literal, lift_policy);

        for (const auto op : axiom.get_body().get_numeric_constraints())
            apply_policy(op, lift_policy);

        apply_policy(axiom.get_head(), lift_policy);
    }

    for (const auto axiom : task.get_axioms())
    {
        auto& parameter_domains = axiom_domain_sets[axiom.get_index().value];

        auto lift_policy = LiftPolicy {
            static_predicate_domain_sets, fluent_predicate_domain_sets, derived_predicate_domain_sets,
            static_function_domain_sets,  fluent_function_domain_sets,  parameter_domains,
        };
        for (const auto literal : axiom.get_body().get_literals<f::StaticTag>())
            apply_policy(literal, lift_policy);

        for (const auto literal : axiom.get_body().get_literals<f::FluentTag>())
            apply_policy(literal, lift_policy);

        for (const auto literal : axiom.get_body().get_literals<f::DerivedTag>())
            apply_policy(literal, lift_policy);

        for (const auto op : axiom.get_body().get_numeric_constraints())
            apply_policy(op, lift_policy);

        apply_policy(axiom.get_head(), lift_policy);
    }

    ///--- Step 5: Convert internal sets to public domain wrapper types.

    auto static_predicate_domains = to_predicate_domain_list<f::StaticTag>(static_predicate_domain_sets);
    auto fluent_predicate_domains = to_predicate_domain_list<f::FluentTag>(fluent_predicate_domain_sets);
    auto derived_predicate_domains = to_predicate_domain_list<f::DerivedTag>(derived_predicate_domain_sets);
    auto static_function_domains = to_function_domain_list<f::StaticTag>(static_function_domain_sets);
    auto fluent_function_domains = to_function_domain_list<f::FluentTag>(fluent_function_domain_sets);
    auto action_domains = to_action_domain_list(action_domain_sets);
    auto axiom_domains = to_axiom_domain_list(axiom_domain_sets);

    return TaskVariableDomains {
        std::move(static_predicate_domains),
        std::move(fluent_predicate_domains),
        std::move(derived_predicate_domains),
        std::move(static_function_domains),
        std::move(fluent_function_domains),
        std::move(action_domains),
        std::move(axiom_domains),
    };
}

}  // namespace tyr::analysis