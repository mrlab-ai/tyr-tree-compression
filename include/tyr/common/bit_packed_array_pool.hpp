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
 *<
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TYR_COMMON_BIT_PACKED_ARRAY_POOL_HPP_
#define TYR_COMMON_BIT_PACKED_ARRAY_POOL_HPP_

#include "tyr/common/bits.hpp"

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace tyr
{

/// Stores fixed-length arrays as bit-packed unsigned integer codes.
/// The exposed value type is obtained via Encoder::decode.
template<std::unsigned_integral Block, typename Coder = bits::ForwardingBlockCoder<Block>>
    requires bits::BlockCoder<Coder, Block>
class BitPackedArrayPool
{
public:
    template<typename Block_>
        requires std::same_as<std::remove_const_t<Block_>, Block>
    class BasicArrayView;

    using value_type = typename Coder::value_type;
    using reference_type = typename bits::int_reference<Block, Coder>;
    using ArrayView = BasicArrayView<Block>;
    using ConstArrayView = BasicArrayView<const Block>;

    static constexpr std::size_t digits = std::numeric_limits<Block>::digits;
    static constexpr size_t block_shift = std::countr_zero(digits);

    static size_t get_segment_index(size_t index) noexcept { return std::bit_width(index + 1) - 1; }
    static size_t get_segment_pos(size_t index) noexcept { return index - ((size_t { 1 } << get_segment_index(index)) - 1); }

    ArrayView operator[](size_t index) noexcept
    {
        const auto seg_idx = get_segment_index(index);
        const auto seg_pos = get_segment_pos(index);
        const auto start_bit = seg_pos * m_width * m_length;

        auto* data = m_segments[seg_idx].data() + (start_bit >> block_shift);
        auto offset = static_cast<uint8_t>(start_bit & (digits - 1));

        return ArrayView(data, m_length, m_width, offset);
    }

    ConstArrayView operator[](size_t index) const noexcept
    {
        const auto seg_idx = get_segment_index(index);
        const auto seg_pos = get_segment_pos(index);
        const auto start_bit = seg_pos * m_width * m_length;

        const auto* data = m_segments[seg_idx].data() + (start_bit >> block_shift);
        auto offset = static_cast<uint8_t>(start_bit & (digits - 1));

        return ConstArrayView(data, m_length, m_width, offset);
    }

    void push_back(std::span<const value_type> array) {}

    void clear() noexcept {}

    size_t length() const noexcept { return m_length; }
    uint8_t width() const noexcept { return m_width; }
    size_t capacity() const noexcept { return m_capacity; }
    size_t size() const noexcept { return m_size; }

    explicit BitPackedArrayPool(size_t length, uint8_t width) : m_length(length), m_width(width), m_capacity(0), m_size(0) {}

    template<typename Block_>
        requires std::same_as<std::remove_const_t<Block_>, Block>
    class BasicArrayView
    {
    public:
        BasicArrayView(Block_* data, size_t length, uint8_t width, uint8_t offset) : m_data(data), m_length(length), m_width(width), m_offset(offset) {}

        reference_type operator[](size_t pos) noexcept
            requires(!std::is_const_v<Block_>)
        {
            assert(pos < m_length);

            const size_t bit_index = static_cast<size_t>(m_offset) + pos * m_width;
            Block_* word = m_data + (bit_index >> block_shift);
            const uint8_t offset = static_cast<uint8_t>(bit_index & (digits - 1));

            return reference_type(word, offset, m_width);
        }

        value_type operator[](size_t pos) const noexcept
        {
            assert(pos < m_length);

            const size_t bit_index = static_cast<size_t>(m_offset) + pos * m_width;
            const Block_* word = m_data + (bit_index >> block_shift);
            const uint8_t offset = static_cast<uint8_t>(bit_index & (digits - 1));

            return Coder::decode(bits::read_int<Block>(word, offset, m_width));
        }

        size_t size() const noexcept { return m_length; }
        uint8_t width() const noexcept { return m_width; }

    private:
        Block_* m_data;
        size_t m_length;
        uint8_t m_width;
        uint8_t m_offset;
    };

private:
    // Segments grow geometrically, i.e., 1, 2, 4, 8, 16, 32, 64, 128 to make index access easy
    std::vector<std::vector<Block>> m_segments;

    size_t m_length;
    uint8_t m_width;

    size_t m_capacity;
    size_t m_size;
};
}

#endif
