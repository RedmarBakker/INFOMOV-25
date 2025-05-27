#pragma once

namespace Tmpl8 {

    // single-level fully associative cache

    #define CACHELINEWIDTH	64
    #define CACHESIZE		4096				// in bytes
    #define N_SETS		    16				    // in bytes
    #define N_BLOCKS		4				    // in bytes
    #define SET_BIT_SIZE    4
    #define OFFSET_BIT_SIZE 6
    #define DRAMSIZE		3276800				// 3.125MB; 1024x800 pixels

    struct CacheLine
    {
        uchar bytes[CACHELINEWIDTH] = {};
        uint tag;
        bool dirty = false;
    };

    class Level // abstract base class for a level in the memory hierarchy
    {
    public:
        virtual void WriteLine( uint address, CacheLine line ) = 0;
        virtual CacheLine ReadLine( uint address ) = 0;
        Level* nextLevel = 0;
        uint r_hit = 0, r_miss = 0, w_hit = 0, w_miss = 0;
    };

    class Cache : public Level // cache level for the memory hierarchy
    {
    public:
        void WriteLine( uint address, CacheLine line );
        CacheLine ReadLine( uint address );
        CacheLine& backdoor( int i ) { return slot[((i / N_SETS) - 1) % N_SETS][i % N_BLOCKS]; } /* for visualization without side effects */
    private:
        CacheLine slot[N_SETS][N_BLOCKS];
    };

    class Memory : public Level // DRAM level for the memory hierarchy
    {
    public:
        Memory()
        {
            mem = new uchar[DRAMSIZE];
            memset( mem, 0, DRAMSIZE );
        }
        void WriteLine( uint address, CacheLine line );
        CacheLine ReadLine( uint address );
        uchar* backdoor() { return mem; } /* for visualization without side effects */
    private:
        uchar* mem = 0;
    };

    class MemHierarchy // memory hierarchy
    {
    public:
        MemHierarchy()
        {
            l1 = new Cache();
            l1->nextLevel = l2 = new Cache();
            l2->nextLevel = l3 = new Cache();
            l3->nextLevel = dram = new Memory();

        }
        void WriteByte( uint address, uchar value );
        uchar ReadByte( uint address );
        void WriteUint( uint address, uint value );
        uint ReadUint( uint address );
        void ResetCounters()
        {
            l1->r_hit = l1->w_hit = l1->r_miss = l1->w_miss = 0;
            l2->r_hit = l2->w_hit = l2->r_miss = l2->w_miss = 0;
            l3->r_hit = l3->w_hit = l3->r_miss = l3->w_miss = 0;
            dram->r_hit = dram->w_hit = dram->r_miss = dram->w_miss = 0;
        }
        Level* l1, *l2, *l3, *dram;
    };

} // namespace Tmpl8