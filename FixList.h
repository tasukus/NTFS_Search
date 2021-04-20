// Bugfix for Linux and NT 4.0 drives prior to NTFS 3.0 or 
// files written by Linux NTFS drivers

#include "ntfs_struct.h"


// limit: 2^32 files

// LinkedList
class CFixList
{
public:
    struct LINKITEM
    {
        unsigned int data;
        unsigned int entry;
        LINKITEM* next;
    };
public:
    static void AddToFixList( const int entry , const int data );
    static void CreateFixList();
    static void ProcessFixList( DISKHANDLE* disk );
};

