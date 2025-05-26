#include "precomp.h"
#include "cache.h"

void Memory::WriteLine( uint addressBase, uint set, CacheLine line )
{
	// verify that the address is a multiple of the cacheline width
	assert( (addressBase & CACHELINEWIDTH - 1) == 0 );

	// verify that the provided cacheline has the right tag
	assert( (addressBase / CACHELINEWIDTH) == line.tag );

	// write the line to simulated DRAM
	memcpy( mem + addressBase, line.bytes, CACHELINEWIDTH );
	w_hit++; // writes to mem always 'hit'
}

CacheLine Memory::ReadLine( uint addressBase, uint set )
{
	// verify that the address is a multiple of the cacheline width
	assert( (addressBase & CACHELINEWIDTH - 1) == 0 );

	// read the line from simulated RAM
	CacheLine retVal;
	memcpy( retVal.bytes, mem + addressBase, CACHELINEWIDTH );
	retVal.tag = addressBase / CACHELINEWIDTH;
	retVal.dirty = false;
	
	// return the data
	r_hit++; // reads from mem always 'hit'
	return retVal;
}

void Cache::WriteLine( uint addressBase, uint set, CacheLine line )
{
	// verify that the address is a multiple of the cacheline width
	assert( (addressBase & CACHELINEWIDTH - 1) == 0 );

	// verify that the provided cacheline has the right tag
	assert( (addressBase / CACHELINEWIDTH) == line.tag );

	for (int i = 0; i < N_WAYS; i++) if (slot[set][i].tag == line.tag)
	{
		// cacheline is already in the cache; overwrite
		slot[set][i] = line;
		w_hit++;
		return;
	}

	// address not found; evict a line

    /* @todo improve the logic that determines which cacheline will be evicted. (task 3 & 4) */
    int randInt = RandomUInt();
	int setToEvict = randInt % N_SETS;
	int slotInSetToEvict = randInt % N_WAYS;
	if (slot[setToEvict][slotInSetToEvict].dirty)
	{
		// evicted line is dirty; write to next level
		nextLevel->WriteLine( slot[setToEvict][slotInSetToEvict].tag * CACHELINEWIDTH, setToEvict, slot[setToEvict][slotInSetToEvict] );
	}
	slot[setToEvict][slotInSetToEvict] = line;
	w_miss++;
}

CacheLine Cache::ReadLine( uint addressBase, uint set )
{
	// verify that the address is a multiple of the cacheline width
	assert( (addressBase & (CACHELINEWIDTH - 1)) == 0 );

    // address >> 6 -> get the tag as integer by removing the set (4 bits) and offset (2 bits)
	uint tag = addressBase / CACHELINEWIDTH;

	for (int i = 0; i < N_WAYS; i++)
	{
		if (slot[set][i].tag == tag)
		{
			// cacheline is in the cache; return data
			r_hit++;
			return slot[set][i]; // by value
		}
	}

	// data is not in this cache; ask the next level
	CacheLine line = nextLevel->ReadLine( addressBase, set );

	// store the retrieved line in this cache
	WriteLine( addressBase, set, line );

	// return the requested data
	r_miss++;
	return line;
}

//void MemHierarchy::WriteByte( uint address, uchar value )
//{
//	// fetch the cacheline for the specified address
//	int offsetInLine = address & (CACHELINEWIDTH - 1);
//	int lineAddress = address - offsetInLine;
//	CacheLine line = l1->ReadLine( lineAddress );
//	line.bytes[offsetInLine] = value;
//	line.dirty = true;
//	l1->WriteLine( lineAddress, line );
//}
//
//uchar MemHierarchy::ReadByte( uint address )
//{
//	// fetch the cacheline for the specified address
//	int offsetInLine = address & (CACHELINEWIDTH - 1);
//	int lineAddress = address - offsetInLine;
//	CacheLine line = l1->ReadLine( lineAddress );
//	return line.bytes[offsetInLine];
//}

void MemHierarchy::WriteUint( uint address, uint value )
{
	// fetch the cacheline for the specified address
    int offset = address & (N_WAYS - 1);
    int set = address >> 2 & (N_SETS - 1);
	int addressBase = (address - (set << 2) - offset);

    printf("addr: %u\n", addressBase);

	CacheLine line = l1->ReadLine( addressBase, set );
    memcpy( line.bytes + offset, &value, sizeof( uint ) );
	line.dirty = true;
	l1->WriteLine( address, set, line );
}

uint MemHierarchy::ReadUint( uint address )
{
	// fetch the cacheline for the specified address
    uint offset = address & (N_WAYS - 1);
    uint set = address >> 2 & (N_SETS - 1);
    uint addressBase = (address - (set << 2) - offset);
	//assert( (offsetInLine & 3) == 0 ); // we will not support straddlers
	CacheLine line = l1->ReadLine( addressBase, set );
	return ((uint*)line.bytes)[offset / 4];
}