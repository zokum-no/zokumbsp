//----------------------------------------------------------------------------
//
// File:        wad.hpp
// Date:        26-Oct-1994
// Programmer:  Marc Rousseau
//
// Description: Object classes for manipulating Doom WAD files
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
//   04-25-01	Added little/big endian conversions
//
//----------------------------------------------------------------------------

#pragma once
#include "wad.hpp"

#include <string>
#include <cctype>   // for std::isdigit, std::isspace
#include <cstring>  // for std::strncmp if needed
#include <vector>
#include <memory>
#include <cstdint>  // for fixed-width integer types

#ifdef _WIN32
// Windows-specific includes and defines
#include <windows.h>
#else
#include <unistd.h>
#endif


#ifndef WAD_HPP_
#define WAD_HPP_

#if ! defined ( COMMON_HPP_ )
    #include "common.hpp"
#endif

#if ! defined ( __STDIO_H )
    #include <stdio.h>
#endif

#if ( BYTE_ORDER == LITTLE_ENDIAN )
    const std::uint32_t IWAD_ID		= 0x44415749;	// ASCII - 'IWAD'
    const std::uint32_t PWAD_ID		= 0x44415750;	// ASCII - 'PWAD'
#else
    const std::uint32_t IWAD_ID		= 0x49574144;	// ASCII - 'IWAD'
    const std::uint32_t PWAD_ID		= 0x50574144;	// ASCII - 'PWAD'

    extern std::uint32_t swap_uint32 ( const unsigned char *ptr );
    extern std::uint16_t swap_uint16 ( const unsigned char *ptr );
#endif

#define IS_TYPE(x,y)	(( * ( std::uint32_t * ) ( x )) == (( std::uint32_t ) y ))

#define MAX_LUMP_NAME	8

enum eLumpType {
    wl_UNCHECKED,
    wl_UNKNOWN,
    wl_PALETTE,
    wl_COLORMAP,
    wl_DEMO,
    wl_TEXTURE_LIST,
    wl_PATCH_NAMES,
    wl_MIDI_MAPPING,
    wl_GRAVIS_PATCH,
    wl_MAP_NAME,
    wl_MAP_DATA,
    wl_PC_SPEAKER,
    wl_SOUND_EFFECT,
    wl_MUSIC,
    wl_FLAT,
    wl_PATCH,
    wl_SPRITE,
    wl_GRAPHIC,
    wl_SCREEN_SHOT,
    wl_TEXT_SCREEN,
    wl_SOUND_INFO,
    wl_SCRIPT,
    wl_SPECIAL
};

enum eWadType {
    wt_UNKNOWN,
    wt_DOOM,
    wt_DOOM2,
    wt_HERETIC,
    wt_HEXEN,
    wt_STRIFE,
    wt_HACX,
	wt_CHEXQUEST,
};

enum eWadStyle {
    wst_UNKNOWN,
    wst_FORMAT_1,		// DOOM / Heretic
    wst_FORMAT_2,		// DOOM ][
    wst_FORMAT_3		// Hexen
};

enum eWadStatus {
    ws_UNKNOWN,
    ws_OK,
    ws_CANT_READ,
    ws_CANT_WRITE,
    ws_INVALID_WAD,
    ws_INVALID_FILE,
    ws_SEEK_ERROR,
    ws_READ_ERROR,
    ws_WRITE_ERROR
};

struct wadHeader {
    char    type [4];
    std::uint32_t  dirSize;			// number of Lumps in WAD
    std::uint32_t  dirStart;			// offset to start of directory
};

typedef char wLumpName [ MAX_LUMP_NAME ];

struct wadDirEntry {
    std::uint32_t     offset;			// offset to start of data
    std::uint32_t     size;			// byte size of data
    wLumpName name;			// name of data block
};

struct wadDirInfo {
    std::uint8_t *cacheData;
    std::uint8_t *newData;
    eLumpType type;
};

class wadList;

class wadFilter {
public:
    virtual const char *getFileSpec () const = 0;
    virtual bool isRecognized ( std::uint32_t, void * ) const = 0;
    virtual bool isRecognized ( const char * ) const = 0;
    virtual bool readData ( FILE *, std::uint32_t *, void ** ) const = 0;
    virtual bool writeData ( FILE *, std::uint32_t, void * ) const = 0;
};

class WAD {

    char               *m_Name;
    FILE               *m_File;
    wadList            *m_List;

    bool                m_bValid;
    bool                m_bRegistered;
    bool                m_bDirChanged;		// wadDirEntry added/deleted

    wadHeader           m_Header;
    wadDirEntry        *m_Directory;
    wadDirInfo         *m_DirInfo;
    eWadStatus          m_Status;
    eWadType            m_Type;
    eWadStyle           m_Style;

    const wadDirEntry  *m_MapStart;
    const wadDirEntry  *m_MapEnd;
    const wadDirEntry  *m_SpriteStart;
    const wadDirEntry  *m_SpriteEnd;
    const wadDirEntry  *m_PatchStart;
    const wadDirEntry  *m_PatchEnd;
    const wadDirEntry  *m_FlatStart;
    const wadDirEntry  *m_FlatEnd;

    void              **m_NewData;

    static int          sm_NoFilters;
    static wadFilter  **sm_Filter;

    bool EnlargeDirectory ( int holePos, int entries );
    bool ReduceDirectory ( int holePos, int entries );

    void FindMarkers ();

    bool ReadHeader ( wadHeader * );
    bool WriteHeader ( FILE *, wadHeader * );

    bool ReadDirEntry ( wadDirEntry * );
    bool WriteDirEntry ( FILE *, wadDirEntry * );

    bool ReadDirectory ();
    bool WriteDirectory ( FILE * );

    std::uint32_t IndexOf ( const wadDirEntry * ) const;

public:

    WAD ( const char * );
   ~WAD ();

    void SetList ( wadList * );

    // static bool IsMap ( const char * );
    static bool IsMap(const std::string& name);
    static bool AddFilter ( wadFilter * );

    // Called by wadList::Save
    void OpenFile ();
    void CloseFile ();

    bool SaveFile ( const char * = NULL );

    void Type ( eWadType );
    void Style ( eWadStyle );
    void Format ( std::uint32_t );

    const char *Name () const;
    eWadStatus  Status () const;
    eWadType    Type () const;
    eWadStyle   Style () const;
    std::uint32_t      Format () const;
    std::uint32_t      FileSize () const;

    bool     IsValid () const;
    bool     IsRegistered () const;
    bool     HasChanged () const;
    bool     HasChanged ( const wadDirEntry * ) const;

    eLumpType   GetLumpType ( const wadDirEntry * );
    const char *GetLumpDescription ( eLumpType ) const;
    const char *GetLumpDescription ( const wadDirEntry * );

    void Seek ( std::uint32_t );
    void ReadBytes ( void *, std::uint32_t, std::uint32_t = 1 );
    void WriteBytes ( void *, std::uint32_t, std::uint32_t = 1 );
    void CopyBytes ( WAD *, std::uint32_t, std::uint32_t = 1 );

    std::uint32_t DirSize () const;

    const wadDirEntry *GetDir ( std::uint32_t ) const;
    const wadDirEntry *FindDir ( const char *, const wadDirEntry * = NULL, const wadDirEntry * = NULL ) const;

    void *ReadEntry ( const char *, std::uint32_t *, const wadDirEntry * = NULL, const wadDirEntry * = NULL, bool = false );
    void *ReadEntry ( const wadDirEntry *, std::uint32_t *, bool = false );
    bool WriteEntry ( const char *, std::uint32_t, void *, bool, const wadDirEntry * = NULL, const wadDirEntry * = NULL );
    bool WriteEntry ( const wadDirEntry *, std::uint32_t, void *, bool );

    bool Extract ( const wLumpName *, const char * = NULL ) const;

    bool InsertBefore ( const wLumpName *, const char *, bool, const wadDirEntry * = NULL );
    bool InsertAfter ( const wLumpName *, const char *, bool, const wadDirEntry * = NULL );

    bool InsertBefore ( const wLumpName *, std::uint32_t, void *, bool, const wadDirEntry * = NULL );
    bool InsertAfter ( const wLumpName *, std::uint32_t, void *, bool, const wadDirEntry * = NULL );

    bool Remove ( const wLumpName *, const wadDirEntry * = NULL, const wadDirEntry * = NULL  );
};

inline void WAD::Format ( std::uint32_t newFormat )    { * ( std::uint32_t * ) m_Header.type = newFormat; }
inline void WAD::Type ( eWadType newType )      { m_Type = newType; }
inline void WAD::Style ( eWadStyle newStyle )   { m_Style = newStyle; }

inline const char *WAD::Name () const           { return m_Name; }
inline std::uint32_t      WAD::DirSize () const        { return m_Header.dirSize; }
inline std::uint32_t      WAD::Format () const 		{ return * ( std::uint32_t * ) m_Header.type; }
inline eWadStatus  WAD::Status () const         { return m_Status; }
inline eWadType    WAD::Type () const           { return m_Type; }
inline eWadStyle   WAD::Style () const          { return m_Style; }
inline bool        WAD::IsValid () const        { return m_bValid; }
inline bool        WAD::IsRegistered () const   { return m_bRegistered; }

struct wadListDirEntry {
    WAD               *wad;
    const wadDirEntry *entry;
};

class wadList {

    struct wadListEntry {
        WAD          *wad;
        wadListEntry *Next;
    };

    std::uint32_t           m_DirSize;
    std::uint32_t           m_MaxSize;
    wadListDirEntry *m_Directory;
    eWadType         m_Type;
    eWadStyle        m_Style;
    wadListEntry    *m_List;

    std::uint32_t  IndexOf ( const wadListDirEntry * ) const;

    int    AddLevel ( std::uint32_t, const wadDirEntry *&, WAD * );
    void   AddDirectory ( WAD *, bool = true );

    std::vector<std::unique_ptr<WAD>> m_Wads;


public:

    wadList ();
    ~wadList ();

    // bool Add ( WAD * );
    bool Add(std::unique_ptr<WAD> wad);
    bool Remove ( WAD * );
    void Clear ();
    void UpdateDirectory ();

    int wadCount () const;
    std::uint32_t FileSize () const;
    WAD *GetWAD ( int ) const;
    eWadType Type () const;
    eWadStyle Style () const;

    bool Save ( const char * = NULL );
    bool Extract ( const wLumpName *, const char *file = NULL );

    std::uint32_t DirSize () const;

    const wadListDirEntry *GetDir ( std::uint32_t ) const;
    const wadListDirEntry *FindWAD ( const char *, const wadListDirEntry * = NULL, const wadListDirEntry * = NULL ) const;

    bool Contains ( WAD * ) const;
    bool IsEmpty () const;
    bool HasChanged () const;
};

inline std::uint32_t    wadList::DirSize () const	{ return m_DirSize; }
// inline bool      wadList::IsEmpty () const	{ return ( m_List == NULL ) ? true : false; }
inline bool wadList::IsEmpty() const {
    return m_Wads.empty();
}
inline eWadType  wadList::Type () const		{ return m_Type; }
inline eWadStyle wadList::Style () const	{ return m_Style; }

#endif

/*
#if defined ( __GNUC__ ) || defined ( __INTEL_COMPILER )
void _fullpath ( char *full, const char *name, int max ) {
	strncpy ( full, name, max );
}
#endif
*/



