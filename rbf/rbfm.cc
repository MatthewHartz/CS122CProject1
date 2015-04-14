
#include "rbfm.h"
#include <cstring>

RecordBasedFileManager* RecordBasedFileManager::_rbf_manager = 0;

RecordBasedFileManager* RecordBasedFileManager::instance()
{
	if(!_rbf_manager)
		_rbf_manager = new RecordBasedFileManager();

	return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
}

RecordBasedFileManager::~RecordBasedFileManager()
{
}

RC RecordBasedFileManager::createFile(const string &fileName) {
	return PagedFileManager::instance()->createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const string &fileName) {
	return PagedFileManager::instance()->destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle) {
	return PagedFileManager::instance()->openFile(fileName, fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
	return PagedFileManager::instance()->closeFile(fileHandle);
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
	// No pages exist, append a new file
	if (fileHandle.getNumberOfPages() == 0) {
		void *temp = malloc(PAGE_SIZE);
		memset(temp, NULL, PAGE_SIZE);
		int numSlots = 0;
		int freeSpace = 0;
		memcpy((char*)temp + NUM_ENTRIES, &numSlots, sizeof(int));
		memcpy((char*)temp + FREE_SPACE, &freeSpace, sizeof(int));
		fileHandle.appendPage(temp);
		free(temp);
	}

	int fieldCount = recordDescriptor.size();
	int nullCount = ceil((double)fieldCount / CHAR_BIT);

	// Get data length
	int recordLength = nullCount; // initialize the record's length to compensate for the null chunk

	for (int i = 0; i < fieldCount; i++) {
		switch (recordDescriptor[i].type) {
			case TypeInt:
				recordLength += sizeof(int);
				break;
			case TypeReal:
				recordLength += sizeof(float);
				break;
			case TypeVarChar:
				int size;
				memcpy(&size, (char *)data + recordLength, sizeof(int));
				recordLength += sizeof(int);
				recordLength += size;
				break;
		}
	}
	
	// Get page number
	int pageNumber = fileHandle.getNumberOfPages() - 1;

	// Iterate through pages to find empty slot
	while (pageNumber >= 0) {
		// Read page
		void* pageData = malloc(PAGE_SIZE);
		if (fileHandle.readPage(pageNumber, pageData) != 0) {
			cout << "Error: Unable to read page";
			return -1;
		}

		// Collect the free space offset
		int freeSpaceOffset;
		memcpy(&freeSpaceOffset, (char *)pageData + FREE_SPACE, sizeof(int));

		// loop to collect all the directory entries
		int numRecords;
		memcpy(&numRecords, (char *)pageData + NUM_ENTRIES, sizeof(int));

		// insert record into free space else collapse records and try again
		int directorySpace = 8 + (8 * numRecords); // 8 for the meta data of the directory and 8 for each record
		if (freeSpaceOffset + recordLength < PAGE_SIZE - (directorySpace + 8)) {		
			// update page with record
			memcpy((char*)pageData + freeSpaceOffset, data, recordLength);

			// update num records
			numRecords++;
			memcpy((char *)pageData + NUM_ENTRIES, &numRecords, sizeof(int));

			// insert record offset into directory
			memcpy((char *)pageData + (NUM_ENTRIES - (numRecords * 8)), &freeSpaceOffset, sizeof(int)); // update offset of record
			memcpy((char *)pageData + (NUM_ENTRIES - (numRecords * 8) + 4), &recordLength, sizeof(int)); // update length of record

			// update free space
			freeSpaceOffset += recordLength;
			memcpy((char *)pageData + FREE_SPACE, &freeSpaceOffset, sizeof(int));

					
			// update RID
			rid.pageNum = pageNumber;
			rid.slotNum = numRecords - 1;

			// insert page back into file
			fileHandle.writePage(pageNumber, pageData);

			return 0;
		}
		
		free(pageData);
		pageNumber--;
	}

	// Could not be appending to any pages currently loaded in file, therefore append new page
	void *temp = malloc(PAGE_SIZE);
	memset(temp, NULL, PAGE_SIZE);

	// write record to the front of the page
	memcpy((char*)temp, data, recordLength);

	// write 1 for num records
	int numRecords = 1;
	memcpy((char *)temp + NUM_ENTRIES, &numRecords, sizeof(int));

	// Write new free space location
	int freeSpace = recordLength;
	memcpy((char *)temp + FREE_SPACE, &freeSpace, sizeof(int));

	// write record entry into first position of directory
	int recordPosition = 0;
	memcpy((char *)temp + (NUM_ENTRIES - 8), &recordPosition, sizeof(int)); // update offset of record
	memcpy((char *)temp + (NUM_ENTRIES - 4), &recordLength, sizeof(int)); // update length of record

	fileHandle.appendPage(temp);

	// update RID
	rid.pageNum = fileHandle.getNumberOfPages() - 1;
	rid.slotNum = numRecords - 1;
	free(temp);

	return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
	// Read the correct page from file
	void* pageData = malloc(PAGE_SIZE);
	fileHandle.readPage(rid.pageNum, pageData);

	// Collect the record's offset
	int recordOffset;
	int recordLength;
	memcpy(&recordOffset, (char *)pageData + (NUM_ENTRIES - ((rid.slotNum + 1) * 8)), sizeof(int));
	memcpy(&recordLength, (char *)pageData + (NUM_ENTRIES - ((rid.slotNum + 1) * 8) + 4), sizeof(int));

	memcpy(data, (char*)pageData + recordOffset, recordLength);

	return 0;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
	string buffer = "";
	int fieldCount = recordDescriptor.size();
	int nullCount = ceil((double)fieldCount / CHAR_BIT);
	int offset = 0;

	// TODO: Handle nulls
	char *nullsIndicator = (char *)malloc(nullCount);
	memcpy(nullsIndicator, (char*)data + offset, nullCount);

	offset += nullCount;

	for (int i = 0; i < fieldCount; i++) {
		//buffer += recordDescriptor[i].name;
		switch (recordDescriptor[i].type) {
			case TypeInt:
			{
				buffer += recordDescriptor[i].name + ": ";

				// tests for null
				if (!isBitSet(nullsIndicator[i / 8], i % 8)) {
					int intVal;
					memcpy(&intVal, (char *)data + offset, sizeof(int));
					offset += sizeof(int);

					buffer.append(to_string(intVal));
				}

				buffer += '\t';
			}
			break;
			case TypeReal:
			{
				buffer += recordDescriptor[i].name + ": ";

				// tests for null
				if (!isBitSet(nullsIndicator[i / 8], i % 8)) {
					int realSize = sizeof(float);
					float temp;
					memcpy(&temp, (char *)data + offset, sizeof(float));

					// initializes an array of spaces
					char* realVal = new char[realSize];
					memset(realVal, ' ', realSize);

					// copy value over to array
					sprintf(realVal, "%.1f", temp);

					buffer.append(realVal);
					offset += sizeof(int);
				}
				
				buffer += '\t';
			}
			break;
			case TypeVarChar:
			{
				buffer.append(recordDescriptor[i].name + ": ");

				// tests for null
				if (!isBitSet(nullsIndicator[i / 8], i % 8)) {
					int size;
					memcpy(&size, (char *)data + offset, sizeof(int));
					offset += sizeof(int);

					// initializes an array of spaces
					char* varcharVal = new char[recordDescriptor[i].length];
					memset(varcharVal, ' ', recordDescriptor[i].length);

					// copy value over to array
					memcpy(varcharVal, (char *)data + offset, size);
					buffer.append(varcharVal, 0, recordDescriptor[i].length);

					offset += size;
				}

				buffer += '\t';	
			}
			break;
		}

	}
	
	cout << buffer << endl;
	free(nullsIndicator);
	return 0;
}