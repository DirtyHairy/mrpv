#ifndef _SHA512_H_
#define _SHA512_H_

#include <cstddef>

namespace sha512 {

const char* calculate(const void* data, size_t len);

}

#endif  // _SHA512_H_
