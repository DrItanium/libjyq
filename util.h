#ifndef LIBJYQ_UTIL_H__
#define LIBJYQ_UTIL_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#include <string>
#include <list>
#include <sstream>
#include "types.h"

namespace jyq {
    std::string getNamespace();
    template<typename ... Args>
    std::string smprint(Args&& ... args) {
        std::ostringstream ss;
        print(ss, args...);
        auto str = ss.str();
        return str;
    }
    std::list<std::string> tokenize(const std::string& str, char delim); 
} // end namespace jyq

#endif // end LIBJYQ_UTIL_H__
