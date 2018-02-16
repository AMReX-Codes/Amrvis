# Compiling on macOS

Tested to work on macOS High Sierra v. 10.13.2.

## Recommended Sofware Stack:

1. Homebrew ([brew.sh](https://brew.sh))

```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

2. GCC

```
brew install gcc
```

Note: **optional** Compiling AMReX and Amrvis dependencies using the `--cc=gcc-N` (where `N` is your GCC version number) option ensures consistent C/C++ and Fortran compilers.

3. OpenMotif

```
brew install openmotif
```

4. XQuarz ([xquarz.org](https://www.xquartz.org))

Download installer from [xquarz.org](https://www.xquartz.org). Works with version [2.7.11](https://dl.bintray.com/xquartz/downloads/XQuartz-2.7.11.dmg).

5. Specify your compiler in `$(AMREX_HOME)/Tools/GNUMake/Make.local`. For example, if you installed GCC version 7.x using homebrew, include:

```
CXX = g++-7
CC  = gcc-7
FC  = gfortran-7
F90 = gfortran-7
```

in your `Make.local`.
