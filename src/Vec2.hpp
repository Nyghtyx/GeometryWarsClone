#pragma once

#include <SFML/Graphics.hpp>
#include <math.h>

template <typename T>
class Vec2
{
public:

    T x = 0;
    T y = 0;

    Vec2() = default;

    Vec2(T xin, T yin)
        : x(xin), y(yin)
    {}

    // constructor to convert from sf::Vector2
    Vec2(const sf::Vector2<T>& vec)
        : x(vec.x), y(vec.y)
    {}

    // allow automatic conversion to sf::Vector2
    // this lets us pass Vec2 into sfml functions
    operator sf::Vector2<T>()
    {
        return sf::Vector2<T>(x, y);
    }

    Vec2 operator + (const Vec2& rhs) const
    {
        
        return Vec2(x + rhs.x, y + rhs.y);
    }

    Vec2 operator - (const Vec2& rhs) const
    {
        return Vec2(x - rhs.x, y - rhs.y);
    }

    Vec2 operator / (const T val) const
    {
        return Vec2(x / val, y / val);
    }

    Vec2 operator * (const T val) const
    {
        return Vec2(x * val, y * val);
    }

    bool operator == (const Vec2& rhs) const
    {
        return (x == rhs.x && y == rhs.y);
    }

    bool operator != (const Vec2& rhs) const
    {
        // TODO
        return false;
    }

    void operator += (const Vec2& rhs)
    {
        // TODO
    }

    void operator -= (const Vec2& rhs)
    {
        // TODO
    }

    void operator *= (const T val)
    {
        // TODO
    }

    void operator /= (const T val)
    {
        // TODO
    }

    float dist(const Vec2& rhs) const
    {
        // TODO
        return 1.0f;
    }
};

using Vec2f = Vec2<float>;