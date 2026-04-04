#ifndef TYR_FORMALISM_UNIFICATION_APPLY_SUBSTITUTION_HPP_
#define TYR_FORMALISM_UNIFICATION_APPLY_SUBSTITUTION_HPP_

#include "tyr/formalism/unification/structure_traits.hpp"
#include "tyr/formalism/unification/substitution.hpp"
#include "tyr/formalism/unification/term_operations.hpp"

namespace tyr::formalism::unification
{

template<typename S>
concept ApplicableSubstitution = requires(const S cs, ParameterIndex p) {
    typename S::value_type;
    { cs.contains_parameter(p) } -> std::same_as<bool>;
    { cs.is_unbound(p) } -> std::same_as<bool>;
    { cs[p] } -> std::same_as<const std::optional<typename S::value_type>&>;
    requires std::constructible_from<Data<Term>, typename S::value_type>;
};

template<ApplicableSubstitution S>
Data<Term> apply_substitution(const Data<Term>& term, const S& rho)
{
    if (!is_parameter(term))
        return term;

    const auto p = get_parameter(term);
    if (!rho.contains_parameter(p) || rho.is_unbound(p))
        return term;

    return Data<Term>(*rho[p]);
}

template<typename T, ApplicableSubstitution S>
T apply_substitution(const T& value, const S& rho)
{
    return structure_traits<T>::transform_terms(value, [&](const Data<Term>& term) { return apply_substitution(term, rho); });
}

}  // namespace tyr::formalism::unification

#endif