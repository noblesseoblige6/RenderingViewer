#pragma once

using namespace acLib::vec;

struct Vertex
{
    Vertex( const Vec3& p, const Vec3& n, const Vec2& t, const Vec4& c)
    : position(p),
      normal( n ),
      texCoord( t ),
      color( c )
    {}

    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;
    Vec4 color;
};