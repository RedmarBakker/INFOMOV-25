#include "precomp.h"
#include "cache.h"

enum EvictionPolicy { RANDOM, LRU, LFU, CLAIRVOYANT };

EvictionPolicy currentPolicy = LFU;

int globalAccessTime = 0;

void Memory::WriteLine( uint address, CacheLine line )
{
    // verify that the address is a multiple of the cacheline width
    assert( (address & CACHELINEWIDTH - 1) == 0 );
    assert( address + CACHELINEWIDTH <= DRAMSIZE ); // prevent out‑of‑bounds mem access

    int tag = address >> (SET_BIT_SIZE + OFFSET_BIT_SIZE);

    // verify that the provided cacheline has the right tag
    assert( tag == line.tag );

    // write the line to simulated DRAM
    memcpy( mem + address, line.bytes, CACHELINEWIDTH );
    w_hit++; // writes to mem always 'hit'
}

CacheLine Memory::ReadLine( uint address )
{
    // verify that the address is a multiple of the cacheline width
    assert( (address & CACHELINEWIDTH - 1) == 0 );
    assert( address + CACHELINEWIDTH <= DRAMSIZE ); // prevent out‑of‑bounds mem access

    // read the line from simulated RAM
    CacheLine retVal;
    memcpy( retVal.bytes, mem + address, CACHELINEWIDTH );
    retVal.tag = (address >> (SET_BIT_SIZE + OFFSET_BIT_SIZE));
    retVal.dirty = false;

    // return the data
    r_hit++; // reads from mem always 'hit'
    return retVal;
}

void Cache::WriteLine( uint address, CacheLine line )
{
    // verify that the address is a multiple of the cacheline width
    assert( (address & CACHELINEWIDTH - 1) == 0 );

    int tag = address >> (SET_BIT_SIZE + OFFSET_BIT_SIZE);
    int set = (address >> OFFSET_BIT_SIZE) & (N_SETS - 1);

    // verify that the provided cacheline has the right tag
    assert( tag == line.tag );

    for (int i = 0; i < n_blocks; i++) if (slot[set][i].tag == line.tag)
    {
        // cacheline is already in the cache; overwrite
        slot[set][i] = line;
        w_hit++;
        return;
    }

    // address not found; choose a victim line to evict
    int slotToEvict = 0;

    switch (currentPolicy)
    {
        case RANDOM:
        {
            uint rand = RandomUInt();
            slotToEvict = rand % n_blocks;
            break;
        }
        case LRU:
        {
            int oldestTime = this->accessCounter[set][0];
            slotToEvict = 0;
            for (int i = 1; i < n_blocks; i++)
            {
                if (this->accessCounter[set][i] < oldestTime)
                {
                    oldestTime = this->accessCounter[set][i];
                    slotToEvict = i;
                }
            }
            break;
        }
        case LFU:
        {
            int minFreq = this->accessFrequency[set][0];
            slotToEvict = 0;
            for (int i = 1; i < n_blocks; i++)
            {
                if (this->accessFrequency[set][i] < minFreq)
                {
                    minFreq = this->accessFrequency[set][i];
                    slotToEvict = i;
                }
            }
            break;
        }
        case CLAIRVOYANT:
        {
            slotToEvict = RandomUInt() % n_blocks;
            break;
        }
    }

    if (slot[set][slotToEvict].dirty)
    {
        nextLevel->WriteLine(
            (slot[set][slotToEvict].tag << (SET_BIT_SIZE + OFFSET_BIT_SIZE)) + (set << OFFSET_BIT_SIZE),
            slot[set][slotToEvict]
        );
    }

    slot[set][slotToEvict] = line;
    w_miss++;
}

CacheLine Cache::ReadLine( uint address )
{
    // verify that the address is a multiple of the cacheline width
    assert( (address & CACHELINEWIDTH - 1) == 0 );

    int tag = address >> (OFFSET_BIT_SIZE + SET_BIT_SIZE);
    int set = (address >> OFFSET_BIT_SIZE) & (N_SETS - 1);

    for (int i = 0; i < n_blocks; i++)
    {
        if (slot[set][i].tag == tag)
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
    int offsetInLine = address & (CACHELINEWIDTH - 1);
    int lineAddress = address - offsetInLine;
    CacheLine line = l1->ReadLine( lineAddress );
    line.bytes[offsetInLine] = value;
    line.dirty = true;
    l1->WriteLine( lineAddress, line );
}

uchar MemHierarchy::ReadByte( uint address )
{
    // fetch the cacheline for the specified address
    int offsetInLine = address & (CACHELINEWIDTH - 1);
    int lineAddress = address - offsetInLine;
    CacheLine line = l1->ReadLine( lineAddress );
    return line.bytes[offsetInLine];
}

void MemHierarchy::WriteUint( uint address, uint value )
{
    // fetch the cacheline for the specified address
    int offsetInLine = address & (CACHELINEWIDTH - 1);
    int lineAddress = address - offsetInLine;
    CacheLine line = l1->ReadLine( lineAddress );
    memcpy( line.bytes + offsetInLine, &value, sizeof( uint ) );
    line.dirty = true;
    l1->WriteLine( lineAddress, line );
}

uint MemHierarchy::ReadUint( uint address )
{
    // fetch the cacheline for the specified address
    int offsetInLine = address & (CACHELINEWIDTH - 1);
    assert( (offsetInLine & 3) == 0 ); // we will not support straddlers
    int lineAddress = address - offsetInLine;
    CacheLine line = l1->ReadLine( lineAddress );
    return ((uint*)line.bytes)[offsetInLine / 4];
}