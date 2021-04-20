// Heaps
//-----------------------------------------------------------------------------------------------------------------------
#ifdef _DEBUG
   #define DEBUG_CLIENTBLOCK   new( _CLIENT_BLOCK, __FILE__, __LINE__)
    #define _CRTDBG_MAP_ALLOC
#endif // _DEBUG
//-----------------------------------------------------------------------------------------------------------------------
#include "stdafx.h"
#include "Heap.h"

CMyHeap::HEAPBLOCK* CMyHeap::CreateHeap( const DWORD size )
{
    //---------------------------------------
    HEAPBLOCK* tmp = static_cast< HEAPBLOCK* >( malloc( sizeof( HEAPBLOCK ) ) );
    if ( tmp == nullptr )
    {
        return nullptr;
    }
    //---------------------------------------
    tmp->current = 0;
    tmp->size = size;
    tmp->next = nullptr;
    tmp->data = static_cast< PUCHAR >( malloc( size ) );
    //---------------------------------------
    if ( tmp->data != nullptr )
    {
        //currentBlock = tmp;
        tmp->end = tmp;
        return tmp;
    }
    //---------------------------------------
    free( tmp );
    return nullptr;
}

bool CMyHeap::FreeHeap( HEAPBLOCK* block )
{
    if ( block != nullptr )
    {
        CMyHeap::FreeAllBlocks( block );
        return true;
    }
    return false;
}

wchar_t* CMyHeap::AllocAndCopyString( HEAPBLOCK* block , wchar_t* string , DWORD size )
{
    const DWORD rsize = ( size + 1 ) * sizeof( wchar_t );
    int asize = ( ( rsize ) & 0xfffffff8 ) + 8;

    if ( asize <= rsize )
    {
        ::DebugBreak();
    }
    HEAPBLOCK* back = nullptr;
    HEAPBLOCK* tmp = block->end;

    if ( tmp != nullptr )
    {
        int t = tmp->size - tmp->current;
        if ( t > asize )
        {
            goto copy;
        }
        back = tmp;

        tmp = tmp->next;
        if ( tmp != nullptr )
        {
            t = tmp->size - tmp->current;
            if ( t > asize )
            {
                block->end = tmp;
                goto copy;

            }
            back = tmp;
            tmp = tmp->next;
        }
    }
    //-----------------------
    //‘«‚è‚È‚­‚È‚Á‚½Žž
    //-----------------------
    tmp = ( HEAPBLOCK* )calloc( 1 , sizeof( HEAPBLOCK ) );
    if ( tmp == nullptr )
    {
        _ASSERT( tmp != nullptr );
        return nullptr;
    }
    tmp->data = ( PUCHAR )malloc( block->size );
    if ( tmp->data == nullptr )
    {
        _ASSERT( tmp->data != nullptr );
        free( tmp );
        return nullptr;
    }
    {
        tmp->size = block->size;
        tmp->next = nullptr;

        if ( back == nullptr )
        {
            back = block->end;
        }
        tmp->end = block;
        back->next = tmp;
        block->end = tmp;
    }
copy:
    PUCHAR ret = &tmp->data[ tmp->current ];
    memcpy( ret , string , rsize );
    ret[ rsize ] = 0;
    ret[ rsize + 1 ] = 0;
    tmp->current += asize;
    return reinterpret_cast< wchar_t* >( ret );

}

bool CMyHeap::FreeAllBlocks( HEAPBLOCK* block )
{
    HEAPBLOCK* tmp = block;

    while ( tmp != nullptr )
    {
        free( tmp->data );
        HEAPBLOCK* back = tmp;
        tmp = tmp->next;
        free( back );
    }

    return true;
}

bool CMyHeap::ReUseBlocks( HEAPBLOCK* block , const bool isClear )
{
    if ( block == nullptr )
    {
        return true;
    }
    //----------------------------
    HEAPBLOCK* tmp = block;
    while ( tmp != nullptr )
    {
        tmp->current = 0;
        if ( isClear == true )
        {
            memset( tmp->data , 0 , tmp->size );
        }
        tmp = tmp->next;
    }
    block->end = block;
    return true;
}
