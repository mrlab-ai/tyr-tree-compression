/*
 * Copyright (C) 2023 Dominik Drexler and Simon Stahlberg
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

#include "tyr/planning/plan.hpp"

#include "tyr/common/config.hpp"
#include "tyr/formalism/planning/ground_action_index.hpp"
#include "tyr/planning/ground_task/node.hpp"
#include "tyr/planning/lifted_task/node.hpp"
#include "tyr/planning/node.hpp"

namespace tyr::planning
{

template<TaskKind Kind>
Plan<Kind>::Plan(Node<Kind> start_node) : Plan(start_node, LabeledNodeList<Kind> {})
{
}

template<TaskKind Kind>
Plan<Kind>::Plan(Node<Kind> start_node, LabeledNodeList<Kind> labeled_succ_nodes) :
    m_start_node(std::move(start_node)),
    m_labeled_succ_nodes(std::move(labeled_succ_nodes))
{
}

template<TaskKind Kind>
const Node<Kind>& Plan<Kind>::get_start_node() const noexcept
{
    return m_start_node;
}

template<TaskKind Kind>
const LabeledNodeList<Kind>& Plan<Kind>::get_labeled_succ_nodes() const noexcept
{
    return m_labeled_succ_nodes;
}

template<TaskKind Kind>
float_t Plan<Kind>::get_cost() const noexcept
{
    return get_length() > 0 ? m_labeled_succ_nodes.back().node.get_metric() : 0.;
}

template<TaskKind Kind>
size_t Plan<Kind>::get_length() const noexcept
{
    return m_labeled_succ_nodes.size();
}

template class Plan<LiftedTag>;
template class Plan<GroundTag>;

}
