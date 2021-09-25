#pragma once

#include<string>
#include<stdexcept>
#include<sstream>

namespace ipip
{
    std::string ipipHtmlHelp(int port);
    std::string ipipConsoleHelp(int port);

    inline void _ipipConcatMessage(std::stringstream & ss) {}

    template<class T1, class ... T>
    void _ipipConcatMessage(std::stringstream & ss, const T1 & msg, const T & ...  msgs) {
        ss << msg; _ipipConcatMessage(ss, msgs ...);
    }

    template<class ... T>
    void ipipAssert(bool cond, T ... msg) {
        if(!cond) {
            std::stringstream ss;
            _ipipConcatMessage(ss, msg ...);
            throw std::runtime_error(ss.str());
        }
    }

} // namespace ipip
