#pragma once
//--------------------------------------------------------------
// NTFS Structures

#include "Heap.h"

#pragma pack(push,1)

struct BIOS_PARAMETER_BLOCK
{
    USHORT BytesPerSector;
    UCHAR  SectorsPerCluster;
    USHORT ReservedSectors;
    UCHAR  Fats;
    USHORT RootEntries;
    USHORT Sectors;
    UCHAR  Media;
    USHORT SectorsPerFat;
    USHORT SectorsPerTrack;
    USHORT Heads;
    ULONG  HiddenSectors;
    ULONG  LargeSectors;
};

//------------------------
// BOOT_BLOCK
//------------------------

struct BOOT_BLOCK
{
    UCHAR Jump[ 3 ];
    UCHAR Format[ 8 ];
    WORD BytesPerSector;
    UCHAR SectorsPerCluster;
    WORD BootSectors;
    UCHAR Mbz1;
    WORD Mbz2;
    WORD Reserved1;
    UCHAR MediaType;
    WORD Mbz3;
    WORD SectorsPerTrack;
    WORD NumberOfHeads;
    ULONG PartitionOffset;
    ULONG Rserved2[ 2 ];
    ULONGLONG TotalSectors;
    ULONGLONG MftStartLcn;
    ULONGLONG Mft2StartLcn;
    ULONG ClustersPerFileRecord;
    ULONG ClustersPerIndexBlock;
    ULONGLONG VolumeSerialNumber;
    UCHAR Code[ 0x1AE ];
    WORD BootSignature;
};
#pragma pack(pop)


//------------------------
//NTFS_RECORD_HEADER
//	type - 'FILE' 'INDX' 'BAAD' 'HOLE' *CHKD'
//------------------------
struct NTFS_RECORD_HEADER
{
    ULONG Type;     ///The type of NTFS record.When the value of Type is considered as a sequence of four one-byte characters,
                    ///it normally spells an acronym for the type. Defined values include:
    WORD UsaOffset; ///The offset, in bytes, from the start of the structure to the Update Sequence Array
    WORD UsaCount; ///更新シーケンス配列内の値の数
    USN Usn;       ///TNTFSレコードの更新シーケンス番号
};

//------------------------
// FILE_RECORD_HEADER
//------------------------
struct FILE_RECORD_HEADER
{
    NTFS_RECORD_HEADER Ntfs;///An NTFS_RECORD_HEADER structure with a Type of ‘FILE’.
    WORD SequenceNumber; ///MFTエントリが再利用された回数
    WORD LinkCount;      ///MFTエントリへのディレクトリリンクの数
    WORD AttributesOffset;///構造体の先頭からMFTの最初の属性までのオフセット（バイト単位） エントリ
    WORD Flags;       /// MFTエントリのプロパティを指定するフラグのビット配列 0x0001 InUse; 0x0002 Directory
    DWORD BytesInUse; /// MFTエントリによって使用されたバイト数
    DWORD BytesAllocated;
    ULARGE_INTEGER BaseFileRecord;
    WORD NextAttributeNumber; //MFTエントリに追加された次の属性に割り当てられる番号
};
//----------------------------
// ATTRIBUTE_TYPE enumeration
//----------------------------
enum ATTRIBUTE_TYPE
{
    Attribute_UNUSED = 0 ,
    Attribute_StandardInformation = 0x00000010 ,
    Attribute_AttributeList = 0x00000020 ,
    Attribute_FileName = 0x00000030 ,
    Attribute_ObjectId = 0x00000040 ,
    Attribute_SecurityDescripter = 0x50 ,
    Attribute_VolumeName = 0x60 ,
    Attribute_VolumeInformation = 0x70 ,
    Attribute_Data = 0x80 ,
    Attribute_IndexRoot = 0x90 ,
    Attribute_IndexAllocation = 0xA0 ,
    Attribute_Bitmap = 0xB0 ,
    Attribute_ReparsePoint = 0x000000C0 ,  // Reparse Point = Symbolic link
    Attribute_EAInformation = 0x000000D0 ,
    Attribute_EA = 0x000000E0 ,
    Attribute_PropertySet = 0x000000F0 ,
    Attribute_LoggedUtilityStream = 0x00000100
};
//----------------------------
// ATTRIBUTE Structure
//----------------------------
struct ATTRIBUTE
{
    ATTRIBUTE_TYPE AttributeType;
    DWORD Length;       /// The size, in bytes, of the resident part of the attribute
    BOOLEAN Nonresident;/// NONRESIDENT_FORM の場合、属性値は存在しない
    UCHAR NameLength;   /// 属性の名前（ある場合）のサイズ（文字数）
    WORD NameOffset;    /// 構造体の先頭から属性名までのオフセット（バイト単位）。属性 nameはUnicode文字列として格納されています。
    WORD Flags;         /// 属性のプロパティを指定するフラグのビット配列.The values defined include:0x0001 = Compressed.  0x4000 = Encrypted, 0x8000 = Sparse
    WORD AttributeNumber; /// 属性のインスタンスの数値識別子
};

//----------------------------
// ATTRIBUTE resident
//----------------------------

struct RESIDENT_ATTRIBUTE
{
    struct ATTRIBUTE Attribute;
    DWORD ValueLength;  //属性値のサイズ（バイト単位）
    WORD ValueOffset;   //構造体の先頭から属性値までのオフセット（バイト単位）
    WORD Flags; //0x0001 Indexed
};
//----------------------------
// ATTRIBUTE nonresident
//----------------------------
struct NONRESIDENT_ATTRIBUTE
{
    struct ATTRIBUTE Attribute;
    ULONGLONG LowVcn;//属性値のこの部分の最小有効仮想クラスタ番号(VCN)。
                     //属性値が非常に断片化されていない限り(属性リストが それを記述するのに必要な）属性値の一部分だけがあり、そして LowVcn はゼロです
    ULONGLONG HighVcn;//属性値のこの部分の最大有効VCN
    WORD RunArrayOffset;//構造体の先頭から次の文字列を含むrun配列までのオフセット（バイト単位） VCNと論理クラスタ番号（LCN）間のマッピング
    UCHAR CompressionUnit;
    UCHAR AligmentOrReserved[ 5 ];
    ULONGLONG AllocatedSize;
    ULONGLONG DataSize;
    ULONGLONG InitializedSize;
    ULONGLONG CompressedSize; //Only when compressed
};

/*
    VolumeName - just a Unicode String
    Data = just data
    SecurityDescriptor - rarely found
    Bitmap - array of bits, which indicate the use of entries
*/
//----------------------------
// AttributeStandardInformation
//----------------------------
struct STANDARD_INFORMATION
{
    FILETIME CreationTime;
    FILETIME ChangeTime;
    FILETIME LastWriteTime;
    FILETIME LastAccessTime;
    ULONG FileAttributes;//FILE_ATTRIBUTES_* like in windows.h and is always resident
    ULONG AligmentOrReservedOrUnknown[ 3 ];//Normally contains zero. Interpretation unknown.
    ULONG QuotaId; //NTFS 3.0 or higher
    ULONG SecurityID; //NTFS 3.0 or higher
    ULONGLONG QuotaCharge; //NTFS 3.0 or higher
    USN Usn; //NTFS 3.0 or higher
};

/* ATTRIBUTE_LIST
    is always nonresident and consists of an array of ATTRIBUTE_LIST
*/
struct ATTRIBUTE_LIST
{
    ATTRIBUTE_TYPE Attribute;
    WORD Length;
    UCHAR NameLength;
    WORD NameOffset; // starts at structure begin
    ULONGLONG LowVcn;
    ULONGLONG FileReferenceNumber;
    WORD AttributeNumber;
    WORD AligmentOrReserved[ 3 ];
};

/* FILENAME_ATTRIBUTE
    is always resident
    ULONGLONG informations only updated, if name changes
*/
struct FILENAME_ATTRIBUTE
{
    ULONGLONG DirectoryFileReferenceNumber; //points to a MFT Index of a directory
    FILETIME CreationTime; //saved on creation, changed when filename changes
    FILETIME ChangeTime; //ファイル属性が最後に更新された時刻(ファイル名が変更され、同じフィールドと異なる可能性がある場合にのみ更新されます)
    FILETIME LastWriteTime;//ファイルが最後に書き込まれた時刻(ファイル名が変更され、同じファイル名のフィールドと異なる場合があります)
    FILETIME LastAccessTime;
    ULONGLONG AllocatedSize;
    ULONGLONG DataSize;
    ULONG FileAttributes; // ditto
    ULONG AligmentOrReserved;
    UCHAR NameLength;
    UCHAR NameType; // 0x01 Long 0x02 Short 0x00 Posix?
    WCHAR Name[ 1 ];
};

#define	NAMESPACE_POSIX		0x00
#define	NAMESPACE_WIN32		0x01
#define	NAMESPACE_DOS		0x02
#define NAMESPACE_WIN32DOS	0x03


// MYSTRUCTS
enum enum_diskType
{
    NTFSDISK = 1 ,
    // not supported
    FAT32DISK = 2 ,
    FATDISK = 4 ,
    EXT2 = 8 ,
    UNKNOWN = 0xff99ff99 ,

};



struct SEARCHFILEINFO
{
    FILETIME CreationTime;
    FILETIME ChangeTime;
    FILETIME LastWriteTime;
    FILETIME LastAccessTime;

    LPCWSTR FileName;
    WORD FileNameLength;
    WORD Flags;
    ULARGE_INTEGER ParentId;

    ULONGLONG DataSize;
    ULONGLONG AllocatedSize;

    char data[ 64 ];
};


struct DISKHANDLE
{
    HANDLE fileHandle;
    enum_diskType type;
    DWORD IsLong;
    DWORD filesCount;
    DWORD realFiles;
    WCHAR DosDevice;
    CMyHeap::HEAPBLOCK* heapBlock;
    union
    {
        struct
        {
            BOOT_BLOCK bootSector;
            DWORD BytesPerFileRecord;
            DWORD BytesPerCluster;
            BOOL complete;
            DWORD sizeMFT;
            DWORD entryCount;
            ULARGE_INTEGER MFTLocation;
            UCHAR* MFT;
            UCHAR* Bitmap;
        } NTFS;
        struct
        {
            DWORD FAT;
        } FAT;
        SEARCHFILEINFO* fFiles;
    };
};

struct STATUSINFO
{
    HWND hWnd;
    DWORD Value;
};

void CallMe( const STATUSINFO* info , const DWORD value );

/* MY FUNCTIONS

*/

#define LONGINFO		1
#define SHORTINFO		2
#define SEARCHINFO		3
#define EXTRALONGINFO	4

typedef DWORD( __cdecl* FETCHPROC )( DISKHANDLE* , FILE_RECORD_HEADER* , PUCHAR );

class CNtfsHelper
{
public:

    static DISKHANDLE* OpenDisk( const wchar_t* disk );
    static DISKHANDLE* OpenDisk( const WCHAR DosDevice );
    static bool CloseDisk( DISKHANDLE* disk );
    static ULONGLONG LoadMFT( DISKHANDLE* disk , BOOL complete );
    static bool FixFileRecord( FILE_RECORD_HEADER* file );

    static ATTRIBUTE* FindAttribute( const FILE_RECORD_HEADER* file , const ATTRIBUTE_TYPE type );

    static DWORD ParseMFT( DISKHANDLE* disk , UINT option , STATUSINFO* info );

    static DWORD ProcessBuffer( DISKHANDLE* disk , PUCHAR buffer , DWORD size , FETCHPROC fetch );

    static STRSAFE_LPWSTR GetPath( DISKHANDLE* disk , int id );
    static STRSAFE_LPWSTR GetCompletePath( DISKHANDLE* disk , int id );
    static bool FetchSearchInfo( DISKHANDLE* disk , FILE_RECORD_HEADER* file , SEARCHFILEINFO* data );

    static ULONG RunLength( PUCHAR run );
    static LONGLONG RunLCN( PUCHAR run );
    static ULONGLONG RunCount( PUCHAR run );
    static bool FindRun( NONRESIDENT_ATTRIBUTE* attr , ULONGLONG vcn , PULONGLONG lcn , PULONGLONG count );

    static DWORD ReadMFTParse( DISKHANDLE* disk , NONRESIDENT_ATTRIBUTE* attr , ULONGLONG vcn , ULONG count , PVOID buffer , FETCHPROC fetch , STATUSINFO* info );
    static DWORD ReadMFTLCN( DISKHANDLE* disk , ULONGLONG lcn , ULONG count , PVOID buffer , FETCHPROC fetch , STATUSINFO* info );

    static bool ReparseDisk( DISKHANDLE* disk , UINT option , STATUSINFO* info );
};
