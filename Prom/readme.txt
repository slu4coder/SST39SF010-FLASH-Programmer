See source code for build instructions.

Using g++ on Windows:
- Download MSYS2-x86_64.xxxxx.exe (64-bit version)
- Install and start MSYS2 terminal and type...
  pacman -Syu (close & reopen terminal)				// -S: search, y: refresh package information, u: update installed packages
  pacman -Syu							// --needed: only installs packages that are not yet installed
  pacman -S --needed mingw-w64-x86_64-gcc make nano		// -R: removes package, -Q: querries if package is installed
- Windows Search: system environment variables / Path = c:\msys2\mingw64\bin;c:\msys2\mingw32\bin;c:\msys2\usr\bin
