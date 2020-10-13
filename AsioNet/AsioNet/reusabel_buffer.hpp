#pragma once

template <size_t SIZE>
class reusabel_buffer :public std::enable_shared_from_this<reusabel_buffer<SIZE>>
{
public:
    inline unsigned char* buffer(size_t explicit_offset = 0)
    {
        return &(_buffer[offset + explicit_offset]);
    }

    inline std::shared_ptr<reusabel_buffer> reset()
    {
        offset = 0;
        length = 0;
        return shared_from_this();
    }

    inline size_t available_capacity()
    {
        return SIZE - offset;
    }

    inline unsigned char* origin_buffer(size_t explicit_offset = 0)
    {
        return &(_buffer[explicit_offset]);
    }

    size_t offset;
    size_t length;

#pragma region string-style
    inline unsigned char& operator[](size_t idx)
    {
        return _buffer[offset + idx];
    }

    inline void resize(size_t s)
    {
        length = s;
    }

    inline size_t size()
    {
        return length;
    }
#pragma endregion


private:
    unsigned char _buffer[SIZE];
};