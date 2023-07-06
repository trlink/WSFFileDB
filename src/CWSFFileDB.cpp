//includes
//////////
#include "CWSFFileDB.h"








CWSFFileDB::CWSFFileDB(fs::FS *fs, char *szFileName, int *pFields, int nFieldCount, bool bCreateIfNotExist)
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
    Serial.println(F("DB: Open Write"));
  #endif 
  
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
 * 
 */
bool CWSFFileDB::readHeader()
{
  //variables
  ///////////
  long lSize;
  byte bData[sizeof(uint32_t) + 1];
  
  xSemaphoreTake(this->m_mutex, portMAX_DELAY);
  
  if(this->m_file)
  {
    lSize = this->m_file.size();
    
    if(lSize >= WSFFileDB_HeaderSize)
    {
	  this->m_file.seek(0);
	  
      this->m_dwRecordCount = 0;
      this->m_dwNextFreePos = 0;  
      this->m_dwTableSize = 0; 
      
      this->m_file.read((byte*)&bData, sizeof(uint32_t));
      memcpy(&this->m_dwRecordCount, bData, sizeof(uint32_t));
  
      this->m_file.read((byte*)&bData, sizeof(uint32_t));
      memcpy(&this->m_dwNextFreePos, bData, sizeof(uint32_t));

      this->m_file.read((byte*)&bData, sizeof(uint32_t));
      memcpy(&this->m_dwTableSize, bData, sizeof(uint32_t));
  
      #if WSFFileDB_Debug == 1
        Serial.print(F("DB: readHeader: FileSize: "));
        Serial.print(lSize);
        
        Serial.print(F(" RecCnt: "));
        Serial.print(this->m_dwRecordCount);

        Serial.print(F(" TabSize: "));
        Serial.print(this->m_dwTableSize);
    
        Serial.print(F(" NextFree: "));
        Serial.print(this->m_dwNextFreePos);   

        Serial.print(F(" - file: "));
        Serial.println(this->m_szFile); 
      #endif
	  
	  xSemaphoreGive(this->m_mutex);
      
      return true;
    }
    else
    {
      #if WSFFileDB_Debug == 1
        Serial.println(F("DB: Header/Size failure"));
      #endif 
	  
	  xSemaphoreGive(this->m_mutex);
      
      return false;
    };
  }
  else
  {
    #if WSFFileDB_Debug == 1
      Serial.println(F("DB: failed to open file for reading"));
    #endif
  };
  
  xSemaphoreGive(this->m_mutex);
  
  return false;
};


bool CWSFFileDB::writeHeader()
{
	//variables
	///////////
	byte bData[sizeof(uint32_t) + 1];  

	#if WSFFileDB_Debug == 1
		Serial.println(F("DB: write Header"));
	#endif 

	if(this->m_file)
	{
		xSemaphoreTake(this->m_mutex, portMAX_DELAY);
		
		this->m_file.seek(0);

		memcpy(bData, (uint32_t*)&this->m_dwRecordCount, sizeof(uint32_t));
		this->m_file.write(bData, sizeof(uint32_t));

		memcpy(bData, (uint32_t*)&this->m_dwNextFreePos, sizeof(uint32_t));
		this->m_file.write(bData, sizeof(uint32_t));

		memcpy(bData, (uint32_t*)&this->m_dwTableSize, sizeof(uint32_t));
		this->m_file.write(bData, sizeof(uint32_t));
		this->m_file.flush();

		#if WSFFileDB_Debug == 1
			Serial.print(F("DB: wrote header: RecCnt: "));
			Serial.print(this->m_dwRecordCount);
			Serial.print(F(" - next free: "));
			Serial.print(this->m_dwNextFreePos);
			Serial.print(F(" allocated TableSize; "));
			Serial.print(this->m_dwTableSize);
			Serial.print(F(" - file: "));
			Serial.println(this->m_szFile); 
		#endif
		
		xSemaphoreGive(this->m_mutex);

		return true;
	}
	else
	{
		#if WSFFileDB_Debug == 1
			Serial.print(F("DB: failed to write header - file: "));
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
  bool bRes = false;
  
  
  if(this->m_bDbOpen == false)
  {
    #if WSFFileDB_Debug == 1
      Serial.print(F("DB: Reading file: "));
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
    
    
    if(bRes == false)
    {
      if(this->m_bCreateIfNotExist == true)
      {
        #if WSFFileDB_Debug == 1
          Serial.println(F("DB: write new file / header"));
        #endif

        if(this->m_file)
        {
          this->m_file.close();
        };

        //create an empty file
        this->m_file 		  = this->m_fs->open(this->m_szFile, "w");
        
        this->m_bDbOpen       = true;
        this->m_dwRecordCount = 0;
        this->m_dwNextFreePos = WSFFileDB_HeaderSize;
        this->m_dwTableSize   = WSFFileDB_HeaderSize;
  
        bRes = this->writeHeader();

        this->m_file.close();

        bRes = this->openWrite();
      };
    };
    
    return bRes;
  }
  else
  {
    #if WSFFileDB_Debug == 1
      Serial.println(F("DB: already open"));
    #endif
    
    return this->m_bDbOpen;
  };
};


bool CWSFFileDB::isOpen()
{
  return this->m_bDbOpen;
};



uint32_t CWSFFileDB::getLastInsertPos()
{
  return this->m_dwLastInsertPos;
};


int  CWSFFileDB::readFromDataFile(uint32_t dwPos, byte *pData, int nLen)
{
	//variables
	///////////
	int nRes;
	
	xSemaphoreTake(this->m_mutex, portMAX_DELAY);
	
	this->m_file.seek(dwPos);
	nRes = this->m_file.read(pData, nLen);
	
	xSemaphoreGive(this->m_mutex);
	
	return nRes;
};


byte CWSFFileDB::readByteFromDataFile(uint32_t dwPos)
{
	//variables
	///////////
	byte bRes;
	
	xSemaphoreTake(this->m_mutex, portMAX_DELAY);
	
	this->m_file.seek(dwPos);
	bRes = this->m_file.read();
	
	xSemaphoreGive(this->m_mutex);
	
	return bRes;
};


bool CWSFFileDB::removeEntry(uint32_t dwPos)
{
	//variables
	///////////
	byte bData[2];
	bool bRes = false;
	
	bData[0] = 0;
	
	xSemaphoreTake(this->m_mutex, portMAX_DELAY);
	
	#if WSFFileDB_Debug == 1
		Serial.print(F("DB: removeEntry() clear flag at: "));
		Serial.println(dwPos);
	#endif 
	
	
	this->m_file.seek(dwPos);
	
	if(this->m_file.read() == 1)
	{
		this->m_file.seek(dwPos);
		this->m_file.write((byte*)&bData, 1);
		this->m_file.flush();
		
		this->m_dwRecordCount -= 1;
		
		if(this->m_dwNextFreePos > dwPos)
		{
			this->m_dwNextFreePos = dwPos;
		};
		
		bRes = true;
	}
	else
	{
		#if WSFFileDB_Debug == 1
			Serial.print(F("DB: removeEntry() failed to clear flag at: "));
			Serial.println(dwPos);
		#endif 
	};	
	
	xSemaphoreGive(this->m_mutex);
	
	return bRes;
};


void CWSFFileDB::writeByteToDataFile(uint32_t dwPos, byte bData)
{
	//variables
	///////////
	byte data[2];
	
	data[0] = bData;
	
	this->writeToDataFile(dwPos, (byte*)&data, 1);
};



void CWSFFileDB::writeToDataFile(uint32_t dwPos, byte *pData, int nLen)
{
	xSemaphoreTake(this->m_mutex, portMAX_DELAY);
	
	this->m_file.seek(dwPos);
	this->m_file.write(pData, nLen);
	this->m_file.flush();
		
	xSemaphoreGive(this->m_mutex);
};



bool CWSFFileDB::insertData(void **pData)
{
  //variables
  ///////////
  int      nPos = 0;
  uint32_t dwSeekPos = WSFFileDB_HeaderSize;
  byte 	   bData[this->m_nEntrySize + 2];
  byte     bFree;

  if(this->m_bDbOpen == true)
  {
    if(this->m_file)
    {
		xSemaphoreTake(this->m_mutex, portMAX_DELAY);
		
		memset((byte*)&bData, 0, sizeof(bData));

		bData[nPos++] = 1; //set entry in use

		//copy data
		for(int n = 0; n < this->m_nFieldCount; ++n)
		{
			memcpy((byte*)&bData + nPos, (byte*)pData[n], this->m_pFields[n]); 
			nPos += this->m_pFields[n];
		};

		this->m_dwLastInsertPos = this->m_dwNextFreePos;

		this->m_file.seek(this->m_dwNextFreePos);
		this->m_file.write((byte*)&bData, nPos);
		this->m_file.flush();

		#if WSFFileDB_Debug == 1
			Serial.print(F("DB: insertData() data size:"));
			Serial.print(nPos);
			Serial.print(F(" table size: "));
			Serial.print(this->m_dwTableSize);
			Serial.print(F(" insert at pos: "));
			Serial.println(this->m_dwNextFreePos);
		#endif

		//increment rec count
		this->m_dwRecordCount   += 1;


		//try to find next insert pos by itterating 
		//through the whole file...    
		dwSeekPos = WSFFileDB_HeaderSize;

		//search for next unused block from the beginning
		//file.size() is unreliable...
		do 
		{
			this->m_file.seek(dwSeekPos);
			bFree = this->m_file.read();

			#if WSFFileDB_Debug == 1
				Serial.print(F("DB: insertData() search: seek to: "));
				Serial.print(dwSeekPos);
				Serial.print(F(" in use: "));
				Serial.println(bFree);
			#endif  

			if(bFree != 1)
			{
				break;
			}
			else
			{
				dwSeekPos += (this->m_nEntrySize + 1);

				if(dwSeekPos >= this->m_dwTableSize)
				{
					this->m_dwTableSize += this->m_nEntrySize + 1;
					break;
				};
			};
		} 
		while(bFree == 1);
        
      
		#if WSFFileDB_Debug == 1
			Serial.print(F("DB: insertData() set new insert pos to: "));
			Serial.print(dwSeekPos);
			Serial.print(F(" new table size: "));
			Serial.println(this->m_dwTableSize);
		#endif  

		this->m_dwNextFreePos = dwSeekPos;

		//correct the file, in case something got weird...
		this->m_file.seek(this->m_dwNextFreePos);
		this->m_file.write(0);  //set entry free
		this->m_file.flush();
		
		xSemaphoreGive(this->m_mutex);
		

		return this->writeHeader();
    };
  };
  
  return false;
};
