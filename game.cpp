/*

Additional points beyond the base 5 may be obtained by completing additional challenges:

1. Besides the spiral and the fractal, add a third algorithm to assess cache behaviour
performance. Try to come up with an access pattern that reveals other characteristics than
the existing two algorithms (up to 1pt).

2. Add support to the code for setting per-level associativity, cache size and cache line width.
These may be compile-time parameters. Use this functionality to answer the following
research question: What is the optimal set of parameters for the spiral, and what parameters
are optimal for the fractal? Which set of parameters do you recommend for general use and
why? (up to 1pt)

3. Implementing LRU, LFU and random eviction for your cache levels and comparing these using
the provided access patterns (spiral + fractal), as well as your own (up to 1pt).

4. Implement a comparison of the implemented eviction policies against the Clairvoyant
algorithm (Bélády's algorithm) (+1pt).

5. Add the option to make the cache inclusive or exclusive. Add a comparison of the efficiency of
both schemes, or include this in the experiments for challenge 2 (up to 1pt).

*/

#include "precomp.h"
#include "game.h"

#define _oOo_oOo_ (O>=V|N>=800?0:(((N<<10)+O)*4)

// -----------------------------------------------------------
// Visualization of the data stored in the memory hierarchy
// -----------------------------------------------------------
void Game::VisualizeMem()
{
    // draw the contents of the simulated DRAM; every pixel is 4 bytes
    // we bypass the Read/Write functions so we don't pollute the cache.
    for (int y = 0; y < 700; y++) for (int x = 0; x < 1024; x++)
    {
        int value = *((uint*)&((Memory*)mem.dram)->backdoor()[(x + y * 1024) * 4]);
        screen->Plot( x + 10, y + 10, (value >> 1) & (currentVisualization == SPIRAL ? 0x4f4f4f : 0x7f7f7f) /* soften */ );
    }

    if (currentVisualization == SPIRAL || currentVisualization == LINE) {

        //draw the L3 cache
        for (int i = 0; i < ((Cache*)mem.l3)->size; i++)
        {
            CacheLine& line = ((Cache*)mem.l3)->backdoor( i );

            int set = (i / (((Cache*)mem.l3)->size / ((Cache*)mem.l3)->n_sets)) % ((Cache*)mem.l3)->n_sets;
            int lineAddress = ((line.tag << ((Cache*)mem.l3)->setBitSize) + set) << ((Cache*)mem.l3)->offsetBitSize;

            int x = (lineAddress / 4) & 1023, y = (lineAddress / 4) / 1024;
            for (int j = 0; j < mem.l3->cacheLineWidth/4; j++) {
                screen->Plot( x + 10 + j, y + 10, ((uint*)line.bytes)[j] >> 16 );
            }
        }

        // draw the L2 cache
        for (int i = 0; i < ((Cache*)mem.l2)->size; i++)
        {
            CacheLine& line = ((Cache*)mem.l2)->backdoor( i );

            int set = (i / (((Cache*)mem.l2)->size / ((Cache*)mem.l2)->n_sets)) % ((Cache*)mem.l2)->n_sets;
            int lineAddress = ((line.tag << ((Cache*)mem.l2)->setBitSize) + set) << ((Cache*)mem.l2)->offsetBitSize;

            int x = (lineAddress / 4) & 1023, y = (lineAddress / 4) / 1024;
            for (int j = 0; j < mem.l2->cacheLineWidth/4; j++) {
                screen->Plot( x + 10 + j, y + 10, ((uint*)line.bytes)[j] >> 8 );
            }
        }

        // draw the L1 cache
        for (int i = 0; i < ((Cache*)mem.l1)->size; i++)
        {
            CacheLine& line = ((Cache*)mem.l1)->backdoor( i );

            int set = (i / (((Cache*)mem.l1)->size / ((Cache*)mem.l1)->n_sets)) % ((Cache*)mem.l1)->n_sets;
            int lineAddress = ((line.tag << ((Cache*)mem.l1)->setBitSize) + set) << ((Cache*)mem.l1)->offsetBitSize;

            int x = (lineAddress / 4) & 1023, y = (lineAddress / 4) / 1024;
            for (int j = 0; j < mem.l1->cacheLineWidth/4; j++) {
                screen->Plot( x + 10 + j, y + 10, ((uint*)line.bytes)[j] );
            }
        }
    }

    // draw hit/miss graphs
    screen->Print( "level 1 R/W", 1050, 10, 0xffffff );
    screen->Print( "level 2 R/W", 1050, 90 , 0xffffff );
    screen->Print( "level 3 R/W", 1050, 170 , 0xffffff );
    screen->Print( "DRAM R/W", 1050, 250 , 0xffffff );
    //screen updates for
    //l1:
    gr[0].Update( screen, 1050, 20, mem.l1->r_hit, mem.l1->r_miss );
    gr[1].Update( screen, 1170, 20, mem.l1->w_hit, mem.l1->w_miss );
    //l2
    gr[2].Update( screen, 1050, 100, mem.l2->r_hit, mem.l2->r_miss );
    gr[3].Update( screen, 1170, 100, mem.l2->w_hit, mem.l2->w_miss );
    //l3
    gr[4].Update( screen, 1050, 180, mem.l3->r_hit, mem.l3->r_miss );
    gr[5].Update( screen, 1170, 180, mem.l3->w_hit, mem.l3->w_miss );
    //DRAM
    gr[6].Update( screen, 1050, 260, mem.dram->r_hit, mem.dram->r_miss );
    gr[7].Update( screen, 1170, 260, mem.dram->w_hit, mem.dram->w_miss );
}

// -----------------------------------------------------------
// Application initialization
// -----------------------------------------------------------
void Game::Init()
{
    for (V = 1024, F = 1, I = 1; I < 4; I++ );

    //run the simulation when clairvoyant to save the future memory addresses
    if (evictionPolicy == CLAIRVOYANT) {
        if (currentVisualization == SPIRAL) {

            for (int i = 0; i < 10; i++)
            {
                int x = (int)(sinf( a ) * r + 512), y = (int)(cosf( a ) * r + 350);
                a += 0.01f; r -= 0.005f;
                if (r < -300) r = -300;

                ((Cache*)mem.l1)->futureAccesses.push_back((x + y * 1024) * 4 - ((x + y * 1024) * 4 % ((Cache*)mem.l1)->cacheLineWidth));
                ((Cache*)mem.l2)->futureAccesses.push_back((x + y * 1024) * 4 - ((x + y * 1024) * 4 % ((Cache*)mem.l2)->cacheLineWidth));
                ((Cache*)mem.l3)->futureAccesses.push_back((x + y * 1024) * 4 - ((x + y * 1024) * 4 % ((Cache*)mem.l3)->cacheLineWidth));
            }
        } else if (currentVisualization == BUDDHA) {

            for(int G,M,T,E=0;++E<2;)for(G=0;++G<V
                <<7;){double B=0,y=0,t=R(),e,z=R();for
                (T=0;T<E<<8;){e=2*B*y+z,B=K[T]=B*B-y*y
               +t,y=Q[T++]=e;

            if(B*B+y*y>9) {
                for (M=0; M<T;) {
                    O=400+.3*V*Q[M],N=.3*V*K[M++]+520;
                    ((Cache*)mem.l1)->futureAccesses.push_back(_oOo_oOo_ - (_oOo_oOo_ % ((Cache*)mem.l1)->cacheLineWidth))));
                    ((Cache*)mem.l2)->futureAccesses.push_back(_oOo_oOo_ - (_oOo_oOo_ % ((Cache*)mem.l2)->cacheLineWidth))));
                    ((Cache*)mem.l3)->futureAccesses.push_back(_oOo_oOo_ - (_oOo_oOo_ % ((Cache*)mem.l3)->cacheLineWidth))));
                    /* END OF BLACK BOX CODE */
                }
                break;
            }}}
        }
    }
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::Tick( float )
{
    // clear the graphics window
    screen->Clear( 0x282828 );

    // reset cache hit/miss counters
    mem.ResetCounters();

    // update memory contents

    /*
     * Branching... I know, but for testing usability much better.
     * */
    if (currentVisualization == SPIRAL) {
        // ACCESS PATTERN: SPIRAL
        for (int i = 0; i < 10; i++)
        {
            int x = (int)(sinf( a ) * r + 512), y = (int)(cosf( a ) * r + 350);
            a += 0.01f; r -= 0.005f;
            if (r < -300) r = -300;
            mem.WriteUint( (x + y * 1024) * 4, 0xff0000 );
        }
    } else if (currentVisualization == LINE) {
        // ACCESS PATTERN: LINE
        const int max = lineIndex + 10;
        for (lineIndex; lineIndex < max; lineIndex++)
        {
            int x = lineIndex % 1024;
            int y = (lineIndex / 1024) % 700;

            mem.WriteUint((x + y * 1024) * 4, 0xff0000);
        }
    } else if (currentVisualization == BUDDHA) {
        // the buddhabrot based on Paul Bourke		ACCESS PATTERN: MOSTLY RANDOM
        for(int G,M,T,E=0;++E<2;)for(G=0;++G<V
        <<7;){double B=0,y=0,t=R(),e,z=R();for
        (T=0;T<E<<8;){e=2*B*y+z,B=K[T]=B*B-y*y
        +t,y=Q[T++]=e;if(B*B+y*y>9){for(M=0;M<		    // data access; to be cached:
        T;){O=400+.3*V*Q[M],N=.3*V*K[M++]+520;		    mem.WriteUint _oOo_oOo_,
                                                  mem.ReadUint _oOo_oOo_ )+545)
        /* END OF BLACK BOX CODE */;}break;}}}
    }

    // visualize the memory hierarchy
    VisualizeMem();
}