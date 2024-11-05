#ifndef UTIL_H
#define UTIL_H

#include <cstring>

namespace util
{
    template <class type> constexpr type max(const type a, const type b)
    {
        return (a < b) ? b : a;
    }

    template <class type> constexpr type min(const type a, const type b)
    {
        return (b < a) ? b : a;
    }

    template <typename type> struct vector
    {
        type x{};
        type y{};

        vector() = default;
        constexpr vector(type x, type y) : x(x), y(y) {};
    };

    /// @brief Checks if a value is in a acceptable range from a desired point.
    ///
    /// @param value The value to be checked.
    /// @param desired The value to be comparated against
    /// @param diff The acceptable difference between the value and desired
    ///
    /// @return true if its in the range
    template <typename type>
    inline bool inrange(const type value, const type desired, const type diff)
    {
        return (desired - diff < value && desired + diff > value);
    };

    template <typename type, size_t t_size> class ring_buffer
    {
        type arena[t_size]{};

        size_t begin = 0;
        size_t end   = 0;
        bool   wrap  = false;

        public:
        size_t write(const type *data, size_t n)
        {
            n = min(n, this->get_free());

            if (n == 0)
            {
                return n;
            }

            const size_t first_chunk = min(n, t_size - end);

            memcpy(this->arena + this->end, data, first_chunk * sizeof(type));
            end = (end + first_chunk) % t_size;

            if (first_chunk < n)
            {
                const size_t second_chunk = n - first_chunk;

                memcpy(
                    this->arena + this->end, data + first_chunk,
                    second_chunk * sizeof(type));
                this->end = (this->end + second_chunk) % t_size;
            }

            if (begin == end)
            {
                wrap = true;
            }

            return n;
        }

        size_t read(type *dest, size_t n)
        {
            n = min(n, this->get_occupied());

            if (n == 0)
            {
                return n;
            }

            if (wrap)
            {
                wrap = false;
            }

            const size_t first_chunk = min(n, t_size - this->begin);
            memcpy(dest, this->arena + this->begin, first_chunk * sizeof(type));
            begin = (begin + first_chunk) % t_size;

            if (first_chunk < n)
            {
                const size_t second_chunk = n - first_chunk;
                memcpy(
                    dest + first_chunk, this->arena + this->begin,
                    second_chunk * sizeof(type));
                this->begin = (this->begin + second_chunk) % t_size;
            }
            return n;
        }

        size_t get_occupied()
        {
            if (this->end == this->begin)
            {
                return this->wrap ? t_size : 0;
            }

            if (this->end > this->begin)
            {
                return this->end - this->begin;
            }

            return t_size + this->end - this->begin;
        }

        size_t get_free()
        {
            return t_size - this->get_occupied();
        }
    };

    template <typename type, size_t lenght> class queue
    {
        ring_buffer<type, lenght> ringbuffer;

        public:
        bool enqueue(const type &data)
        {
            if (this->ringbuffer.get_free() < 1)
            {
                return false;
            }

            return this->ringbuffer.write(&data, 1) == 1;
        };

        bool dequeue(type &data)
        {
            if (this->ringbuffer.get_occupied() < 1)
            {
                return false;
            }

            return this->ringbuffer.read(&data, 1) == 1;
        }
    };
}; // namespace util

#endif
