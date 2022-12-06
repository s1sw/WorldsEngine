#include "Export.hpp"
#include <nlohmann/json.hpp>

extern "C"
{
    EXPORT bool nmjson_contains(nlohmann::json* j, const char* key)
    {
        return j->contains(key);
    }

    EXPORT nlohmann::json* nmjson_get(nlohmann::json* j, const char* key)
    {
        return &j->operator[](key);
    }

    EXPORT int nmjson_getAsInt(nlohmann::json* j)
    {
        return j->get<int>();
    }

    EXPORT uint32_t nmjson_getAsUint(nlohmann::json* j)
    {
        return j->get<uint32_t>();
    }

    EXPORT float nmjson_getAsFloat(nlohmann::json* j)
    {
        return j->get<float>();
    }

    EXPORT float nmjson_getAsDouble(nlohmann::json* j)
    {
        return j->get<double>();
    }

    EXPORT const char* nmjson_getAsString(nlohmann::json* j)
    {
        return strdup(j->get<std::string>().c_str());
    }

    EXPORT uint8_t nmjson_getType(nlohmann::json* j)
    {
        return (uint8_t)j->type();
    }

    EXPORT int nmjson_getCount(nlohmann::json* j)
    {
        return (int)j->size();
    }

    EXPORT nlohmann::json* nmjson_getArrayElement(nlohmann::json* j, int idx)
    {
        return &j->operator[](idx);
    }
}
