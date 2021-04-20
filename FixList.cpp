//-----------------------------------------------------------------------------------------------------------------------
#ifdef _DEBUG
   #define DEBUG_CLIENTBLOCK   new( _CLIENT_BLOCK, __FILE__, __LINE__)
    #define _CRTDBG_MAP_ALLOC
#endif // _DEBUG
//-----------------------------------------------------------------------------------------------------------------------
#include "stdafx.h"
#include "FixList.h"

CFixList::LINKITEM* fixlist = nullptr;
CFixList::LINKITEM* curfix = nullptr;


void CFixList::AddToFixList( const int entry , const int data )
{
    curfix->entry = entry;
    curfix->data = data;

    curfix->next = ( LINKITEM* )malloc( sizeof( LINKITEM ) );
    curfix = curfix->next;
    curfix->next = nullptr;
}

void CFixList::CreateFixList()
{
    fixlist = ( LINKITEM* )calloc( 1 , sizeof( LINKITEM ) );
    fixlist->next = nullptr;
    curfix = fixlist;
}

void CFixList::ProcessFixList( DISKHANDLE* disk )
{
    while ( fixlist->next != nullptr )
    {
        SEARCHFILEINFO* info = &disk->fFiles[ fixlist->entry ];
        SEARCHFILEINFO* src = &disk->fFiles[ fixlist->data ];
        info->FileName = src->FileName;
        info->FileNameLength = src->FileNameLength;

        info->ParentId = src->ParentId;
        // hide all that we used for cleanup
        src->ParentId.QuadPart = 0;
        //-----------------------
        LINKITEM* item = fixlist;
        fixlist = fixlist->next;
        free( item );
    }
    free( fixlist );
    fixlist = nullptr;
    curfix = nullptr;
}

