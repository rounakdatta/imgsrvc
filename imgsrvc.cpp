/*==================INCLUDES============*/
#include <stdio.h>
#include <iostream>
#include "common.h"
using namespace std;

/*==========TYPEDEFS================*/
typedef struct tagIndexRecStruct
{
	u64 ImgKey;
	u32 AltKey;
	u16 Flags;
	u64 Offset;
	u32 RecSize;
}IndexRecStruct;

typedef struct tagIndexFileHdrStruct
{
	u64 RecsCount;
}IndexFileHdrStruct;

typedef struct tagStoreFileHdrStruct
{
	u64 RecsCount;
}StoreFileHdrStruct;

typedef struct tagStoreRecHdrStruct
{
	u32 MagicNo;
	u64 ImgKey; /*64-bit = 2 * 32-bit*/
	u32 AltKey;
	u64 Cookie;
	u64 Offset;
	u32 RecSize;
	u16 Flags;
	u8 Reserved[2]; 
}StoreRecHdrStruct;

typedef struct tagStoreRecFtrStruct
{
	u32 MagicNo;
	u32 Checksum;
}StoreRecFtrStruct;

/*=================MACROS====================*/
#define HDR_MAGIC_NUM	0xAABBCCDD
#define FTR_MAGIC_NUM	0x11223344

/*=================PROTOTYPES================*/
bool initImgService(void);
bool upload(char *pacFilename);
bool retrieve(u64 Cookie);

u64 genKey(void);
u64 genCookie(u64 Key);
u64 getImgKeyFromCookie(u64 Cookie);
bool getIndexRecPosForImgKey(u64 ImgKey, u32 * IndexRecPos);
void incRecCount(void);

/*=================GLOBALS===================*/

IndexRecStruct gastIndexRecs[1000];

FILE *gfptrStoreFile;

FILE *gfptrIndexFile;

u64 gRecsCount;

int main()
{	
	u32 i;
	initImgService();
	int ch;
	do
	{
		cout<<"\n\n1.Upload\n2.Retrieve\n ---?";
		cin>>ch;
		if(ch == 1)
		{
			char NameOfFile[100];
			cout<<"NameOfFile : ";
			cin>>NameOfFile;
			upload(NameOfFile);
		}
		if(ch == 2)
		{
			int imgkey;
			cout<<"ImgKey : ";
			cin>>imgkey;
			if (retrieve(imgkey)!=true)
				printf("oh!!retrieval failed???\n");
			else
				cout<<"saved back into storefile";
		}
	}while(ch != 0);


#if 1
	printf("Rec Count : %lu\n", gRecsCount);	
	for(i = 0; i < gRecsCount; i++)
	{
		printf("====Rec[%d]=====\n", i+1);
		printf("ImgKey = %lu\n", gastIndexRecs[i].ImgKey);
		printf("Offset = %lu\n", gastIndexRecs[i].Offset);
		printf("RecSize = %d\n", gastIndexRecs[i].RecSize);
	}
#endif /* #if 0*/	
}

bool initImgService()
{
	StoreFileHdrStruct stStoreFileHdr;
	IndexFileHdrStruct stIndexFileHdr;

	u8 Junk;	

	gfptrStoreFile = fopen("storefile", "r+");
	if(NULL==gfptrStoreFile)
	{
		/* create storefile */
		gfptrStoreFile = fopen("storefile", "w+");

		if (NULL==gfptrStoreFile)
			return false;

		/* setup HDR */
		stStoreFileHdr.RecsCount = 0;
		fwrite(&stStoreFileHdr, sizeof(StoreFileHdrStruct), 1, gfptrStoreFile);
		
		/* fill junk for reserved space */
		{
			Junk = 0xFF;
			u16 JunkCount = (4*1024) - sizeof(StoreFileHdrStruct);

			while(JunkCount)
			{
				fwrite(&Junk, sizeof(u8), 1, gfptrStoreFile);
				--JunkCount;
			}
		}
	}

	gfptrIndexFile = fopen("indexfile", "r+");
	if(NULL==gfptrIndexFile)
	{

		/* create indexfile */
		gfptrIndexFile = fopen("indexfile", "w+");

		if(NULL==gfptrIndexFile)
			return false;

		/* setup HDR */
		stIndexFileHdr.RecsCount = 0;
		fwrite(&stIndexFileHdr, sizeof(IndexFileHdrStruct), 1, gfptrIndexFile);

		/* fill junk for reserved space */
		{
			Junk = 0xFF;
			u16 JunkCount = (4*1024) - sizeof(IndexFileHdrStruct);

			while(JunkCount)
			{
				fwrite(&Junk, sizeof(u8), 1, gfptrIndexFile);
				--JunkCount;
			}
		}
		/* NASTY right??? */
		return true;
	}

	/* read StoreFileHdr */
	fread(&stStoreFileHdr, sizeof(StoreFileHdrStruct), 1, gfptrStoreFile);

	/* read IndexFileHdr */
	fread(&stIndexFileHdr, sizeof(IndexFileHdrStruct), 1, gfptrIndexFile);
	/* skip reserved area */
	fseek(gfptrIndexFile, 4*1024, SEEK_SET);
	fread(gastIndexRecs, sizeof(IndexRecStruct), stIndexFileHdr.RecsCount, gfptrIndexFile);

	/* check SYNC, if needed, DO Correct */
	if(stStoreFileHdr.RecsCount != stIndexFileHdr.RecsCount)
	{
		/* CORRECT ME */
	}	
		
	
	/* init GLOBALS */
	gRecsCount = stStoreFileHdr.RecsCount;

	return true;	
}

bool upload(char *pacFilename)
{
	StoreRecHdrStruct stStoreRecHdr;
	StoreRecFtrStruct stStoreRecFtr;
	FILE * fptrFileToUpload;
	u32 FileSize;

	stStoreRecHdr.ImgKey 	= genKey();
	stStoreRecHdr.AltKey 	= 0;
	stStoreRecHdr.Cookie 	= genCookie(stStoreRecHdr.ImgKey);
	
	fptrFileToUpload = fopen(pacFilename, "r");

	if (NULL==fptrFileToUpload) /*file upload FAILS*/
		return false;

	fseek(fptrFileToUpload, 0, SEEK_END);
	stStoreRecHdr.RecSize = ftell(fptrFileToUpload);
	fseek(fptrFileToUpload, 0, SEEK_SET); /* re-position to BOF */	
	
	fseek(gfptrStoreFile,0,SEEK_END); /* seek to END of STORE FILE */
	/* WHAT IF pgm crashes in middle of StoreFile WRITE??, good to use ftell??? */
	stStoreRecHdr.Offset 	= ftell(gfptrStoreFile); 
	stStoreRecHdr.Flags 	= 0;
	stStoreRecHdr.MagicNo	= HDR_MAGIC_NUM;

	/* write HDR to store file - WATCH OUT!! - What If fileWrite crashes??? */
	fwrite(&stStoreRecHdr, sizeof(StoreRecHdrStruct), 1, gfptrStoreFile);

	/* write IMG to store file - FileToUpload MUST have size>0 */
	i32 i32FileSize = stStoreRecHdr.RecSize;
	do
	{
		u8 aBuffer[4*1024];

		if (i32FileSize<(4*1024))
		{	
			fread(aBuffer, i32FileSize, 1, fptrFileToUpload);
			fwrite(aBuffer, i32FileSize, 1, gfptrStoreFile);
		}
		else
		{
			fread(aBuffer, 4*1024, 1, fptrFileToUpload);
			fwrite(aBuffer, 4*1024, 1, gfptrStoreFile);	
		}	
		 						
		i32FileSize -= 4*1024;
				
	}while (i32FileSize > 0);

	/* close */
	fclose(fptrFileToUpload);

	/* write FTR to store file */
	stStoreRecFtr.MagicNo = FTR_MAGIC_NUM;
	/* TODO: calc checksum */
	stStoreRecFtr.Checksum = 0;

	fwrite(&stStoreRecFtr, sizeof(StoreRecFtrStruct), 1, gfptrStoreFile);

	/* write padding to store file */
	{
		u64 u64FilePos = ftell(gfptrStoreFile);

		if (u64FilePos & 0x07)
		{	
			u16 LoopCount = 8-(u64FilePos & 0x07);
			u8 Padding = 0xFF;

			while (LoopCount)
			{	
				fwrite(&Padding, 1, 1, gfptrStoreFile);
				--LoopCount;
			}	
		}	
	}

	/* update RAM IndexRecs */
	gastIndexRecs[gRecsCount].ImgKey 	= stStoreRecHdr.ImgKey;
	gastIndexRecs[gRecsCount].AltKey 	= stStoreRecHdr.AltKey;
	gastIndexRecs[gRecsCount].Offset 	= stStoreRecHdr.Offset;
	gastIndexRecs[gRecsCount].RecSize 	= stStoreRecHdr.RecSize;

	/* update RAM record to IndexFile - MUST be at EOF */
	fwrite(&gastIndexRecs[gRecsCount], sizeof(IndexRecStruct), 1, gfptrIndexFile);

	/* dont MISS to update */
	incRecCount();
	
	/* update StoreFileHdr */
	{	
		StoreFileHdrStruct stStoreFileHdr;	

		stStoreFileHdr.RecsCount = gRecsCount;
		
		/* rewind */
		fseek(gfptrStoreFile, 0, SEEK_SET);

		/* update StoreFileHdr RecsCount */
		fwrite(&stStoreFileHdr, sizeof(StoreFileHdrStruct), 1, gfptrStoreFile);

		/* move to EOF */
		fseek(gfptrStoreFile, 0, SEEK_END);
	}
	/* update IndexFileHdr */
	{
		IndexFileHdrStruct stIndexFileHdr;	

		stIndexFileHdr.RecsCount = gRecsCount;
		
		/* rewind */
		fseek(gfptrIndexFile, 0, SEEK_SET);

		/* update StoreFileHdr RecsCount */
		fwrite(&stIndexFileHdr, sizeof(IndexFileHdrStruct), 1, gfptrIndexFile);

		/* move to EOF */
		fseek(gfptrIndexFile, 0, SEEK_END);	
	}

	return true;
}


bool retrieve(u64 Cookie)
{
	IndexRecStruct stIndexRec;
	StoreRecHdrStruct stStoreRecHdr;
	u64 ImgKey;
	u32 IndexRecPos;

	ImgKey = getImgKeyFromCookie(Cookie);

	printf("fetching Rec[%lu]\n", ImgKey);

	if (getIndexRecPosForImgKey(ImgKey, &IndexRecPos) != true)
		return false;

	/* within RANGE? */
	if(IndexRecPos >= gRecsCount)
		return false;

	stIndexRec = gastIndexRecs[IndexRecPos];

	/* if size 0 or DEL flag is set?? */
	if(0==stIndexRec.RecSize)
		return false;

	/* seek to STORE REC pos */
	fseek(gfptrStoreFile, stIndexRec.Offset, SEEK_SET);
	fread(&stStoreRecHdr, sizeof(StoreRecHdrStruct), 1, gfptrStoreFile);

	/* check whether IndexRec info matches StoreFileHdr? */
	if(stStoreRecHdr.MagicNo!=HDR_MAGIC_NUM ||\
		stStoreRecHdr.ImgKey!=stIndexRec.ImgKey || \
			stStoreRecHdr.RecSize!=stIndexRec.RecSize)
	{
		return false;
	}	

	/* pls DO CHECK FTR */

	/* read FILE */
	{
		i32 RecSize = stIndexRec.RecSize;
		u8 aBuffer[4*1024];

		FILE *fptrOutFile;

		/* to test RETRIEVE */
		fptrOutFile = fopen("outfile", "w");

		do
		{
			if(RecSize < 4*1024)
			{	
				fread(aBuffer, RecSize, 1, gfptrStoreFile);
				fwrite(aBuffer, RecSize, 1, fptrOutFile);
			}
			else
			{
				fread(aBuffer, 4*1024, 1, gfptrStoreFile);
				fwrite(aBuffer, 4*1024, 1, fptrOutFile);	
			}

			RecSize -= 4*1024;

		}while(RecSize > 0);

		fclose(fptrOutFile);
	}

	return true;
}

/*=============helper functions==================*/ 
u64 genKey()
{
	return (u64)((gRecsCount+1));
}

u64 genCookie(u64 Key)
{
	return Key;
}

u64 getImgKeyFromCookie(u64 Cookie)
{
	return Cookie; /* for TIME BEING ImgKey==Cookie */
}

bool getIndexRecPosForImgKey(u64 ImgKey, u32 *IndexRecPos)
{
	*IndexRecPos = (u32)ImgKey-1;

	return true;
} 

void incRecCount()
{
	++gRecsCount;
}