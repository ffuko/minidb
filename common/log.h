#ifndef COMMON_LOG_H
#define COMMON_LOG_H
#include <iostream>

class Log {
public:
    static std::ostream &GlobalLog() {
        // #ifdef DEBUG
        //
        // #endif // DEBUG
        return std::cerr;
    }

private:
    Log() : log(std::cerr) {}

    std::ostream &log;
};

#endif
