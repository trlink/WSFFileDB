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
  
  
  if(this->m_file)
  {
    lSize = this->m_file.size();
    this->m_file.seek(0);
  
    if(lSize >= WSFFileDB_HeaderSize)
    {
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
        Serial.print(F("DB: Open: FileSize: "));
        Serial.println(lSize);
        
        Serial.print(F("RecCnt: "));
        Serial.println(this->m_dwRecordCount);

        Serial.print(F("TabSize: "));
        Serial.println(this->m_dwTableSize);
    
        Serial.print(F("NextFree: "));
        Serial.print(this->m_dwNextFreePos);   

        Serial.print(F(" - file: "));
        Serial.println(this->m_szFile); 
      #endif
      
      return true;
    }
    else
    {
      #if WSFFileDB_Debug == 1
        Serial.println(F("DB: Header/Size failure"));
      #endif 
      
      return false;
    };
  }
  else
  {
    #if WSFFileDB_Debug == 1
      Serial.println(F("DB: failed to open file for reading"));
    #endif
  };
  
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
        this->m_fs->open(this->m_szFile, "w");
        
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



bool CWSFFileDB::insertData(void **pData)
{
  //variables
  ///////////
  int      nPos = 1;
  uint32_t dwSeekPos = WSFFileDB_HeaderSize;
  uint32_t dwFileSize;
  byte     bFree;

  if(this->m_bDbOpen == true)
  {
    if(this->m_file)
    {
	  this->m_dwLastInsertPos = this->m_dwNextFreePos;
      this->m_file.seek(this->m_dwNextFreePos);
      this->m_file.write(1);  //set entry in use
    
      for(int n = 0; n < this->m_nFieldCount; ++n)
      {
        this->m_file.write((byte*)pData[n], this->m_pFields[n]);
        nPos += this->m_pFields[n];
      };

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
      this->m_dwTableSize     += nPos;
      
      dwFileSize = this->m_file.size();
  
      #if WSFFileDB_Debug == 1
        Serial.print(F("DB: insertData() file size after write: "));
        Serial.println(dwFileSize); 
      #endif 
  
      if((this->m_dwNextFreePos + nPos) == this->m_dwTableSize)
      {
        #if WSFFileDB_Debug == 1
          Serial.print(F("DB: insertData() insert at end, new size: "));
          Serial.println(this->m_dwTableSize); 
        #endif 
  
        dwSeekPos = this->m_dwTableSize;  
      }
      else
      {
        //try to find next insert pos by itterating 
        //through the whole file...    
        dwSeekPos = WSFFileDB_HeaderSize;
  
        //search for next unused block
        do 
        {
          this->m_file.seek(dwSeekPos);
          bFree = this->m_file.read();
    
          #if WSFFileDB_Debug == 1
            Serial.print(F("DB: insertDat() search: seek to: "));
            Serial.print(dwSeekPos);
            Serial.print(F(" in use: "));
            Serial.println(bFree);
          #endif  
    
          if(bFree == 0)
          {
            break;
          }
          else
          {
            dwSeekPos += this->m_nEntrySize;
      
            if(dwSeekPos >= this->m_dwTableSize)
            {
              dwSeekPos = this->m_dwTableSize;
              break;
            };
          };
        } 
        while(bFree == 1);
      };
        
      
      #if WSFFileDB_Debug == 1
        Serial.print(F("DB: insertData() set new insert pos to: "));
        Serial.println(dwSeekPos);
      #endif  
  
      this->m_dwNextFreePos = dwSeekPos;
      
      return this->writeHeader();
    };
  };
  
  return false;
};
