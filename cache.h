#pragma once

#include <vector>

namespace Tmpl8 {

    // single-level fully associative cache

    #define CACHELINEWIDTH	64
    #define N_SETS		    16				    // in bytes
    #define SET_BIT_SIZE    4
    #define OFFSET_BIT_SIZE 6

    #define L1_SIZE         64
    #define L2_SIZE         512
    #define L3_SIZE         8192
    #define DRAMSIZE		3276800				// 3.125MB; 1024x800 pixels

    struct CacheLine
    {
        uchar bytes[CACHELINEWIDTH] = {};
        uint tag;
        bool dirty = false;
        bool valid = false;
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
        Cache(int total_cache_lines)
        {
            n_blocks = total_cache_lines / N_SETS;

            assert(n_blocks >= 1);

            slot.resize(N_SETS, std::vector<CacheLine>(n_blocks));
            accessFrequency.resize(N_SETS, std::vector<int>(n_blocks, 0));
            accessCounter.resize(N_SETS, std::vector<int>(n_blocks, 0));

            printf("Cache: %u sets, %u blocks\n", N_SETS, n_blocks);
        }
        void WriteLine( uint address, CacheLine line );
        CacheLine ReadLine( uint address );
        CacheLine& backdoor( int i ) {
            int set = (i / n_blocks) % N_SETS;
            int block = i % n_blocks;

            assert(set >= 0 && set <= N_SETS);
            assert(block >= 0 && block < n_blocks);

            //printf("%u: %u, %u\n", i, set, block);

            return slot[set][block];
        } /* for visualization without side effects */
    private:
        std::vector<std::vector<CacheLine>> slot;
        int n_blocks;

        std::vector<std::vector<int>> accessFrequency;
        std::vector<std::vector<int>> accessCounter;
    };

    class Memory : public Level // DRAM level for the memory hierarchy
    {
    public:
        Memory(uint dramSize)
        {
            mem = new uchar[dramSize];
            memset( mem, 0, dramSize );
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
            l1 = new Cache(L1_SIZE);
            l1->nextLevel = l2 = new Cache(L2_SIZE);
            l2->nextLevel = l3 = new Cache(L3_SIZE);
            l3->nextLevel = dram = new Memory(DRAMSIZE);
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