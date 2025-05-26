#pragma once

namespace Tmpl8 {

// single-level fully associative cache

#define CACHELINEWIDTH	64
#define CACHESIZE		4096				// in bytes
#define N_WAYS          4
#define N_SETS          16
#define N_SLOTS         64                  // N_SETS * N_WAYS or CACHESIZE / CACHELINEWIDTH
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
	virtual void WriteLine( uint address, uint set, CacheLine line ) = 0;
	virtual CacheLine ReadLine( uint address, uint set ) = 0;
	Level* nextLevel = 0;
	uint r_hit = 0, r_miss = 0, w_hit = 0, w_miss = 0;
};

class Cache : public Level // cache level for the memory hierarchy
{
public:
	void WriteLine( uint address, uint set, CacheLine line );
	CacheLine ReadLine( uint address, uint set );
	CacheLine& backdoor( int i ) { return slot[i / N_SETS][i % N_SETS]; } /* for visualization without side effects */
private:
	CacheLine slot[N_SETS][N_WAYS];
};

class Memory : public Level // DRAM level for the memory hierarchy
{
public:
	Memory()
	{
		mem = new uchar[DRAMSIZE];
		memset( mem, 0, DRAMSIZE );
	}
	void WriteLine( uint address, uint set, CacheLine line );
	CacheLine ReadLine( uint address, uint set );
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
		l3->nextLevel = l4 = new Memory();
	}
	void WriteByte( uint address, uchar value );
	uchar ReadByte( uint address );
	void WriteUint( uint address, uint value );
	uint ReadUint( uint address );
	void ResetCounters()
	{
		l1->r_hit = l1->w_hit = l1->r_miss = l1->r_hit = 0;
		l2->r_hit = l2->w_hit = l2->r_miss = l2->r_hit = 0;
	}
	Level* l1, *l2, *l3, *l4;
};

} // namespace Tmpl8