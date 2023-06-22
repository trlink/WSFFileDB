//includes
//////////
#include "CWSFFileDB.h"



CWSFFileDBRecordset::CWSFFileDBRecordset(CWSFFileDB *pDB)
{
  this->m_pDB             = pDB;
  this->m_dwPos           = 0;
  this->m_bHaveValidEntry = false;

  if(this->m_pDB->isOpen() == true)
  {
    if(this->m_pDB->m_file)
    {
      this->m_pDB->readHeader();

      this->moveNext();
    };
  };
};


CWSFFileDBRecordset::CWSFFileDBRecordset(CWSFFileDB *pDB, uint32_t dwPos)
{
  //variables
  ///////////
  uint32_t dwFileSize;  

  this->m_pDB             = pDB;
  this->m_dwPos           = 0;
  this->m_bHaveValidEntry = false;

  //check pos
  if(this->m_pDB->isOpen() == true)
  {
    if(this->m_pDB->m_file == true)
    {
      this->m_pDB->readHeader();
      
      dwFileSize = this->m_pDB->m_dwTableSize;

      if((dwPos >= WSFFileDB_HeaderSize) && (dwPos <= (dwFileSize - this->m_pDB->m_nEntrySize)))
      {
        this->m_pDB->m_file.seek(dwPos);
        
        if(this->m_pDB->m_file.read() == 1)
        {
		  #if WSFFileDB_Debug == 1
            Serial.print(F("DB RS: set pos to (on init): "));
            Serial.println(this->m_dwPos);  
          #endif
          
		  this->m_dwPos = dwPos;
          this->m_bHaveValidEntry = true;
        };
      };
    };
  };
};



CWSFFileDBRecordset::~CWSFFileDBRecordset()
{
};


uint32_t CWSFFileDBRecordset::getRecordPos()
{
  if(this->m_bHaveValidEntry == true)
  {
    return this->m_dwPos;
  };

  return 0;
};



bool    CWSFFileDBRecordset::moveFirst()
{
  if(this->m_pDB->isOpen() == true)
  {
    this->m_dwPos = 0;

    return this->moveNext();
  };

  return false;
};


bool    CWSFFileDBRecordset::moveNext()
{
  //variables
  ///////////
  uint32_t dwFileSize;

  this->m_bHaveValidEntry = false;
  
  if(this->m_pDB->isOpen() == true)
  {
    if(this->m_pDB->m_file)
    {
      dwFileSize = this->m_pDB->m_dwTableSize;
      
      do
      {
        if((this->m_dwPos >= WSFFileDB_HeaderSize) || (dwFileSize <= WSFFileDB_HeaderSize))
        {
          if((this->m_dwPos + this->m_pDB->m_nEntrySize) >= dwFileSize)
          {
            #if WSFFileDB_Debug == 1
              Serial.print(F("DB RS: MoveNext reached EOF - size: "));
              Serial.print(dwFileSize);
              Serial.print(F(" pos: "));
              Serial.println(this->m_dwPos);  
            #endif
            
            return false;
          };
  
          this->m_dwPos += this->m_pDB->m_nEntrySize;
        }
        else
        {
          this->m_dwPos = WSFFileDB_HeaderSize;
        };
        
        #if WSFFileDB_Debug == 1
          Serial.print(F("DB RS: Move next to: "));
          Serial.print(this->m_dwPos);
          Serial.print(F(" file size: "));
          Serial.println(dwFileSize);
        #endif

        //read pos, every entry starts with a byte 
        //indicating if the pos is in use (1) or not (0)
        this->m_pDB->m_file.seek(this->m_dwPos);

        if(this->m_pDB->m_file.read() == 1)
        {
	      this->m_bHaveValidEntry = true;

		  return true;		 
        };

        #if WSFFileDB_Debug == 1
          Serial.print(F("DB RS: use-flag not matched at: "));
          Serial.println(this->m_dwPos);
        #endif
      }
      while(true);
    }
    else
    {
      #if WSFFileDB_Debug == 1
        Serial.println(F("DB RS: Failed to open file"));
      #endif
    };
  };

  return false;
};



bool CWSFFileDBRecordset::remove()
{
  if(this->m_bHaveValidEntry == true)
  {
    if(this->m_pDB->isOpen() == true)
    {
      if(this->m_pDB->m_file)
      {    
        this->m_pDB->m_file.seek(this->m_dwPos);
        this->m_pDB->m_file.write(0);
        this->m_pDB->m_file.flush();

        this->m_pDB->m_dwRecordCount -= 1;

        if(this->m_pDB->m_dwNextFreePos == this->m_pDB->m_dwTableSize)
        {
          this->m_pDB->m_dwNextFreePos  = this->m_dwPos;
        };
        
        this->m_pDB->writeHeader();
        
        this->m_bHaveValidEntry = false;
        
        return true;
      };
    };
  };
  
  return false;  
};


bool CWSFFileDBRecordset::haveValidEntry()
{
  return this->m_bHaveValidEntry;
};



bool CWSFFileDBRecordset::setData(int nFieldIndex, void *pData, int nLength)
{
  //variables
  ///////////
  int nStartRead = 1; //first byte use indicator

  if(this->m_bHaveValidEntry == true)
  {
    if((nFieldIndex >= 0) && (nFieldIndex < this->m_pDB->m_nFieldCount))
    {
      for(int n = 0; n < nFieldIndex; ++n)
      {
        nStartRead += this->m_pDB->m_pFields[n];
      };

      if(nLength <= this->m_pDB->m_pFields[nFieldIndex])
      {
        if(this->m_pDB->isOpen() == true)
        {
          if(this->m_pDB->m_file)
          {    
            #if WSFFileDB_Debug == 1
              Serial.print(F("DB RS: setData() - at: "));
              Serial.print(this->m_dwPos);
              Serial.print(F(" pos: "));
              Serial.println(nStartRead);  
            #endif
            
            this->m_pDB->m_file.seek(this->m_dwPos + nStartRead);
            this->m_pDB->m_file.write((byte*)pData, nLength);
            this->m_pDB->m_file.flush();
    
            return true;
          };
        };
      };
    };
  };
  
  return false;
};


bool CWSFFileDBRecordset::getData(int nFieldIndex, void *pData, int nMaxSize)
{
	return this->getData(nFieldIndex, pData, nMaxSize, false);
};



bool CWSFFileDBRecordset::getData(int nFieldIndex, void *pData, int nMaxSize, bool bInternal)
{
  //variables
  ///////////
  int nStartRead = 1; //first byte indicator

  if((this->m_bHaveValidEntry == true) || (bInternal == true))
  {
    if((nFieldIndex >= 0) && (nFieldIndex < this->m_pDB->m_nFieldCount))
    {
      for(int n = 0; n < nFieldIndex; ++n)
      {
        nStartRead += this->m_pDB->m_pFields[n];
      };

      if(nMaxSize >= this->m_pDB->m_pFields[nFieldIndex])
      {
        if(this->m_pDB->isOpen() == true)
        {
          if(this->m_pDB->m_file)
          {    
            this->m_pDB->m_file.seek(this->m_dwPos + nStartRead);
            this->m_pDB->m_file.read((byte*)pData, this->m_pDB->m_pFields[nFieldIndex]);
    
            return true;
          };
        };
      };
    };
  };
  
  return false;
};
