#ifndef TYR_FORMALISM_UNIFICATION_MATCH_POLICY_HPP_
#define TYR_FORMALISM_UNIFICATION_MATCH_POLICY_HPP_

#include "tyr/formalism/parameter_index.hpp"
#include "tyr/formalism/unification/match_state.hpp"

#include <optional>

namespace tyr::formalism::unification
{

enum class ParameterRole
{
    rigid,
    sigma,
    counted,
    ignored
};

struct DefaultMatchPolicy
{
    template<typename T>
    bool match_parameter_parameter(ParameterIndex lhs, ParameterIndex rhs, MatchState<T>& state) const
    {
        const auto lhs_role = resolve_role(lhs, state);
        const auto rhs_role = resolve_role(rhs, state);

        if (lhs_role == ParameterRole::ignored || rhs_role == ParameterRole::ignored)
            return true;

        if (lhs_role == ParameterRole::sigma)
            return state.sigma.assign_or_check(lhs, T(rhs));

        if (lhs_role == ParameterRole::counted)
            return state.counted.assign_or_check(lhs, T(rhs));

        if (rhs_role == ParameterRole::sigma)
            return state.sigma.assign_or_check(rhs, T(lhs));

        if (rhs_role == ParameterRole::counted)
            return state.counted.assign_or_check(rhs, T(lhs));

        return lhs == rhs;
    }

    template<typename T>
    bool match_parameter_object(ParameterIndex lhs, const T& rhs, MatchState<T>& state) const
    {
        const auto lhs_role = resolve_role(lhs, state);

        switch (lhs_role)
        {
            case ParameterRole::sigma:
                return state.sigma.assign_or_check(lhs, rhs);
            case ParameterRole::counted:
                return state.counted.assign_or_check(lhs, rhs);
            case ParameterRole::ignored:
                return true;
            case ParameterRole::rigid:
                return false;
        }

        return false;
    }

    template<typename T>
    bool match_object_parameter(const T& lhs, ParameterIndex rhs, MatchState<T>& state) const
    {
        const auto rhs_role = resolve_role(rhs, state);

        switch (rhs_role)
        {
            case ParameterRole::sigma:
                return state.sigma.assign_or_check(rhs, lhs);
            case ParameterRole::counted:
                return state.counted.assign_or_check(rhs, lhs);
            case ParameterRole::ignored:
                return true;
            case ParameterRole::rigid:
                return false;
        }

        return false;
    }

private:
    template<typename T>
    static ParameterRole resolve_role(ParameterIndex p, const MatchState<T>& state)
    {
        const bool in_sigma = state.sigma.contains_parameter(p);
        const bool in_counted = state.counted.contains_parameter(p);

        if (in_sigma)
            return ParameterRole::sigma;
        if (in_counted)
            return ParameterRole::counted;
        return ParameterRole::rigid;
    }
};

}

#endif