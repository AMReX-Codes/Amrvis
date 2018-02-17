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

4. (macOS only) XQuarz ([xquarz.org](https://www.xquartz.org))

Download installer from [xquarz.org](https://www.xquartz.org). Works with version [2.7.11](https://dl.bintray.com/xquartz/downloads/XQuartz-2.7.11.dmg).

5. AMReX ([amrex-codes.github.io](https://amrex-codes.github.io)). 

No need to compile.

6. Plotting 3D files requires Volpack([www.graphics.stanford.edu](http://www.graphics.stanford.edu/software/volpack/)).

(macOS only) Getting this to compile on macOS is a bit of a pain. Instead we recommend our distribution:

```
git clone https://user-name@bitbucket.org/berkeleylab/volpack.git
```

which does not rely on autotools. Please contact the AMReX team for access to the volpack account. Run `make` from the volpack directory to build the volpack library.

## Building

1. Specify the locations of the AMReX and Volpack in the `GNUMakefile`. The defaults are:

```
AMREX_HOME = ../amrex
VOLPACKDIR = ../volpack
```

(hence placing Amrvis alongside AMReX and Volpack requires no configuration).

2. (macOS only) Specify your compiler in `$(AMREX_HOME)/Tools/GNUMake/Make.local`. For example, if you installed GCC version 7.x using homebrew, include:

```
CXX = g++-7
CC  = gcc-7
FC  = gfortran-7
F90 = gfortran-7
```

in your AMReX's `Make.local`.

3. Build by running `make` from the Amrvis directory. For 3D-Amrvis run `make DIM=3`.


## Configuring

As a final step, Amrvis needs a default config and a default palette. As a starting point, copy the `amrvis.default` and `Palette` into your home directory. For sanity, you can hide these files from view by renaming them (`.amrvis.default` and `.Palette`). Finally the `amrvis.default` needs to "point" at your default palette (use absolute paths, as `~` is not expanded).
