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

#ifndef TYR_FORMALISM_DATALOG_REPOSITORY_HPP_
#define TYR_FORMALISM_DATALOG_REPOSITORY_HPP_

// Include specialization headers first
#include "tyr/formalism/datalog/datas.hpp"
#include "tyr/formalism/datalog/indices.hpp"
//
#include "tyr/buffer/declarations.hpp"
#include "tyr/buffer/indexed_hash_set.hpp"
#include "tyr/buffer/segmented_buffer.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/common/tuple.hpp"
#include "tyr/formalism/datalog/declarations.hpp"
#include "tyr/formalism/datalog/relation_table_repository.hpp"
#include "tyr/formalism/datalog/serialization_repository.hpp"
#include "tyr/formalism/function_view.hpp"
#include "tyr/formalism/predicate_view.hpp"

#include <cassert>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tyr::formalism::datalog
{

class Repository
{
private:
    const Repository* m_parent;
    SerializationRepository m_serialization_repository;
    RelationTableRepository m_relation_repository;
    size_t m_num_objects;

public:
    Repository(size_t num_objects, const Repository* parent = nullptr) :
        m_parent(parent),
        m_serialization_repository(m_parent ? &m_parent->m_serialization_repository : nullptr),
        m_relation_repository(num_objects, m_parent ? &m_parent->m_relation_repository : nullptr),
        m_num_objects(num_objects)
    {
        clear();
    }

    auto get_num_objects() const noexcept { return m_num_objects; }

    /**
     * SerializationRepository forwarding.
     */

    template<typename T>
    std::optional<View<Index<T>, Repository>> find_with_hash(const Data<T>& builder, size_t h) const noexcept
    {
        const auto index_or_nullopt = m_serialization_repository.find_with_hash(builder, h);
        if (index_or_nullopt)
            return make_view(*index_or_nullopt, *this);
        return std::nullopt;
    }

    template<typename T>
    std::optional<View<Index<T>, Repository>> find(const Data<T>& builder) const noexcept
    {
        const auto index_or_nullopt = m_serialization_repository.find(builder);
        if (index_or_nullopt)
            return make_view(*index_or_nullopt, *this);
        return std::nullopt;
    }

    template<typename T>
    std::pair<View<Index<T>, Repository>, bool> get_or_create(Data<T>& builder, buffer::Buffer& buf)
    {
        const auto [index, success] = m_serialization_repository.get_or_create(builder, buf);

        return { make_view(index, *this), success };
    }

    template<typename T>
    const Data<T>& operator[](Index<T> index) const noexcept
    {
        return m_serialization_repository[index];
    }

    template<typename T>
    const Data<T>& front() const
    {
        return m_serialization_repository.template front<T>();
    }

    template<typename T>
    size_t size() const noexcept
    {
        return m_serialization_repository.template size<T>();
    }

    void clear() noexcept
    {
        m_serialization_repository.clear();
        m_relation_repository.clear();
    }

    template<typename T>
    const Repository& get_canonical_context(Index<T> index) const noexcept
    {
        if (!m_serialization_repository.template is_local(index))
        {
            assert(m_parent && "Element not found in the repository chain.");
            return m_parent->get_canonical_context(index);
        }
        return *this;
    }

    /**
     * RelationTableRepository forwarding
     */

    template<IndexConcept I>
    std::optional<View<std::pair<I, Index<Binding>>, Repository>> find_with_hash(I g, const IndexList<Object>& builder, size_t h) const noexcept
    {
        const auto row_or_nullopt = m_relation_repository.find_with_hash(g, builder, h);
        if (row_or_nullopt)
            return make_view(std::make_pair(g.get_index(), *row_or_nullopt), *this);
        return std::nullopt;
    }

    template<IndexConcept I>
    std::optional<View<std::pair<I, Index<Binding>>, Repository>> find(I g, const IndexList<Object>& builder) const noexcept
    {
        const auto row_or_nullopt = m_relation_repository.find(g, builder);
        if (row_or_nullopt)
            return make_view(std::make_pair(g.get_index(), *row_or_nullopt), *this);
        return std::nullopt;
    }

    template<IndexConcept I>
    std::pair<View<std::pair<I, Index<Binding>>, Repository>, bool> get_or_create(View<I, Repository> g, const IndexList<Object>& builder)
    {
        const auto [row, success] =
            m_relation_repository.get_or_create(g.get_index(), g.get_arity(), std::max<uint8_t>(1, std::bit_width(m_num_objects)), builder);

        return { make_view(std::make_pair(g.get_index(), row), *this), success };
    }

    template<IndexConcept I>
    auto operator[](std::pair<I, Index<Binding>> index) const noexcept
    {
        return m_relation_repository[index];
    }

    template<IndexConcept I>
    size_t size(I g) const noexcept
    {
        return m_relation_repository.size(g);
    }

    template<IndexConcept I>
    const Repository& get_canonical_context(std::pair<I, Index<Binding>> index) const noexcept
    {
        if (!m_relation_repository.template is_local(index))
        {
            assert(m_parent && "Element not found in the repository chain.");
            return m_parent->template get_canonical_context(index);
        }
        return *this;
    }
};

static_assert(RepositoryConcept<Repository>);
static_assert(Context<Repository>);

}

#endif
