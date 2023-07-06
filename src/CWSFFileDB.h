/*
  CWSFFileDB.h - Library for storing data on a sdcard with a OleDb-like interface.

  Provided as it is, free for non-commercial projects, for commercial use contact: cwallukat (a) gmx.at...
  It's not allowed to use this library in eastern states, which are involded in ruzzias unprovoked war against Ukraine (belarus, china, hungary (Yes, Orcban is an asshole!), iran,...!
  Slava Ukraini!
  
  Copyright (c) 2023 Christian Wallukat.  All right reserved.
*/
#ifndef __CWSFFileDB__
#define __CWSFFileDB__


//includes
//////////
#include <Arduino.h>
#include <mutex>
#include <FS.h>


//defines
/////////
#define WSFFileDB_Debug 1 //set to 0 if debugging is not used
#define WSFFileDB_HeaderSize (sizeof(uint32_t) * 3)


/**
 * this class handles the database file. It's not threadsafe!
 * You can open multiple recordsets at the same time...
 */
class CWSFFileDB
{
  public:

    /**
     * Constructor: Creates/Opens a database file...
     * Parameters:
     *  @fs                 Filesystem (SD or SPIFFS)
     *  @szFileName         Name of the database file
     *  @pFields            Array of field lengths (best practise: use sizeof() for non byte variables)
     *  @nFieldCount        Number of entries in pFields
     *  @bCreateIfNotExist  Set to true, to create the database if the file does not exist
     */
    CWSFFileDB(fs::FS *fs, char *szFileName, int *pFields, int nFieldCount, bool bCreateIfNotExist);
    ~CWSFFileDB();

    /**
     * this method opens the database (-file), it returns true, when OK
     */
    bool open();

    /**
     * this method returns true, when OK
     */
    bool isOpen();

    /**
     * This method iserts an entry to the database. The entry must contain data for all fields. It uses the order 
     * provided in the constructor...
     * 
     * this method returns true, when OK
     */
    bool insertData(void **pData);


	/**
	 * This method creates a row in the table without data (everything is set to 0x0). The return value is a position
	 * of the entry and will be returned as valid recordset. You can use the Recordset-Class to individually set each
	 * position in the entry by calling set data.
	 *
	 * This function is used, to save memory, when the tables or values have a huge amount of memory allocated...
	 */
	uint32_t insertEmptyDataRow();


	//returns the position of the last insert
	//this value can be used to open the recordset
	uint32_t getLastInsertPos();

    /**
     * returns the number of records in the file...
     */
    uint32_t getRecordCount();


	/**
	 * This method returns the amount of memory needed to store an entry into memory
	 */
	int getEntrySize();
    

    
    friend class CWSFFileDBRecordset;
    
    
  private:
  
    //variables
    ///////////
    File      			m_file;
    fs::FS   			*m_fs;
    char      			m_szFile[33];
    int      			*m_pFields;
    int       			m_nFieldCount;
    uint32_t  			m_dwRecordCount;
    uint32_t  			m_dwNextFreePos;
    uint32_t  			m_dwTableSize;
	uint32_t  			m_dwLastInsertPos;
    bool      			m_bCreateIfNotExist;  
    int       			m_nEntrySize;  
    bool      			m_bDbOpen;   
	SemaphoreHandle_t	m_mutex;
     

    bool openWrite();
    
    bool readHeader();
    bool writeHeader();

    int  calculateEntrySize();
	
	void writeByteToDataFile(uint32_t dwPos, byte bData);
	void writeToDataFile(uint32_t dwPos, byte *pData, int nLen);
	byte readByteFromDataFile(uint32_t dwPos);
	int  readFromDataFile(uint32_t dwPos, byte *pData, int nLen);
	
	bool removeEntry(uint32_t dwPos);
};





//this class implements an oledb style interface
//it's like a select *
//
//as in real live, to get the record count, you need to call moveLast first.... :D
class CWSFFileDBRecordset
{
  public:
  
    CWSFFileDBRecordset(CWSFFileDB *pDB);
    CWSFFileDBRecordset(CWSFFileDB *pDB, uint32_t dwPos);
    ~CWSFFileDBRecordset();
    
    bool    moveNext();
    bool    moveFirst();

	bool     getData(int nFieldIndex, void *pData, int nMaxSize, bool bInternal);
    bool     getData(int nFieldIndex, void *pData, int nMaxSize);

    //update
    bool    setData(int nFieldIndex, void *pData, int nLength);

    //remove entry
    bool    remove();


    uint32_t getRecordPos();


	//like rs.EOF
    bool    haveValidEntry();
    

  private:
	
	
    
	
    //variables
    ///////////
    CWSFFileDB 		*m_pDB;
    uint32_t    	m_dwPos;
    bool       		m_bHaveValidEntry;	
};


#endif
