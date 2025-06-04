#pragma once

#include <vector>

enum EvictionPolicy { RANDOM, LRU, LFU, CLAIRVOYANT, PLRU };

namespace Tmpl8 {

    // single-level fully associative cache

    #define DRAMSIZE		3276800				// 3.125MB; 1024x800 pixels

    struct CacheLine
    {
        uchar* bytes;
        uint tag;
        bool dirty = false;
        bool valid = false;

        CacheLine(int cacheLineWidth)
        {
            bytes = new uchar[cacheLineWidth]();
        }
    };

    class Level // abstract base class for a level in the memory hierarchy
    {
    public:
        virtual void WriteLine( uint address, CacheLine line ) = 0;
        virtual CacheLine ReadLine( uint address ) = 0;
        Level* nextLevel = 0;
        std::vector<std::vector<int>> plruTreeState;
        uint r_hit = 0, r_miss = 0, w_hit = 0, w_miss = 0;
        uint total_r_hit = 0, total_r_miss = 0, total_w_hit = 0, total_w_miss = 0;
        int cacheLineWidth;
        int offsetBitSize;
        int setBitSize;
    };

    class Cache : public Level // cache level for the memory hierarchy
    {
    public:
        Cache(int totalCacheLines, int nSets, int lineWidth, EvictionPolicy policy)
        {
            assert((totalCacheLines % nSets) == 0);

            cacheLineWidth = lineWidth;
            offsetBitSize = static_cast<int>(log2(lineWidth));

            size = totalCacheLines;
            setBitSize = static_cast<int>(log2(nSets));

            n_blocks = totalCacheLines / nSets;

            assert(n_blocks >= 1);

            n_sets = nSets;

            int plruBitsPerSet = n_blocks - 1;
            plruTreeState.resize(n_sets);

            for (int i = 0; i < n_sets; i++) {
                plruTreeState[i] = std::vector<int>(plruBitsPerSet, 0); // initialize to all 0s
            }

            // Construct slot with CacheLine(cacheLineWidth)
            slot.resize(nSets);
            for (int set = 0; set < nSets; ++set)
            {
                slot[set].reserve(n_blocks);
                for (int block = 0; block < n_blocks; ++block)
                {
                    slot[set].emplace_back(cacheLineWidth);
                }
            }

            accessFrequency.resize(nSets, std::vector<int>(n_blocks, 0));
            accessCounter.resize(nSets, std::vector<int>(n_blocks, 0));

            evictionPolicy = policy;

        }
        void WriteLine( uint address, CacheLine line );
        CacheLine ReadLine( uint address );
        CacheLine& backdoor( int i ) {
            int set = (i / n_blocks) % n_sets;
            int block = i % n_blocks;

            assert(set >= 0 && set <= n_sets);
            assert(block >= 0 && block < n_blocks);

            return slot[set][block];
        } /* for visualization without side effects */

        int size;
        int n_blocks;
        int n_sets;

        EvictionPolicy evictionPolicy;

        std::vector<uint> futureAccesses;
        int futureAccessIndex = 0;
    private:
        std::vector<std::vector<CacheLine>> slot;

        std::vector<std::vector<int>> accessFrequency;
        std::vector<std::vector<int>> accessCounter;
    };

    class Memory : public Level // DRAM level for the memory hierarchy
    {
    public:
        Memory(uint dramSize, int lineWidth, int nSets)
        {
            mem = new uchar[dramSize];
            memset( mem, 0, dramSize );
            cacheLineWidth = lineWidth;

            cacheLineWidth = lineWidth;
            offsetBitSize = static_cast<int>(log2(lineWidth));
            setBitSize = static_cast<int>(log2(nSets));
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
        MemHierarchy(int l1Size, int l2Size, int l3Size, int nSets, int cacheLineWidth, EvictionPolicy policy)
        {
            l1 = new Cache(l1Size, nSets, cacheLineWidth, policy);
            l1->nextLevel = l2 = new Cache(l2Size, nSets, cacheLineWidth, policy);
            l2->nextLevel = l3 = new Cache(l3Size, nSets, cacheLineWidth, policy);
            l3->nextLevel = dram = new Memory(DRAMSIZE, cacheLineWidth, nSets);
        }
        void WriteByte( uint address, uchar value );
        uchar ReadByte( uint address );
        void WriteUint( uint address, uint value );
        uint ReadUint( uint address );
        void ResetCounters()
        {
            l1->total_r_hit += l1->r_hit;
            l1->total_r_miss += l1->r_miss;
            l1->total_w_hit += l1->w_hit;
            l1->total_w_miss += l1->w_miss;

            l2->total_r_hit += l2->r_hit;
            l2->total_r_miss += l2->r_miss;
            l2->total_w_hit += l2->w_hit;
            l2->total_w_miss += l2->w_miss;

            l3->total_r_hit += l3->r_hit;
            l3->total_r_miss += l3->r_miss;
            l3->total_w_hit += l3->w_hit;
            l3->total_w_miss += l3->w_miss;

            dram->total_r_hit += dram->r_hit;
            dram->total_r_miss += dram->r_miss;
            dram->total_w_hit += dram->w_hit;
            dram->total_w_miss += dram->w_miss;

            l1->r_hit = l1->w_hit = l1->r_miss = l1->w_miss = 0;
            l2->r_hit = l2->w_hit = l2->r_miss = l2->w_miss = 0;
            l3->r_hit = l3->w_hit = l3->r_miss = l3->w_miss = 0;
            dram->r_hit = dram->w_hit = dram->r_miss = dram->w_miss = 0;
        }
        Level *l1, *l2, *l3, *dram;
    };

} // namespace Tmpl8