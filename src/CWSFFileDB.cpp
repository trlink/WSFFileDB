//includes
//////////
#include "CWSFFileDB.h"








CWSFFileDB::CWSFFileDB(fs::FS *fs, char *szFileName, int *pFields, int nFieldCount, bool bCreateIfNotExist, uint32_t dwReserveSpace)
{
	strcpy(this->m_szFile, szFileName);
	this->m_fs = fs;
	this->m_pFields = pFields;
	this->m_nFieldCount = nFieldCount;
	this->m_dwRecordCount = 0;
	this->m_dwNextFreePos = WSFFileDB_HeaderSize;
	this->m_bCreateIfNotExist = bCreateIfNotExist; 
	this->m_nEntrySize = this->calculateEntrySize();
	this->m_dwTableSize = 0;
	this->m_bDbOpen = false;
	this->m_dwLastInsertPos = 0;
	this->m_mutex = xSemaphoreCreateMutex();
	this->m_dwReserveSpace = dwReserveSpace;
	this->m_bCreated = false;
	this->m_dwMaxPosWritten = 0;
  
	#if WSFFileDB_Debug == 1
		Serial.print(F("DB ("));
		Serial.print(this->m_szFile);
		Serial.print(F("): Create Instance, Entry size: "));
		Serial.print(this->m_nEntrySize);
		Serial.print(F(", FieldCount: "));
		Serial.println(this->m_nFieldCount);
	#endif
};



CWSFFileDB::~CWSFFileDB()
{
  if(this->m_file)
  {
    this->m_file.close();
  };
};


int CWSFFileDB::getEntrySize()
{
  return this->m_nEntrySize;
};


uint32_t CWSFFileDB::getRecordCount()
{
  return this->m_dwRecordCount;
};


bool CWSFFileDB::openWrite()
{
	#if WSFFileDB_Debug == 1
		Serial.print(F("DB ("));
		Serial.print(this->m_szFile);
		Serial.print(F("): "));
		Serial.println(F("Open Write"));
	#endif 

	//close file if open (maybe reopen)
	if(this->m_file)
	{
		#if WSFFileDB_Debug == 1
			Serial.print(F("DB ("));
			Serial.print(this->m_szFile);
			Serial.print(F("): "));
			Serial.println(F("close before open"));
		#endif 
	
		this->m_file.flush();
		this->m_file.close();
	};


	this->m_file = this->m_fs->open(this->m_szFile, "r+");

	if(this->m_file)
	{
		return true;
	};

	return false;
};
    

/*
 * The database file starts with a file header, which contains the following data:
 * 
 * uint32_t       4       Record Count
 * uint32_t       4       next free insert position
 * uint32_t       4       size of the table (had probs with size()) 
 * uint32_t       4       field count
 * uint32_t		  4		  highest pos written (to speed things up)
 */
bool CWSFFileDB::readHeader()
{
	//variables
	///////////
	long 		lSize;
	byte 		bData[sizeof(uint32_t) + 1];
	uint32_t  	dwFieldCount = 0;
	
	this->m_dwRecordCount = 0;
	this->m_dwNextFreePos = 0;  
	this->m_dwTableSize = 0; 
	this->m_dwMaxPosWritten = 0;
  
	if(this->m_file)
	{
		lSize = this->m_file.size();

		if(lSize >= WSFFileDB_HeaderSize)
		{
			this->m_file.seek(0);

			this->m_file.read((byte*)&bData, sizeof(uint32_t));
			memcpy(&this->m_dwRecordCount, bData, sizeof(uint32_t));

			this->m_file.read((byte*)&bData, sizeof(uint32_t));
			memcpy(&this->m_dwNextFreePos, bData, sizeof(uint32_t));

			this->m_file.read((byte*)&bData, sizeof(uint32_t));
			memcpy(&this->m_dwTableSize, bData, sizeof(uint32_t));

			this->m_file.read((byte*)&bData, sizeof(uint32_t));
			memcpy(&dwFieldCount, bData, sizeof(uint32_t));
			
			this->m_file.read((byte*)&bData, sizeof(uint32_t));
			memcpy(&this->m_dwMaxPosWritten, bData, sizeof(uint32_t));
			
			
			#if WSFFileDB_Debug == 1
				Serial.print(F("DB ("));
				Serial.print(this->m_szFile);
				Serial.print(F("): "));
				Serial.print(F("readHeader: FileSize: "));
				Serial.print(lSize);
				Serial.print(F(" field count: "));
				Serial.print(dwFieldCount);

				Serial.print(F(" RecCnt: "));
				Serial.print(this->m_dwRecordCount);

				Serial.print(F(" TabSize: "));
				Serial.print(this->m_dwTableSize);

				Serial.print(F(" NextFree: "));
				Serial.print(this->m_dwNextFreePos);   

				Serial.print(F(" - file: "));
				Serial.println(this->m_szFile); 
			#endif
			
			//the size must be corrected, maybe the header is wrong, which normally indicates a db error
			if(lSize != this->m_dwTableSize)
			{
				#if WSFFileDB_Debug == 1
					Serial.print(F("DB ("));
					Serial.print(this->m_szFile);
					Serial.print(F("): "));
					Serial.print(F("Error size, set filesize to: "));
					Serial.println(lSize);
				#endif
				
				this->m_dwTableSize = lSize;
			};

			//check if fieldcount in header matches requested count, maybe
			//the table layout was changed or the file is corrupt - indicate an error...
			if(dwFieldCount != this->m_nFieldCount)
			{
				#if WSFFileDB_Debug == 1
					Serial.print(F("DB ("));
					Serial.print(this->m_szFile);
					Serial.println(F("): ERROR Field count doesnt match!"));
				#endif

				this->m_nFieldCount = 0;

				return false;
			};

			return true;
		}
		else
		{
			#if WSFFileDB_Debug == 1
				Serial.print(F("DB ("));
				Serial.print(this->m_szFile);
				Serial.print(F("): "));
				Serial.println(F("Header/Size failure"));
			#endif 

			return false;
		};
	}
	else
	{
		#if WSFFileDB_Debug == 1
			Serial.print(F("DB ("));
			Serial.print(this->m_szFile);
			Serial.print(F("): "));
			Serial.println(F("failed to open file for reading"));
		#endif
	};

	return false;
};


bool CWSFFileDB::writeHeader()
{
	//variables
	///////////
	byte 	 bData[sizeof(uint32_t) + 1];  
	uint32_t dwFieldCount = this->m_nFieldCount;
	

	#if WSFFileDB_Debug == 1
		Serial.print(F("DB ("));
		Serial.print(this->m_szFile);
		Serial.print(F("): "));
		Serial.println(F("write Header"));
	#endif 

	if(this->m_file)
	{
		this->m_file.seek(0);

		memcpy(bData, (uint32_t*)&this->m_dwRecordCount, sizeof(uint32_t));
		this->m_file.write(bData, sizeof(uint32_t));

		memcpy(bData, (uint32_t*)&this->m_dwNextFreePos, sizeof(uint32_t));
		this->m_file.write(bData, sizeof(uint32_t));

		memcpy(bData, (uint32_t*)&this->m_dwTableSize, sizeof(uint32_t));
		this->m_file.write(bData, sizeof(uint32_t));
		
		memcpy(bData, (uint32_t*)&dwFieldCount, sizeof(uint32_t));
		this->m_file.write(bData, sizeof(uint32_t));
		
		memcpy(bData, (uint32_t*)&this->m_dwMaxPosWritten, sizeof(uint32_t));
		this->m_file.write(bData, sizeof(uint32_t));
		
		this->m_file.flush();

		#if WSFFileDB_Debug == 1
			Serial.print(F("DB ("));
			Serial.print(this->m_szFile);
			Serial.print(F("): "));
			Serial.print(F("wrote header: RecCnt: "));
			Serial.print(this->m_dwRecordCount);
			Serial.print(F(" - next free: "));
			Serial.print(this->m_dwNextFreePos);
			Serial.print(F(" allocated TableSize; "));
			Serial.print(this->m_dwTableSize);
			Serial.print(F(" - file: "));
			Serial.print(this->m_szFile);
			Serial.print(F(" fsize: "));
			Serial.println(this->m_file.size());
		#endif

		return true;
	}
	else
	{
		#if WSFFileDB_Debug == 1
			Serial.print(F("DB ("));
			Serial.print(this->m_szFile);
			Serial.print(F("): "));
			Serial.print(F("failed to write header - file: "));
			Serial.println(this->m_szFile); 
		#endif
	};   

	return false;
};


int  CWSFFileDB::calculateEntrySize()
{

  //variables
  ///////////
  int nSize = sizeof(byte);

  for(int n = 0; n < this->m_nFieldCount; ++n)
  { 
    nSize += this->m_pFields[n];
  };

  return nSize;
};


bool CWSFFileDB::open()
{
	//variaböes
	///////////
	bool 		bRes = false;
	byte 		bData[256];
	uint32_t 	dwEntrySize = this->calculateEntrySize();

	if(this->m_bDbOpen == false)
	{
		memset(bData, 0, sizeof(bData));
		
		#if WSFFileDB_Debug == 1
			Serial.print(F("DB ("));
			Serial.print(this->m_szFile);
			Serial.print(F("): "));
			Serial.print(F("Reading file: "));
			Serial.print(this->m_szFile);
			Serial.print(F(" create: "));
			Serial.println(this->m_bCreateIfNotExist);
		#endif

		if(this->openWrite() == true)
		{
			if(this->readHeader() == true)
			{
				bRes            = true;
				this->m_bDbOpen = true;
			};
		};
    
    
		if((bRes == false) && (this->m_nFieldCount > 0))
		{
			if(this->m_bCreateIfNotExist == true)
			{
				if(this->m_file)
				{
					this->m_file.close();
				};

				#if WSFFileDB_Debug == 1
					Serial.print(F("DB ("));
					Serial.print(this->m_szFile);
					Serial.print(F("): "));
					Serial.println(F("write new file / header"));
				#endif

				//create an empty file
				this->m_file 		    = this->m_fs->open(this->m_szFile, "w");
				
				if(this->m_file)
				{
					//prepare an empty file
					if(this->m_dwReserveSpace > 0)
					{
						#if WSFFileDB_Debug == 1
							Serial.print(F("DB ("));
							Serial.print(this->m_szFile);
							Serial.print(F("): "));
							Serial.print(F("reserve space: "));
							Serial.println(this->m_dwReserveSpace * dwEntrySize);
						#endif


						//create empty space (actually reserve it on the filesystem), since SPIFFS
						//can't handle multiple growing files...
						for(uint32_t n = 0; n < (this->m_dwReserveSpace * dwEntrySize); n += sizeof(bData))
						{
							if(this->m_file.write((byte*)&bData, sizeof(bData)) <= 0)
							{
								#if WSFFileDB_Debug == 1
									Serial.print(F("DB ("));
									Serial.print(this->m_szFile);
									Serial.print(F("): "));
									Serial.print(F("failed to reserve space at: "));
									Serial.println(n);
								#endif
								
								break;
							};
						};
						
						this->m_file.flush();
					}
					else
					{
						this->m_file.write((byte*)&bData, WSFFileDB_HeaderSize + 1);
						this->m_file.flush();
					};

				
					this->m_bDbOpen       	= true;
					this->m_dwRecordCount 	= 0;
					this->m_dwNextFreePos 	= WSFFileDB_HeaderSize;
					this->m_dwTableSize   	= this->m_file.size();
					this->m_dwLastInsertPos = 0;
					this->m_dwMaxPosWritten = 0;

					bRes = this->writeHeader();

					this->m_file.close();

					//reopen...
					bRes = this->openWrite();
				}
				else
				{
					#if WSFFileDB_Debug == 1
						Serial.print(F("DB ("));
						Serial.print(this->m_szFile);
						Serial.print(F("): "));
						Serial.println(F("unable to create file!"));
					#endif

					bRes = false;
				};
			};
		};

		return bRes;
	}
	else
	{
		#if WSFFileDB_Debug == 1
			Serial.print(F("DB ("));
			Serial.print(this->m_szFile);
			Serial.print(F("): "));
			Serial.println(F("already open"));
		#endif

		return this->m_bDbOpen;
	};
};


bool CWSFFileDB::check()
{
	//variables
	///////////
	uint32_t dwPos 			= WSFFileDB_HeaderSize;
	uint32_t dwPosMax 		= WSFFileDB_HeaderSize;
	int  	 nEntrySize 	= this->calculateEntrySize();
	int		 nByte;
	uint32_t dwFound 	  	= 0;
	bool	 bRes 			= true;
	
	if(this->m_file)
	{		
		while(dwPos < this->m_dwTableSize)
		{
			nByte = this->readByteFromDataFile(dwPos);
			
			if(nByte == 1)
			{
				dwFound  += 1;
				dwPosMax  = dwPos;
				
				#if WSFFileDB_Debug == 1
					Serial.print(F("DB ("));
					Serial.print(this->m_szFile);
					Serial.print(F("): check() found entry at: "));
					Serial.print(dwPos);
					Serial.print(F(", count: "));
					Serial.println(dwFound);
				#endif
			};
			
			dwPos += nEntrySize;
		};
		
		if((dwFound != this->m_dwRecordCount) || (this->m_dwMaxPosWritten != dwPosMax))
		{
			#if WSFFileDB_Debug == 1
				Serial.print(F("DB ("));
				Serial.print(this->m_szFile);
				Serial.print(F("): check() expected: "));
				Serial.print(this->m_dwRecordCount);
				Serial.print(F(" but found: "));
				Serial.print(dwFound);
				Serial.print(F(", last: "));
				Serial.println(dwPosMax);
			#endif
			
			this->m_dwMaxPosWritten = dwPosMax;
			this->m_dwRecordCount   = dwFound;
			
			this->writeHeader();
			
			bRes = false;
		};
		
		return bRes;
	};
	
	return false;
};


bool CWSFFileDB::empty()
{
	//variables
	///////////
	uint32_t dwPos 			= WSFFileDB_HeaderSize;
	int  	 nEntrySize 	= this->calculateEntrySize();
	int		 nByte;
	
	if(this->m_file)
	{
		//reset 
		this->m_dwRecordCount 	= 0;
		this->m_dwNextFreePos 	= WSFFileDB_HeaderSize;
		this->m_dwTableSize   	= this->m_file.size();
		this->m_dwLastInsertPos = 0;
		this->m_dwMaxPosWritten = 0;
		
		while(dwPos < this->m_dwTableSize)
		{
			nByte = this->readByteFromDataFile(dwPos);
			
			if((nByte == 1) || (nByte != 0))
			{
				#if WSFFileDB_Debug == 1
					Serial.print(F("DB ("));
					Serial.print(this->m_szFile);
					Serial.print(F("): "));
					Serial.print(F("empty() clear flag at: "));
					Serial.println(dwPos);
				#endif 
				
				this->writeByteToDataFile(dwPos, 0);
			};
			
			dwPos += nEntrySize;
		};
		
		this->writeHeader();
		this->openWrite();
	};
	
	
	return false;
};


bool CWSFFileDB::isOpen()
{
	return this->m_bDbOpen;
};



uint32_t CWSFFileDB::getLastInsertPos()
{
	return this->m_dwLastInsertPos;
};


void CWSFFileDB::enterCriticalSection()
{
	xSemaphoreTake(this->m_mutex, portMAX_DELAY);
};


void CWSFFileDB::leaveCriticalSection()
{
	xSemaphoreGive(this->m_mutex);
};


int  CWSFFileDB::readFromDataFile(uint32_t dwPos, byte *pData, int nLen)
{
	//variables
	///////////
	int nRes = -1;
		
	if(this->m_file.seek(dwPos) == 1)
	{
		nRes = this->m_file.read(pData, nLen);
	};
	
	return nRes;
};


byte CWSFFileDB::readByteFromDataFile(uint32_t dwPos)
{
	//variables
	///////////
	byte bRes;
		
	if(this->m_file.seek(dwPos) == 1)
	{
		bRes = this->m_file.read();
	}
	else 
	{
		bRes = 0xFF;
	};
	
	return bRes;
};


bool CWSFFileDB::removeEntry(uint32_t dwPos)
{
	//variables
	///////////
	bool bRes = false;

	
	xSemaphoreTake(this->m_mutex, portMAX_DELAY);
	
	#if WSFFileDB_Debug == 1
		Serial.print(F("DB ("));
		Serial.print(this->m_szFile);
		Serial.print(F("): "));
		Serial.print(F("removeEntry() clear flag at: "));
		Serial.println(dwPos);
	#endif 
	
	
	if(this->m_file.seek(dwPos) == 1)
	{
		if(this->m_file.read() == 1)
		{
			this->m_file.seek(dwPos);
			this->m_file.write(0);
			this->m_file.flush();
			
			if(this->m_dwRecordCount > 0)
			{
				this->m_dwRecordCount -= 1;
			};
			
			if(this->m_dwNextFreePos > dwPos)
			{
				this->m_dwNextFreePos = dwPos;
			};
			
			bRes = this->writeHeader();
		}
		else
		{
			#if WSFFileDB_Debug == 1
				Serial.print(F("DB ("));
				Serial.print(this->m_szFile);
				Serial.print(F("): "));
				Serial.print(F("removeEntry() failed to clear flag at: "));
				Serial.println(dwPos);
			#endif 
		};		
	}
	else
	{
		#if WSFFileDB_Debug == 1
			Serial.print(F("DB ("));
			Serial.print(this->m_szFile);
			Serial.print(F("): "));
			Serial.print(F("removeEntry() failed to clear flag at: "));
			Serial.println(dwPos);
		#endif 
	};
	
	xSemaphoreGive(this->m_mutex);
	
	return bRes;
};


bool CWSFFileDB::writeByteToDataFile(uint32_t dwPos, byte bData)
{
	//variables
	///////////
	byte data[2];
	data[0] = bData;
	
	return this->writeToDataFile(dwPos, (byte*)&data, 1);
};



bool CWSFFileDB::writeToDataFile(uint32_t dwPos, byte *pData, int nLen)
{
	//variables
	///////////
	bool bRes = false;
		
	if(this->m_file.seek(dwPos) == 1)
	{
		if(this->m_file.write(pData, nLen) > 0)
		{
			this->m_file.flush();
			
			bRes = true;
		};
	};
	
	return bRes;
};



uint32_t CWSFFileDB::getMaxPosWritten()
{
	return this->m_dwMaxPosWritten;
};

bool CWSFFileDB::insertData(void **pData)
{
	//variables
	///////////
	int      nPos = 0;
	uint32_t dwSeekPos = WSFFileDB_HeaderSize;
	byte 	 bData[this->m_nEntrySize + 2];
	byte     bFree;

	if(this->m_bDbOpen == true)
	{
		if(this->m_file)
		{
			memset((byte*)&bData, 0, sizeof(bData));

			bData[nPos++] = 1; //set entry in use

			//copy data
			for(int n = 0; n < this->m_nFieldCount; ++n)
			{
				memcpy((byte*)&bData + nPos, (byte*)pData[n], this->m_pFields[n]); 
				nPos += this->m_pFields[n];
			};


			xSemaphoreTake(this->m_mutex, portMAX_DELAY);
			
			
			if(this->m_file.seek(this->m_dwNextFreePos) == 1)
			{
				#if WSFFileDB_Debug == 1
					Serial.print(F("DB ("));
					Serial.print(this->m_szFile);
					Serial.print(F("): "));
					Serial.print(F("insertData() data size: "));
					Serial.print(nPos);
					Serial.print(F(" table size before insert: "));
					Serial.print(this->m_dwTableSize);
					Serial.print(F(" insert at pos: "));
					Serial.println(this->m_dwNextFreePos);
				#endif
				
				if(this->m_file.write((byte*)&bData, nPos) > 0)
				{
					//update highest written pos
					if(this->m_dwNextFreePos > this->m_dwMaxPosWritten)
					{
						this->m_dwMaxPosWritten = this->m_dwNextFreePos;
					};

					//increment rec count
					this->m_dwRecordCount   += 1;
					this->m_dwLastInsertPos  = this->m_dwNextFreePos;
					
					//check if next insert > size
					if((this->m_dwNextFreePos + this->m_nEntrySize) >= this->m_dwTableSize) 
					{
						#if WSFFileDB_Debug == 1
							Serial.print(F("DB ("));
							Serial.print(this->m_szFile);
							Serial.print(F("): "));
							Serial.println(F("insertData() at EOF"));
						#endif
						
						this->m_dwTableSize += this->m_nEntrySize + 1;
						
						//append an unused indicator when writing at the end of the 
						//file
						this->m_file.write(0); 
					};
					
					this->m_file.flush();
				

					//try to find next insert pos by itterating 
					//through the whole file...    
					dwSeekPos = WSFFileDB_HeaderSize;

					//search for next unused block from the beginning
					//file.size() is unreliable...
					do 
					{
						if(this->m_file.seek(dwSeekPos) == 1)
						{
							bFree 		= this->m_file.read();
							
							#if WSFFileDB_Debug == 1
								Serial.print(F("DB ("));
								Serial.print(this->m_szFile);
								Serial.print(F("): "));
								Serial.print(F("insertData() search at "));
								Serial.print(dwSeekPos);
								Serial.print(F(": in use: "));
								Serial.println(bFree);
							#endif  

							if((bFree != 1) && (bFree == 0))
							{
								break;
							}
							else
							{
								if((bFree != 1) && (bFree != 0))
								{
									//set unused, when something else is read
									this->m_file.write(0); 
									this->m_file.flush();
									
									break;
								};
							};
							
							dwSeekPos  += this->m_nEntrySize;
						}
						else
						{
							#if WSFFileDB_Debug == 1
								Serial.print(F("DB ("));
								Serial.print(this->m_szFile);
								Serial.print(F("): "));
								Serial.print(F("insertData() EOF: failed to seek to: "));
								Serial.println(dwSeekPos);
							#endif 
													
							break;
						};
					} 
					while((bFree == 1) && (dwSeekPos < this->m_dwTableSize));
					
				  
					#if WSFFileDB_Debug == 1
						Serial.print(F("DB ("));
						Serial.print(this->m_szFile);
						Serial.print(F("): "));
						Serial.print(F("insertData() set new insert pos to: "));
						Serial.print(dwSeekPos);
						Serial.print(F(" new table size: "));
						Serial.println(this->m_dwTableSize);
					#endif  

					this->m_dwNextFreePos = dwSeekPos;
				}
				else
				{
					#if WSFFileDB_Debug == 1
						Serial.print(F("DB ("));
						Serial.print(this->m_szFile);
						Serial.print(F("): "));
						Serial.print(F("insertData() error write to: "));
						Serial.print(this->m_dwNextFreePos);
						Serial.print(F(", table size: "));
						Serial.println(this->m_dwTableSize);
					#endif
					
					xSemaphoreGive(this->m_mutex);
					
					return false;
				};	
			}
			else
			{
				#if WSFFileDB_Debug == 1
					Serial.print(F("DB ("));
					Serial.print(this->m_szFile);
					Serial.print(F("): "));
					Serial.print(F("insertData() error seek to: "));
					Serial.print(this->m_dwNextFreePos);
					Serial.print(F(", table size: "));
					Serial.println(this->m_dwTableSize);
				#endif
				
				xSemaphoreGive(this->m_mutex);
				
				return false;
			};
			
			xSemaphoreGive(this->m_mutex);
			

			return this->writeHeader();
		};
	};

	return false;
};
