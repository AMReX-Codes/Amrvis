### ------------------------------------------------------
### GNUmakefile for Amrvis
### ------------------------------------------------------
AMREX_HOME ?= ../amrex

PRECISION = FLOAT
PRECISION = DOUBLE

PROFILE       = FALSE
TRACE_PROFILE = FALSE
COMM_PROFILE  = FALSE

COMP      = intel
COMP      = gnu

DEBUG     = FALSE
#DEBUG     = TRUE

DIM       = 1
DIM       = 3
DIM       = 2

USE_ARRAYVIEW = TRUE
USE_ARRAYVIEW = FALSE

USE_MPI=TRUE
USE_MPI=FALSE

USE_CXX11     = TRUE

USE_VOLRENDER = FALSE
USE_VOLRENDER = TRUE

USE_PARALLELVOLRENDER = TRUE
USE_PARALLELVOLRENDER = FALSE

USE_PROFPARSER = TRUE
USE_PROFPARSER = FALSE

ifeq ($(DIM), 1)
  USE_PROFPARSER = FALSE
endif
ifeq ($(DIM), 3)
  USE_PROFPARSER = FALSE
endif
ifeq ($(USE_PROFPARSER), TRUE)
  PROFILE       = TRUE
  TRACE_PROFILE = TRUE
endif

#DEFINES += -DAV_CGS_FIXSLNC

include $(AMREX_HOME)/Tools/GNUMake/Make.defs

EBASE = amrvis
HERE = .

INCLUDE_LOCATIONS += $(HERE)
INCLUDE_LOCATIONS += $(AMREX_HOME)/Src/Base
INCLUDE_LOCATIONS += $(AMREX_HOME)/Src/Extern/amrdata

DEFINES += -DBL_OPTIO

############################################### x includes and libraries

ifeq ($(MACHINE), OSF1)
  LIBRARIES += -lXm -lXt -lX11
endif

ifeq ($(WHICHLINUX), ATLAS)
  LIBRARY_LOCATIONS += /usr/X11R6/lib64
endif

ifneq ($(which_site), unknown)
  LIBRARY_LOCATIONS += /usr/lib64
  INCLUDE_LOCATIONS += /usr/include/Xm
  INCLUDE_LOCATIONS += /usr/include/

  LIBRARIES += -lXm -lXt -lXext -lSM -lICE -lXpm -lX11

  ifeq ($(which_computer), edison)
    ifeq ($(USE_MPI), TRUE)
        LDFLAGS += -dynamic
    endif
  endif

  ifeq ($(which_computer), cori)
    ifeq ($(USE_MPI), TRUE)
        LDFLAGS += -dynamic
    endif
  endif
endif

ifeq ($(MACHINE), AIX)
  INCLUDE_LOCATIONS += /usr/include/X11
  INCLUDE_LOCATIONS += /usr/include/Xm
  #INCLUDE_LOCATIONS += /usr/include/X11/Xaw
  LIBRARIES += -lXm -lXt -lX11
  DEFINES += -D_ALL_SOURCE
endif

ifeq ($(MACHINE), CYGWIN_NT)
  INCLUDE_LOCATIONS += /usr/X11R6/include
  LIBRARY_LOCATIONS += /usr/X11R6/lib
  LIBRARIES += -lXm -lXt -lSM -lICE -lXpm -lX11
endif

# last chance catch-all
ifeq ($(which_site), unknown)
  INCLUDE_LOCATIONS += /usr/include/

  # check if running on macOS (as there are some subtle differences to linux).
  UNAME_S = $(shell uname -s)
  ifeq ($(UNAME_S), Darwin)
	# these assume that dependencies are installed via homebrew, which symlinks
    # everything into the /usr/local tree.
    INCLUDE_LOCATIONS += /usr/local/include
    LIBRARY_LOCATIONS += /usr/local/lib
	# newer versions of Homebrew install into /opt/homebrew instead
    INCLUDE_LOCATIONS += /opt/homebrew/include
    LIBRARY_LOCATIONS += /opt/homebrew/lib

    # if dependencies installed via macport, everything symlinks
    # to /opt/local/
    INCLUDE_LOCATIONS += /opt/local/include
    LIBRARY_LOCATIONS += /opt/local/lib

    # on macOS X11 is installed into the /opt tree
    INCLUDE_LOCATIONS += /opt/X11/include
    LIBRARY_LOCATIONS += /opt/X11/lib
  else
	# if not running macOS, then assume we are looking at a standard
    # ubuntu-like linux.
    INCLUDE_LOCATIONS += /usr/include/Xm
    LIBRARY_LOCATIONS += /usr/lib64
  endif

  LIBRARIES += -lXm -lXt -lXext -lSM -lICE -lXpm -lX11
endif


# JFG: this line is needed on hive
# LIBRARY_LOCATIONS += /usr/X11R6/lib64



############################################### arrayview
ifeq ($(USE_ARRAYVIEW), TRUE)
  DEFINES += -DBL_USE_ARRAYVIEW
  ARRAYVIEWDIR = .
  INCLUDE_LOCATIONS += $(ARRAYVIEWDIR)
  #LIBRARY_LOCATIONS += $(ARRAYVIEWDIR)
  #LIBRARIES += -larrayview$(DIM)d.$(machineSuffix)
endif


############################################### volume rendering
ifeq ($(DIM),3)
  ifeq ($(MACHINE), T3E)
    USE_VOLRENDER = FALSE
  endif
  ifeq ($(MACHINE), AIX)
#   USE_VOLRENDER = FALSE
  endif
  ifeq ($(USE_VOLRENDER), TRUE)
    DEFINES += -DBL_VOLUMERENDER
    VOLPACKDIR = ../volpack
    INCLUDE_LOCATIONS += $(VOLPACKDIR)
    LIBRARY_LOCATIONS += $(VOLPACKDIR)
    LIBRARIES += -lvolpack
    #DEFINES += -DVOLUMEBOXES
  endif
endif

############################################### parallel volume rendering
ifeq ($(DIM),3)
  ifeq ($(USE_PARALLELVOLRENDER), TRUE)
    DEFINES += -DBL_PARALLELVOLUMERENDER
  endif
endif

############################################### other defines
#DEFINES += -DSCROLLBARERROR
#DEFINES += -DFIXDENORMALS

ifdef CONFIG_FILEPATH
  DEFINES += -DAMRVIS_CONFIG_FILEPATH='"${CONFIG_FILEPATH}"'
endif
############################################### float fix
# if we are using float override FOPTF which sets -real_size 64
ifeq ($(PRECISION), FLOAT)
  ifeq ($(MACHINE), OSF1)
    FDEBF += -C
    FDEBF += -fpe2
    FOPTF  = -fast -O5 -tune ev5
  endif
endif

include $(HERE)/Make.package
include $(AMREX_HOME)/Src/Base/Make.package
#include $(AMREX_HOME)/Src/Extern/ProfParser/Make.package
include $(AMREX_HOME)/Src/Extern/amrdata/Make.package

#INCLUDE_LOCATIONS += $(AMREX_HOME)/Src/Extern/ProfParser

VPATH_LOCATIONS += $(HERE)
VPATH_LOCATIONS += $(AMREX_HOME)/Src/Base
#VPATH_LOCATIONS += $(AMREX_HOME)/Src/Extern/ProfParser
VPATH_LOCATIONS += $(AMREX_HOME)/Src/Extern/amrdata

ifeq ($(USE_PROFDATA), TRUE)
  #SED0 = | sed 's/\#define vout/\/\//'
  #SED1 = | sed 's/vout/\/\//'
  SED0 =
  SED1 =
endif

vpath %.cpp $(VPATH_LOCATIONS)
vpath %.H   $(VPATH_LOCATIONS)
vpath %.F   $(VPATH_LOCATIONS)
vpath %.f   $(VPATH_LOCATIONS)
vpath %.f90 $(VPATH_LOCATIONS)
vpath %.c   $(VPATH_LOCATIONS)
vpath %.h   $(VPATH_LOCATIONS)
vpath %.l   $(VPATH_LOCATIONS)
vpath %.y   $(VPATH_LOCATIONS)
vpath %.a   $(LIBRARY_LOCATIONS)

all:	$(executable)

ifeq ($(USE_PROFDATA), TRUE)

#BLProfParser.tab.H BLProfParser.tab.cpp: $(AMREX_HOME)/Src/Extern/ProfParser/BLProfParser.y
#	cat $(AMREX_HOME)/Src/Extern/ProfParser/BLProfParser.y $(SED0) $(SED1) > BLProfParserNC.y
#	bison --defines=BLProfParser.tab.H --output=BLProfParser.tab.cpp \
#		BLProfParserNC.y
#	rm BLProfParserNC.y


#BLProfParser.lex.yy.cpp: BLProfParser.tab.H $(AMREX_HOME)/Src/Extern/ProfParser/BLProfParser.l
#	flex --outfile=BLProfParser.lex.yy.cpp $(AMREX_HOME)/Src/Extern/ProfParser/BLProfParser.l

endif

include $(AMREX_HOME)/Tools/GNUMake/Make.rules

### ------------------------------------------------------
### ------------------------------------------------------
