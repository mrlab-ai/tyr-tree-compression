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

#include "matching.hpp"

namespace tyr::formalism::planning::invariant
{
namespace
{
bool is_effect_local_parameter(ParameterIndex parameter, size_t num_action_variables) { return static_cast<uint_t>(parameter) >= num_action_variables; }

ParameterIndex to_effect_local_parameter(ParameterIndex parameter, size_t num_action_variables)
{
    return ParameterIndex(static_cast<uint_t>(parameter) - num_action_variables);
}

std::optional<InvariantSubstitution> match_cover_against_atom(const Invariant& inv, const TempAtom& pattern, const TempAtom& element)
{
    if (pattern.predicate != element.predicate)
        return std::nullopt;

    if (pattern.terms.size() != element.terms.size())
        return std::nullopt;

    InvariantSubstitution substitution(inv.num_rigid_variables + inv.num_counted_variables);

    for (uint_t i = 0; i < pattern.terms.size(); ++i)
    {
        const auto& lhs = pattern.terms[i];
        const auto& rhs = element.terms[i];

        const bool ok = std::visit(
            [&](auto&& lhs_arg) -> bool
            {
                using Lhs = std::decay_t<decltype(lhs_arg)>;

                return std::visit(
                    [&](auto&& rhs_arg) -> bool
                    {
                        using Rhs = std::decay_t<decltype(rhs_arg)>;

                        if constexpr (std::is_same_v<Lhs, ParameterIndex>)
                        {
                            const bool is_counted = (static_cast<uint_t>(lhs_arg) >= inv.num_rigid_variables);

                            if (is_counted)
                                return substitution.assign_or_check(lhs_arg, rhs);

                            if constexpr (std::is_same_v<Rhs, ParameterIndex>)
                                return lhs_arg == rhs_arg;
                            else
                                return false;
                        }
                        else if constexpr (std::is_same_v<Lhs, Index<Object>>)
                        {
                            if constexpr (std::is_same_v<Rhs, Index<Object>>)
                                return lhs_arg == rhs_arg;
                            else
                                return false;
                        }
                        else
                        {
                            static_assert(dependent_false<Lhs>::value, "Missing case");
                        }
                    },
                    rhs.value);
            },
            lhs.value);

        if (!ok)
            return std::nullopt;
    }

    return substitution;
}

std::optional<EffectSubstitution>
match_effect_cover_against_atom(const Invariant& inv, const TempAtom& pattern, const TempAtom& element, size_t num_action_variables)
{
    if (pattern.predicate != element.predicate)
        return std::nullopt;

    if (pattern.terms.size() != element.terms.size())
        return std::nullopt;

    EffectSubstitution sigma_eff;

    uint_t max_local_index = 0;
    bool has_local = false;

    for (const auto& term : element.terms)
    {
        std::visit(
            [&](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, ParameterIndex>)
                {
                    if (is_effect_local_parameter(arg, num_action_variables))
                    {
                        has_local = true;
                        max_local_index = std::max(max_local_index, static_cast<uint_t>(to_effect_local_parameter(arg, num_action_variables)));
                    }
                }
            },
            term.value);
    }

    if (has_local)
        sigma_eff.resize(max_local_index + 1);

    std::vector<std::optional<Data<Term>>> counted_bindings(inv.num_counted_variables, std::nullopt);

    for (uint_t i = 0; i < pattern.terms.size(); ++i)
    {
        const auto& lhs = pattern.terms[i];
        const auto& rhs = element.terms[i];

        const bool ok = std::visit(
            [&](auto&& lhs_arg) -> bool
            {
                using Lhs = std::decay_t<decltype(lhs_arg)>;

                return std::visit(
                    [&](auto&& rhs_arg) -> bool
                    {
                        using Rhs = std::decay_t<decltype(rhs_arg)>;

                        if constexpr (std::is_same_v<Lhs, ParameterIndex>)
                        {
                            const auto lhs_index = static_cast<uint_t>(lhs_arg);
                            const bool lhs_is_counted = lhs_index >= inv.num_rigid_variables;

                            if (!lhs_is_counted)
                            {
                                // Rigid invariant variable: must match exactly.
                                if constexpr (std::is_same_v<Rhs, ParameterIndex>)
                                {
                                    if (is_effect_local_parameter(rhs_arg, num_action_variables))
                                    {
                                        const auto local = to_effect_local_parameter(rhs_arg, num_action_variables);
                                        return sigma_eff.assign_or_check(local, Data<Term>(lhs_arg));
                                    }
                                    return lhs_arg == rhs_arg;
                                }
                                else if constexpr (std::is_same_v<Rhs, Index<Object>>)
                                {
                                    return false;
                                }
                                else
                                {
                                    static_assert(dependent_false<Rhs>::value, "Missing case");
                                }
                            }
                            else
                            {
                                // Counted invariant variable: any term is allowed, but repeated
                                // occurrences must stay consistent whenever we can determine that.
                                const auto counted_index = lhs_index - inv.num_rigid_variables;
                                auto& binding = counted_bindings[counted_index];

                                if constexpr (std::is_same_v<Rhs, ParameterIndex>)
                                {
                                    if (is_effect_local_parameter(rhs_arg, num_action_variables))
                                    {
                                        const auto local = to_effect_local_parameter(rhs_arg, num_action_variables);

                                        if (binding.has_value())
                                            return sigma_eff.assign_or_check(local, *binding);

                                        // Leave unconstrained for now.
                                        return true;
                                    }

                                    if (!binding.has_value())
                                    {
                                        binding = Data<Term>(rhs_arg);
                                        return true;
                                    }

                                    return *binding == Data<Term>(rhs_arg);
                                }
                                else if constexpr (std::is_same_v<Rhs, Index<Object>>)
                                {
                                    if (!binding.has_value())
                                    {
                                        binding = Data<Term>(rhs_arg);
                                        return true;
                                    }

                                    return *binding == Data<Term>(rhs_arg);
                                }
                                else
                                {
                                    static_assert(dependent_false<Rhs>::value, "Missing case");
                                }
                            }
                        }
                        else if constexpr (std::is_same_v<Lhs, Index<Object>>)
                        {
                            if constexpr (std::is_same_v<Rhs, ParameterIndex>)
                            {
                                if (is_effect_local_parameter(rhs_arg, num_action_variables))
                                {
                                    const auto local = to_effect_local_parameter(rhs_arg, num_action_variables);
                                    return sigma_eff.assign_or_check(local, Data<Term>(lhs_arg));
                                }

                                return false;
                            }
                            else if constexpr (std::is_same_v<Rhs, Index<Object>>)
                            {
                                return lhs_arg == rhs_arg;
                            }
                            else
                            {
                                static_assert(dependent_false<Rhs>::value, "Missing case");
                            }
                        }
                        else
                        {
                            static_assert(dependent_false<Lhs>::value, "Missing case");
                        }
                    },
                    rhs.value);
            },
            lhs.value);

        if (!ok)
            return std::nullopt;
    }

    return sigma_eff;
}

}

bool covers(const Invariant& inv, const TempAtom& element)
{
    for (const auto& atom : inv.atoms)
    {
        if (match_cover_against_atom(inv, atom, element).has_value())
            return true;
    }

    return false;
}

std::optional<InvariantSubstitution> match_invariant_against_ground_atom(const Invariant& inv, const TempAtom& pattern, const TempAtom& ground_atom)
{
    if (pattern.predicate != ground_atom.predicate)
        return std::nullopt;

    if (pattern.terms.size() != ground_atom.terms.size())
        return std::nullopt;

    InvariantSubstitution sigma(inv.num_rigid_variables + inv.num_counted_variables);

    for (uint_t i = 0; i < pattern.terms.size(); ++i)
    {
        const auto& lhs = pattern.terms[i];
        const auto& rhs = ground_atom.terms[i];

        const bool ok = std::visit(
            [&](auto&& lhs_arg) -> bool
            {
                using Lhs = std::decay_t<decltype(lhs_arg)>;

                return std::visit(
                    [&](auto&& rhs_arg) -> bool
                    {
                        using Rhs = std::decay_t<decltype(rhs_arg)>;

                        if constexpr (std::is_same_v<Lhs, ParameterIndex>)
                        {
                            // In the initial state, both rigid and counted variables
                            // may bind to concrete objects, but repeated occurrences
                            // must stay consistent.
                            return sigma.assign_or_check(lhs_arg, rhs);
                        }
                        else if constexpr (std::is_same_v<Lhs, Index<Object>>)
                        {
                            if constexpr (std::is_same_v<Rhs, Index<Object>>)
                                return lhs_arg == rhs_arg;
                            else
                                return false;
                        }
                        else
                        {
                            static_assert(dependent_false<Lhs>::value, "Missing case");
                        }
                    },
                    rhs.value);
            },
            lhs.value);

        if (!ok)
            return std::nullopt;
    }

    return sigma;
}

std::vector<ActionSubstitution> enumerate_action_alignments(const Invariant& inv, const TempAtom& element, size_t num_action_variables)
{
    auto result = std::vector<ActionSubstitution> {};

    for (const auto& pattern : inv.atoms)
    {
        if (pattern.predicate != element.predicate)
            continue;

        if (pattern.terms.size() != element.terms.size())
            continue;

        ActionSubstitution sigma(num_action_variables);
        bool mismatch = false;

        for (uint_t i = 0; i < pattern.terms.size(); ++i)
        {
            const auto& lhs = pattern.terms[i];
            const auto& rhs = element.terms[i];

            const bool ok = std::visit(
                [&](auto&& lhs_arg) -> bool
                {
                    using Lhs = std::decay_t<decltype(lhs_arg)>;

                    return std::visit(
                        [&](auto&& rhs_arg) -> bool
                        {
                            using Rhs = std::decay_t<decltype(rhs_arg)>;

                            if constexpr (std::is_same_v<Lhs, ParameterIndex>)
                            {
                                const bool is_counted = (static_cast<uint_t>(lhs_arg) >= inv.num_rigid_variables);

                                if (is_counted)
                                {
                                    // Counted invariant variables do not constrain
                                    // the action-parameter alignment.
                                    return true;
                                }

                                // Rigid invariant variables can only align with
                                // action parameters, not effect-local variables.
                                if constexpr (std::is_same_v<Rhs, ParameterIndex>)
                                {
                                    if (static_cast<uint_t>(rhs_arg) >= num_action_variables)
                                        return false;

                                    return sigma.assign_or_check(rhs_arg, Data<Term>(lhs_arg));
                                }
                                else
                                {
                                    return false;
                                }
                            }
                            else if constexpr (std::is_same_v<Lhs, Index<Object>>)
                            {
                                if constexpr (std::is_same_v<Rhs, ParameterIndex>)
                                {
                                    if (static_cast<uint_t>(rhs_arg) >= num_action_variables)
                                        return false;

                                    return sigma.assign_or_check(rhs_arg, Data<Term>(lhs_arg));
                                }
                                else if constexpr (std::is_same_v<Rhs, Index<Object>>)
                                {
                                    return lhs_arg == rhs_arg;
                                }
                                else
                                {
                                    return false;
                                }
                            }
                            else
                            {
                                static_assert(dependent_false<Lhs>::value, "Missing case");
                            }
                        },
                        rhs.value);
                },
                lhs.value);

            if (!ok)
            {
                mismatch = true;
                break;
            }
        }

        if (!mismatch)
            result.push_back(std::move(sigma));
    }

    return result;
}

std::vector<EffectSubstitution>
enumerate_effect_renamings(const TempEffect& effect, const TempAtom& element, const Invariant& inv, const ActionSubstitution& sigma_op)
{
    auto result = std::vector<EffectSubstitution> {};

    const auto partially_renamed = apply_substitution(element, sigma_op);

    for (const auto& pattern : inv.atoms)
    {
        auto sigma_eff = match_effect_cover_against_atom(inv, pattern, partially_renamed, effect.num_action_variables);

        if (!sigma_eff.has_value())
            continue;

        const auto fully_renamed = apply_substitution(partially_renamed, *sigma_eff, effect.num_action_variables);

        if (covers(inv, fully_renamed))
            result.push_back(std::move(*sigma_eff));
    }

    return result;
}

}
