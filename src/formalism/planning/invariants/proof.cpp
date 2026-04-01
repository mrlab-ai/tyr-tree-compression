#include "proof.hpp"

#include "constraints.hpp"
#include "matching.hpp"

#include <algorithm>
#include <optional>
#include <variant>
#include <vector>

namespace tyr::formalism::planning::invariant
{
namespace
{

bool is_effect_local_parameter(ParameterIndex parameter, size_t num_action_variables) { return static_cast<uint_t>(parameter) >= num_action_variables; }

uint_t get_effect_local_index(ParameterIndex parameter, size_t num_action_variables) { return static_cast<uint_t>(parameter) - num_action_variables; }

Data<Term> alpha_rename_effect_term(const Data<Term>& term, size_t num_action_variables, size_t fresh_base)
{
    return std::visit(
        [&](auto&& arg) -> Data<Term>
        {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, ParameterIndex>)
            {
                if (!is_effect_local_parameter(arg, num_action_variables))
                    return term;

                const auto local_index = get_effect_local_index(arg, num_action_variables);
                return Data<Term>(ParameterIndex(fresh_base + local_index));
            }
            else if constexpr (std::is_same_v<T, Index<Object>>)
            {
                return term;
            }
            else
            {
                static_assert(dependent_false<T>::value, "Missing case");
            }
        },
        term.value);
}

TempAtom alpha_rename_effect_atom(const TempAtom& atom, size_t num_action_variables, size_t fresh_base)
{
    std::vector<Data<Term>> terms;
    terms.reserve(atom.terms.size());

    for (const auto& term : atom.terms)
        terms.push_back(alpha_rename_effect_term(term, num_action_variables, fresh_base));

    return TempAtom {
        .predicate = atom.predicate,
        .terms = std::move(terms),
    };
}

TempLiteral alpha_rename_effect_literal(const TempLiteral& lit, size_t num_action_variables, size_t fresh_base)
{
    return TempLiteral {
        .atom = alpha_rename_effect_atom(lit.atom, num_action_variables, fresh_base),
        .polarity = lit.polarity,
    };
}

TempLiteralList alpha_rename_effect_condition(const TempLiteralList& lits, size_t num_action_variables, size_t fresh_base)
{
    TempLiteralList result;
    result.reserve(lits.size());

    for (const auto& lit : lits)
        result.push_back(alpha_rename_effect_literal(lit, num_action_variables, fresh_base));

    return result;
}

EqualityConjunction make_cover_equality_conjunction(const TempAtom& atom, const Invariant& inv)
{
    const auto it = std::find_if(inv.atoms.begin(), inv.atoms.end(), [&](const auto& p) { return p.predicate == atom.predicate; });
    assert(it != inv.atoms.end());

    const auto& pattern = *it;
    std::vector<std::pair<ConstraintTerm, ConstraintTerm>> equalities;

    for (size_t pos = 0; pos < pattern.terms.size(); ++pos)
    {
        std::visit(
            [&](auto&& x)
            {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, ParameterIndex>)
                {
                    const auto idx = static_cast<uint_t>(x);
                    if (idx < inv.num_rigid_variables)
                    {
                        equalities.emplace_back(make_invariant_parameter_term(idx), make_constraint_term(atom.terms[pos]));
                    }
                }
            },
            pattern.terms[pos].value);
    }

    return EqualityConjunction(std::move(equalities));
}

ConstraintSystem
make_param_system(const TempAction& op, const TempEffect& add_effect, const Invariant& inv, const TempAtom& add_atom, const ActionSubstitution& sigma_op)
{
    ConstraintSystem system;

    const auto covered_add_atom = apply_substitution(add_atom, sigma_op);
    const auto add_cover = make_cover_equality_conjunction(covered_add_atom, inv);
    const auto& representative = add_cover.get_representative();

    std::vector<ParameterIndex> params;
    for (size_t i = 0; i < op.num_variables; ++i)
        params.push_back(ParameterIndex(i));
    for (size_t i = 0; i < add_effect.num_effect_variables; ++i)
        params.push_back(ParameterIndex(add_effect.num_action_variables + i));

    for (const auto param : params)
    {
        const auto term = make_constraint_term(Data<Term>(param));
        const auto repr = representative.contains(term) ? representative.at(term) : term;

        if (std::holds_alternative<InvariantParameter>(repr) || std::holds_alternative<VariableTerm>(repr))
        {
            system.add_not_constant(term);
        }
    }

    for (size_t i = 0; i < params.size(); ++i)
    {
        for (size_t j = i + 1; j < params.size(); ++j)
        {
            const auto t1 = make_constraint_term(Data<Term>(params[i]));
            const auto t2 = make_constraint_term(Data<Term>(params[j]));
            const auto r1 = representative.contains(t1) ? representative.at(t1) : t1;
            const auto r2 = representative.contains(t2) ? representative.at(t2) : t2;

            if (r1 != r2)
                system.add_inequality_disjunction(InequalityDisjunction({ { t1, t2 } }));
        }
    }

    return system;
}

std::optional<ConstraintSystem> make_balance_system(const TempAction& op,
                                                    const TempEffect& add_effect,
                                                    const TempEffect& del_effect,
                                                    const TempAtom& add_atom,
                                                    const TempAtom& del_atom,
                                                    const ActionSubstitution& sigma_op,
                                                    const EffectSubstitution& sigma_eff)
{
    ConstraintSystem system;

    std::map<PredicateView<FluentTag>, std::vector<TempLiteral>> produced_by_pred;

    auto add_effect_produced = apply_substitution(op.precondition, sigma_op);
    auto add_cond = apply_substitution(add_effect.condition, sigma_op);
    add_effect_produced.insert(add_effect_produced.end(), add_cond.begin(), add_cond.end());
    add_effect_produced.push_back(TempLiteral { .atom = apply_substitution(add_atom, sigma_op), .polarity = false });

    for (const auto& lit : add_effect_produced)
        produced_by_pred[lit.atom.predicate].push_back(lit);

    auto del_required = apply_substitution(del_effect.condition, sigma_op);
    del_required = apply_substitution(del_required, sigma_eff, del_effect.num_action_variables);
    del_required.push_back(TempLiteral { .atom = del_atom, .polarity = true });

    for (const auto& lit : del_required)
    {
        std::vector<EqualityConjunction> possibilities;

        const auto it = produced_by_pred.find(lit.atom.predicate);
        if (it == produced_by_pred.end())
            return std::nullopt;

        for (const auto& match : it->second)
        {
            if (match.polarity != lit.polarity)
                continue;

            std::vector<std::pair<ConstraintTerm, ConstraintTerm>> eqs;
            for (size_t i = 0; i < lit.atom.terms.size(); ++i)
                eqs.emplace_back(make_constraint_term(lit.atom.terms[i]), make_constraint_term(match.atom.terms[i]));

            possibilities.emplace_back(std::move(eqs));
        }

        if (possibilities.empty())
            return std::nullopt;

        system.add_equality_DNF(std::move(possibilities));
    }

    ensure_inequality(system, apply_substitution(add_atom, sigma_op), del_atom);
    return system;
}

}  // namespace

bool is_operator_too_heavy(const TempAction& op, const Invariant& inv)
{
    size_t max_num_effect_variables = 0;
    for (const auto& eff : op.effects)
        max_num_effect_variables = std::max(max_num_effect_variables, eff.num_effect_variables);

    const size_t fresh_base_lhs = op.num_variables;
    const size_t fresh_base_rhs = op.num_variables + max_num_effect_variables + 1;

    for (const auto& eff_lhs_raw : op.effects)
    {
        const auto eff_lhs_cond = alpha_rename_effect_condition(eff_lhs_raw.condition, op.num_variables, fresh_base_lhs);

        for (const auto& eff_rhs_raw : op.effects)
        {
            const auto eff_rhs_cond = alpha_rename_effect_condition(eff_rhs_raw.condition, op.num_variables, fresh_base_rhs);

            for (const auto& atom_lhs_raw : eff_lhs_raw.add_effects)
            {
                if (!inv.predicates.contains(atom_lhs_raw.predicate))
                    continue;

                const auto atom_lhs_alpha = alpha_rename_effect_atom(atom_lhs_raw, op.num_variables, fresh_base_lhs);

                for (const auto& sigma_op : enumerate_action_alignments(inv, atom_lhs_alpha, op.num_variables))
                {
                    const auto atom_lhs = apply_substitution(atom_lhs_alpha, sigma_op);
                    if (!covers(inv, atom_lhs))
                        continue;

                    for (const auto& atom_rhs_raw : eff_rhs_raw.add_effects)
                    {
                        if (!inv.predicates.contains(atom_rhs_raw.predicate))
                            continue;

                        const auto atom_rhs_alpha = alpha_rename_effect_atom(atom_rhs_raw, op.num_variables, fresh_base_rhs);
                        const auto atom_rhs = apply_substitution(atom_rhs_alpha, sigma_op);

                        if (atom_lhs == atom_rhs)
                            continue;
                        if (!covers(inv, atom_rhs))
                            continue;

                        ConstraintSystem system;
                        ensure_inequality(system, atom_lhs, atom_rhs);
                        ensure_cover(system, atom_lhs, inv);
                        ensure_cover(system, atom_rhs, inv);

                        auto test_formula = apply_substitution(op.precondition, sigma_op);
                        auto cond_lhs = apply_substitution(eff_lhs_cond, sigma_op);
                        auto cond_rhs = apply_substitution(eff_rhs_cond, sigma_op);

                        test_formula.insert(test_formula.end(), cond_lhs.begin(), cond_lhs.end());
                        test_formula.insert(test_formula.end(), cond_rhs.begin(), cond_rhs.end());
                        test_formula.push_back(TempLiteral { .atom = atom_lhs, .polarity = false });
                        test_formula.push_back(TempLiteral { .atom = atom_rhs, .polarity = false });

                        ensure_conjunction_sat(system, test_formula);

                        if (system.is_solvable())
                            return true;
                    }
                }
            }
        }
    }

    return false;
}

bool is_add_effect_unbalanced(const TempAction& op, const TempEffect& add_effect, const TempAtom& add_atom, const Invariant& inv)
{
    for (const auto& sigma_op : enumerate_action_alignments(inv, add_atom, op.num_variables))
    {
        const auto add_cover_atom = apply_substitution(add_atom, sigma_op);
        auto param_system = make_param_system(op, add_effect, inv, add_atom, sigma_op);

        for (const auto& del_eff : op.effects)
        {
            for (const auto& eprime_raw : del_eff.del_effects)
            {
                for (const auto& sigma_eff : enumerate_effect_renamings(del_eff, eprime_raw, inv, sigma_op))
                {
                    const auto del_atom_partial = apply_substitution(eprime_raw, sigma_op);
                    const auto del_atom = apply_substitution(del_atom_partial, sigma_eff, del_eff.num_action_variables);

                    if (!covers(inv, del_atom))
                        continue;

                    auto balance_system = make_balance_system(op, add_effect, del_eff, add_atom, del_atom, sigma_op, sigma_eff);
                    if (!balance_system.has_value())
                        continue;

                    ConstraintSystem system;
                    ensure_cover(system, add_cover_atom, inv);
                    ensure_cover(system, del_atom, inv);
                    system.extend(*balance_system);
                    system.extend(param_system);

                    if (system.is_solvable())
                        return false;
                }
            }
        }
    }

    return true;
}

ProofResult prove_invariant(const Invariant& inv, const TempActionList& ops)
{
    for (size_t op_index = 0; op_index < ops.size(); ++op_index)
    {
        const auto& op = ops[op_index];

        if (is_operator_too_heavy(op, inv))
            return { ProofStatus::TooHeavy, std::nullopt };

        for (size_t effect_index = 0; effect_index < op.effects.size(); ++effect_index)
        {
            const auto& conj_eff = op.effects[effect_index];

            for (size_t add_index = 0; add_index < conj_eff.add_effects.size(); ++add_index)
            {
                const auto& atom = conj_eff.add_effects[add_index];

                if (!inv.predicates.contains(atom.predicate))
                    continue;

                if (is_add_effect_unbalanced(op, conj_eff, atom, inv))
                    return { ProofStatus::UnbalancedAddEffect, Threat { op_index, effect_index, add_index } };
            }
        }
    }

    return { ProofStatus::Proven, std::nullopt };
}

}  // namespace tyr::formalism::planning::invariant