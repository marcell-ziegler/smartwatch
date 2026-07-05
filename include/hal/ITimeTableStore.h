#pragma once
#include <string>

class ITimeTableStore
{
public:
    virtual ~ITimeTableStore() = default;
    virtual bool begin() = 0;
    virtual bool readFile(const char *path, std::string &out) = 0;
};