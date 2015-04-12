#include "pfm.h"
#include <fstream>
#include <sys/stat.h>

PagedFileManager* PagedFileManager::_pf_manager = 0;

PagedFileManager* PagedFileManager::instance()
{
	if(!_pf_manager)
		_pf_manager = new PagedFileManager();

	return _pf_manager;
}


PagedFileManager::PagedFileManager()
{
}


PagedFileManager::~PagedFileManager()
{
}


RC PagedFileManager::createFile(const string &fileName)
{
	/*struct stat info;

	if (stat(fileName.c_str(), &info) == 0) return -1;*/

	ifstream f(fileName.c_str());
	if (f.good()) {
		f.close();
		return -1;
	}

	ofstream file;

	file.open(fileName);
	file.close();
	return 0;
}


RC PagedFileManager::destroyFile(const string &fileName)
{
	if (remove(fileName.c_str()) != 0) {
		perror("Error deleting file");
		return -1;
	}
	return 0;
}


RC PagedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
	struct stat info;

	if (stat(fileName.c_str(), &info) != 0) return -1;

	//fileHandle.inHandle = new ifstream(fileName, ios::in | ios::binary | ios::ate);
	//fileHandle.outHandle = new ofstream(fileName, ios::app | ios::binary | ios::ate);
	fileHandle.handle = new fstream(fileName, ios::in | ios::out | ios::binary | ios::ate);


	//fileHandle.numPages = (unsigned int) fileHandle.inHandle->tellg() / PAGE_SIZE;
	fileHandle.numPages = (unsigned int)fileHandle.handle->tellg() / PAGE_SIZE;

	return 0;

}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
	//fileHandle.outHandle->close();
	//fileHandle.inHandle->close();

	//delete fileHandle.outHandle;
	//delete fileHandle.inHandle;

	fileHandle.handle->close();
	delete fileHandle.handle;

	return 0;
}


FileHandle::FileHandle()
{
	readPageCounter = 0;
	writePageCounter = 0;
	appendPageCounter = 0;
	numPages = 1;
	//inHandle = NULL;
	//outHandle = NULL;
	handle = NULL;
}


FileHandle::~FileHandle()
{
}


RC FileHandle::readPage(PageNum pageNum, void *data)
{
	if (pageNum >= numPages) {
		return -1;
	}

	// use in file to read in from file
	//inHandle->seekg(pageNum*PAGE_SIZE);
	//inHandle->read((char*)data, PAGE_SIZE);

	handle->seekg(pageNum*PAGE_SIZE);
	handle->read((char*)data, PAGE_SIZE);

	readPageCounter++;
	return 0;
}


RC FileHandle::writePage(PageNum pageNum, const void *data)
{
	if (pageNum >= numPages) {
		return -1;
	}

	//outHandle->seekp(pageNum*PAGE_SIZE);
	//outHandle->write((char*)data, PAGE_SIZE);

	handle->seekp(pageNum*PAGE_SIZE);
	handle->write((char*)data, PAGE_SIZE);

	writePageCounter++;
	return 0;
}


RC FileHandle::appendPage(const void *data)
{
	//outHandle->write((char*)data, PAGE_SIZE);
	handle->seekp(numPages*PAGE_SIZE);
	handle->write((char*)data, PAGE_SIZE);
	numPages += 1;

	appendPageCounter++;
	return 0;
}


unsigned FileHandle::getNumberOfPages()
{
	return numPages;
}


RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
	readPageCount = readPageCounter;
	writePageCount = writePageCounter;
	appendPageCount = appendPageCounter;

	return 0;
}
