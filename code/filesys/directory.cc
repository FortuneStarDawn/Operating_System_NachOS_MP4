// directory.cc 
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------

Directory::Directory(int size)
{
    table = new DirectoryEntry[size];
	
	// MP4 mod tag
	memset(table, 0, sizeof(DirectoryEntry) * size);  // dummy operation to keep valgrind happy
	
    tableSize = size;
    for (int i = 0; i < tableSize; i++)
	table[i].inUse = FALSE;
}

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory()
{ 
    delete [] table;
} 

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void
Directory::FetchFrom(OpenFile *file)
{
    (void) file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void
Directory::WriteBack(OpenFile *file)
{
    (void) file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::FindIndex(char *name)
{
    for (int i = 0; i < tableSize; i++)
        if (table[i].inUse && !strncmp(table[i].name, name, FileNameMaxLen))
	    return i;
    return -1;		// name not in directory
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't 
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::Find(char *name)
{
    int i = FindIndex(name);

    if (i != -1)
	return table[i].sector;
    return -1;
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------

bool
Directory::Add(char *name, int newSector)
{ 
	//printf("-d Call Directory  Add %s",name);
	//store path ex. "/test1/test2/test3"
	//path[0] = "/test1\0"
	//path[1] = "/test2\0"
	//path[2] = "/test3\0"
	int depth, i;
	char path[MaxDepth][FileNameMaxLen+1];

  depth = cutPath(name, path);
  
	//printf("-d Call Directory::Add %s depth %d\n",name,depth);
	//for(int m=0;m<depth;m++)
		//printf("-d path[%d] = %s\n",m,path[m]);
	if(depth == 1)
	{
		for (int i = 0; i < tableSize; i++)
		{
			if (!table[i].inUse)
			{
				table[i].inUse = TRUE;
				strncpy(table[i].name, path[0], FileNameMaxLen); 
				table[i].sector = newSector;
				table[i].isFile = TRUE ;// it is a file
				return TRUE;
			}
		}
		return FALSE;	// no space.  Fix when we have extensible files.
	}
	else
	{
		Directory *DirPath[depth]; // recode the path 
		int SectorLocation;
		DirPath[0] = this; // first directory
		OpenFile *DirectoryFile, *preFile;
		for(i=0; i < depth - 1;i++) // the last is a target so find until depth-1 
		{
			SectorLocation = DirPath[i] ->Find(path[i]);
			if(i > 0)
				preFile = DirectoryFile;
			DirPath[i+1] = new Directory(NumDirEntries); // NumDirectoryEntry = 64
			DirectoryFile = new OpenFile(SectorLocation);
			if(i > 0)
				delete preFile;
			DirPath[i+1] ->FetchFrom(DirectoryFile); 
		}
		bool result = DirPath[depth-1] ->Add(path[depth-1], newSector);
		DirPath[depth-1] ->WriteBack(DirectoryFile);
		for(i=1; i < depth;i++)
			delete DirPath[i];
		delete DirectoryFile;
		return result;
	}  
}

bool 
Directory::AddDirectory(char *name, int Sector, PersistentBitmap *freeMap)
{
	int depth, i;
  char path[MaxDepth][FileNameMaxLen+1];
  
  depth = cutPath(name, path);

	//printf("-d Call Directory AddDirectory %s level %d\n",name,depth);
	//for(int m=0;m<depth;m++)
		//printf("-d path[%d] = %s\n",m,path[m]);
	if(depth == 1)
	{
		Directory *directory;
		directory = new Directory(NumDirEntries);
		FileHeader *dirHeader = new FileHeader;
		dirHeader ->Allocate(freeMap, DirectoryFileSize);
		dirHeader ->WriteBack(Sector);
		OpenFile *dirFile;
		dirFile = new OpenFile(Sector);
		directory ->WriteBack(dirFile);
		delete directory;
		delete dirFile;
		delete dirHeader;
		for(int i=0;i<tableSize;i++)
		{
			if(!table[i].inUse)
			{
				table[i].inUse = TRUE;
				strncpy(table[i].name, path[0], FileNameMaxLen); 
				table[i].sector = Sector;
				table[i].isFile = FALSE; // it is a directory
				return TRUE;
			}
		}
		return FALSE;
	}
	else
	{
		Directory *DirPath[depth]; // recode the path 
		int SectorLocation;
		DirPath[0] = this; // first directory
		OpenFile *DirectoryFile, *preFile;
		for(i=0; i < depth - 1;i++) // the last is a target so find until depth-1 
		{
			SectorLocation = DirPath[i] ->Find(path[i]);
			if(i > 0)
				preFile = DirectoryFile;
			DirPath[i+1] = new Directory(NumDirEntries); // NumDirectoryEntry = 64
			DirectoryFile = new OpenFile(SectorLocation);
			if(i > 0)
				delete preFile;
			DirPath[i+1] ->FetchFrom(DirectoryFile);
		}
		bool result = DirPath[depth-1] ->AddDirectory(path[depth-1], Sector, freeMap);
		DirPath[depth-1] ->WriteBack(DirectoryFile);
		for(i=1; i < depth;i++)
			delete DirPath[i];
		delete DirectoryFile;
		return result;
	}
}
//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory. 
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

bool
Directory::Remove(char *name)
{ 
//printf("remove = %s",name);
    int i = FindIndex(name);
//printf("remove = %s 2",name);
    if (i == -1)
	return FALSE; 		// name not in directory
    table[i].inUse = FALSE;
    return TRUE;	
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory. 
//----------------------------------------------------------------------

void
Directory::List()
{
   int num = 0;
   for (int i = 0; i < tableSize; i++)
   {
	   if (table[i].inUse)
		{	
			if(table[i].isFile)
				printf("[%d] %s F\n",num++, table[i].name+1);
			else
				printf("[%d] %s	D\n",num++, table[i].name+1);
		}
   }    
}
void Directory::RecList(int depth)
{
    Directory *dir = new Directory(NumDirEntries);	
	OpenFile *DirectoryFile;
    int num = 0;
    for (int i = 0; i < tableSize; i++)
	{	
		if (table[i].inUse)
		{	
			for(int m=0;m<depth;m++)
				printf("        ");
			if(table[i].isFile)
				printf("[%d] %s F\n",num++, table[i].name+1);
			else
				printf("[%d] %s D\n",num++, table[i].name+1);
			
			if(!table[i].isFile)
			{
				DirectoryFile = new OpenFile(table[i].sector);	// go to next directory
				dir->FetchFrom(DirectoryFile);
				dir->RecList(depth+1);
			}
		}
    }
}
//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------

void
Directory::Print()
{ 
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    for (int i = 0; i < tableSize; i++)
	if (table[i].inUse) {
	    printf("Name: %s, Sector: %d\n", table[i].name, table[i].sector);
	    hdr->FetchFrom(table[i].sector);
	    hdr->Print();
	}
    printf("\n");
    delete hdr;
}
