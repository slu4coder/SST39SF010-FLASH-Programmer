// Chip Programmer written by Cartsten Herting (2020-2023)
// for parallel FLASH SST39SF0x0A (max. 512KB)

#include <windows.h>
#include <iostream>
#include <fstream>

class CSerial
{
public:
	CSerial() { mComHandle = NULL; }
	~CSerial() { Close(); }
	bool Open(int portnumber, int bitRate)
	{
		std::string portname = "COM" + std::to_string(portnumber);

		mComHandle = CreateFile(portname.c_str(), GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
		if(mComHandle == INVALID_HANDLE_VALUE) return false;

		COMMTIMEOUTS cto = { MAXDWORD, 0, 0, 0, 0};		// set the timeouts
		if(!SetCommTimeouts(mComHandle, &cto)) { Close(); return false; }		

		DCB dcb;
		memset(&dcb, 0, sizeof(dcb));										// set the DCB
		dcb.DCBlength = sizeof(dcb);
		dcb.BaudRate = bitRate;
		dcb.fBinary = 1;
		dcb.fDtrControl = DTR_CONTROL_DISABLE;
		dcb.fRtsControl = RTS_CONTROL_DISABLE;
		dcb.Parity = NOPARITY;
		dcb.StopBits = ONESTOPBIT;										// ONESTOPBIT
		dcb.ByteSize = 8;
		if(!SetCommState(mComHandle, &dcb)) { Close(); return false; }

		return true;
	}
	void Close() { CloseHandle(mComHandle); }
	int SendData(std::string buffer)
	{
		DWORD numWritten;
		WriteFile(mComHandle, buffer.c_str(), buffer.size(), &numWritten, nullptr); 
		return numWritten;
	}
	int SendData(const char* buffer, int bytesize)
	{
		DWORD numWritten;
		WriteFile(mComHandle, buffer, bytesize, &numWritten, nullptr); 
		return numWritten;
	}
	int SendByte(UCHAR ch)
	{
		DWORD numWritten;
		WriteFile(mComHandle, &ch, 1, &numWritten, nullptr);
		return 1;
	}
	int ReadData(UCHAR *buffer, int buffLimit)
	{
		DWORD numRead;
		BOOL ret = ReadFile(mComHandle, buffer, buffLimit, &numRead, NULL);
		return numRead;
	}
	void Flush()
	{
		UCHAR buffer[10];
		int numBytes = ReadData(buffer, 10);
		while(numBytes != 0) { numBytes = ReadData(buffer, 10); }
	}
	int ReadDataWaiting()		// peeks at the buffer and returns the number of bytes waiting
	{
		if (mComHandle == nullptr) return 0;
		DWORD dwErrorFlags;
		COMSTAT ComStat;
		ClearCommError(mComHandle, &dwErrorFlags, &ComStat);
		return int(ComStat.cbInQue);
	}	
	int GetFirstComPort() 	//added function to find the first present serial port (COM)
	{
		char buffer[100]; 	// buffer to store the path of the COMPORTS
		for (int i = 0; i <256; i++) // checking ports from COM0 to COM255
		{
			std::string str = "COM" + std::to_string(i); // converting to COM0, COM1, COM2
			if (QueryDosDeviceA(str.c_str(), buffer, sizeof(buffer)) != 0) return i;
		}
		return -1;					// did not find any comport
	}		
protected:
	HANDLE mComHandle;
};

void helpscreen()
{
	std::cout << "Usage: prom <file>\n";
	std::cout << "Writes the binary content of <file> to FLASH.\n";
	std::cout << "All data is read back and verified.\n";
	std::cout << "Press Ctrl+C to exit.\n";
}

int main(int argc, char *argv[])
{		
	SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), 0b111);		// enable ANSI control sequences in WINDOWS console
	std::cout << "\nSST39SF0x0A FLASH Programmer v2.1\n";
	std::cout <<   "Written by C. Herting (slu4) 2023\n\n";
	if (argc == 2)
	{
		std::cout << "o Loading image file... ";
		std::ifstream file(&argv[1][0], std::ios::binary | std::ios::in);
		if (file.is_open())
		{
			file.seekg (0, file.end);				// get bytesize of binary image file
			int bytesize = file.tellg();
			file.seekg (0, file.beg);
			char filebuf[bytesize];
			file.read(filebuf, bytesize);
			file.close();
			std::cout << bytesize << " bytes" << std::endl;

			std::cout << "o Opening serial port... ";
			CSerial com;
			int port = com.GetFirstComPort();
			if (port != -1 && com.Open(port, 115200))
			{
				std::cout << "COM" << port << std::endl;

				std::cout << "o Looking for programmer... ";
				com.SendByte('a'); // send request for handshake
				unsigned char rec=0; while (com.ReadData(&rec, 1) == 0); // wait for any handshake byte
				if(rec == 'A') // first handshake received from Arduino?
				{
					std::cout << "OK" << std::endl;
					
					std::cout << "o Sending bytesize... ";
					com.SendData(std::to_string(bytesize));
					com.SendByte('b');

					int recsize = 0; rec=0;
					do
					{
						if (com.ReadData(&rec, 1) == 1 && rec >= '0' && rec <= '9') recsize = recsize*10 + rec - '0';
					} while (rec != 'B'); // expect handshake byte
					
					if (recsize == bytesize)
					{
						std::cout << "OK" << std::endl;
						std::cout << "o Erasing FLASH... ";
						rec = 0; while (com.ReadData(&rec, 1) == 0); // wait for any handshake byte
						if(rec == 'C') // first handshake received from Arduino?
						{
							std::cout << "OK" << std::endl;

							std::cout << "\e[Go Writing...";
							int pos=0;
							int oldper = -1;
							while (pos < bytesize)
							{
								int chunk = 32; // max buffersize of Arduino UART is 64 bytes
								if (pos + chunk > bytesize) chunk = bytesize - pos;
								com.SendData((const char *)&filebuf[pos], chunk);
								pos += chunk;
								while (com.ReadData(&rec, 1) == 0); // wait for any confirmation
								int per = 100*pos/bytesize;
								if (per != oldper) { std::cout << "\e[Go Writing... " << per << "%"; oldper = per; }
							}
							std::cout << " OK" << std::endl;

							std::cout << "\e[Go Verifying...";
							unsigned int nowticks, lastticks = GetTickCount();
							int errors = 0;
							pos = 0;
							oldper = -1;
							do
							{
								nowticks = GetTickCount();
								if (com.ReadData(&rec, 1) == 1)
								{
									if (rec != UCHAR(filebuf[pos])) errors++;
									pos++;
									lastticks = nowticks;
									int per = 100*(pos)/bytesize;
									if (per != oldper) { std::cout << "\e[Go Verifying... " << per << "%"; oldper = per; }
								}
							} while (nowticks - lastticks < 1000 && pos < bytesize);
							if (pos == bytesize) // check for size mismatch
							{
								std::cout << " OK" << std::endl << std::endl;
								if (errors == 0) std::cout << "SUCCESS" << std::endl;
								else std::cout << errors << " ERRORS" << std::endl;
							} else std::cout << "\nERROR: File size mismatch." << std::endl;
						} else std::cout << "\nERROR: Programmer can't erase FLASH." << std::endl;
					} else std::cout << "\nERROR: Programmer doesn't confirm bytesize." << std::endl;
				} else std::cout << "\nERROR: Programmer doesn't respond." << std::endl;
				com.Close();
			} else std::cout << "\nERROR: Can't open COM port." << std::endl;
		} else std::cout << "\nERROR: Can't open file '" << &argv[1][0] << "'" << std::endl;
	} else helpscreen();
	return 0;
}

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2023 Carsten Herting
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
