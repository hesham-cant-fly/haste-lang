#ifndef __UTF8_HPP
#define __UTF8_HPP

#include <cstdint>

struct UTF8Iter {
  const char *current;
  std::uint32_t current_codepoint;
  std::uint32_t prev_codepoint;

  
};

#endif // !__UTF8_HPP
