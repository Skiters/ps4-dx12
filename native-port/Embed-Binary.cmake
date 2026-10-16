file(READ "${INPUT}" _bytes HEX)
string(REGEX REPLACE "(..)" "0x\\1," _bytes "${_bytes}")
file(WRITE "${OUTPUT}" "#pragma once\n#include <cstdint>\nnamespace NativeDX12 { inline constexpr uint8_t ${SYMBOL}[]{${_bytes}}; }\n")
