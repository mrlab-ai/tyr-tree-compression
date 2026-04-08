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
#include "tyr/formalism/datalog/datas.hpp"
#include "tyr/formalism/datalog/formatter.hpp"
#include "tyr/formalism/datalog/repository.hpp"
#include "tyr/formalism/datalog/views.hpp"

#include <algorithm>
#include <assert.h>
#include <gtl/phmap.hpp>
#include <stddef.h>
#include <type_traits>

namespace f = tyr::formalism;
namespace fd = tyr::formalism::datalog;

namespace tyr::analysis
{
namespace
{

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

RuleDomainList to_rule_domain_list(const DomainSetListList& sets)
{
    auto result = RuleDomainList {};
    result.reserve(sets.size());

    for (const auto& rule_variable_domains : sets)
        result.push_back(RuleDomain { to_variable_domain_list(rule_variable_domains) });

    return result;
}

template<f::FactKind T>
DomainSetListList initialize_predicate_domain_sets(fd::PredicateListView<T> predicates)
{
    auto predicate_domain_sets = DomainSetListList(predicates.size());

    for (const auto predicate : predicates)
        predicate_domain_sets[predicate.get_index().value].resize(predicate.get_arity());

    return predicate_domain_sets;
}

template<f::FactKind T>
void insert_into_predicate_domain_sets(fd::GroundAtomListView<T> atoms, DomainSetListList& predicate_domain_sets)
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
DomainSetListList initialize_function_domain_sets(fd::FunctionListView<T> functions)
{
    auto function_domain_sets = DomainSetListList(functions.size());

    for (const auto function : functions)
        function_domain_sets[function.get_index().value].resize(function.get_arity());

    return function_domain_sets;
}

template<f::FactKind T>
void insert_into_function_domain_sets(fd::GroundFunctionTermValueListView<T> fterm_values, DomainSetListList& function_domain_sets)
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
void for_each_term_with_position(Fn&& fn, fd::TermListView terms)
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
    DomainSetListList& predicate_domain_sets;
    DomainSetListList& function_domain_sets;

    template<typename Element>
    bool should_skip(Element) const
    {
        return false;
    }

    bool should_skip(fd::FunctionView<f::FluentTag>) const { return true; }

    template<f::FactKind T>
    auto& get_domains(fd::PredicateView<T>)
    {
        return predicate_domain_sets;
    }

    template<f::FactKind T>
    auto& get_domains(fd::FunctionView<T>)
    {
        return function_domain_sets;
    }

    template<typename Symbol>
    void on_object(size_t pos, fd::ObjectView object, Symbol symbol)
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
    const DomainSetListList& predicate_domain_sets;
    const DomainSetListList& function_domain_sets;
    DomainSetList& parameter_domains;

    template<typename Element>
    bool should_skip(Element) const
    {
        return false;
    }

    template<f::FactKind T>
    bool should_skip(fd::LiteralView<T> literal) const
    {
        return !literal.get_polarity();
    }

    bool should_skip(fd::FunctionView<f::FluentTag>) const { return true; }

    template<f::FactKind T>
    const auto& get_domains(fd::PredicateView<T>) const
    {
        return predicate_domain_sets;
    }

    template<f::FactKind T>
    const auto& get_domains(fd::FunctionView<T>) const
    {
        return function_domain_sets;
    }

    template<typename Symbol>
    void on_object(size_t, fd::ObjectView, Symbol)
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
    DomainSetListList& predicate_domain_sets;
    DomainSetListList& function_domain_sets;
    const DomainSetList& parameter_domains;

    template<typename Element>
    bool should_skip(Element) const
    {
        return false;
    }

    bool should_skip(fd::FunctionView<f::StaticTag>) const { return true; }

    template<f::FactKind T>
    auto& get_domains(fd::PredicateView<T>)
    {
        return predicate_domain_sets;
    }

    template<f::FactKind T>
    auto& get_domains(fd::FunctionView<T>)
    {
        return function_domain_sets;
    }

    template<typename Symbol>
    void on_object(size_t pos, fd::ObjectView object, Symbol symbol)
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
void apply_policy(fd::FunctionExpressionView element, Policy& policy);

template<typename Policy>
void apply_policy(float_t, Policy&)
{
}

template<f::OpKind O, typename Policy>
void apply_policy(fd::LiftedUnaryOperatorView<O> element, Policy& policy)
{
    apply_policy(element.get_arg(), policy);
}

template<f::OpKind O, typename Policy>
void apply_policy(fd::LiftedBinaryOperatorView<O> element, Policy& policy)
{
    apply_policy(element.get_lhs(), policy);
    apply_policy(element.get_rhs(), policy);
}

template<f::OpKind O, typename Policy>
void apply_policy(fd::LiftedMultiOperatorView<O> element, Policy& policy)
{
    for (const auto arg : element.get_args())
        apply_policy(arg, policy);
}

template<f::FactKind T, typename Policy>
void apply_policy(fd::AtomView<T> element, Policy& policy)
{
    const auto predicate = element.get_predicate();

    if (policy.should_skip(predicate))
        return;

    for_each_term_with_position(
        [&](size_t pos, auto&& arg)
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, fd::ObjectView>)
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
void apply_policy(fd::LiteralView<T> element, Policy& policy)
{
    if (policy.should_skip(element))
        return;

    apply_policy(element.get_atom(), policy);
}

template<f::FactKind T, typename Policy>
void apply_policy(fd::FunctionTermView<T> element, Policy& policy)
{
    const auto function = element.get_function();

    if (policy.should_skip(function))
        return;

    for_each_term_with_position(
        [&](size_t pos, auto&& arg)
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, fd::ObjectView>)
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

template<typename Policy>
void apply_policy(fd::LiftedArithmeticOperatorView element, Policy& policy)
{
    visit([&](auto&& arg) { apply_policy(arg, policy); }, element.get_variant());
}

template<typename Policy>
void apply_policy(fd::FunctionExpressionView element, Policy& policy)
{
    visit([&](auto&& arg) { apply_policy(arg, policy); }, element.get_variant());
}

template<typename Policy>
void apply_policy(fd::LiftedBooleanOperatorView element, Policy& policy)
{
    visit([&](auto&& arg) { apply_policy(arg, policy); }, element.get_variant());
}

}  // namespace

ProgramVariableDomains compute_variable_domains(fd::ProgramView program)
{
    auto objects = std::vector<Index<f::Object>> {};
    for (const auto object : program.get_objects())
        objects.push_back(object.get_index());
    auto universe = DomainSet(objects.begin(), objects.end());

    ///--- Step 1: Initialize static and fluent predicate parameter domains

    auto static_predicate_domain_sets = initialize_predicate_domain_sets(program.get_predicates<f::StaticTag>());
    auto fluent_predicate_domain_sets = initialize_predicate_domain_sets(program.get_predicates<f::FluentTag>());
    insert_into_predicate_domain_sets(program.get_atoms<f::StaticTag>(), static_predicate_domain_sets);
    insert_into_predicate_domain_sets(program.get_atoms<f::FluentTag>(), fluent_predicate_domain_sets);

    ///--- Step 2: Initialize static and fluent function parameter domains

    auto static_function_domain_sets = initialize_function_domain_sets(program.get_functions<f::StaticTag>());
    auto fluent_function_domain_sets = initialize_function_domain_sets(program.get_functions<f::FluentTag>());
    insert_into_function_domain_sets(program.get_fterm_values<f::StaticTag>(), static_function_domain_sets);
    insert_into_function_domain_sets(program.get_fterm_values<f::FluentTag>(), fluent_function_domain_sets);

    ///--- Step 2.5: Important not to forget constants in schemas

    {
        auto insert_policy = InsertConstantPolicy { static_predicate_domain_sets, static_function_domain_sets };

        for (const auto rule : program.get_rules())
        {
            for (const auto literal : rule.get_body().get_literals<f::StaticTag>())
                apply_policy(literal.get_atom(), insert_policy);

            for (const auto op : rule.get_body().get_numeric_constraints())
                apply_policy(op, insert_policy);
        }
    }

    ///--- Step 3: Compute rule parameter domains as tightest bound from the previously computed domains of the static predicates.

    auto rule_domain_sets = DomainSetListList();
    {
        for (const auto rule : program.get_rules())
        {
            auto variables = rule.get_body().get_variables();
            auto parameter_domains = DomainSetList(variables.size(), universe);

            auto restrict_policy = RestrictPolicy { static_predicate_domain_sets, static_function_domain_sets, parameter_domains };

            for (const auto literal : rule.get_body().get_literals<f::StaticTag>())
                apply_policy(literal, restrict_policy);

            for (const auto op : rule.get_body().get_numeric_constraints())
                apply_policy(op, restrict_policy);

            assert(rule.get_index().value == rule_domain_sets.size());
            rule_domain_sets.push_back(std::move(parameter_domains));
        }
    }

    ///--- Step 4: Lift the fluent predicate domains given the variable relationships in the rules.

    for (const auto rule : program.get_rules())
    {
        auto& parameter_domains = rule_domain_sets[rule.get_index().value];

        auto static_lift_policy = LiftPolicy { static_predicate_domain_sets, static_function_domain_sets, parameter_domains };
        auto fluent_lift_policy = LiftPolicy { fluent_predicate_domain_sets, fluent_function_domain_sets, parameter_domains };

        for (const auto literal : rule.get_body().get_literals<f::StaticTag>())
            apply_policy(literal, static_lift_policy);

        for (const auto literal : rule.get_body().get_literals<f::FluentTag>())
            apply_policy(literal, fluent_lift_policy);

        for (const auto op : rule.get_body().get_numeric_constraints())
            apply_policy(op, fluent_lift_policy);

        apply_policy(rule.get_head(), fluent_lift_policy);
    }

    ///--- Step 5: Convert internal sets to public domain wrapper types.

    auto static_predicate_domains = to_predicate_domain_list<f::StaticTag>(static_predicate_domain_sets);
    auto fluent_predicate_domains = to_predicate_domain_list<f::FluentTag>(fluent_predicate_domain_sets);
    auto static_function_domains = to_function_domain_list<f::StaticTag>(static_function_domain_sets);
    auto fluent_function_domains = to_function_domain_list<f::FluentTag>(fluent_function_domain_sets);
    auto rule_domains = to_rule_domain_list(rule_domain_sets);

    return ProgramVariableDomains { std::move(static_predicate_domains),
                                    std::move(fluent_predicate_domains),
                                    std::move(static_function_domains),
                                    std::move(fluent_function_domains),
                                    std::move(rule_domains) };
}

}  // namespace tyr::analysis