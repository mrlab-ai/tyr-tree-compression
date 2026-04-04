#ifndef TYR_FORMALISM_UNIFICATION_MATCH_STATE_HPP_
#define TYR_FORMALISM_UNIFICATION_MATCH_STATE_HPP_

#include "tyr/formalism/unification/substitution.hpp"

namespace tyr::formalism::unification
{

template<typename T>
struct MatchState
{
    SubstitutionFunction<T> sigma;
    SubstitutionFunction<T> counted;
};

using TermMatchState = MatchState<Data<Term>>;

}

#endif