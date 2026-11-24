#include "TransparentStringHash.h"

std::uint64_t TransparentStringHash::operator() (std::string_view key) const noexcept
{
    return jam::Hash<std::string_view> {}(key);
}
