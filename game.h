// Template, IGAD version 3
// Get the latest version from: https://github.com/jbikker/tmpl8
// IGAD/NHTV/BUAS/UU - Jacco Bikker - 2006-2023

#pragma once
#include "cache.h"

enum CacheVisualization { SPIRAL, BUDDHA, LINE };

namespace Tmpl8
{

    class Graph
    {
    public:
        void Update( Surface* screen, float x, float y, int hit, int miss )
        {
            memcpy( &ratio[0], &ratio[1], 99 * sizeof( float ) );
            ratio[99] = min( 1.0f, hit / max( 0.001f, (float)(hit + miss) ) );
            screen->Bar( (int)x, (int)y, (int)x + 99, (int)y + 50, 0xff0000 );
            for (int i = 0; i < 100; i++) if (ratio[i] > 0)
                    screen->Line( i + x, y, i + x, y + ratio[i] * 50, 0xff00 );
        }
        float ratio[100] = {};
    };

    class Game : public TheApp
    {
    public:
        Game(int l1Size, int l2Size, int l3Size, int nSets, int cacheLineWidth, EvictionPolicy evictionPolicy, CacheVisualization visualization) : mem(l1Size, l2Size, l3Size, nSets, cacheLineWidth, evictionPolicy) {
            currentVisualization = visualization;
        }
        // game flow methods
        void Init();
        void Tick( float deltaTime );
        void VisualizeMem();
        void Shutdown() { /* implement if you want to do something on exit */ }
        // input handling
        void MouseUp( int ) { /* implement if you want to detect mouse button presses */ }
        void MouseDown( int ) { /* implement if you want to detect mouse button presses */ }
        void MouseMove( int x, int y ) { mousePos.x = x, mousePos.y = y; }
        void MouseWheel( float ) { /* implement if you want to handle the mouse wheel */ }
        void KeyUp( int ) { /* implement if you want to handle keys */ }
        void KeyDown( int ) { /* implement if you want to handle keys */ }
        // data members
        int2 mousePos;
        MemHierarchy mem;
        CacheVisualization currentVisualization;

        float a = 0, r = 300;
        Graph gr[8];
        uint* image[4], I,N,F,O,M,_O,V=2019;
        double K[999], Q[999];
        float R(){I^=I<<13;I^=I>>17;I^=I<<5;return I*2.3283064365387e-10f*6-3;} // rng
        int lineIndex = 0;
    };

} // namespace Tmpl8