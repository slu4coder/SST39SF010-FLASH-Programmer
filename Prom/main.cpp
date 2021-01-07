// Chip Programmer written by Cartsten Herting (2020)
// for parallel EEPROM AT28C64B (8K) and parallel FLASH SST39SF010A (128K)

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
		for (int i = 0; i < 255; i++) // checking ports from COM0 to COM255
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
	std::cout << "DIY AT28C64B EEPROM Programmer v1.0\n";
	std::cout << "written by Carsten Herting (2020)\n\n";
	std::cout << "Usage: prom [OPTION]\n";
	std::cout << " -wFILENAME     Writes the content of FILENAME to the EEPROM.\n";
	std::cout << "                The content of the EEPROM is read back and verified.\n";
	std::cout << " -r[FILENAME]   Reads the content of the EEPROM [to FILENAME].\n";
	std::cout << " -cFILENAME     Prints the 32-bit checksum of FILENAME.\n";
	std::cout << " -h or --help   Prints this help screen.\n";
}

int main(int argc, char *argv[])
{		
	SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), 0b111);		// enable ANSI control sequences in WINDOWS console

	if (argc == 2 && strlen(argv[1]) >= 2 && argv[1][0] == '-')
	{
		switch (argv[1][1])
		{
			case 'c':
			{
				std::ifstream file(&argv[1][2], std::ios::binary | std::ios::in);
				uint32_t checksum = 0;
				if (file.is_open())
				{
					file.seekg (0, file.end);				// get byte length of file
					int bytesize = file.tellg();
					file.seekg (0, file.beg);					
					
					for(int i=0; i<bytesize; i++)
					{
						UCHAR to;
						file.read((char*)&to, 1);
						checksum += to;
					}
					std::cout << "FILE bytesize = " << bytesize << ", checksum = " << checksum << std::endl;
					file.close();
				} else std::cout << "ERROR: File not found." << std::endl;
				break;
			}			
			case 'w':
			{
				std::ifstream file(&argv[1][2], std::ios::binary | std::ios::in);
				if (file.is_open())
				{
					file.seekg (0, file.end);				// get byte length of file
					int bytesize = file.tellg();
					file.seekg (0, file.beg);

					CSerial com;
					int port = com.GetFirstComPort();
					if (port != -1 && com.Open(port, 115200))
					{
						std::cout << "Opened COM" << port << std::endl;
						UCHAR rec;
						do { com.SendByte('w'); Sleep(100); } while(com.ReadData(&rec, 1) == 0);
						if(rec == 'W')
						{
							std::cout << "WRITE MODE" << std::endl;
							char filebuf[bytesize];
							file.read(filebuf, bytesize);
							uint32_t checksum = 0;
							for (int i=0; i<bytesize; i++) checksum += UCHAR(filebuf[i]);
							std::cout << "FILE bytesize = " << bytesize << ", checksum = " << checksum << std::endl;							
							for(int i=0; i<bytesize; i+=32)
							{
								com.SendData((const char *)&filebuf[i], 32);
								while(com.ReadData(&rec, 1) == 0);
								std::cout << "\e[GWRITING " << 100*i/bytesize << "%";
							} 
							std::cout << "\e[GWRITING 100%" << std::endl;
							com.SendByte('r');											// switch to read mode and verify
							while(com.ReadData(&rec, 1) == 0);
							if(rec == 'R')
							{
								uint32_t nowticks, lastticks = GetTickCount();
								int errors = 0, pos = 0;
								checksum = 0;
								do
								{
									nowticks = GetTickCount();
									if (com.ReadData(&rec, 1) != 0)
									{
										if (rec != UCHAR(filebuf[pos])) errors++;
										pos++;
										checksum += rec;
										lastticks = nowticks;
									}
								} while (nowticks - lastticks < 500);
								std::cout << "VERIFYING: " << errors << " errors" << std::endl;
								std::cout << "READ bytesize = " << pos << ", checksum = " << checksum << std::endl;
							} else std::cout << "ERROR: Unable to verify: " << int(rec) << std::endl;
							com.Close();
						} else std::cout << "ERROR: Unable to enter WRITE mode." << std::endl;
					} else std::cout << "ERROR: Can't open COM port." << std::endl;
					file.close();
				} else std::cout << "ERROR: File not found." << std::endl;
				break;
			}
			case 'r':
			{
				CSerial com;
				int port = com.GetFirstComPort();
				if (port != -1 && com.Open(port, 115200))
				{
					std::cout << "Opened COM" << port << std::endl;
					UCHAR rec;
					do
					{
						com.SendByte('r');
						Sleep(100);
					} while(com.ReadData(&rec, 1) == 0);
					if (rec == 'R')
					{
						std::cout << "READ MODE" << std::endl;
						uint32_t bytesize = 0, checksum = 0;
						std::ofstream file;
						if (strlen(argv[1]) > 2) file.open(&argv[1][2], std::ios::binary | std::ios::out);
						uint32_t nowticks, lastticks = GetTickCount();
						do
						{
							nowticks = GetTickCount();
							if (com.ReadData(&rec, 1) != 0)
							{
								bytesize++;
								checksum += rec;
								if (file.is_open()) file << rec;
								lastticks = nowticks;
							}
						} while (nowticks - lastticks < 500);
						if (file.is_open()) file.close();
						std::cout << "READ bytesize = " << bytesize << ", checksum = " << checksum << std::endl;
						com.Close();
					} else std::cout << "ERROR: Unable to enter READ mode." << std::endl;
				} else std::cout << "ERROR: Can't open COM port." << std::endl;
				break;
			}
			default: helpscreen(); break;
		}
	} else helpscreen();

	return 0;
}

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2020 Carsten Herting
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
