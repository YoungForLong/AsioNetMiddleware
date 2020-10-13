#pragma once

class UnitTestInterface
{
public:
#pragma region required
    virtual void test_memory() = 0;

    virtual void test_logic() = 0;

    virtual void test_time() = 0;
#pragma endregion

#pragma region optional
    virtual void test_threadsafe() {}
#pragma endregion
};