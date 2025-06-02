#include "precomp.h"
#include "cache.h"

int globalAccessTime = 0;

void Memory::WriteLine( uint address, CacheLine line )
{
    // verify that the address is a multiple of the cacheline width
    assert( (address & cacheLineWidth - 1) == 0 );
    assert( address + cacheLineWidth <= DRAMSIZE ); // prevent out‑of‑bounds mem access

    int tag = address >> (setBitSize + offsetBitSize);

    // verify that the provided cacheline has the right tag
    assert( tag == line.tag );

    // write the line to simulated DRAM
    memcpy( mem + address, line.bytes, cacheLineWidth );
    w_hit++; // writes to mem always 'hit'
}

CacheLine Memory::ReadLine( uint address )
{
    // verify that the address is a multiple of the cacheline width
    assert( (address & cacheLineWidth - 1) == 0 );
    assert( address + cacheLineWidth <= DRAMSIZE ); // prevent out‑of‑bounds mem access

    // read the line from simulated RAM
    CacheLine retVal = CacheLine(cacheLineWidth);
    memcpy( retVal.bytes, mem + address, cacheLineWidth );
    retVal.tag = (address >> (setBitSize + offsetBitSize));
    retVal.dirty = false;

    // return the data
    r_hit++; // reads from mem always 'hit'

    return retVal;
}

void Cache::WriteLine( uint address, CacheLine line )
{
    // verify that the address is a multiple of the cacheline width
    assert( (address & cacheLineWidth - 1) == 0 );

    int tag = address >> (setBitSize + offsetBitSize);
    int set = (address >> offsetBitSize) & (n_sets - 1);

    // verify that the provided cacheline has the right tag
    assert( tag == line.tag );


    for (int i = 0; i < n_blocks; i++) if (slot[set][i].tag == line.tag)
    {
        // cacheline is already in the cache; overwrite
        slot[set][i] = line;
        w_hit++;
        return;
    }

    // address not found; check for a free slot
    for (int i = 0; i < n_blocks; i++) {
        if (! slot[set][i].valid) {
            slot[set][i] = line;
            slot[set][i].valid = true;
            w_miss++;
            return;
        }
    }

    // no free slot; choose a victim line to evict
    int blockToEvict = 0;

    switch (evictionPolicy)
    {
        case RANDOM:
        {
            uint rand = RandomUInt();
            blockToEvict = rand % n_blocks;
            break;
        }
        case LRU:
        {
            int oldestTime = this->accessCounter[set][0];
            blockToEvict = 0;
            for (int i = 1; i < n_blocks; i++)
            {
                if (this->accessCounter[set][i] < oldestTime)
                {
                    oldestTime = this->accessCounter[set][i];
                    blockToEvict = i;
                }
            }

            this->accessCounter[set][blockToEvict] = globalAccessTime;

            break;
        }
        case LFU:
        {
            int minFreq = this->accessFrequency[set][0];
            blockToEvict = 0;
            for (int i = 1; i < n_blocks; i++)
            {
                if (this->accessFrequency[set][i] < minFreq)
                {
                    minFreq = this->accessFrequency[set][i];
                    blockToEvict = i;
                }
            }

            this->accessFrequency[set][blockToEvict] = 1;

            break;
        }
        case CLAIRVOYANT:
        {
            int farthestUse = -1;
            blockToEvict = 0;

            for (int i = 0; i < n_blocks; i++) {
                int currentTag = slot[set][i].tag;
                bool found = false;

                for (int j = futureAccessIndex; j < futureAccesses.size(); j++) {
                    uint futureAddress = futureAccesses[j];
                    int futureTag = futureAddress >> (setBitSize + offsetBitSize);
                    int futureSet = (futureAddress >> offsetBitSize) & (n_sets - 1);

                    if (futureSet == set && futureTag == currentTag) {
                        if (j > farthestUse) {
                            farthestUse = j;
                            blockToEvict = i;
                        }

                        found = true;

                        break;
                    }
                }

                // If not found in future accesses, evict immediately
                if (! found) {
                    blockToEvict = i;
                    break;
                }
            }
            break;
        }
        case PLRU:
        {
            // Tree-based PLRU using binary tree representation for any n_blocks (assume n_blocks is power of 2)
            // plruState[set] represents a vector of bits for the tree nodes
            int levels = log2(n_blocks);
            int node = 0;

            for (int l = 0; l < levels; l++) {
                int bit = plruTreeState[set][node];
                node = 2 * node + 1 + bit;
            }

            blockToEvict = node - (n_blocks - 1);

            int leaf = blockToEvict + (n_blocks - 1);
            for (int l = levels - 1; l >= 0; l--) {
                int parent = (leaf - 1) / 2;
                int direction = (leaf % 2 == 0) ? 1 : 0;
                plruTreeState[set][parent] = direction;
                leaf = parent;
            }
        }
    }

    if (slot[set][blockToEvict].dirty)
    {
        nextLevel->WriteLine(
            (slot[set][blockToEvict].tag << (setBitSize + offsetBitSize)) + (set << offsetBitSize),
            slot[set][blockToEvict]
        );
    }

    slot[set][blockToEvict] = line;

    w_miss++;
}

CacheLine Cache::ReadLine( uint address )
{
    // verify that the address is a multiple of the cacheline width
    assert( (address & cacheLineWidth - 1) == 0 );

    int tag = address >> (offsetBitSize + setBitSize);
    int set = (address >> offsetBitSize) & (n_sets - 1);

    for (int i = 0; i < n_blocks; i++)
    {
        if (slot[set][i].valid && slot[set][i].tag == tag)
        {
            // cacheline is in the cache; return data
            this->accessCounter[set][i] = ++globalAccessTime;
            this->accessFrequency[set][i]++;
            r_hit++;
            return slot[set][i]; // by value
        }
    }

    // data is not in this cache; ask the next level
    CacheLine line = nextLevel->ReadLine( address );

    // store the retrieved line in this cache
    WriteLine( address, line );

    // return the requested data
    r_miss++;
    return line;
}

void MemHierarchy::WriteByte( uint address, uchar value )
{
    // fetch the cacheline for the specified address
    int offsetInLine = address & (l1->cacheLineWidth - 1);
    int lineAddress = address - offsetInLine;
    CacheLine line = l1->ReadLine( lineAddress );
    line.bytes[offsetInLine] = value;
    line.dirty = true;
    l1->WriteLine( lineAddress, line );
}

uchar MemHierarchy::ReadByte( uint address )
{
    // fetch the cacheline for the specified address
    int offsetInLine = address & (l1->cacheLineWidth - 1);
    int lineAddress = address - offsetInLine;
    CacheLine line = l1->ReadLine( lineAddress );
    return line.bytes[offsetInLine];
}

void MemHierarchy::WriteUint( uint address, uint value )
{
    // fetch the cacheline for the specified address
    int offsetInLine = address & (l1->cacheLineWidth - 1);
    int lineAddress = address - offsetInLine;
    CacheLine line = l1->ReadLine( lineAddress );
    memcpy( line.bytes + offsetInLine, &value, sizeof( uint ) );
    line.dirty = true;
    l1->WriteLine( lineAddress, line );
}

uint MemHierarchy::ReadUint( uint address )
{
    // fetch the cacheline for the specified address
    int offsetInLine = address & (l1->cacheLineWidth - 1);
    assert( (offsetInLine & 3) == 0 ); // we will not support straddlers

    ((Cache*)l1)->futureAccessIndex++;
    ((Cache*)l2)->futureAccessIndex++;
    ((Cache*)l3)->futureAccessIndex++;

    int lineAddress = address - offsetInLine;
    CacheLine line = l1->ReadLine( lineAddress );
    return ((uint*)line.bytes)[offsetInLine / 4];
}