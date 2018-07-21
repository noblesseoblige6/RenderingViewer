#pragma once

using namespace acLib::vec;

struct Vertex
{
    Vertex( const Vec3f& p, const Vec3f& n, const Vec2f& t, const Vec4f& c)
    : position(p),
      normal( n ),
      texCoord( t ),
      color( c )
    {}

    Vec3f position;
    Vec3f normal;
    Vec2f texCoord;
    Vec4f color;
};