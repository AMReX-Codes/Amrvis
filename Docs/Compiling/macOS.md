# Compiling on macOS

Tested to work on macOS High Sierra v. 10.13.2.

## Recommended Sofware Stack:

1. Homebrew ([brew.sh](https://brew.sh))

```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

2. GCC

```
brew install gcc@7
```

Note: **optional** Compiling AMReX and Amrvis dependencies using the `--cc=gcc-7` option ensures consistent C/C++ and Fortran compilers.

3. OpenMotif

```
brew install openmotif
```

4. XQuarz ([xquarz.org](https://www.xquartz.org))

Download installer from [xquarz.org](https://www.xquartz.org). Works with version [2.7.11](https://dl.bintray.com/xquartz/downloads/XQuartz-2.7.11.dmg).
