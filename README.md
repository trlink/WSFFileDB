# WSFFileDB
Arduino library to store data on a sdcard with an OleDb-like interface. The library is thread-safe.


To create a database you need to provide a table definition:

`
int nFields[] = {sizeof(uint32_t), sizeof(uint32_t), 25};
`

This definition allows 3 values, with 2 uint32_t and 25 bytes additional storage for other data (byte, char, ...).
Now you can create an instance of the database class:

`
CWSFFileDB db((fs::SDFS*)&SD, "/data.tbl", (int*)&nFields, 3, true, 25);
`

The data-file will be created and you can now open an recordset object to read or manipulate data:

`
//create the recordset

CWSFFileDBRecordset rs(&db);
`

The CWSFFileRecordSet-class provides methods to navigate through the data (moveNext(), moveFirst()) or to change/delete data...


Check the sample for more information...



