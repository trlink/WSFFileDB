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
		this->moveNext();
	};
};


CWSFFileDBRecordset::CWSFFileDBRecordset(CWSFFileDB *pDB, uint32_t dwPos)
{
	this->m_pDB             = pDB;
	this->m_dwPos           = 0;
	this->m_bHaveValidEntry = false;

	//check pos
	if(this->m_pDB->isOpen() == true)
	{
		if((dwPos >= WSFFileDB_HeaderSize) && (dwPos <= (this->m_pDB->m_dwTableSize - this->m_pDB->m_nEntrySize)))
		{
			this->m_pDB->enterCriticalSection();
			
			if(this->m_pDB->readByteFromDataFile(dwPos) == 1)
			{
				this->m_dwPos = dwPos;
				this->m_bHaveValidEntry = true;
				
				#if WSFFileDB_Debug == 1
					Serial.print(F("DB RS("));
					Serial.print(this->m_pDB->m_szFile);
					Serial.print(F("): "));
					Serial.print(F("set pos to (on init): "));
					Serial.println(this->m_dwPos);  
				#endif
				
				this->m_pDB->leaveCriticalSection();
				
				return;
			};
			
			this->m_pDB->leaveCriticalSection();
		};
	};
	
	#if WSFFileDB_Debug == 1
		Serial.print(F("DB RS("));
		Serial.print(this->m_pDB->m_szFile);
		Serial.print(F("): "));
		Serial.print(F("failed to set pos to (on init): "));
		Serial.println(this->m_dwPos);  
	#endif
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
		this->m_pDB->enterCriticalSection();
		
		dwFileSize = this->m_pDB->m_dwTableSize;

		do
		{
			if((this->m_dwPos >= WSFFileDB_HeaderSize) || (dwFileSize <= WSFFileDB_HeaderSize))
			{
				if((this->m_dwPos + this->m_pDB->m_nEntrySize) >= dwFileSize)
				{
					#if WSFFileDB_Debug == 1
						Serial.print(F("DB RS("));
						Serial.print(this->m_pDB->m_szFile);
						Serial.print(F("): "));
						Serial.print(F("MoveNext reached EOF - size: "));
						Serial.print(dwFileSize);
						Serial.print(F(" pos: "));
						Serial.println(this->m_dwPos);  
					#endif
					
					this->m_pDB->leaveCriticalSection();

					return false;
				};

				this->m_dwPos += this->m_pDB->m_nEntrySize;
				
				if(this->m_dwPos > this->m_pDB->getMaxPosWritten())
				{
					#if WSFFileDB_Debug == 1
						Serial.print(F("DB RS("));
						Serial.print(this->m_pDB->m_szFile);
						Serial.print(F("): "));
						Serial.print(F("MoveNext past last entry - last: "));
						Serial.print(this->m_pDB->getMaxPosWritten());
						Serial.print(F(" pos: "));
						Serial.println(this->m_dwPos);  
					#endif
					
					this->m_pDB->leaveCriticalSection();
					
					return false;
				};
			}
			else
			{
				#if WSFFileDB_Debug == 1
					Serial.print(F("DB RS("));
					Serial.print(this->m_pDB->m_szFile);
					Serial.print(F("): "));
					Serial.print(F("MoveNext set pos to start: "));
					Serial.println(this->m_dwPos);  
				#endif
				
				this->m_dwPos = WSFFileDB_HeaderSize;
			};

			#if WSFFileDB_Debug == 1
				Serial.print(F("DB RS("));
				Serial.print(this->m_pDB->m_szFile);
				Serial.print(F("): "));
				Serial.print(F("Move next to: "));
				Serial.print(this->m_dwPos);
				Serial.print(F(" file size: "));
				Serial.println(dwFileSize);
			#endif

			if(this->m_pDB->readByteFromDataFile(this->m_dwPos) == 1)
			{
				this->m_bHaveValidEntry = true;

				this->m_pDB->leaveCriticalSection();
				
				return true;		 
			}
			else
			{
				#if WSFFileDB_Debug == 1
					Serial.print(F("DB RS("));
					Serial.print(this->m_pDB->m_szFile);
					Serial.print(F("): "));
					Serial.print(F("use-flag not matched at: "));
					Serial.println(this->m_dwPos);
				#endif
			};
		}
		while(true);
		
		this->m_pDB->leaveCriticalSection();
	};
	
	return false;
};



bool CWSFFileDBRecordset::remove()
{
	if(this->m_bHaveValidEntry == true)
	{
		if(this->m_pDB->isOpen() == true)
		{
			this->m_pDB->removeEntry(this->m_dwPos);
			this->m_bHaveValidEntry = false;

			return true;
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
	int nWriteLen = nLength;

	if(this->m_bHaveValidEntry == true)
	{
		if((nFieldIndex >= 0) && (nFieldIndex < this->m_pDB->m_nFieldCount))
		{
			for(int n = 0; n < nFieldIndex; ++n)
			{
				nStartRead += this->m_pDB->m_pFields[n];
			};

			if(nWriteLen > this->m_pDB->m_pFields[nFieldIndex])
			{
				nWriteLen = this->m_pDB->m_pFields[nFieldIndex];
			};


			if(this->m_pDB->isOpen() == true)
			{   
				#if WSFFileDB_Debug == 1
					Serial.print(F("DB RS("));
					Serial.print(this->m_pDB->m_szFile);
					Serial.print(F("): "));
					Serial.print(F("setData() - at: "));
					Serial.print(this->m_dwPos);
					Serial.print(F(" pos: "));
					Serial.println(nStartRead);  
				#endif

				this->m_pDB->enterCriticalSection();
				this->m_pDB->writeByteToDataFile(this->m_dwPos, 1);
				this->m_pDB->writeToDataFile(this->m_dwPos + nStartRead, (byte*)pData, nWriteLen);
				this->m_pDB->leaveCriticalSection();

				return true;
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
	int nWriteLen = nMaxSize;
	
	if((this->m_bHaveValidEntry == true) || (bInternal == true))
	{
		if((nFieldIndex >= 0) && (nFieldIndex < this->m_pDB->m_nFieldCount))
		{
			for(int n = 0; n < nFieldIndex; ++n)
			{
				nStartRead += this->m_pDB->m_pFields[n];
			};

			if(nWriteLen > this->m_pDB->m_pFields[nFieldIndex])
			{
				nWriteLen = this->m_pDB->m_pFields[nFieldIndex];
			};
				
			if(this->m_pDB->isOpen() == true)
			{
				this->m_pDB->enterCriticalSection();
				
				if(this->m_pDB->readFromDataFile(this->m_dwPos + nStartRead, (byte*)pData, nWriteLen) != -1)
				{
					this->m_pDB->leaveCriticalSection();
					
					return true;
				}
				else
				{
					this->m_pDB->leaveCriticalSection();
					
					return false;
				};
			};
		};
	};

	return false;
};
