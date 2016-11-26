//----------------------------------------------------------------------------
//
// File:        level.cpp
// Date:        26-Oct-1994
// Programmer:  Marc Rousseau
//
// Description: Object classes for manipulating Doom Maps
//
// Copyright (c) 1994-2004 Marc Rousseau, All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "level.hpp"
#include "wad.hpp"

DBG_REGISTER ( __FILE__ );

#define SWAP_ENDIAN_16(x)   (((( x ) & 0xFF ) << 8 ) | ((( x ) >> 8 ) & 0xFF ))

//#define min(x,y)    ((( x ) < ( y )) ? x : y )

#if defined ( __GNUC__ ) || defined ( __INTEL_COMPILER )

char *strupr ( char *ptr )
{
    for ( int i = 0; ptr[i]; i++ ) {
        ptr[i] = toupper ( ptr[i] );
    }
    return ptr;
}

#endif

DoomLevel::sLevelLump::sLevelLump () :
    changed ( false ),
    byteOrder ( BYTE_ORDER ),
    elementSize ( 0 ),
    elementCount ( 0 ),
    dataSize ( 0 ),
    rawData ( NULL )
{
}

DoomLevel::DoomLevel ( const char *_name, WAD *_wad, bool bLoadData ) :
    m_Wad ( _wad ),
    m_IsHexen ( false ),
    m_Title ( NULL ),
    m_Music ( NULL ),
    m_Cluster ( 0 ),
    m_ThingData ( NULL ),
    m_LineDefData ( NULL )
{
    FUNCTION_ENTRY ( this, "DoomLevel ctor", true );

    m_Title     = NULL;
    m_Music     = NULL;
    m_Cluster   = -1;
    m_IsHexen = false;

    memset ( m_Name, 0, sizeof ( m_Name ));
    for ( int i = 0; i < 8; i++ ) {
        if ( _name[i] == '\0' ) break;
        m_Name[i] = ( char ) toupper ( _name[i] );
    }

    m_Map.elementSize       = 1;
    m_Thing.elementSize     = 1;
    m_LineDef.elementSize   = 1;
    m_SideDef.elementSize   = sizeof ( wSideDef );
    m_Vertex.elementSize    = sizeof ( wVertex );
    m_Sector.elementSize    = sizeof ( wSector );
    m_Segs.elementSize      = sizeof ( wSegs );
    m_SubSector.elementSize = sizeof ( wSSector );
    m_Node.elementSize      = sizeof ( wNode );
    m_Reject.elementSize    = 1;
    m_BlockMap.elementSize  = 1;
    m_Behavior.elementSize  = 1;

    if ( bLoadData == true ) Load ();

    LoadHexenInfo ();
}

DoomLevel::~DoomLevel ()
{
    FUNCTION_ENTRY ( this, "DoomLevel dtor", true );

    if ( m_Title != NULL ) free (( char * ) m_Title );
    if ( m_Music != NULL ) free (( char * ) m_Music );
    CleanUp ();
}

bool DoomLevel::IsValid ( bool checkBSP, bool print ) const
{
    FUNCTION_ENTRY ( this, "DoomLevel::isValid", true );

    bool isValid = true;

    bool *used = new bool [ SideDefCount () ];
    memset ( used, false, sizeof ( bool ) * SideDefCount ());

    // Sanity check for LINEDEFS
    const wLineDef *lineDef = GetLineDefs ();

    for ( int i = 0; i < LineDefCount (); i++ ) {
        if ( lineDef [i].start >= VertexCount ()) {
            if ( print == true ) fprintf ( stderr, "LINEDEFS[%d].%s vertex is invalid (%d/%d)\n", i, "start", lineDef [i].start, VertexCount ());
            isValid = false;
        }
        if ( lineDef [i].end >= VertexCount ()) {
            if ( print == true ) fprintf ( stderr, "LINEDEFS[%d].%s vertex is invalid (%d/%d)\n", i, "end", lineDef [i].end, VertexCount ());
            isValid = false;
        }
        if ( lineDef [i].sideDef [ LEFT_SIDEDEF ] != NO_SIDEDEF ) {
            if ( lineDef [i].sideDef [ LEFT_SIDEDEF ] >= SideDefCount ()) {
                if ( print == true ) fprintf ( stderr, "LINEDEFS[%d].sideDef[%s] is invalid (%d/%d)\n", i, "left", lineDef [i].sideDef [LEFT_SIDEDEF], SideDefCount ());
                isValid = false;
            } else {
                used [ lineDef [i].sideDef [ LEFT_SIDEDEF ]] = true;
            }
        }
        if ( lineDef [i].sideDef [ RIGHT_SIDEDEF ] != NO_SIDEDEF ) {
            if ( lineDef [i].sideDef [ RIGHT_SIDEDEF ] >= SideDefCount ()) {
                if ( print == true ) fprintf ( stderr, "LINEDEFS[%d].sideDef[%s] is invalid (%d/%d)\n", i, "right", lineDef [i].sideDef [RIGHT_SIDEDEF], SideDefCount ());
                isValid = false;
            } else {
                used [ lineDef [i].sideDef [ RIGHT_SIDEDEF ]] = true;
            }
        }
    }

    // Sanity check for SIDEDEFS
    const wSideDef *sideDef = GetSideDefs ();

    for ( int i = 0; i < SideDefCount (); i++ ) {
        if (( sideDef [i].sector >= SectorCount ()) && ( used [i] == true )) {
            if ( print == true ) fprintf ( stderr, "SIDEDEFS[%d].sector is invalid (%d/%d)\n", i, sideDef [i].sector, SectorCount ());
            isValid = false;
        }
    }

    delete [] used;

    if ( checkBSP == true ) {

        // Sanity check for SEGS
        const wSegs *segs = GetSegs ();

        for ( int i = 0; i < SegCount (); i++ ) {
            if ( segs [i].start >= VertexCount ()) {
                if ( print == true ) fprintf ( stderr, "SEGS[%d].%s m_Vertex is invalid (%d/%d)\n", i, "start", segs [i].start, VertexCount ());
                isValid = false;
            }
            if ( segs [i].end >= VertexCount ()) {
                if ( print == true ) fprintf ( stderr, "SEGS[%d].%s m_Vertex is invalid (%d/%d)\n", i, "end", segs [i].end, VertexCount ());
                isValid = false;
            }
            if ( segs [i].lineDef >= LineDefCount ()) {
                if ( print == true ) fprintf ( stderr, "SEGS[%d].lineDef is invalid (%d/%d)\n", i, segs [i].lineDef, LineDefCount ());
                isValid = false;
            }
            if ( segs [i].start == segs [i].end ) {
                if ( print == true ) fprintf ( stderr, "SEGS[%d] is zero length\n", i );
                isValid = false;
	    }
        }

        // Sanity check for SSECTORS
        const wSSector *subSector = GetSubSectors ();

        for ( int i = 0; i < SubSectorCount (); i++ ) {
            if ( subSector [i].first >= SegCount ()) {
                if ( print == true ) fprintf ( stderr, "SSECTORS[%d].first is invalid (%d/%d)\n", i, subSector [i].first, SegCount ());
                isValid = false;
            }
            if ( subSector [i].first + subSector [i].num > SegCount ()) {
                if ( print == true ) fprintf ( stderr, "SSECTORS[%d].num is invalid (%d/%d)\n", i, subSector [i].num, SegCount ());
                isValid = false;
            }
        }

        // Sanity check for NODES
        const wNode *node = GetNodes ();

        if ( NodeCount () < 2 ) {
            if ( print == true ) fprintf ( stderr, "NODES structure is invalid\n" );
            isValid = false;
        }

        for ( int i = 0; i < NodeCount (); i++ ) {
            UINT16 child;
            child = node [i].child [0];
            if (( node [i].dx == 0 ) && ( node[i].dy == 0 )) {
                if ( print == true ) fprintf ( stderr, "NODES[%d] is invalid (dx == dy == 0)\n", i );
                isValid = false;
	    }
            if ( child & 0x8000 ) {
                if (( child & 0x7FFF ) >= SubSectorCount ()) {
                    if ( print == true ) fprintf ( stderr, "NODES[%d].child[%d] is invalid (0x8000 | %d/%d)\n", i, 0, child & 0x7FFF, SubSectorCount ());
                    isValid = false;
                }
            } else {
                if ( child >= NodeCount ()) {
                    if ( print == true ) fprintf ( stderr, "NODES[%d].child[%d] is invalid (%d/%d)\n", i, 0, child, NodeCount ());
                    isValid = false;
                }
            }
            child = node [i].child [1];
            if ( child & 0x8000 ) {
                if (( child & 0x7FFF ) >= SubSectorCount ()) {
                    if ( print == true ) fprintf ( stderr, "NODES[%d].child[%d] is invalid (0x8000 | %d/%d)\n", i, 1, child & 0x7FFF, SubSectorCount ());
                    isValid = false;
                }
            } else {
                if ( child >= NodeCount ()) {
                    if ( print == true ) fprintf ( stderr, "NODES[%d].child[%d] is invalid (%d/%d)\n", i, 1, child, NodeCount ());
                    isValid = false;
                }
            }
        }
    }

    return isValid;
}

bool DoomLevel::IsDirty () const
{
    FUNCTION_ENTRY ( this, "DoomLevel::IsDirty", true );

    if ( m_Map.changed == true ) return true;
    if ( m_Thing.changed == true ) return true;
    if ( m_LineDef.changed == true ) return true;
    if ( m_SideDef.changed == true ) return true;
    if ( m_Vertex.changed == true ) return true;
    if ( m_Sector.changed == true ) return true;
    if ( m_Segs.changed == true ) return true;
    if ( m_SubSector.changed == true ) return true;
    if ( m_Node.changed == true ) return true;
    if ( m_Reject.changed == true ) return true;
    if ( m_BlockMap.changed == true ) return true;
    if ( m_Behavior.changed == true ) return true;

    return false;
}

void DoomLevel::CleanUpEntry ( sLevelLump *entry )
{
    FUNCTION_ENTRY ( this, "DoomLevel::CleanUpEntry", true );

    delete [] ( char * ) entry->rawData;

    entry->changed      = false;
    entry->byteOrder    = BYTE_ORDER;
    entry->elementCount = 0;
    entry->dataSize     = 0;
    entry->rawData      = NULL;
}

void DoomLevel::CleanUp ()
{
    FUNCTION_ENTRY ( this, "DoomLevel::CleanUp", true );

    CleanUpEntry ( &m_Map );
    CleanUpEntry ( &m_Thing );
    CleanUpEntry ( &m_LineDef );
    CleanUpEntry ( &m_SideDef );
    CleanUpEntry ( &m_Vertex );
    CleanUpEntry ( &m_Sector );
    CleanUpEntry ( &m_Segs );
    CleanUpEntry ( &m_SubSector );
    CleanUpEntry ( &m_Node );
    CleanUpEntry ( &m_Reject );
    CleanUpEntry ( &m_BlockMap );
    CleanUpEntry ( &m_Behavior );

    delete [] m_ThingData;
    delete [] m_LineDefData;

    m_ThingData   = NULL;
    m_LineDefData = NULL;
}

#if ( BYTE_ORDER == BIG_ENDIAN )

void DoomLevel::AdjustByteOrderMap ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderMap", true );

    if ( m_Map.byteOrder != byteOrder ) {
        // Nothing to do for this type
    }

    m_Map.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrderThing ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderThing", true );

    if ( m_Thing.byteOrder != byteOrder ) {
        wThing *thing = m_ThingData;
        for ( int i = 0; i < m_Thing.elementCount; i++ ) {
            thing [i].xPos     = SWAP_ENDIAN_16 ( thing [i].xPos );
            thing [i].yPos     = SWAP_ENDIAN_16 ( thing [i].yPos );
            thing [i].angle    = SWAP_ENDIAN_16 ( thing [i].angle );
            thing [i].type     = SWAP_ENDIAN_16 ( thing [i].type );
            thing [i].attr     = SWAP_ENDIAN_16 ( thing [i].attr );
            thing [i].tid      = SWAP_ENDIAN_16 ( thing [i].tid );
            thing [i].altitude = SWAP_ENDIAN_16 ( thing [i].altitude );
        }
    }

    m_Thing.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrderLineDef ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderLineDef", true );

    if ( m_LineDef.byteOrder != byteOrder ) {
        wLineDef *lineDef = m_LineDefData;
        for ( int i = 0; i < m_LineDef.elementCount; i++ ) {
            lineDef [i].start       = SWAP_ENDIAN_16 ( lineDef [i].start );
            lineDef [i].end         = SWAP_ENDIAN_16 ( lineDef [i].end );
            lineDef [i].flags       = SWAP_ENDIAN_16 ( lineDef [i].flags );
            lineDef [i].type        = SWAP_ENDIAN_16 ( lineDef [i].type );
            lineDef [i].tag         = SWAP_ENDIAN_16 ( lineDef [i].tag );
            lineDef [i].sideDef [0] = SWAP_ENDIAN_16 ( lineDef [i].sideDef [0] );
            lineDef [i].sideDef [1] = SWAP_ENDIAN_16 ( lineDef [i].sideDef [1] );
        }
    }

    m_LineDef.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrderSideDef ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderSideDef", true );

    if ( m_SideDef.byteOrder != byteOrder ) {
        wSideDef *sideDef = ( wSideDef * ) m_SideDef.rawData;
        for ( int i = 0; i < m_SideDef.elementCount; i++ ) {
            sideDef [i].xOff   = SWAP_ENDIAN_16 ( sideDef [i].xOff );
            sideDef [i].yOff   = SWAP_ENDIAN_16 ( sideDef [i].yOff );
            sideDef [i].sector = SWAP_ENDIAN_16 ( sideDef [i].sector );
        }
    }

    m_SideDef.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrderVertex ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderVertex", true );

    if ( m_Vertex.byteOrder != byteOrder ) {
        wVertex *vertex = ( wVertex * ) m_Vertex.rawData;
        for ( int i = 0; i < m_Vertex.elementCount; i++ ) {
            vertex [i].x = SWAP_ENDIAN_16 ( vertex [i].x );
            vertex [i].y = SWAP_ENDIAN_16 ( vertex [i].y );
        }
    }

    m_Vertex.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrderSector ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderSector", true );

    if ( m_Sector.byteOrder != byteOrder ) {
        wSector *sector = ( wSector * ) m_Sector.rawData;
        for ( int i = 0; i < m_Sector.elementCount; i++ ) {
            sector [i].floorh  = SWAP_ENDIAN_16 ( sector [i].floorh );
            sector [i].ceilh   = SWAP_ENDIAN_16 ( sector [i].ceilh );
            sector [i].light   = SWAP_ENDIAN_16 ( sector [i].light );
            sector [i].special = SWAP_ENDIAN_16 ( sector [i].special );
            sector [i].trigger = SWAP_ENDIAN_16 ( sector [i].trigger );
        }
    }

    m_Sector.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrderSegs ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderSegs", true );

    if ( m_Segs.byteOrder != byteOrder ) {
        wSegs *segs = ( wSegs * ) m_Segs.rawData;
        for ( int i = 0; i < m_Segs.elementCount; i++ ) {
            segs [i].start   = SWAP_ENDIAN_16 ( segs [i].start );
            segs [i].end     = SWAP_ENDIAN_16 ( segs [i].end );
            segs [i].angle   = SWAP_ENDIAN_16 ( segs [i].angle );
            segs [i].lineDef = SWAP_ENDIAN_16 ( segs [i].lineDef );
            segs [i].flip    = SWAP_ENDIAN_16 ( segs [i].flip );
            segs [i].offset  = SWAP_ENDIAN_16 ( segs [i].offset );
        }
    }

    m_Segs.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrderSubSector ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderSubSector", true );

    if ( m_SubSector.byteOrder != byteOrder ) {
        wSSector *ssector = ( wSSector * ) m_SubSector.rawData;
        for ( int i = 0; i < m_SubSector.elementCount; i++ ) {
            ssector [i].num   = SWAP_ENDIAN_16 ( ssector [i].num );
            ssector [i].first = SWAP_ENDIAN_16 ( ssector [i].first );
        }
    }

    m_SubSector.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrderNode ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderNode", true );

    if ( m_Node.byteOrder != byteOrder ) {
        // The entire structure is composed of 16-bit entries
        UINT16 *ptr = ( UINT16 * ) m_Node.rawData;
        for ( int i = 0; i < ( int ) m_Node.dataSize / 2; i++ ) {
            ptr [i] = SWAP_ENDIAN_16 ( ptr [i] );
        }
    }

    m_Node.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrderReject ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderReject", true );

    if ( m_Reject.byteOrder != byteOrder ) {
        // Nothing to do for this type
    }

    m_Reject.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrderBlockMap ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderBlockMap", true );


    if ( m_BlockMap.byteOrder != byteOrder ) {
        // The entire structure is composed of 16-bit entries
        UINT16 *ptr = ( UINT16 * ) m_BlockMap.rawData;
        for ( int i = 0; i < ( int ) m_BlockMap.dataSize / 2; i++ ) {
            ptr [i] = SWAP_ENDIAN_16 ( ptr [i] );
        }
    }

    m_BlockMap.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrderBehavior ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrderBehavior", true );

    if ( m_Behavior.byteOrder != byteOrder ) {
        // Nothing to do for this type
    }

    m_Behavior.byteOrder = byteOrder;
}

void DoomLevel::AdjustByteOrder ( int byteOrder )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AdjustByteOrder", true );

    AdjustByteOrderMap ( byteOrder );
    AdjustByteOrderThing ( byteOrder );
    AdjustByteOrderLineDef ( byteOrder );
    AdjustByteOrderSideDef ( byteOrder );
    AdjustByteOrderVertex ( byteOrder );
    AdjustByteOrderSector ( byteOrder );
    AdjustByteOrderSegs ( byteOrder );
    AdjustByteOrderSubSector ( byteOrder );
    AdjustByteOrderNode ( byteOrder );
    AdjustByteOrderReject ( byteOrder );
    AdjustByteOrderBlockMap ( byteOrder );
    AdjustByteOrderBehavior ( byteOrder );
}

#endif

void DoomLevel::ReplaceVertices ( int *map, wVertex *newVertices, int count )
{
    wLineDef *lineDef = ( wLineDef * ) GetLineDefs ();
    for ( int i = 0; i < LineDefCount (); i++ ) {
        lineDef [i].start = ( UINT16 ) map [ lineDef [i].start ];
        lineDef [i].end   = ( UINT16 ) map [ lineDef [i].end ];
    }

    m_LineDef.changed = true;

    wSegs *segs = ( wSegs * ) GetSegs ();
    for ( int i = 0; i < SegCount (); i++ ) {
        segs [i].start = ( UINT16 ) map [ segs [i].start ];
        segs [i].end   = ( UINT16 ) map [ segs [i].end ];
    }

    m_Segs.changed    = true;

    delete [] ( char * ) m_Vertex.rawData;
    delete [] map;

    m_Vertex.changed      = true;
    m_Vertex.elementCount = count;
    m_Vertex.rawData      = newVertices;
}

void DoomLevel::TrimVertices ()
{
    FUNCTION_ENTRY ( this, "DoomLevel::TrimVertices", true );

    int *used = new int [ VertexCount () ];
    memset ( used, 0, sizeof ( int ) * VertexCount ());

    wLineDef *lineDef = ( wLineDef * ) GetLineDefs ();
    for ( int i = 0; i < LineDefCount (); i++ ) {
        used [ lineDef [i].start ] = 1;
        used [ lineDef [i].end ]   = 1;
    }

    wSegs *segs = ( wSegs * ) GetSegs ();
    for ( int i = 0; i < SegCount (); i++ ) {
        used [ segs [i].start ] = 1;
        used [ segs [i].end ]   = 1;
    }

    const wVertex *oldVertices = GetVertices ();
    wVertex *newVertices = new wVertex [ VertexCount () ];

    int count = 0;
    for ( int i = 0; i < VertexCount (); i++ ) {
        if ( used [i] == 1 ) {
            newVertices [count] = oldVertices [i];
            used [i]            = count++;
        }
    }

    if ( VertexCount () == count ) {
        delete [] newVertices;
        delete [] used;
        return;
    }

    ReplaceVertices ( used, newVertices, count );
}

void DoomLevel::PackVertices ()
{
    FUNCTION_ENTRY ( this, "DoomLevel::PackVertices", true );

    int *used = new int [ VertexCount () ];
    memset ( used, 0, sizeof ( int ) * VertexCount ());

    int count = 0;
    UINT32 *vert = ( UINT32 * ) m_Vertex.rawData;

    for ( int i = 0, j; i < VertexCount (); i++ ) {
        UINT32 currentVert = vert [i];
        for ( j = 0; j < i; j++ ) {
            if ( vert [j] == currentVert ) break;
        }
        used [i] = j;
        if ( i == j ) count++;
    }

    if ( VertexCount () == count ) {
        delete [] used;
        return;
    }

    const wVertex *oldVertices = GetVertices ();
    wVertex *newVertices = new wVertex [ count ];

    count = 0;
    for ( int i = 0; i < VertexCount (); i++ ) {
        if ( used [i] == i ) {
            newVertices [count] = oldVertices [i];
            used [i]            = count++;
        } else {
            used [i] = used [ used [i]];
        }
    }

    ReplaceVertices ( used, newVertices, count );
}

void DoomLevel::ConvertRaw1ToThing ( int max, wThing1 *src, wThing *dest )
{
    FUNCTION_ENTRY ( NULL, "DoomLevel::ConvertRaw1ToThing", true );

    memset ( dest, 0, sizeof ( wThing ) * max );
    for ( int i = 0; i < max; i++ ) {
        memcpy ( &dest [i], &src [i], sizeof ( wThing1 ));
    }
}

void DoomLevel::ConvertRaw2ToThing ( int max, wThing2 *src, wThing *dest )
{
    FUNCTION_ENTRY ( NULL, "DoomLevel::ConvertRaw2ToThing", true );

    memset ( dest, 0, sizeof ( wThing ) * max );
    for ( int i = 0; i < max; i++ ) {
        dest [i].xPos     = src [i].xPos;
        dest [i].yPos     = src [i].yPos;
        dest [i].angle    = src [i].angle;
        dest [i].type     = src [i].type;
        dest [i].attr     = src [i].attr;
        dest [i].tid      = src [i].tid;
        dest [i].altitude = src [i].altitude;
        dest [i].special  = src [i].special;
        memcpy ( dest [i].arg, src [i].arg, sizeof ( src[i].arg ));
    }
}

void DoomLevel::ConvertThingToRaw1 ( int max, wThing *src, wThing1 *dest )
{
    FUNCTION_ENTRY ( NULL, "DoomLevel::ConvertThingToRaw1", true );

    memset ( dest, 0, sizeof ( wThing1 ) * max );
    for ( int i = 0; i < max; i++ ) {
        memcpy ( &dest [i], &src [i], sizeof ( wThing1 ));
    }
}

void DoomLevel::ConvertThingToRaw2 ( int max, wThing *src, wThing2 *dest )
{
    FUNCTION_ENTRY ( NULL, "DoomLevel::ConvertThingToRaw2", true );

    memset ( dest, 0, sizeof ( wThing2 ) * max );
    for ( int i = 0; i < max; i++ ) {
        dest [i].xPos     = src [i].xPos;
        dest [i].yPos     = src [i].yPos;
        dest [i].angle    = src [i].angle;
        dest [i].type     = src [i].type;
        dest [i].attr     = src [i].attr;
        dest [i].tid      = src [i].tid;
        dest [i].altitude = src [i].altitude;
        dest [i].special  = src [i].special;
        memcpy ( dest [i].arg, src [i].arg, sizeof ( dest[i].arg ));
    }
}

void DoomLevel::ConvertRaw1ToLineDef ( int max, wLineDef1 *src, wLineDef *dest )
{
    FUNCTION_ENTRY ( NULL, "DoomLevel::ConvertRaw1ToLineDef", true );

    memset ( dest, 0, sizeof ( wLineDef ) * max );
    for ( int i = 0; i < max; i++ ) {
        memcpy ( &dest [i], &src [i], sizeof ( wLineDef1 ));
    }
}

void DoomLevel::ConvertRaw2ToLineDef ( int max, wLineDef2 *src, wLineDef *dest )
{
    FUNCTION_ENTRY ( NULL, "DoomLevel::ConvertRaw2toLineDef", true );

    memset ( dest, 0, sizeof ( wLineDef ) * max );
    for ( int i = 0; i < max; i++ ) {
        dest [i].start      = src [i].start;
        dest [i].end        = src [i].end;
        dest [i].flags      = src [i].flags;
        dest [i].type       = 0;
        dest [i].tag        = 0;
        dest [i].sideDef[0] = src [i].sideDef[0];
        dest [i].sideDef[1] = src [i].sideDef[1];
        dest [i].special    = src [i].special;
        memcpy ( dest [i].arg, src [i].arg, sizeof ( src[i].arg ));
    }
}

void DoomLevel::ConvertLineDefToRaw1 ( int max, wLineDef *src, wLineDef1 *dest )
{
    FUNCTION_ENTRY ( NULL, "DoomLevel::ConvertLineDefToRaw1", true );

    memset ( dest, 0, sizeof ( wLineDef1 ) * max );
    for ( int i = 0; i < max; i++ ) {
        memcpy ( &dest [i], &src [i], sizeof ( wLineDef1 ));
    }
}

void DoomLevel::ConvertLineDefToRaw2 ( int max, wLineDef *src, wLineDef2 *dest )
{
    FUNCTION_ENTRY ( NULL, "DoomLevel::ConvertLineDefToRaw2", true );

    memset ( dest, 0, sizeof ( wLineDef2 ) * max );
    for ( int i = 0; i < max; i++ ) {
        dest [i].start      = src [i].start;
        dest [i].end        = src [i].end;
        dest [i].flags      = src [i].flags;
        dest [i].sideDef[0] = src [i].sideDef[0];
        dest [i].sideDef[1] = src [i].sideDef[1];
        dest [i].special    = src [i].special;
        memcpy ( dest [i].arg, src [i].arg, sizeof ( dest[i].arg ));
    }
}

bool DoomLevel::LoadThings ( bool hexenFormat )
{
    FUNCTION_ENTRY ( this, "DoomLevel::LoadThings", true );

    int size  = 0;
    int count = 0;

    delete [] m_ThingData;

    if ( hexenFormat == false ) {
        size        = sizeof ( wThing1 );
        count       = m_Thing.dataSize / size;
        m_ThingData = new wThing [ count ];
        ConvertRaw1ToThing ( count, ( wThing1 * ) m_Thing.rawData, m_ThingData );
    } else {
        size        = sizeof ( wThing2 );
        count       = m_Thing.dataSize / size;
        m_ThingData = new wThing [ count ];
        ConvertRaw2ToThing ( count, ( wThing2 * ) m_Thing.rawData, m_ThingData );
    }

    m_Thing.byteOrder    = LITTLE_ENDIAN;
    m_Thing.elementCount = count;
    m_Thing.elementSize  = size;

    return (( int ) m_Thing.dataSize == size * count ) ? true : false;
}

bool DoomLevel::LoadLineDefs ( bool hexenFormat )
{
    FUNCTION_ENTRY ( this, "DoomLevel::LoadLineDefs", true );

    int size  = 0;
    int count = 0;

    delete [] m_LineDefData;

    if ( hexenFormat == false ) {
        size        = sizeof ( wLineDef1 );
        count       = m_LineDef.dataSize / size;
        m_LineDefData = new wLineDef [ count ];
        ConvertRaw1ToLineDef ( count, ( wLineDef1 * ) m_LineDef.rawData, m_LineDefData );
    } else {
        size        = sizeof ( wLineDef2 );
        count       = m_LineDef.dataSize / size;
        m_LineDefData = new wLineDef [ count ];
        ConvertRaw2ToLineDef ( count, ( wLineDef2 * ) m_LineDef.rawData, m_LineDefData );
    }

    m_LineDef.byteOrder    = LITTLE_ENDIAN;
    m_LineDef.elementCount = count;
    m_LineDef.elementSize  = size;

    return (( int ) m_LineDef.dataSize == size * count ) ? true : false;
}

bool DoomLevel::ReadEntry ( sLevelLump *entry, const char *name, const wadDirEntry *start, const wadDirEntry *end, bool required )
{
    FUNCTION_ENTRY ( this, "DoomLevel::ReadEntry", true );

    const wadDirEntry *dir = m_Wad->FindDir ( name, start, end );

    if ( dir == NULL ) return ( required == true ) ? false : true;

    entry->rawData      = m_Wad->ReadEntry ( dir, &entry->dataSize );
    entry->elementCount = entry->dataSize / entry->elementSize;
    entry->byteOrder    = LITTLE_ENDIAN;

    return true;
}

void DoomLevel::DetermineType ()
{
    FUNCTION_ENTRY ( this, "DoomLevel::DetermineType", true );

    m_IsHexen = false;

    // Look for the easy things first
    if ( m_Behavior.rawData != NULL ) {
        m_IsHexen = true;
        return;
    }

    // See if we have an exact match in structure sizes in only 1 of the two formats
    int isType1 = ( m_Thing.dataSize % sizeof ( wThing1 )) + ( m_LineDef.dataSize % sizeof ( wLineDef1 ));
    int isType2 = ( m_Thing.dataSize % sizeof ( wThing2 )) + ( m_LineDef.dataSize % sizeof ( wLineDef2 ));

    if ( isType1 != isType2 ) {
        if ( isType1 == 0 ) return;
        if ( isType2 == 0 ) {
            m_IsHexen = true;
            return;
        }
    }

    // See if we have a valid level in only 1 of the two formats
    LoadThings ( false );
    LoadLineDefs ( false );

    // Make sure we have the correct byte order
    AdjustByteOrder ( BYTE_ORDER );

    bool isValid1 = IsValid ( false, false );

    LoadThings ( true );
    LoadLineDefs ( true );

    // Make sure we have the correct byte order
    AdjustByteOrder ( BYTE_ORDER );

    bool isValid2 = IsValid ( false, false );

    if ( isValid1 != isValid2 ) {
        if ( isValid1 == true ) return;
        if ( isValid2 == true ) {
            m_IsHexen = true;
            return;
        }
    }

    // Pick the one with the fewest invalid things found?
    fprintf ( stderr, "Argh!!!\n" );
}

bool DoomLevel::Load ()
{
    FUNCTION_ENTRY ( this, "DoomLevel::Load", true );

    if ( m_Wad == NULL ) return false;

    const wadDirEntry *start = m_Wad->FindDir ( Name ());

    if ( start == NULL ) return false;

    const wadDirEntry *end = start + min ( 11, ( int ) ( m_Wad->DirSize () - 1 ));

    bool valid = true;

    valid &= ReadEntry ( &m_Map,       Name (),    start, end, true );
    valid &= ReadEntry ( &m_Thing,     "THINGS",   start, end, true );
    valid &= ReadEntry ( &m_LineDef,   "LINEDEFS", start, end, true );
    valid &= ReadEntry ( &m_SideDef,   "SIDEDEFS", start, end, true );
    valid &= ReadEntry ( &m_Vertex,    "VERTEXES", start, end, true );
    valid &= ReadEntry ( &m_Sector,    "SECTORS",  start, end, true );
    valid &= ReadEntry ( &m_Segs,      "SEGS",     start, end, false );
    valid &= ReadEntry ( &m_SubSector, "SSECTORS", start, end, false );
    valid &= ReadEntry ( &m_Node,      "NODES",    start, end, false );
    valid &= ReadEntry ( &m_Reject,    "REJECT",   start, end, false );
    valid &= ReadEntry ( &m_BlockMap,  "BLOCKMAP", start, end, false );
    valid &= ReadEntry ( &m_Behavior,  "BEHAVIOR", start, end, false );

    if ( RejectSize () != 0 ) {
        int mask = ( 0xFF >> ( RejectSize () * 8 - SectorCount () * SectorCount ())) & 0xFF;
        (( UINT8 * ) m_Reject.rawData ) [ RejectSize () - 1 ] &= ( UINT8 ) mask;
    }

    DetermineType ();

    LoadThings ( m_IsHexen );
    LoadLineDefs ( m_IsHexen );

    // Switch to native byte ordering
    AdjustByteOrder ( BYTE_ORDER );

    return valid;
}

bool DoomLevel::LoadHexenInfo ()
{
    FUNCTION_ENTRY ( this, "DoomLevel::LoadHexenInfo", true );

    if ( m_Wad == NULL ) return false;

    const wadDirEntry *dir = m_Wad->FindDir ( "MAPINFO" );

    if ( dir == NULL ) return false;

    int level;
    sscanf ( m_Name, "MAP%02d", &level );

    UINT32 Size;
    char *buffer = ( char * ) m_Wad->ReadEntry ( dir, &Size, true );
    char *ptr = buffer;

    if ( m_Title != NULL ) free (( char * ) m_Title );
    m_Title = NULL;

    do {
        if (( ptr = strstr ( ptr, "\nmap " )) == NULL ) break;
        if ( atoi ( &ptr[5] ) == level ) {
            while ( *ptr++ != '"' );
            strtok ( ptr, "\"" );
            m_Title = strdup ( ptr );
            ptr += strlen ( ptr ) + 1;
            char *clstr = strstr ( ptr, "\ncluster " );
            char *next = strstr ( ptr, "\n\r" );
            if ( clstr && ( clstr < next )) {
                m_Cluster = atoi ( clstr + 9 );
            }
            break;
        } else {
            ptr++;
        }
    } while ( ptr && *ptr );

    delete [] buffer;

    if ( m_Title != NULL ) {
        ptr = ( char * ) m_Title + 1;
        while ( *ptr ) {
            *ptr = ( char ) tolower ( *ptr );
            if ( *ptr == ' ' ) {
                while ( *ptr == ' ' ) ptr++;
                if ( strncmp ( ptr, "OF ", 3 ) == 0 ) ptr--;
            }
            if ( *ptr ) ptr++;
        }
    }

    dir = m_Wad->FindDir ( "SNDINFO" );
    if ( dir == NULL ) return true;

    buffer = ( char * ) m_Wad->ReadEntry ( dir, &Size, true );
    ptr = buffer;

    do {
        if (( ptr = strstr ( ptr, "\n$MAP" )) == NULL ) break;
        if ( atoi ( &ptr[5] ) == level ) {
            ptr += 25;
            strtok ( ptr, "\r" );
            m_Music = strupr ( strdup ( ptr ));
            break;
        } else {
            ptr++;
        }
    } while ( ptr && *ptr );

    delete [] buffer;

    return true;
}

void DoomLevel::AddToWAD ( WAD *m_Wad )
{
    FUNCTION_ENTRY ( this, "DoomLevel::AddToWAD", true );

    // Make sure data is in little endian when writing to the file
    AdjustByteOrder ( LITTLE_ENDIAN );

    StoreThings ();
    StoreLineDefs ();

    m_Wad->InsertAfter (( const wLumpName * ) Name (),    m_Map.dataSize,       m_Map.rawData,       false );
    m_Wad->InsertAfter (( const wLumpName * ) "THINGS",   m_Thing.dataSize,     m_Thing.rawData,     false );
    m_Wad->InsertAfter (( const wLumpName * ) "LINEDEFS", m_LineDef.dataSize,   m_LineDef.rawData,   false );
    m_Wad->InsertAfter (( const wLumpName * ) "SIDEDEFS", m_SideDef.dataSize,   m_SideDef.rawData,   false );
    m_Wad->InsertAfter (( const wLumpName * ) "VERTEXES", m_Vertex.dataSize,    m_Vertex.rawData,    false );
    m_Wad->InsertAfter (( const wLumpName * ) "SEGS",     m_Segs.dataSize,      m_Segs.rawData,      false );
    m_Wad->InsertAfter (( const wLumpName * ) "SSECTORS", m_SubSector.dataSize, m_SubSector.rawData, false );
    m_Wad->InsertAfter (( const wLumpName * ) "NODES",    m_Node.dataSize,      m_Node.rawData,      false );
    m_Wad->InsertAfter (( const wLumpName * ) "SECTORS",  m_Sector.dataSize,    m_Sector.rawData,    false );
    m_Wad->InsertAfter (( const wLumpName * ) "REJECT",   m_Reject.dataSize,    m_Reject.rawData,    false );
    m_Wad->InsertAfter (( const wLumpName * ) "BLOCKMAP", m_BlockMap.dataSize,  m_BlockMap.rawData,  false );

    if (( m_IsHexen == true ) && ( m_Behavior.rawData != NULL )) {
        m_Wad->InsertAfter (( const wLumpName * ) "BEHAVIOR", m_Behavior.dataSize,  m_Behavior.rawData,  false );
    }

    // Switch back to native byte ordering
    AdjustByteOrder ( BYTE_ORDER );
}

void DoomLevel::StoreThings ()
{
    FUNCTION_ENTRY ( this, "DoomLevel::StoreThings", true );

    AdjustByteOrderThing ( LITTLE_ENDIAN );

    if (( int ) m_Thing.dataSize < ThingCount () * m_Thing.elementSize ) {
        delete [] ( char * ) m_Thing.rawData;
        m_Thing.rawData = new char [ ThingCount () * m_Thing.elementSize ];
    }

    if ( m_IsHexen == false ) {
        ConvertThingToRaw1 ( ThingCount (), m_ThingData, ( wThing1 * ) m_Thing.rawData );
    } else {
        ConvertThingToRaw2 ( ThingCount (), m_ThingData, ( wThing2 * ) m_Thing.rawData );
    }

    m_LineDef.dataSize = ThingCount () * m_LineDef.elementSize;
}

void DoomLevel::StoreLineDefs ()
{
    FUNCTION_ENTRY ( this, "DoomLevel::StoreLineDefs", true );

    AdjustByteOrderLineDef ( LITTLE_ENDIAN );

    if (( int ) m_LineDef.dataSize < LineDefCount () * m_LineDef.elementSize ) {
        delete [] ( char * ) m_LineDef.rawData;
        m_LineDef.rawData = new char [ LineDefCount () * m_LineDef.elementSize ];
    }

    if ( m_IsHexen == false ) {
        ConvertLineDefToRaw1 ( LineDefCount (), m_LineDefData, ( wLineDef1 * ) m_LineDef.rawData );
    } else {
        ConvertLineDefToRaw2 ( LineDefCount (), m_LineDefData, ( wLineDef2 * ) m_LineDef.rawData );
    }

    m_LineDef.dataSize = LineDefCount () * m_LineDef.elementSize;
}

bool DoomLevel::UpdateEntry ( sLevelLump *lump, const char *name, const char *follows, bool required )
{
    FUNCTION_ENTRY ( this, "DoomLevel::UpdateEntry", true );

    if ( lump->changed == false ) return false;

    lump->changed = false;

    const wadDirEntry *start = m_Wad->FindDir ( Name ());
    const wadDirEntry *end   = start + min ( 10, ( int ) ( m_Wad->DirSize () - 1 ));

    bool changed = false;

    const wadDirEntry *dir = m_Wad->FindDir ( name, start, end );
    if ( dir == NULL ) {
        if ( required == true ) {
            fprintf ( stderr, "Map %s is missing required lump: %s\n", Name (), name );
        } else {
            const wadDirEntry *last = m_Wad->FindDir ( follows, start, end );
            changed |= m_Wad->InsertAfter (( const wLumpName * ) name, lump->dataSize, lump->rawData, false, last );
        }
    } else {
        changed |= m_Wad->WriteEntry ( dir, lump->dataSize, lump->rawData, false );
    }

    return changed;
}

bool DoomLevel::UpdateWAD ()
{
    FUNCTION_ENTRY ( this, "DoomLevel::UpdateWAD", true );

    if ( m_Wad == NULL ) return false;
    if ( m_Wad->FindDir ( Name ()) == NULL ) return false;
    if ( IsDirty () == false ) return false;

    // Make sure data is in little endian when writing to the file
    AdjustByteOrder ( LITTLE_ENDIAN );

    StoreThings ();
    StoreLineDefs ();

    bool changed = false;

    changed |= UpdateEntry ( &m_Thing,       "THINGS",        NULL,  true );
    changed |= UpdateEntry ( &m_LineDef,   "LINEDEFS",        NULL,  true );
    changed |= UpdateEntry ( &m_SideDef,   "SIDEDEFS",        NULL,  true );
    changed |= UpdateEntry ( &m_Vertex,    "VERTEXES",        NULL,  true );
    changed |= UpdateEntry ( &m_Segs,          "SEGS",  "VERTEXES", false );
    changed |= UpdateEntry ( &m_SubSector, "SSECTORS",      "SEGS", false );
    changed |= UpdateEntry ( &m_Node,         "NODES",  "SSECTORS", false );
    changed |= UpdateEntry ( &m_Sector,     "SECTORS",     "NODES", false );
    changed |= UpdateEntry ( &m_Reject,      "REJECT",   "SECTORS", false );
    changed |= UpdateEntry ( &m_BlockMap,  "BLOCKMAP",    "REJECT", false );

    if (( m_IsHexen == true ) && ( m_Behavior.rawData != NULL )) {
        changed |= UpdateEntry ( &m_Behavior,  "BEHAVIOR", "BLOCKMAP", false );
    }

    // Switch back to native byte ordering
    AdjustByteOrder ( BYTE_ORDER );

    return changed;
}

void DoomLevel::NewEntry ( sLevelLump *entry, int newCount, void *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewEntry", true );

    delete [] ( char * ) entry->rawData;

    entry->byteOrder    = BYTE_ORDER;
    entry->changed      = true;
    entry->elementCount = newCount;
    entry->dataSize     = newCount * entry->elementSize;
    entry->rawData      = newData;
}

void DoomLevel::NewThings ( int newCount, wThing *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewThings", true );

    delete [] ( char * ) m_ThingData;

    m_ThingData = newData;

    m_Thing.byteOrder    = BYTE_ORDER;
    m_Thing.changed      = true;
    m_Thing.elementCount = newCount;
}

void DoomLevel::NewLineDefs ( int newCount, wLineDef *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewLineDefs", true );

    delete [] ( char * ) m_LineDefData;

    m_LineDefData = newData;

    m_LineDef.byteOrder    = BYTE_ORDER;
    m_LineDef.changed      = true;
    m_LineDef.elementCount = newCount;
}

void DoomLevel::NewSideDefs ( int newCount, wSideDef *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewSideDefs", true );

    NewEntry ( &m_SideDef, newCount, newData );
}

void DoomLevel::NewVertices ( int newCount, wVertex *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewVertices", true );

    NewEntry ( &m_Vertex, newCount, newData );
}

void DoomLevel::NewSectors ( int newCount, wSector *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewSectors", true );

    NewEntry ( &m_Sector, newCount, newData );
}

void DoomLevel::NewSegs ( int newCount, wSegs *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewSegs", true );

    NewEntry ( &m_Segs, newCount, newData );
}

void DoomLevel::NewSubSectors ( int newCount, wSSector *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewSubSectors", true );

    NewEntry ( &m_SubSector, newCount, newData );
}

void DoomLevel::NewNodes ( int newCount, wNode *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewNodes", true );

    NewEntry ( &m_Node, newCount, newData );
}

void DoomLevel::NewReject ( int newSize, UINT8 *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewReject", true );

    int mask = ( 0xFF >> ( newSize * 8 - SectorCount () * SectorCount ())) & 0xFF;

    newData [ newSize - 1 ] &= ( UINT8 ) mask;

    NewEntry ( &m_Reject, newSize, newData );
}

void DoomLevel::NewBlockMap ( int newSize, wBlockMap *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewBlockMap", true );

    NewEntry ( &m_BlockMap, newSize, newData );
}

void DoomLevel::NewBehavior ( int newSize, char *newData )
{
    FUNCTION_ENTRY ( this, "DoomLevel::NewBehavior", true );

    NewEntry ( &m_Behavior, newSize, newData );
}
