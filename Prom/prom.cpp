// FLASH Programmer written by Carsten Herting (2020–2023) SST39SF0x0A (max. 512KB)
// ported to Linux by Carsten Herting (2025)

// Build on Windows: g++ -O2 -oprom.exe prom.cpp -s
// Build on Linux: g++ -O2 -oprom prom.cpp -s

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <vector>
#include <thread>

#if defined(_WIN32)
  #include <windows.h>
  class CSerial
  {
  public:
    CSerial() { mComHandle = INVALID_HANDLE_VALUE; }
    ~CSerial() { Close(); }
    bool Open(int portnumber, int bitRate)
    {
      std::string portname = "COM" + std::to_string(portnumber);
      return Open(portname, bitRate);
    }
    bool Open(const std::string& portname, int bitRate)
    {
      mComHandle = CreateFileA(portname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
      if (mComHandle == INVALID_HANDLE_VALUE) return false;
      COMMTIMEOUTS cto = { MAXDWORD, 0, 0, 0, 0 };
      if (!SetCommTimeouts(mComHandle, &cto)) { Close(); return false; }
      DCB dcb;
      SecureZeroMemory(&dcb, sizeof(dcb));
      dcb.DCBlength    = sizeof(dcb);
      dcb.BaudRate     = bitRate;
      dcb.fBinary      = 1;
      dcb.fDtrControl  = DTR_CONTROL_DISABLE;
      dcb.fRtsControl  = RTS_CONTROL_DISABLE;
      dcb.Parity       = NOPARITY;
      dcb.StopBits     = ONESTOPBIT;
      dcb.ByteSize     = 8;
      if (!SetCommState(mComHandle, &dcb)) { Close(); return false; }
      return true;
    }
    void Close()
    {
      if (mComHandle != INVALID_HANDLE_VALUE)
      {
        CloseHandle(mComHandle);
        mComHandle = INVALID_HANDLE_VALUE;
      }
    }
    int SendData(const std::string& buffer)
    {
      if (mComHandle == INVALID_HANDLE_VALUE) return 0;
      DWORD numWritten = 0;
      WriteFile(mComHandle, buffer.c_str(), DWORD(buffer.size()), &numWritten, nullptr);
      return int(numWritten);
    }
    int SendData(const char* buffer, int bytesize)
    {
      if (mComHandle == INVALID_HANDLE_VALUE) return 0;
      DWORD numWritten = 0;
      WriteFile(mComHandle, buffer, DWORD(bytesize), &numWritten, nullptr);
      return int(numWritten);
    }
    int SendByte(unsigned char ch)
    {
      if (mComHandle == INVALID_HANDLE_VALUE) return 0;
      DWORD numWritten = 0;
      WriteFile(mComHandle, &ch, 1, &numWritten, nullptr);
      return (numWritten == 1 ? 1 : 0);
    }
    int ReadData(unsigned char* buffer, int buffLimit)
    {
      if (mComHandle == INVALID_HANDLE_VALUE) return 0;
      DWORD numRead = 0;
      ReadFile(mComHandle, buffer, DWORD(buffLimit), &numRead, NULL);
      return int(numRead);
    }
    void Flush()
    {
      unsigned char tmp[16];
      while (ReadData(tmp, sizeof(tmp)) > 0) {}
    }
    int ReadDataWaiting()
    {
      if (mComHandle == INVALID_HANDLE_VALUE)
        return 0;
      DWORD errors = 0;
      COMSTAT stat = {};
      ClearCommError(mComHandle, &errors, &stat);
      return int(stat.cbInQue);
    }
	  int GetFirstComPort()
    {
      char buffer[100];
      for (int i = 0; i < 256; ++i)
      {
        std::string name = "COM" + std::to_string(i);
        if (QueryDosDeviceA(name.c_str(), buffer, sizeof(buffer)) != 0) return i;
      }
      return -1;
    }
  private:
    HANDLE mComHandle;
  };

#elif defined(__linux__)
  #include <termios.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/ioctl.h>
  #include <cstdio>
  #include <cstring>

  class CSerial
  {
  public:
    CSerial() { m_fd = -1; }
    ~CSerial() { Close(); }
    bool Open(int portnumber, int bitRate)
    {
      std::string dev = GetFirstComPort();
      if (dev.empty()) return false;
      return Open(dev, bitRate);
    }
    bool Open(const std::string& device, int bitRate)
    {
      m_fd = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
      if (m_fd < 0) return false;
      termios tty = {};
      if (tcgetattr(m_fd, &tty) != 0) { Close(); return false; }
      m_orig = tty; // store original setting so we can restore at the end of the program
      cfsetospeed(&tty, MapBaud(bitRate));
      cfsetispeed(&tty, MapBaud(bitRate));
      tty.c_cflag  = (tty.c_cflag & ~CSIZE) | CS8;
      tty.c_cflag |= (CLOCAL | CREAD);
      tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
      tty.c_lflag  = 0;
      tty.c_iflag  = 0;
      tty.c_oflag  = 0;
      tty.c_cc[VMIN]  = 0;
      tty.c_cc[VTIME] = 0;
			int status;
			ioctl(m_fd, TIOCMGET, &status);
			status &= ~TIOCM_DTR; 
			ioctl(m_fd, TIOCMSET, &status);
      if (tcsetattr(m_fd, TCSANOW, &tty) != 0) { Close(); return false; }
      m_device = device;
      return true;
    }
    void Close()
    {
      if (m_fd >= 0)
      {
        tcsetattr(m_fd, TCSANOW, &m_orig);
        ::close(m_fd);
        m_fd = -1;
        m_device.clear();
      }
    }
    int SendData(const std::string& buf)
    {
      if (m_fd < 0) return 0;
      ssize_t n = ::write(m_fd, buf.c_str(), buf.size());
      return (n < 0 ? 0 : int(n));
    }
    int SendData(const char* buf, int len)
    {
      if (m_fd < 0) return 0;
      ssize_t n = ::write(m_fd, buf, len);
      return (n < 0 ? 0 : int(n));
    }
    int SendByte(unsigned char ch)
    {
      if (m_fd < 0) return 0;
      ssize_t n = ::write(m_fd, &ch, 1);
      return (n < 0 ? 0 : 1);
    }
    int ReadData(unsigned char* buf, int maxlen)
    {
      if (m_fd < 0) return 0;
      ssize_t n = ::read(m_fd, buf, maxlen);
      return (n < 0 ? 0 : int(n));
    }
    void Flush()
    {
      unsigned char tmp[16];
      while (ReadData(tmp, sizeof(tmp)) > 0) {}
    }
    int ReadDataWaiting()
    {
      if (m_fd < 0) return 0;
      int bytes = 0;
      if (ioctl(m_fd, FIONREAD, &bytes) < 0) return 0;
      return bytes;
    }
    std::string GetFirstComPort()
    {
      const char* prefixes[] = { "/dev/ttyUSB", "/dev/ttyACM", "/dev/ttyS" };
      char path[32];
      for (auto p : prefixes)
      {
        for (int i = 0; i < 256; ++i)
        {
          std::snprintf(path, sizeof(path), "%s%d", p, i);
          if (::access(path, R_OK | W_OK) == 0) return std::string(path);
        }
      }
      return {};
    }
    std::string DevicePath() const { return m_device; }
  private:
    static speed_t MapBaud(int baud)
    {
      switch (baud)
      {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return B9600;
      }
    }
    int m_fd;
    termios m_orig;
    std::string m_device;
  };

#else
  #error Platform not supported
#endif

void helpscreen()
{
  #if defined(_WIN32)
    std::cout << "Windows version:\n";
    std::cout << "Usage (Windows version): prom <file> [<portnum>]\n";
    std::cout << "Writes the binary content of <file> to SST39SF0x0A FLASH.\n";
    std::cout << "Optional: Specify COM <portnum> manually (example: 1).\n";
    std::cout << "All data is read back and verified.\n";
    std::cout << "Press Ctrl+C to exit.\n" << std::flush;
  #elif defined(__linux__)
    std::cout << "Linux version:\n";
    std::cout << "Usage: ./prom <file> [<portname>]\n";
    std::cout << "Writes the binary content of <file> to SST39SF0x0A FLASH.\n";
    std::cout << "Optional: Specify serial <portname> manually (example: /dev/ttyUSB0).\n";
    std::cout << "All data is read back and verified.\n";
    std::cout << "Press Ctrl+C to exit.\n" << std::flush;
  #else
    #error Platform not supported
  #endif
}

int dt_millis(std::chrono::time_point<std::chrono::steady_clock> t1, std::chrono::time_point<std::chrono::steady_clock> t0)
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
}

int main(int argc, char* argv[])
{		
  #if defined(_WIN32)
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), 0b111); // enable CSI sequences on Windows
  #endif	

  std::cout << "\nSST39SF0x0A FLASH Programmer v2.2\nWritten by C. Herting (slu4) 2023-2025\n\n" << std::flush;
  if (argc < 2) { helpscreen(); return 1; }

  std::cout << "o Loading image file... " << std::flush;
  std::ifstream file(argv[1], std::ios::binary);
  if (!file) { std::cout << "ERROR: Can't open file '" << argv[1] << "'\n" << std::flush; return 1; }

  file.seekg(0, file.end);
  int bytesize = int(file.tellg());
  file.seekg(0, file.beg);

  std::vector<char> filebuf(bytesize);
  file.read(filebuf.data(), bytesize);
  file.close();
  std::cout << bytesize << " bytes\n" << std::flush;

  std::cout << "o Opening serial port... " << std::flush;
  CSerial com;
  #if defined(_WIN32)
    int port;
    if (argc > 2) port = std::stoi(argv[2]);
    else port = com.GetFirstComPort();
    if (port < 0 || !com.Open(port, 115200))
    {
      std::cout << "ERROR: Can't open COM port.\n" << std::flush; return 1;
    }
    std::cout << "COM" << port << "\n" << std::flush;
  #else
    std::string dev;
    if (argc > 2) dev = std::string(argv[2]);
    else dev = com.GetFirstComPort();
    if (dev.empty() || !com.Open(dev, 115200))
    {
      std::cout << "ERROR: Can't open serial device.\n" << std::flush; return 1;
    }
    std::cout << dev << "\n" << std::flush;
  #endif
	
  std::cout << "o Waiting 2 seconds...\n" << std::flush;
	std::this_thread::sleep_for(std::chrono::seconds(2));
	com.Flush();

  auto now = std::chrono::steady_clock::now();
  auto last = now;

	std::cout << "o Looking for programmer... " << std::flush;
  com.SendByte('a');
  unsigned char rec = 0;
  while (com.ReadData(&rec, 1) == 0 && dt_millis(now, last) < 1000)
  {
    now = std::chrono::steady_clock::now();
  }
  if (rec != 'A') { std::cout << "ERROR: Programmer doesn't respond.\n" << std::flush; return 1; }
  std::cout << "OK\n" << std::flush;

  std::cout << "o Sending bytesize... " << std::flush;
  com.SendData(std::to_string(bytesize));
  com.SendByte('b');

  int recsize = 0;
  rec = 0;
  do
  {
    if (com.ReadData(&rec, 1) == 1 && rec >= '0' && rec <= '9') recsize = recsize * 10 + (rec - '0');
  } while (rec != 'B');

  if (recsize != bytesize) { std::cout << "ERROR: Programmer doesn't confirm bytesize.\n" << std::flush; return 1; }
  std::cout << "OK\n" << std::flush;
  std::cout << "o Erasing FLASH... " << std::flush;
  rec = 0;
  while (com.ReadData(&rec, 1) == 0) {}
  if (rec != 'C') { std::cout << "ERROR: Programmer can't erase FLASH.\n" << std::flush; return 1; }
  std::cout << "OK\n" << std::flush;
  std::cout << "\e[Go Writing..." << std::flush;
  int pos = 0, oldper = -1;

  while (pos < bytesize)
  {
    int chunk = std::min(32, bytesize - pos);
    com.SendData(&filebuf[pos], chunk);
    pos += chunk;
    // *** wait for the Arduino to ACK this chunk ***
    while (com.ReadData(&rec, 1) == 0) {}
    int per = (100 * pos) / bytesize;
    if (per != oldper) { std::cout << "\e[Go Writing... " << per << "%" << std::flush; oldper = per; }
  }
  std::cout << " OK\n" << std::flush;

  std::cout << "\e[Go Verifying..." << std::flush;
  int errors = 0;
  pos = 0; oldper = -1;
  do
  {
    now = std::chrono::steady_clock::now();
    if (com.ReadData(&rec, 1) == 1)
    {
      if (rec != static_cast<unsigned char>(filebuf[pos])) ++errors;
      ++pos;
      last = now;
      int per = 100 * pos / bytesize;
      if (per != oldper) { std::cout << "\e[Go Verifying... " << per << "%" << std::flush; oldper = per; }
    }
  } while (pos < bytesize && dt_millis(now, last) < 1000);

  if (pos != bytesize) { std::cout << "\nERROR: File size mismatch.\n" << std::flush; return 1; }
  std::cout << " OK\n\n";
  if (errors == 0) std::cout << "SUCCESS\n" << std::flush;
  else std::cout << errors << " ERRORS\n" << std::flush;
  com.Close();
  return (errors == 0 ? 0 : 1);
}
