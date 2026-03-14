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

#include "tyr/formalism/datalog/repository.hpp"

#include "tyr/formalism/datalog/canonicalization.hpp"
#include "tyr/formalism/object_view.hpp"

namespace tyr::formalism::datalog
{
namespace
{
template<FactKind T>
void copy_predicates_impl(Repository& self, const Repository& other)
{
    auto buffer = buffer::Buffer {};
    auto builder = Data<Predicate<T>> {};

    for (uint_t i = 0; i < other.size<Predicate<T>>(); ++i)
    {
        const auto& predicate = other[Index<Predicate<T>>(i)];
        builder.clear();
        builder.name = predicate.name;
        builder.arity = predicate.arity;
        self.get_or_create(builder, buffer);
    }
}

template<FactKind T>
void copy_functions_impl(Repository& self, const Repository& other)
{
    auto buffer = buffer::Buffer {};
    auto builder = Data<Function<T>> {};

    for (uint_t i = 0; i < other.size<Function<T>>(); ++i)
    {
        const auto& function = other[Index<Function<T>>(i)];
        builder.clear();
        builder.name = function.name;
        builder.arity = function.arity;
        self.get_or_create(builder, buffer);
    }
}
}

void Repository::copy_objects(const Repository& other)
{
    auto buffer = buffer::Buffer {};
    auto builder = Data<Object> {};

    for (uint_t i = 0; i < other.size<Object>(); ++i)
    {
        const auto& object = other[Index<Object>(i)];
        builder.clear();
        builder.name = object.name;
        get_or_create(builder, buffer);
    }
}

void Repository::copy_predicates(const Repository& other)
{
    copy_predicates_impl<StaticTag>(*this, other);
    copy_predicates_impl<FluentTag>(*this, other);
}

void Repository::copy_functions(const Repository& other)
{
    copy_functions_impl<StaticTag>(*this, other);
    copy_functions_impl<FluentTag>(*this, other);
}

void Repository::copy_fundamental_structures(const Repository& other)
{
    copy_objects(other);
    copy_predicates(other);
    copy_functions(other);
}

}