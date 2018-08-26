#pragma once

struct Vertex
{
    Vertex()
        : Vertex( Vec3f::ZERO, Vec3f::ZERO, Vec2f::ZERO, Vec4f::ZERO )
    {}

    Vertex( const Vec3f& p, const Vec3f& n, const Vec2f& t, const Vec4f& c )
        : position( p )
        , normal( n )
        , texCoord( t )
        , color( c )
    {}

    Vec3f position;
    Vec3f normal;
    Vec2f texCoord;
    Vec4f color;
};