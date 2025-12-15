#pragma once

#include <lib/types.hpp>

namespace mm
{

constexpr virtaddr_t kernel_mapping_offset = 0xffffffff80000000;
constexpr virtaddr_t direct_mapping_offset = 0xffff800000000000;

}
