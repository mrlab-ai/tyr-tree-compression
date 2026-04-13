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

#include "tyr/planning/lifted_task/state_storage/hash_set/atom.hpp"

#include "tyr/planning/lifted_task.hpp"
#include "tyr/planning/lifted_task/state_storage/hash_set/context.hpp"

namespace tyr::planning
{

AtomStorageBackend<LiftedTag, HashSet>::AtomStorageBackend(StateStorageContext<LiftedTag, HashSet>& ctx) :
    m_uint_vec_set(ctx.uint_vec_set),
    m_atom_to_global(ctx.atom_to_global),
    m_fact_to_global(ctx.fact_to_global),
    m_is_atom(ctx.is_atom),
    m_global_to_local(ctx.global_to_local),
    m_buffer()
{
}

typename AtomStorageBackend<LiftedTag, HashSet>::Packed
AtomStorageBackend<LiftedTag, HashSet>::insert(const typename AtomStorageBackend<LiftedTag, HashSet>::FactUnpacked& fact_unpacked,
                                               const typename AtomStorageBackend<LiftedTag, HashSet>::AtomUnpacked& atom_unpacked)
{
    m_buffer.clear();

    // Fact
    const auto& fact_bits = fact_unpacked.indices;
    for (auto i = fact_bits.find_first(); i != boost::dynamic_bitset<>::npos; i = fact_bits.find_next(i))
    {
        if (i >= m_fact_to_global.size())
        {
            m_fact_to_global.resize(i + 1, std::numeric_limits<uint_t>::max());
        }
        if (m_fact_to_global[i] == std::numeric_limits<uint_t>::max())
        {
            const auto g = m_global_to_local.size();
            m_fact_to_global[i] = g;
            m_global_to_local.push_back(i);
            m_is_atom.resize(g + 1, false);
        }
        m_buffer.push_back(m_fact_to_global[i]);
    }

    // Atom
    const auto& atom_bits = atom_unpacked.indices;
    for (auto i = atom_bits.find_first(); i != boost::dynamic_bitset<>::npos; i = atom_bits.find_next(i))
    {
        if (i >= m_atom_to_global.size())
        {
            m_atom_to_global.resize(i + 1, std::numeric_limits<uint_t>::max());
        }
        if (m_atom_to_global[i] == std::numeric_limits<uint_t>::max())
        {
            const auto g = m_global_to_local.size();
            m_atom_to_global[i] = g;
            m_global_to_local.push_back(i);
            m_is_atom.resize(g + 1, false);
            m_is_atom[g] = true;
        }
        m_buffer.push_back(m_atom_to_global[i]);
    }

    std::sort(m_buffer.begin(), m_buffer.end());
    const auto index = m_uint_vec_set.insert(m_buffer);
    return AtomStorageBackend<LiftedTag, HashSet>::Packed { index };
}

void AtomStorageBackend<LiftedTag, HashSet>::unpack(const typename AtomStorageBackend<LiftedTag, HashSet>::Packed& packed,
                                                    typename AtomStorageBackend<LiftedTag, HashSet>::FactUnpacked& fact_unpacked,
                                                    typename AtomStorageBackend<LiftedTag, HashSet>::AtomUnpacked& atom_unpacked)
{
    const auto view = m_uint_vec_set[packed.index];

    auto& fact_indices = fact_unpacked.indices;
    fact_indices.clear();
    auto& atom_indices = atom_unpacked.indices;
    atom_indices.clear();

    for (uint_t i = 0; i < view.size(); ++i)
        if (!m_is_atom[view[i]])
            tyr::set(m_global_to_local[view[i]], true, fact_indices);
        else
            tyr::set(m_global_to_local[view[i]], true, atom_indices);
}

}
