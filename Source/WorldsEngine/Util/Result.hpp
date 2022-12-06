#pragma once

namespace worlds
{
    template <typename T, typename err> struct Result
    {
        T value;
        err error;

        Result(T val) : value(val), error((err)0)
        {
        }
        Result(err error) : error(error)
        {
        }
    };
}