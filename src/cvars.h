#pragma once

#include <iostream>
#include <mutex>
#include <string_utils.h>

enum class CVarType : char {
    INT,
    FLOAT,
    STRING,
};

enum class CVarFlags : uint32_t {
    None = 0,
    Noedit = 1 << 1,
    EditReadOnly = 1 << 2,
    Advanced = 1 << 3,

    EditCheckbox = 1 << 8,
    EditFloatDrag = 1 << 9,
};

template <typename T>
struct CVarStorage {
    T value;
    CVarType type;
    CVarFlags flags;
    std::string name;
    std::string description;

    CVarStorage(T value, CVarType type, CVarFlags flags, const std::string& name, const std::string& description)
        : value(value)
        , type(type)
        , flags(flags)
        , name(name)
        , description(description)
    {
    }
};

template <typename T>
struct CVarMap {
    std::unordered_map<std::string, std::shared_ptr<CVarStorage<T>>> map;

    void create(const std::shared_ptr<CVarStorage<T>>& cvar)
    {
        if (map[cvar->name]) {
            return;
        }
        map[cvar->name] = cvar;
    }
    std::shared_ptr<CVarStorage<T>> get(std::string name)
    {
        return map[name];   
    }
};

class CVarSystem {
public:
    static CVarSystem* Get()
    {
        if (instance == nullptr) {
            std::lock_guard lock(mutex);
            instance = new CVarSystem();
        }
        return instance;
    }

    CVarMap<int> intCVars;
    CVarMap<float> floatCVars;
    CVarMap<std::string> stringCVars;

    CVarSystem(const CVarSystem&) = delete;
    CVarSystem& operator=(const CVarSystem&) = delete;

private:
    CVarSystem() = default;
    ~CVarSystem() = default;

    inline static CVarSystem* instance = nullptr;
    inline static std::mutex mutex; // Mutex for thread safety
};
