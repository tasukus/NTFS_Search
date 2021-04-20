//-----------------------------------------------------------------------------------------------------------------------
#ifdef _DEBUG
   #define DEBUG_CLIENTBLOCK   new( _CLIENT_BLOCK, __FILE__, __LINE__)
    #define _CRTDBG_MAP_ALLOC
#endif // _DEBUG
//-----------------------------------------------------------------------------------------------------------------------
#include "stdafx.h"
//------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
//------------------------------------------------------------
#include "FixList.h"

//------------------------------------------------------------
// HEAPHeap
CMyHeap::HEAPBLOCK* currentBlock = nullptr;

enum
{
    CLUSTERSPERREAD = 1024
};

ULONGLONG AttributeLength( const ATTRIBUTE* attr )
{
    return attr->Nonresident
        ? reinterpret_cast< const NONRESIDENT_ATTRIBUTE* const >( attr )->DataSize
        : reinterpret_cast< const RESIDENT_ATTRIBUTE* const >( attr )->ValueLength;
}

ULONGLONG AttributeLengthAllocated( const ATTRIBUTE* const attr )
{
    return attr->Nonresident ?
        reinterpret_cast< const NONRESIDENT_ATTRIBUTE* const >( attr )->AllocatedSize
        : reinterpret_cast< const RESIDENT_ATTRIBUTE* const >( attr )->ValueLength;
}

DISKHANDLE* CNtfsHelper::OpenDisk( const WCHAR DosDevice )
{
    WCHAR path[ 8 ] = { 0 };
    path[ 0 ] = L'\\';
    path[ 1 ] = L'\\';
    path[ 2 ] = L'.';
    path[ 3 ] = L'\\';
    path[ 4 ] = DosDevice;
    path[ 5 ] = L':';
    path[ 6 ] = L'\0';
    //---------------------
    DISKHANDLE* disk = OpenDisk( path );
    if ( disk != nullptr )
    {
        disk->DosDevice = DosDevice;
        return disk;
    }
    return nullptr;
}

DISKHANDLE* CNtfsHelper::OpenDisk( const wchar_t* disk )
{
    //------------------------------------------
    if ( disk == nullptr )
    {
        _ASSERT( disk != nullptr );
        return nullptr;
    }
    //------------------------------------------
    HANDLE hDrive = ::CreateFileW( disk , GENERIC_READ , FILE_SHARE_READ | FILE_SHARE_WRITE , nullptr , OPEN_EXISTING , 0 , nullptr );
    if ( hDrive == INVALID_HANDLE_VALUE )
    {
        return nullptr;
    }
    //------------------------------------------
    DISKHANDLE* tmpDisk = ( DISKHANDLE* )calloc( 1 , sizeof( DISKHANDLE ) );
    if ( tmpDisk == nullptr )
    {
        CloseHandle( hDrive );
        return nullptr;
    }
    //------------------------------------------
    tmpDisk->fileHandle = hDrive;
    //------------------------------------------
    DWORD read = 0;
    ::ReadFile( tmpDisk->fileHandle , &tmpDisk->NTFS.bootSector , sizeof( BOOT_BLOCK ) , &read , nullptr );
    if ( read == sizeof( BOOT_BLOCK ) )
    {
        if ( strncmp( "NTFS" , reinterpret_cast< const char* >( &tmpDisk->NTFS.bootSector.Format ) , 4 ) == 0 )
        {
            tmpDisk->type = NTFSDISK;
            tmpDisk->NTFS.BytesPerCluster = tmpDisk->NTFS.bootSector.BytesPerSector * tmpDisk->NTFS.bootSector.SectorsPerCluster;
            tmpDisk->NTFS.BytesPerFileRecord = tmpDisk->NTFS.bootSector.ClustersPerFileRecord < 0x80 ? tmpDisk->NTFS.bootSector.ClustersPerFileRecord * tmpDisk->NTFS.BytesPerCluster : 1 << ( 0x100 - tmpDisk->NTFS.bootSector.ClustersPerFileRecord );

            tmpDisk->NTFS.complete = FALSE;
            tmpDisk->NTFS.MFTLocation.QuadPart = tmpDisk->NTFS.bootSector.MftStartLcn * tmpDisk->NTFS.BytesPerCluster;
            tmpDisk->NTFS.MFT = nullptr;
            tmpDisk->heapBlock = nullptr;
            tmpDisk->IsLong = 0;
            tmpDisk->NTFS.sizeMFT = 0;
        }
        else
        {
            tmpDisk->type = UNKNOWN;
            tmpDisk->fFiles = nullptr;
        }
    }
    return tmpDisk;
}

bool CNtfsHelper::CloseDisk( DISKHANDLE* disk )
{
    if ( disk == nullptr )
    {
        _ASSERT( disk != nullptr );
        return false;
    }
    //-------------------------------
    if ( disk->fileHandle != INVALID_HANDLE_VALUE || disk->fileHandle == 0 )
    {
        CloseHandle( disk->fileHandle );
    }
    if ( disk->type == NTFSDISK )
    {
        SAFE_DELETE( disk->NTFS.MFT );
        SAFE_DELETE( disk->NTFS.Bitmap );
    }
    if ( disk->heapBlock != nullptr )
    {
        CMyHeap::FreeHeap( disk->heapBlock );
        disk->heapBlock = nullptr;
    }

    free( disk->fFiles );

    free( disk );
    return true;
};


ULONGLONG CNtfsHelper::LoadMFT( DISKHANDLE* disk , BOOL complete )
{
    if ( disk == nullptr )
    {
        return 0;
    }
    //-------------------------------
    if ( disk->type != NTFSDISK )
    {
        return 0;
    }
    //-------------------------------
    {
        ULARGE_INTEGER offset = disk->NTFS.MFTLocation;

        ::SetFilePointer( disk->fileHandle , offset.LowPart , reinterpret_cast< PLONG >( &offset.HighPart ) , FILE_BEGIN );
        UCHAR* buf = new UCHAR[ disk->NTFS.BytesPerCluster ];
        //----------------------------
        DWORD read;
        ::ReadFile( disk->fileHandle , buf , disk->NTFS.BytesPerCluster , &read , nullptr );

        FILE_RECORD_HEADER* file = reinterpret_cast< FILE_RECORD_HEADER* >( buf );
        FixFileRecord( file );
        //----------------------------

        NONRESIDENT_ATTRIBUTE* nattr = nullptr;
        if ( file->Ntfs.Type == 'ELIF' )
        {
            NONRESIDENT_ATTRIBUTE* nattr2 = nullptr;
            ATTRIBUTE* attr = reinterpret_cast< ATTRIBUTE* >( reinterpret_cast< PUCHAR >( file ) + file->AttributesOffset );
            const int stop = std::min( ( WORD )8 , file->NextAttributeNumber );

            for ( int i = 0; i < stop; i++ )
            {
                if ( attr->AttributeType < 0 || attr->AttributeType>0x100 )
                {
                    break;
                }

                switch ( attr->AttributeType )
                {
                    case Attribute_AttributeList:
                        return 3;
                        break;
                    case Attribute_Data:
                        nattr = reinterpret_cast< NONRESIDENT_ATTRIBUTE* >( attr );
                        break;
                    case Attribute_Bitmap:
                        nattr2 = reinterpret_cast< NONRESIDENT_ATTRIBUTE* >( attr );
                    default:
                        break;
                };

                if ( attr->Length > 0 && attr->Length < file->BytesInUse )
                {
                    attr = ( ATTRIBUTE* )( PUCHAR( attr ) + attr->Length );
                }
                else if ( attr->Nonresident == TRUE )
                {
                    attr = ( ATTRIBUTE* )( PUCHAR( attr ) + sizeof( NONRESIDENT_ATTRIBUTE ) );
                }
            }
            if ( nattr == nullptr )
            {
                return 0;
            }
            if ( nattr2 == nullptr )
            {
                return 0;
            }
        }
        disk->NTFS.sizeMFT = static_cast< DWORD >( nattr->DataSize );
        disk->NTFS.MFT = buf;
        disk->NTFS.entryCount = disk->NTFS.sizeMFT / disk->NTFS.BytesPerFileRecord;

        return nattr->DataSize;
    }
    return 0;
};

ATTRIBUTE* CNtfsHelper::FindAttribute( const FILE_RECORD_HEADER* file , const ATTRIBUTE_TYPE type )
{
    ATTRIBUTE* attr = reinterpret_cast< ATTRIBUTE* >( ( PUCHAR )( file )+file->AttributesOffset );

    for ( USHORT index = 1; index < file->NextAttributeNumber; index++ )
    {
        if ( attr->AttributeType == type )
        {
            return attr;
        }
        if ( attr->AttributeType < 1 || attr->AttributeType>0x100 )
        {
            break;
        }
        if ( attr->Length > 0 && attr->Length < file->BytesInUse )
        {
            attr = ( ATTRIBUTE* )( PUCHAR( attr ) + attr->Length );
            continue;
        }
        if ( attr->Nonresident == TRUE )
        {
            attr = ( ATTRIBUTE* )( PUCHAR( attr ) + sizeof( NONRESIDENT_ATTRIBUTE ) );
        }
    }
    return nullptr;
}

DWORD CNtfsHelper::ParseMFT( DISKHANDLE* disk , UINT option , STATUSINFO* info )
{
    if ( disk == nullptr )
    {
        return 0;
    }
    //-----------------------
    if ( disk->type == NTFSDISK )
    {

        CFixList::CreateFixList();

        FILE_RECORD_HEADER* fh = ( FILE_RECORD_HEADER* )( disk->NTFS.MFT );
        FixFileRecord( fh );

        disk->IsLong = 1;//sizeof(SEARCHFILEINFO);

        if ( disk->heapBlock == nullptr )
        {
            disk->heapBlock = CMyHeap::CreateHeap( 0x100000 );
        }
        NONRESIDENT_ATTRIBUTE* nattr = ( NONRESIDENT_ATTRIBUTE* )FindAttribute( fh , Attribute_Data );
        if ( nattr != nullptr )
        {
            UCHAR* buffer = ( UCHAR* )malloc( sizeof( UCHAR ) * CLUSTERSPERREAD * disk->NTFS.BytesPerCluster );
            ReadMFTParse( disk , nattr , 0 , ULONG( nattr->HighVcn ) + 1 , buffer , nullptr , info );
            free( buffer );
        }

        CFixList::ProcessFixList( disk );
    }

    return 0;
}

DWORD CNtfsHelper::ReadMFTParse( DISKHANDLE* disk , NONRESIDENT_ATTRIBUTE* attr , ULONGLONG vcn , ULONG count , PVOID buffer , FETCHPROC fetch , STATUSINFO* info )
{
    ULONG readcount = 0;
    DWORD ret = 0;
    auto bytes = ( PUCHAR )buffer;

    const int x = ( disk->NTFS.entryCount + 16 );//*sizeof(SEARCHFILEINFO);
    disk->fFiles = ( SEARCHFILEINFO* )calloc( x , sizeof( SEARCHFILEINFO ) );

    for ( ULONG left = count; left > 0; left -= readcount )
    {
        ULONGLONG lcn , runcount;
        FindRun( attr , vcn , &lcn , &runcount );
        readcount = ULONG( std::min( runcount , ( ULONGLONG )left ) );
        ULONG n = readcount * disk->NTFS.BytesPerCluster;
        if ( lcn == 0 )
        {
            // spares file?
            memset( bytes , 0 , n );
        }
        else
        {
            ret += ReadMFTLCN( disk , lcn , readcount , buffer , fetch , info );
        }
        vcn += readcount;
        bytes += n;
    }
    return ret;
}

ULONG CNtfsHelper::RunLength( const PUCHAR run )
{
    // i guess it must be this way
    return ( *run & 0xf ) + ( ( *run >> 4 ) & 0xf ) + 1;
}

LONGLONG CNtfsHelper::RunLCN( const PUCHAR run )
{
    const UCHAR n1 = *run & 0xf;
    const UCHAR n2 = ( *run >> 4 ) & 0xf;
    LONGLONG lcn = n2 == 0 ? 0 : CHAR( run[ n1 + n2 ] );

    for ( LONG i = n1 + n2 - 1; i > n1; i-- )
    {
        lcn = ( lcn << 8 ) + run[ i ];
    }
    return lcn;
}

ULONGLONG CNtfsHelper::RunCount( const PUCHAR run )
{
    // count the runs we have to process
    const UCHAR k = *run & 0xf;
    ULONGLONG count = 0;

    for ( ULONG index = k; index > 0; index-- )
    {
        count = ( count << 8 ) + run[ index ];
    }
    return count;
}

bool CNtfsHelper::FindRun( NONRESIDENT_ATTRIBUTE* attr , ULONGLONG vcn , PULONGLONG lcn , PULONGLONG count )
{
    if ( vcn < attr->LowVcn || vcn > attr->HighVcn )
    {
        return false;
    }
    *lcn = 0;

    ULONGLONG base = attr->LowVcn;

    for ( PUCHAR run = PUCHAR( PUCHAR( attr ) + attr->RunArrayOffset ); *run != 0; run += RunLength( run ) )
    {
        *lcn += RunLCN( run );
        *count = RunCount( run );
        if ( base <= vcn && vcn < base + *count )
        {
            *lcn = RunLCN( run ) == 0 ? 0 : *lcn + vcn - base;
            *count -= ULONG( vcn - base );
            return true;
        }
        base += *count;
    }

    return false;
}

DWORD CNtfsHelper::ReadMFTLCN( DISKHANDLE* disk , ULONGLONG lcn , ULONG count , PVOID buffer , FETCHPROC fetch , STATUSINFO* info )
{
    LARGE_INTEGER offset;

    offset.QuadPart = lcn * disk->NTFS.BytesPerCluster;
    ::SetFilePointer( disk->fileHandle , offset.LowPart , &offset.HighPart , FILE_BEGIN );

    const DWORD cnt = count / CLUSTERSPERREAD;

    DWORD read = 0;
    DWORD c = 0 , pos = 0;
    for ( DWORD index = 1; index <= cnt; index++ )
    {

        ::ReadFile( disk->fileHandle , buffer , CLUSTERSPERREAD * disk->NTFS.BytesPerCluster , &read , nullptr );
        c += CLUSTERSPERREAD;
        pos += read;

        ProcessBuffer( disk , static_cast< PUCHAR >( buffer ) , read , fetch );
        CallMe( info , disk->filesCount );

    }

    ::ReadFile( disk->fileHandle , buffer , ( count - c ) * disk->NTFS.BytesPerCluster , &read , nullptr );
    ProcessBuffer( disk , static_cast< PUCHAR >( buffer ) , read , fetch );
    CallMe( info , disk->filesCount );

    pos += read;
    return pos;
}

DWORD CNtfsHelper::ProcessBuffer( DISKHANDLE* disk , PUCHAR buffer , DWORD size , FETCHPROC fetch )
{
    const auto end = PUCHAR( buffer ) + size;
    SEARCHFILEINFO* data = disk->fFiles + disk->filesCount;

    while ( buffer < end )
    {
        FILE_RECORD_HEADER* fh = reinterpret_cast< FILE_RECORD_HEADER* >( buffer );
        FixFileRecord( fh );
        if ( FetchSearchInfo( disk , fh , data ) )
        {
            disk->realFiles++;
        }
        buffer += disk->NTFS.BytesPerFileRecord;
        ++data;
        disk->filesCount++;
    }
    return 0;
}

STRSAFE_LPWSTR CNtfsHelper::GetPath( DISKHANDLE* disk , int id )
{
    int a = id;
    DWORD PathStack[ 64 ] = { 0 };
    int PathStackPos = 0;

    for ( int i = 0; i < 64; i++ )
    {
        PathStack[ PathStackPos++ ] = a;
        DWORD pt = a * disk->IsLong;
        a = disk->fFiles[ pt ].ParentId.LowPart;

        if ( a == 0 || a == 5 )
        {
            break;
        }
    }
    static WCHAR glPath[ 0xffff ];
    int CurrentPos = 0;
    if ( disk->DosDevice != NULL )
    {
        glPath[ 0 ] = disk->DosDevice;
        glPath[ 1 ] = L':';
        CurrentPos = 2;
    }
    else
    {
        glPath[ 0 ] = L'\0';
    }
    for ( int index = PathStackPos - 1; index > 0; index-- )
    {
        const DWORD pt = PathStack[ index ] * disk->IsLong;
        glPath[ CurrentPos++ ] = L'\\';
        //--------------------------
        memcpy( &glPath[ CurrentPos ] , disk->fFiles[ pt ].FileName , disk->fFiles[ pt ].FileNameLength * 2 );
        CurrentPos += disk->fFiles[ pt ].FileNameLength;
    }
    glPath[ CurrentPos ] = L'\\';
    glPath[ CurrentPos + 1 ] = L'\0';
    return glPath;
}

STRSAFE_LPWSTR CNtfsHelper::GetCompletePath( DISKHANDLE* disk , const int id )
{
    int a = id;
    DWORD PathStack[ 64 ];
    int PathStackPos = 0;

    for ( int index = 0; index < 64; index++ )
    {
        PathStack[ PathStackPos++ ] = a;
        const DWORD pt = a * disk->IsLong;
        //--------------------------
        a = disk->fFiles[ pt ].ParentId.LowPart;

        if ( a == 0 || a == 5 )
        {
            break;
        }
    }
    static WCHAR glPath[ 0xffff ];
    int CurrentPos = 0;
    if ( disk->DosDevice != NULL )
    {
        glPath[ 0 ] = disk->DosDevice;
        glPath[ 1 ] = L':';
        CurrentPos = 2;
    }
    else
    {
        glPath[ 0 ] = L'\0';
    }
    for ( int index = PathStackPos - 1; index >= 0; index-- )
    {
        const DWORD pt = PathStack[ index ] * disk->IsLong;
        glPath[ CurrentPos++ ] = L'\\';
        //--------------------------
        memcpy( &glPath[ CurrentPos ] , disk->fFiles[ pt ].FileName , disk->fFiles[ pt ].FileNameLength * 2 );
        CurrentPos += disk->fFiles[ pt ].FileNameLength;
    }
    glPath[ CurrentPos ] = L'\0';
    return glPath;
}

void inline CallMe( const STATUSINFO* info , const DWORD value )
{
    if ( info != nullptr )
    {
        SendMessage( info->hWnd , PBM_SETPOS , value , 0 );
    }
    return;
}


bool CNtfsHelper::FetchSearchInfo( DISKHANDLE* disk , FILE_RECORD_HEADER* file , SEARCHFILEINFO* data )
{
    //------------------------------------
    if ( file->Ntfs.Type != 'ELIF' )
    {
        return false;
    }
    //------------------------------------
    const int stop = std::min( ( WORD )8 , file->NextAttributeNumber );

    bool fileNameFound = false;
    bool fileSizeFound = false;
    bool dataFound = false;
    ATTRIBUTE* attr = reinterpret_cast< ATTRIBUTE* >( reinterpret_cast< PUCHAR >( file ) + file->AttributesOffset );

    data->Flags = file->Flags;

    for ( int i = 0; i < stop; i++ )
    {
        if ( attr->AttributeType < 0 || attr->AttributeType>0x100 )
        {
            break;
        }

        const RESIDENT_ATTRIBUTE* pRESIDENT_ATTRIBUTE = ( RESIDENT_ATTRIBUTE* )( attr );
        switch ( attr->AttributeType )
        {
            case Attribute_FileName:
                {
                    FILENAME_ATTRIBUTE* fn = ( FILENAME_ATTRIBUTE* )( PUCHAR( attr ) + pRESIDENT_ATTRIBUTE->ValueOffset );

                    if ( ( fn->NameType & NAMESPACE_WIN32 ) == NAMESPACE_WIN32 || fn->NameType == NAMESPACE_POSIX )
                    {
                        fn->Name[ fn->NameLength ] = L'\0';
                        data->CreationTime = fn->CreationTime;
                        data->LastWriteTime = fn->LastWriteTime;
                        data->ChangeTime = fn->ChangeTime;
                        data->LastAccessTime = fn->LastAccessTime;

                        data->FileName = ( wchar_t* )CMyHeap::AllocAndCopyString( disk->heapBlock , fn->Name , fn->NameLength );
                        data->FileNameLength = std::min<WORD>( ( WORD )fn->NameLength , ( WORD )wcslen( data->FileName ) );
                        data->ParentId.QuadPart = fn->DirectoryFileReferenceNumber;
                        data->ParentId.HighPart &= 0x0000ffff;

                        if ( fn->DataSize || fn->AllocatedSize )
                        {
                            data->DataSize = fn->DataSize;
                            data->AllocatedSize = fn->AllocatedSize;
                        }

                        if ( file->BaseFileRecord.LowPart != 0 )// && file->BaseFileRecord.HighPart !=0x10000)
                        {
                            CFixList::AddToFixList( file->BaseFileRecord.LowPart , disk->filesCount );
                        }

                        if ( dataFound && fileSizeFound )
                        {
                            return true;
                        }
                        fileNameFound = true;
                    }
                    break;
                }
            case Attribute_Data:
                if ( !attr->Nonresident && pRESIDENT_ATTRIBUTE->ValueLength > 0 )
                {
                    memcpy( data->data ,
                        PUCHAR( attr ) + pRESIDENT_ATTRIBUTE->ValueOffset ,
                        std::min( ( DWORD )sizeof( data->data ) , pRESIDENT_ATTRIBUTE->ValueLength ) );

                    if ( fileNameFound && fileSizeFound )
                    {
                        return true;
                    }
                    dataFound = true;
                }
            case Attribute_UNUSED: // falls through
                if ( AttributeLength( attr ) > 0 || AttributeLengthAllocated( attr ) > 0 )
                {
                    data->DataSize = std::max( data->DataSize , AttributeLength( attr ) );
                    data->AllocatedSize = std::max( data->AllocatedSize , AttributeLengthAllocated( attr ) );
                    if ( fileNameFound && dataFound )
                    {
                        return true;
                    }
                    fileSizeFound = true;
                }
                break;
            default:
                break;
        };

        if ( attr->Length > 0 && attr->Length < file->BytesInUse )
        {
            attr = ( ATTRIBUTE* )( PUCHAR( attr ) + attr->Length );
        }
        else if ( attr->Nonresident == TRUE )
        {
            attr = ( ATTRIBUTE* )( PUCHAR( attr ) + sizeof( NONRESIDENT_ATTRIBUTE ) );
        }
    }
    return fileNameFound;
}



bool CNtfsHelper::FixFileRecord( FILE_RECORD_HEADER* file )
{
    //int sec = 2048;
    const PUSHORT usa = PUSHORT( PUCHAR( file ) + file->Ntfs.UsaOffset );
    PUSHORT sector = PUSHORT( file );

    if ( file->Ntfs.UsaCount > 4 )
    {
        return false;
    }
    for ( WORD index = 1; index < file->Ntfs.UsaCount; index++ )
    {
        sector[ 255 ] = usa[ index ];
        sector += 256;
    }

    return true;
}

bool CNtfsHelper::ReparseDisk( DISKHANDLE* disk , UINT option , STATUSINFO* info )
{
    if ( disk == nullptr )
    {
        return false;
    }
    //------------------------
    if ( disk->type == NTFSDISK )
    {
        SAFE_DELETE( disk->NTFS.MFT );
        SAFE_DELETE( disk->NTFS.Bitmap );
    }
    if ( disk->heapBlock != nullptr )
    {
        CMyHeap::ReUseBlocks( disk->heapBlock , false );
    }
    free( disk->fFiles );
    disk->fFiles = nullptr;

    disk->filesCount = 0;
    disk->realFiles = 0;

    if ( CNtfsHelper::LoadMFT( disk , FALSE ) != 0 )
    {
        CNtfsHelper::ParseMFT( disk , option , info );
    }
    //------------------------
    return true;
};
