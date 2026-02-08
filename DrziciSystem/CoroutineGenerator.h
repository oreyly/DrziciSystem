#pragma once

#include <iostream>
#include <coroutine>
#include <array>
#include <optional>

template<typename T>
struct CoroutineGenerator
{
    struct promise_type
    {
        const T* current_value; // Ukládáme pouze ukazatel, abychom nekopírovali pole

        CoroutineGenerator get_return_object()
        {
            return CoroutineGenerator {std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend()
        {
            return {};
        }
        std::suspend_always final_suspend() noexcept
        {
            return {};
        }

        // co_yield uloží adresu pole a pozastaví korutinu
        std::suspend_always yield_value(const T& value)
        {
            current_value = std::addressof(value);
            return {};
        }

        void unhandled_exception()
        {
            std::terminate();
        }
        void return_void()
        { }
    };

    std::coroutine_handle<promise_type> handle;

    CoroutineGenerator(std::coroutine_handle<promise_type> h) : handle(h)
    { }
    ~CoroutineGenerator()
    {
        if (handle) handle.destroy();
    }

    // Move-only sémantika
    CoroutineGenerator(const CoroutineGenerator&) = delete;
    CoroutineGenerator(CoroutineGenerator&& other) noexcept : handle(other.handle)
    {
        other.handle = nullptr;
    }

    bool next()
    {
        if (handle && !handle.done())
        {
            handle.resume();
            return !handle.done();
        }
        return false;
    }

    const T& value() const
    {
        return *(handle.promise().current_value);
    }

    struct Iterator
    {
        std::coroutine_handle<promise_type> handle;

        // Posun v cyklu ( zavolá naši metodu next() )
        Iterator& operator++()
        {
            handle.resume();
            return *this;
        }

        // Získání hodnoty ( dereference )
        const T& operator*() const
        {
            return *(handle.promise().current_value);
        }

        // Porovnání pro ukonèení cyklu
        bool operator!=(std::default_sentinel_t) const
        {
            return !handle.done();
        }
    };

    Iterator begin()
    {
        if (handle)
        {
            handle.resume(); // První posun, aby se kód rozbìhl k prvnímu co_yield
        }
        return Iterator {handle};
    }

    std::default_sentinel_t end()
    {
        return {};
    }
};