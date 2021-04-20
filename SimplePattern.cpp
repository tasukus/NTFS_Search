//-----------------------------------------------------------------------------------------------------------------------
#ifdef _DEBUG
   #define DEBUG_CLIENTBLOCK   new( _CLIENT_BLOCK, __FILE__, __LINE__)
    #define _CRTDBG_MAP_ALLOC
#endif // _DEBUG
//-----------------------------------------------------------------------------------------------------------------------
// Code to support wildchars

#include "stdafx.h"
#include "SimplePattern.h"

int __cdecl wcsnrcmp( const wchar_t* first , const wchar_t* last , size_t count );

// Prepares the searchpattern struct
SEARCHP* StartSearch( wchar_t* string , int len )
{
    //--------------------------------
    if ( len <= 0 )
    {
        return nullptr;
    }
    //--------------------------------
    SEARCHP* ptr = ( SEARCHP* )calloc( 1 , sizeof( SEARCHP ) );
    if ( ptr == nullptr )
    {
        return nullptr;
    }
    ptr->mode = 0;
    //--------------------------------
    if ( string[ len - 1 ] == L'*' )
    {
        ptr->mode += 2;
        string[ len - 1 ] = 0;
        len--;
    }
    if ( string[ 0 ] == L'*' )
    {
        ptr->mode += 1;
        string++;
        len--;
    }

    ptr->string = string;
    ptr->len = len;
    ptr->totallen = ptr->len;
    if ( ptr->mode == 0 )
    {
        wchar_t* res = wcschr( string , L'*' );
        if ( res != nullptr )
        {
            ptr->mode = 42;
            *res = L'\0';
            ptr->len = res - string;
            ptr->extra = &res[ 1 ];
            ptr->extralen = len - ptr->len - 1;
            ptr->totallen = ptr->len + ptr->extralen;
        }
    }
    return ptr;
}

// does the actual search
bool SearchStr( const SEARCHP* pattern , const wchar_t* string , const int len )
{
    //--------------------------------
    //引数チェック
    if ( pattern == nullptr || string == nullptr )
    {
        _ASSERT( pattern != nullptr );
        _ASSERT( string != nullptr );
        return false;
    }
    //--------------------------------
    if ( pattern->totallen > len )
    {
        return false;
    }
    switch ( pattern->mode )
    {
        case 0:
            {
                if ( wcscmp( string , pattern->string ) == 0 )
                {
                    return true;
                }
            }
            break;
        case 1:
            {
                if ( wcsnrcmp( string + len , pattern->string + pattern->len , pattern->len + 1 ) == 0 )
                {
                    return true;
                }
            }
            break;
        case 2:
            {
                if ( wcsncmp( string , pattern->string , pattern->len ) == 0 )
                {
                    return true;
                }
            }
            break;
        case 3:
            {
                if ( wcsstr( string , pattern->string ) != nullptr )
                {
                    return true;
                }
            }
            break;
        case 42:
            if ( wcsnrcmp( string + len , pattern->extra + pattern->extralen , pattern->extralen + 1 ) == 0 )
            {
                if ( wcsncmp( string , pattern->string , pattern->len ) == 0 )
                {
                    return true;
                }
            }
            break;
    }
    return false;
}
#if 0
bool SearchStr( const SEARCHP* pattern , const std::wstring_view& string , const int len )
{
    //--------------------------------
    //引数チェック
    if ( pattern == nullptr || string == nullptr )
    {
        _ASSERT( pattern != nullptr );
        _ASSERT( string != nullptr );
        return false;
    }
    //--------------------------------
    if ( pattern->totallen > len )
    {
        return false;
    }
    switch ( pattern->mode )
    {
        case 0:
            if ( string.compare( pattern->string ) == 0 )
            {
                return true;
            }
            break;
        case 1:
            if ( std::wcsnrcmp( string + len , pattern->string + pattern->len , pattern->len + 1 ) == 0 )
            {
                return true;
            }
            break;
        case 2:
            if ( std::wcsncmp( string , pattern->string , pattern->len ) == 0 )
            {
                return true;
            }
            break;
        case 3:
            if ( std::wcsstr( string , pattern->string ) != nullptr )
            {
                return true;
            }
            break;
        case 42:
            if ( std::wcsnrcmp( string + len , pattern->extra + pattern->extralen , pattern->extralen + 1 ) == 0 )
            {
                if ( std::wcsncmp( string , pattern->string , pattern->len ) == 0 )
                {
                    return true;
                }
            }
            break;
    }
    return false;
}
#endif
// just the simple cleanup, because no copies were made
void EndSearch( SEARCHP* pattern )
{
    free( pattern );
    return;
}
//後ろから検索
int __cdecl wcsnrcmp( const wchar_t* first , const wchar_t* last , size_t count )
{
    if ( count == 0u )
    {
        return( 0 );
    }

    while ( ( --count != 0u ) && *first == *last )
    {
        first--;
        last--;
    }

    return ( ( *first - *last ) );
}