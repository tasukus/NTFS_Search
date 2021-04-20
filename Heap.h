#pragma once
//--------------------------------------------------------
class CMyHeap
{
public:
    struct HEAPBLOCK
    {
        PUCHAR data;
        DWORD current;
        DWORD size;
        HEAPBLOCK* end;
        HEAPBLOCK* next;
    };
    //--------------------------------------------------------
public:
    static HEAPBLOCK* CreateHeap( const DWORD size );
    static bool FreeHeap( HEAPBLOCK* block );

    static wchar_t* AllocAndCopyString( HEAPBLOCK* block , wchar_t* string , DWORD size );

    static bool FreeAllBlocks( HEAPBLOCK* block );

    static bool ReUseBlocks( HEAPBLOCK* block , const bool isClear );
};
//--------------------------------------------------------

