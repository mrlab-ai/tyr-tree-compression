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

#ifndef TYR_PLANNING_STATE_STORAGE_CONCEPTS_HPP_
#define TYR_PLANNING_STATE_STORAGE_CONCEPTS_HPP_

#include "tyr/planning/state_index.hpp"

#include <concepts>

namespace tyr::planning
{
/**
 * Concept
 */

template<typename Packed, typename Unpacked>
concept StorageTranslationConcept = requires(Packed& packed, const Packed& cpacked, Unpacked& unpacked, const Unpacked& cunpacked) {
    { encode(cunpacked, packed) } -> std::same_as<void>;
    { decode(cpacked, unpacked) } -> std::same_as<void>;
};

template<typename T, typename Task, typename Packed>
concept StorageBackendConcept = requires(T& backend, const T& cbackend, const Packed& packed, Index<State<Task>> state_index) {
    { backend.insert(packed) } -> std::same_as<std::pair<Index<State<Task>>, bool>>;
    { cbackend[state_index] } -> std::same_as<const Packed&>;
};
}

#endif
