#pragma once
#include <JuceHeader.h>

/** @brief Hashes a string key, so that a hash container finds a std::string key from a std::string_view.
 *
 *  The lookup needs no std::string copy of the view.
 */
struct TransparentStringHash
{
    /** @brief Marks the hash as transparent, so that a container accepts keys of another type for lookup. */
    using is_transparent = void;

    /** @brief Marks the hash as avalanching, so that the container adds no further mixing of the hash bits. */
    using is_avalanching = void;

    /** @brief Hashes a string view.
     *
     *  @param key The text to hash.
     *
     *  @return The 64-bit hash of the text.
     */
    std::uint64_t operator() (std::string_view key) const noexcept;
};
