#ifndef TOP_SHARED_MEM_HEADER_H
#define TOP_SHARED_MEM_HEADER_H

// #include "NosuchDebug.h"
#include "NosuchGraphics.h"

#define TOP_SHM_MAGIC_NUMBER 	0xe95df673
#define TOP_SHM_VERSION_NUMBER 	2

#define MMTT_SHM_MAGIC_NUMBER 	0xe95df674
#define MMTT_SHM_VERSION_NUMBER 	1
#define MMTT_CURSORS_MAX 100
#define MMTT_OUTLINES_MAX 1000
#define MMTT_POINTS_MAX 100000

class CBlob;

// If you add new members to this after it's released, add them after dataOffset
class TOP_SharedMemHeader {

public:
    /* Begin version 1 */
    // Magic number to make sure we are looking at the correct memory
    // must be set to TOP_SHM_MAGIC_NUMBER (0xe95df673)
    int							magicNumber;  

    // version number of this header, must be set to TOP_SHM_VERSION_NUMBER
    int							version;

    // image width
    int							width; 

    // image height
    int							height;

    // X aspect of the image
    float						aspectx;

    // Y aspect of the image
    float						aspecty;

    // Format of the image data in memory (RGB, RGBA, BGR, BGRA etc.)
    int							dataFormat;

    // The data type of the image data in memory (unsigned char, float)
    int							dataType; 

    // The desired pixel format of the texture to be created in Touch (RGBA8, RGBA16, RGBA32 etc.)
    int							pixelFormat; 

    // This offset (in bytes) is the diffrence between the start of this header,
    // and the start of the image data
    // The SENDER is required to set this. Unless you are doing something custom
    // you should set this to calcOffset();
    // If you are the RECEIVER, don't change this value.
    int							dataOffset; 

    /* End version 1 */


    // Both the sender and the reciever can use this to get the pointer to the actual
    // image data (as long as dataOffset is set beforehand).
    void						*getImage()
								 {
								     char *c = (char*)this;
								     c += dataOffset;
								     return (void*)c;
								 }

    
    int							calcDataOffset()
    {
		return sizeof(TOP_SharedMemHeader);
    }

};

typedef struct OutlineMem {
	int region;
	int sid;
	float x;
	float y;
	float z;
	int npoints;
	int index_of_firstpoint;
} OutlineMem;

#define BUFF_UNSET (-1)
#define NUM_BUFFS 3
typedef int buff_index;

class MMTT_SharedMemHeader
{
public:
    /* Begin version 1 */
    // Magic number to make sure we are looking at the correct memory
    // must be set to MMTT_SHM_MAGIC_NUMBER
    int							magicNumber;  

    // version number of this header, must be set to MMTT_SHM_VERSION_NUMBER
    int							version;

    int							ncursors_max;
    int							noutlines_max; 
    int							npoints_max; 

	// These are the values that, whenever they are looked at or changed,
	// need to be locked. //////////////////////////////////////////////////
	buff_index		buff_being_constructed; //  -1, 0, 1, 2
	buff_index		buff_displayed_last_time; //  -1, 0, 1, 2
	buff_index		buff_to_display_next; //  -1, 0, 1, 2
	buff_index		buff_to_display;
	bool			buff_inuse[3];
	////////////////////////////////////////////////////////////////////////

    int							numoutlines[3]; 
    int							numpoints[3]; 

    // This offset (in bytes) is the distance from the start of the data.
	// WARNING: do not re-order these fields, the calc.* methods depend on it.
    // int							cursorsOffset[3]; 
    int							outlinesOffset[3]; 
    int							pointsOffset[3]; 

	int	seqnum;
	long lastUpdateTime;  // directly from timeGetTime()
	int active;

    /* End version 1 */

	char *Data() {
		return (char*)this + sizeof(MMTT_SharedMemHeader);
	}

    OutlineMem* outline(int buffnum, int outlinenum) {
		int offset = calcOutlineOffset(buffnum,outlinenum);
		OutlineMem* om = (OutlineMem*)( Data() + offset);
		return om;
	}
    PointMem* point(int buffnum, int pointnum) {
		return (PointMem*)( Data() + calcPointOffset(buffnum,pointnum));
	}

    int	calcOutlineOffset(int buffnum, int outlinenum = 0) {
		// int v1 = calcCursorOffset(0) + NUM_BUFFS*ncursors_max*sizeof(CursorSharedMem);
		int v1 = 0;
		int v2 = buffnum*noutlines_max*sizeof(OutlineMem);
		int v3 = outlinenum*sizeof(OutlineMem);
		// NosuchDebug("calcOutlineOffset v1=0x%x v2=0x%x v3=0x%x  return 0x%x",v1,v2,v3,v1+v2+v3);
		return v1 + v2 + v3;
    }
    int	calcPointOffset(int buffnum, int pointnum = 0) {
		return calcOutlineOffset(0) + NUM_BUFFS*noutlines_max*sizeof(OutlineMem)
			+ buffnum*npoints_max*sizeof(PointMem)
			+ pointnum*sizeof(PointMem);
    }

	int addPoint(buff_index b, float x, float y, float z);
	int addCursorOutline(buff_index b, int region, int sid, float x, float y, float z, int npoints);

	void xinit();
	void clear_lists(buff_index b);
	void check_sanity();
	buff_index grab_unused_buffer();
};

 void print_buff_info(char *prefix, MMTT_SharedMemHeader* h);

#endif 
