#pragma once

namespace worlds
{
    template <typename T>
    class UniquePtr
    {
    public:
        UniquePtr() : ptr(nullptr) {}

        UniquePtr(T* ptr)
            : ptr(ptr)
        {}

        UniquePtr(const UniquePtr&) = delete;

        UniquePtr(UniquePtr&& other)
        {
            ptr = other.ptr;
            other.ptr = nullptr;
        }

        void operator=(T* newPtr)
        {
            if (ptr != nullptr) delete ptr;
            ptr = newPtr;
        }

        T* Get() { return ptr; }

        void Reset()
        {
            delete ptr;
            ptr = nullptr;
        }

        T* operator->()
        {
            return ptr;
        }

        ~UniquePtr()
        {
            delete ptr;
        }
    private:
        T* ptr;
    };
}