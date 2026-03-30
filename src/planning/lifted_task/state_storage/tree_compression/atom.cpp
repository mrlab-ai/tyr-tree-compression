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

#include "tyr/planning/lifted_task/state_storage/tree_compression/atom.hpp"

#include "tyr/planning/lifted_task.hpp"
#include "tyr/planning/lifted_task/state_storage/tree_compression/context.hpp"

namespace tyr::planning
{

AtomStorageBackend<LiftedTag, TreeCompression>::AtomStorageBackend(StateStorageContext<LiftedTag, TreeCompression>& ctx) :
    m_uint_nodes(ctx.uint_nodes),
    m_atom_to_global(ctx.atom_to_global),
    m_fact_to_global(ctx.fact_to_global),
    m_is_atom(ctx.is_atom),
    m_global_to_local(ctx.global_to_local),
    m_buffer()
{
}

typename AtomStorageBackend<LiftedTag, TreeCompression>::Packed
AtomStorageBackend<LiftedTag, TreeCompression>::insert(const typename AtomStorageBackend<LiftedTag, TreeCompression>::FactUnpacked& fact_unpacked,
                                                       const typename AtomStorageBackend<LiftedTag, TreeCompression>::AtomUnpacked& atom_unpacked)
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
    const auto slot = valla::insert_sequence(m_buffer, m_uint_nodes);
    return AtomStorageBackend<LiftedTag, TreeCompression>::Packed { slot };
}

void AtomStorageBackend<LiftedTag, TreeCompression>::unpack(const typename AtomStorageBackend<LiftedTag, TreeCompression>::Packed& packed,
                                                            typename AtomStorageBackend<LiftedTag, TreeCompression>::FactUnpacked& fact_unpacked,
                                                            typename AtomStorageBackend<LiftedTag, TreeCompression>::AtomUnpacked& atom_unpacked)
{
    m_buffer.clear();
    valla::read_sequence(packed.slot, m_uint_nodes, std::back_inserter(m_buffer));

    auto& fact_indices = fact_unpacked.indices;
    fact_indices.clear();
    auto& atom_indices = atom_unpacked.indices;
    atom_indices.clear();

    for (const auto i : m_buffer)
    {
        if (!m_is_atom[i])
            tyr::set(m_global_to_local[i], true, fact_indices);
        else
            tyr::set(m_global_to_local[i], true, atom_indices);
    }
}

}
