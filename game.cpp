#include "precomp.h"
#include "game.h"

#define LINES		750
#define LINEFILE	"lines750.dat"
#define ITERATIONS	16

int lx1[LINES], ly1[LINES], lx2[LINES], ly2[LINES];			// lines: start and end coordinates
uint lc[LINES];												// lines: colors
int x1_, y1_, x2_, y2_;										// room for storing line backup
uint c_;													// line color backup
int fitness;												// similarity to reference image
int lidx = 0;												// current line to be mutated
float peak = 0;												// peak line rendering performance
//int count = SCRWIDTH * SCRHEIGHT;
Surface* reference, *backup;								// surfaces
Timer timer;

#define BYTE unsigned char
#define DWORD unsigned int
#define COLORREF DWORD
#define __int64 long long
#define RGB(r,g,b) ( ((DWORD)(BYTE)r)|((DWORD)((BYTE)g)<<8)|((DWORD)((BYTE)b)<<16) )
#define GetRValue(RGBColor) (BYTE) (RGBColor)
#define GetGValue(RGBColor) (BYTE) (((uint)RGBColor) >> 8)
#define GetBValue(RGBColor) (BYTE) (((uint)RGBColor) >> 16)

// -----------------------------------------------------------
// Mutate
// Randomly modify or replace one line.
// -----------------------------------------------------------
void MutateLine( int i )
{
	// backup the line before modifying it
	x1_ = lx1[i], y1_ = ly1[i];
	x2_ = lx2[i], y2_ = ly2[i];
	c_ = lc[i];
	do
	{
		if (rand() & 1)
		{
			// color mutation (50% probability)
			lc[i] = RandomUInt() & 0xffffff;
		}
		else if (rand() & 1)
		{
			// small mutation (25% probability)
			lx1[i] += RandomUInt() % 6 - 3, ly1[i] += RandomUInt() % 6 - 3;
			lx2[i] += RandomUInt() % 6 - 3, ly2[i] += RandomUInt() % 6 - 3;
			// ensure the line stays on the screen
			lx1[i] = min( SCRWIDTH - 1, max( 0, lx1[i] ) );
			lx2[i] = min( SCRWIDTH - 1, max( 0, lx2[i] ) );
			ly1[i] = min( SCRHEIGHT - 1, max( 0, ly1[i] ) );
			ly2[i] = min( SCRHEIGHT - 1, max( 0, ly2[i] ) );
		}
		else
		{
			// new line (25% probability)
			lx1[i] = RandomUInt() % SCRWIDTH, lx2[i] = RandomUInt() % SCRWIDTH;
			ly1[i] = RandomUInt() % SCRHEIGHT, ly2[i] = RandomUInt() % SCRHEIGHT;
		}
	} while ((abs( lx1[i] - lx2[i] ) < 3) || (abs( ly1[i] - ly2[i] ) < 3));
}

void UndoMutation( int i )
{
	// restore line i to the backuped state
	lx1[i] = x1_, ly1[i] = y1_;
	lx2[i] = x2_, ly2[i] = y2_;
	lc[i] = c_;
}

// -----------------------------------------------------------
// Branchless color blending for Wu lines (extracted from DrawWuLine)
// -----------------------------------------------------------
inline uint BlendColorBranchless(uint lineClr, uint bgClr, int grayl, unsigned short Weighting, unsigned short WeightingXOR, bool inverse = false)
{
    BYTE rl = GetRValue(lineClr);
    BYTE gl = GetGValue(lineClr);
    BYTE bl = GetBValue(lineClr);

    BYTE rb = GetRValue(bgClr);
    BYTE gb = GetGValue(bgClr);
    BYTE bb = GetBValue(bgClr);

    int grayb = (rb * 299 + gb * 587 + bb * 114) >> 10;
    int intWeight = ((grayl < grayb) ^ inverse ? Weighting : WeightingXOR);

    BYTE rr = rl + ((rb - rl) & -(rb < rl)) + ((intWeight * abs(rb - rl)) >> 8);
    BYTE gr = gl + ((gb - gl) & -(gb < gl)) + ((intWeight * abs(gb - gl)) >> 8);
    BYTE br = bl + ((bb - bl) & -(bb < bl)) + ((intWeight * abs(bb - bl)) >> 8);

    return RGB(rr, gr, br);
}

#if defined(USE_SIMD) && defined(__ARM_NEON) && (defined(__APPLE__) || defined(__aarch64__))
#include <arm_neon.h>

    // BlendColorNEON: SIMD-accelerated color blending for Wu lines using NEON
    inline uint BlendColorNEON(uint lineClr, uint bgClr, int grayl, unsigned short Weighting, unsigned short WeightingXOR, bool inverse = false)
    {
        BYTE rl = GetRValue(lineClr);
        BYTE gl = GetGValue(lineClr);
        BYTE bl = GetBValue(lineClr);

        BYTE rb = GetRValue(bgClr);
        BYTE gb = GetGValue(bgClr);
        BYTE bb = GetBValue(bgClr);

        int16x4_t rgbLine = { rl, gl, bl, 0 };
        int16x4_t rgbBackground = { rb, gb, bb, 0 };

        int16x4_t rgbDelta = vsubq_s32(rgbBackground, rgbLine);
        int16x4_t neg_mask = vshrq_n_s32(rgbDelta, 31);
        int16x4_t abs_diff = vsubq_s32(veorq_s32(rgbDelta, neg_mask), neg_mask);

        int grayb = (299 * rb + 587 * gb + 114 * bb) >> 10;
        int intWeight = ((grayl < grayb) ^ inverse ? Weighting : WeightingXOR);

        int16x4_t weight_vec = vdupq_n_s32(intWeight);
        int16x4_t scaled = vmulq_s32(abs_diff, weight_vec);
        int16x4_t shifted = vshrq_n_s32(scaled, 8);
        int16x4_t blended = vaddq_s32(rgbLine, shifted);

        BYTE rr = vgetq_lane_s32(blended, 0);
        BYTE gr = vgetq_lane_s32(blended, 1);
        BYTE br = vgetq_lane_s32(blended, 2);

        return RGB(rr, gr, br);
    }
#endif

#if defined(USE_SIMD) && defined(__SSE2__)
#include <emmintrin.h>

    inline uint BlendColorSSE(uint lineClr, uint bgClr, int grayl, unsigned short Weighting, unsigned short WeightingXOR, bool inverse = false)
    {
        int rl = GetRValue(lineClr);
        int gl = GetGValue(lineClr);
        int bl = GetBValue(lineClr);

        int rb = GetRValue(bgClr);
        int gb = GetGValue(bgClr);
        int bb = GetBValue(bgClr);

        __m128i rgbLine = _mm_set_epi32(0, bl, gl, rl);
        __m128i rgbBackground = _mm_set_epi32(0, bb, gb, rb);

        __m128i rgbDelta = _mm_sub_epi32(rgbBackground, rgbLine);
        __m128i neg_mask = _mm_srai_epi32(rgbDelta, 31);
        __m128i abs_diff = _mm_sub_epi32(_mm_xor_si128(rgbDelta, neg_mask), neg_mask);

        int grayb = (299 * rb + 587 * gb + 114 * bb) >> 10;
        int intWeight = ((grayl < grayb) ^ inverse ? Weighting : WeightingXOR);

        __m128i weight_vec = _mm_set1_epi32(intWeight);
        __m128i scaled = _mm_mullo_epi32(abs_diff, weight_vec);
        __m128i shifted = _mm_srli_epi32(scaled, 8);
        __m128i blended = _mm_add_epi32(rgbLine, shifted);

        int rr = _mm_cvtsi128_si32(blended);
        blended = _mm_srli_si128(blended, 4);
        int gr = _mm_cvtsi128_si32(blended);
        blended = _mm_srli_si128(blended, 4);
        int br = _mm_cvtsi128_si32(blended);

        return RGB(rr, gr, br);
    }
#endif

inline int fast_abs(int x) {
    return (x ^ (x >> 31)) - (x >> 31);
}

inline BYTE blend_channel(BYTE line, BYTE bg, int weight) {
    return line + ((bg - line) & -(bg < line)) + ((weight * abs(bg - line)) >> 8);
}

// -----------------------------------------------------------
// DrawWuLine
// Anti-aliased line rendering.
// Straight from:
// https://www.codeproject.com/Articles/13360/Antialiasing-Wu-Algorithm
// -----------------------------------------------------------
void DrawWuLine( Surface *screen, int X0, int Y0, int X1, int Y1, uint clrLine )
{
	//const float weightNorm = 1.f / 255.f;
    /* Make sure the line runs top to bottom */
    if (Y0 > Y1)
    {
        std::swap( Y0, Y1 );
		std::swap( X0, X1 );
    }

    /* Draw the initial pixel, which is always exactly intersected by
    the line and so needs no weighting */
    screen->Plot( X0, Y0, clrLine );

    int XDir = 1;
	int DeltaX = X1 - X0;
    if( DeltaX < 0 )
    {
        XDir   = -1;
        DeltaX = -DeltaX; /* make DeltaX positive */
    }

    /* Special-case horizontal, vertical, and diagonal lines, which
    require no weighting because they go right through the center of
    every pixel */
    int DeltaY = Y1 - Y0;

    unsigned short ErrorAdj;
    unsigned short ErrorAccTemp, Weighting, WeightingXOR;

    /* Line is not horizontal, diagonal, or vertical */
    /* initialize the line error accumulator to 0 */
    unsigned short ErrorAcc = 0;

    BYTE rl = GetRValue( clrLine );
    BYTE gl = GetGValue( clrLine );
    BYTE bl = GetBValue( clrLine );

	int grayl = (rl * 299 + gl * 587 + bl * 114) >> 10;
	uint current_pixel_index = X0 + Y0 * SCRWIDTH;

    /* Is this an X-major or Y-major line? */
    if (DeltaY > DeltaX)
    {
        /* Y-major line; calculate 16-bit fixed-point fractional part of a
        pixel that X advances each time Y advances 1 pixel, truncating the
        result so that we won't overrun the endpoint along the X axis */
        ErrorAdj = ((unsigned long) DeltaX << 16) / (unsigned long) DeltaY;

        /* Draw all pixels other than the first and last */
        while (--DeltaY) {
            /* remember currrent accumulated error */
            ErrorAccTemp = ErrorAcc;

            /* calculate error for next pixel */
            ErrorAcc += ErrorAdj;

            if (ErrorAcc <= ErrorAccTemp) {
                /* The error accumulator turned over, so advance the X coord */
                X0 += XDir;
            	current_pixel_index += XDir;
            }

            Y0++;
        	current_pixel_index += SCRWIDTH;

            Weighting = ErrorAcc >> 8;
            WeightingXOR = Weighting ^ 255;

            COLORREF clrBackGround = screen->pixels[current_pixel_index];
            BYTE rb = GetRValue( clrBackGround );
            BYTE gb = GetGValue( clrBackGround );
            BYTE bb = GetBValue( clrBackGround );

            int intWeight = (grayl < ((rb * 299 + gb * 587 + bb * 114) >> 10) ? Weighting : WeightingXOR);

            BYTE rr = rl + ((rb - rl) & -(rb < rl)) + ((intWeight * abs(rb - rl)) >> 8);
            BYTE gr = gl + ((gb - gl) & -(gb < gl)) + ((intWeight * abs(gb - gl)) >> 8);
            BYTE br = bl + ((bb - bl) & -(bb < bl)) + ((intWeight * abs(bb - bl)) >> 8);

            screen->Plot( X0, Y0, RGB( rr, gr, br ) );

            clrBackGround = screen->pixels[current_pixel_index + XDir];
            rb = GetRValue( clrBackGround );
            gb = GetGValue( clrBackGround );
            bb = GetBValue( clrBackGround );

        	intWeight = (grayl < ((rb * 299 + gb * 587 + bb * 114) >> 10) ? WeightingXOR : Weighting);

            rr = rl + ((rb - rl) & -(rb < rl)) + ((intWeight * abs(rb - rl)) >> 8);
            gr = gl + ((gb - gl) & -(gb < gl)) + ((intWeight * abs(gb - gl)) >> 8);
            br = bl + ((bb - bl) & -(bb < bl)) + ((intWeight * abs(bb - bl)) >> 8);

            screen->Plot( X0 + XDir, Y0, RGB( rr, gr, br ) );
        }

        /* Draw the final pixel, which is always exactly intersected by the line
        and so needs no weighting */
        screen->Plot( X1, Y1, clrLine );
    } else {
        /* It's an X-major line; calculate 16-bit fixed-point fractional part of a
        pixel that Y advances each time X advances 1 pixel, truncating the
        result to avoid overrunning the endpoint along the X axis */
        ErrorAdj = ((unsigned long) DeltaY << 16) / (unsigned long) DeltaX;

        /* Draw all pixels other than the first and last */
        while (--DeltaX) {
            /* remember currrent accumulated error */
            ErrorAccTemp = ErrorAcc;

            /* calculate error for next pixel */
            ErrorAcc += ErrorAdj;

            if (ErrorAcc <= ErrorAccTemp) {
                /* The error accumulator turned over, so advance the Y coord */
                Y0++;
                current_pixel_index += SCRWIDTH;
            }

            X0 += XDir;
        	current_pixel_index += XDir;

            Weighting = ErrorAcc >> 8;
            WeightingXOR = Weighting ^ 255;

            COLORREF clrBackGround = screen->pixels[current_pixel_index];
            BYTE rb = GetRValue( clrBackGround );
            BYTE gb = GetGValue( clrBackGround );
            BYTE bb = GetBValue( clrBackGround );

            int intWeight = (grayl < ((rb * 299 + gb * 587 + bb * 114) >> 10) ? Weighting : WeightingXOR);

            BYTE rr = rl + ((rb - rl) & -(rb < rl)) + ((intWeight * abs(rb - rl)) >> 8);
            BYTE gr = gl + ((gb - gl) & -(gb < gl)) + ((intWeight * abs(gb - gl)) >> 8);
            BYTE br = bl + ((bb - bl) & -(bb < bl)) + ((intWeight * abs(bb - bl)) >> 8);

            screen->Plot( X0, Y0, RGB( rr, gr, br ) );

            clrBackGround = screen->pixels[current_pixel_index + SCRWIDTH];
            rb = GetRValue( clrBackGround );
            gb = GetGValue( clrBackGround );
            bb = GetBValue( clrBackGround );

            intWeight = (grayl < ((rb * 299 + gb * 587 + bb * 114) >> 10) ? WeightingXOR : Weighting);

            rr = rl + ((rb - rl) & -(rb < rl)) + ((intWeight * abs(rb - rl)) >> 8);
            gr = gl + ((gb - gl) & -(gb < gl)) + ((intWeight * abs(gb - gl)) >> 8);
            br = bl + ((bb - bl) & -(bb < bl)) + ((intWeight * abs(bb - bl)) >> 8);

            screen->Plot( X0, Y0 + 1, RGB( rr, gr, br ) );
        }

        /* Draw the final pixel, which is always exactly intersected by the line
        and so needs no weighting */
        screen->Plot( X1, Y1, clrLine );
    }
}

// -----------------------------------------------------------
// Fitness evaluation
// Compare current generation against reference image.
// -----------------------------------------------------------
int Game::Evaluate()
{
	__int64 diff = 0;
	const uint count = SCRWIDTH * SCRHEIGHT;
	uint* srcSet = screen->pixels;
	uint* refSet = reference->pixels;
	uint* end = srcSet + count;
	while (srcSet < end)
	//for( uint i = 0; i < count; i++ )
	{
		uint src = *srcSet++, ref = *refSet++;
		//uint src = screen->pixels[i];
		//uint ref = reference->pixels[i];
		//int r0 = (src >> 16) & 255, g0 = (src >> 8) & 255, b0 = src & 255;
		//int r1 = ref >> 16, g1 = (ref >> 8) & 255, b1 = ref & 255;

		int dr = ((src >> 16) & 255) - (ref >> 16),
			dg = ((src >> 8) & 255) - ((ref >> 8) & 255),
			db = (src & 255) - (ref & 255);
		// calculate squared color difference;
		// take into account eye sensitivity to red, green and blue
		//int dr_sq = dr * dr;
		//int dg_sq = dg * dg;
		//diff += (dr * dr << 1) + dr * dr + (dg * dg << 2) + (dg * dg << 1) + db * db;
		diff += 3 * dr * dr + 6 * dg * dg + db * db;
	}
	return (int)(diff >> 5);
}

// -----------------------------------------------------------
// Application initialization
// Load a previously saved generation, if available.
// -----------------------------------------------------------
void Game::Init()
{
	for (int i = 0; i < LINES; i++) MutateLine( i );
	FILE* f = fopen( LINEFILE, "rb" );
	/*if (f)
	{
		fread( lx1, 4, LINES, f );
		fread( ly1, 4, LINES, f );
		fread( lx2, 4, LINES, f );
		fread( ly2, 4, LINES, f );
		fread( lc, 4, LINES, f );
		fclose( f );
	}*/
	reference = new Surface( "assets/bird.png" );
	backup = new Surface( SCRWIDTH, SCRHEIGHT );
	memset( screen->pixels, 255, SCRWIDTH * SCRHEIGHT * 4 );
	for (int j = 0; j < LINES; j++)
	{
		DrawWuLine( screen, lx1[j], ly1[j], lx2[j], ly2[j], lc[j] );
	}
	fitness = Evaluate();
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::Tick( float /* deltaTime */ )
{
	timer.reset();
	int lineCount = 0;
	int iterCount = 0;
	// draw up to lidx
	memset( screen->pixels, 255, SCRWIDTH * SCRHEIGHT * 4 );
	for (int j = 0; j < lidx; j++, lineCount++)
	{
		DrawWuLine( screen, lx1[j], ly1[j], lx2[j], ly2[j], lc[j] );
	}
	int base = lidx;
	screen->CopyTo( backup, 0, 0 );
	// iterate and draw from lidx to end
	for (int k = 0; k < ITERATIONS; k++)
	{
		backup->CopyTo( screen, 0, 0 );
		MutateLine( lidx );
		for (int j = base; j < LINES; j++, lineCount++)
		{
			DrawWuLine( screen, lx1[j], ly1[j], lx2[j], ly2[j], lc[j] );
		}
		int diff = Evaluate();
		if (diff < fitness) fitness = diff; else UndoMutation( lidx );
		lidx = (lidx + 1) % LINES;
		iterCount++;
	}
	// stats
	char t[128];
	float elapsed = timer.elapsed();
	float lps = (float)lineCount / elapsed;
	peak = max( lps, peak );
	sprintf( t, "fitness: %i", fitness );
	screen->Bar( 0, SCRHEIGHT - 33, 130, SCRHEIGHT - 1, 0 );
	screen->Print( t, 2, SCRHEIGHT - 24, 0xffffff );
	sprintf( t, "lps:     %5.2fK", lps );
	screen->Print( t, 2, SCRHEIGHT - 16, 0xffffff );
	sprintf( t, "ips:     %5.2f", (iterCount * 1000) / elapsed );
	screen->Print( t, 2, SCRHEIGHT - 8, 0xffffff );
	sprintf( t, "peak:    %5.2f", peak );
	screen->Print( t, 2, SCRHEIGHT - 32, 0xffffff );
}

// -----------------------------------------------------------
// Application termination
// Save the current generation, so we can continue later.
// -----------------------------------------------------------
void Game::Shutdown()
{
	FILE* f = fopen( LINEFILE, "wb" );
	fwrite( lx1, 4, LINES, f );
	fwrite( ly1, 4, LINES, f );
	fwrite( lx2, 4, LINES, f );
	fwrite( ly2, 4, LINES, f );
	fwrite( lc, 4, LINES, f );
	fclose( f );
}