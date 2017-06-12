### ------------------------------------------------------
### GNUmakefile for Amrvis
### ------------------------------------------------------

PRECISION = FLOAT
PRECISION = DOUBLE
PROFILE   = TRUE
PROFILE   = FALSE

COMP      = intel
COMP      = gnu

DEBUG     = FALSE
DEBUG     = TRUE

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

USE_PROFDATA = TRUE
USE_PROFDATA = FALSE
ifeq ($(DIM), 3)
  USE_PROFDATA = FALSE
endif
ifeq ($(USE_PROFDATA), TRUE)
  AMRPROFPARSER_HOME = ../AMRProfParser
  DEFINES += -DAV_PROFDATA
  PROFILE   = TRUE
  TRACE_PROFILE   = TRUE
endif

AMREX_HOME = ../amrex

include $(AMREX_HOME)/Tools/GNUMake/Make.defs

EBASE = amrvis
HERE = .

INCLUDE_LOCATIONS += $(HERE)
INCLUDE_LOCATIONS += $(AMREX_HOME)/Src/Base
INCLUDE_LOCATIONS += $(AMREX_HOME)/Src/Extern/amrdata

DEFINES += -DBL_PARALLEL_IO
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
    # -lquadmath needed for GNU; not sure about other compilers
    LIBRARIES += -lquadmath
    ifeq ($(USE_MPI), TRUE)
        LDFLAGS += -dynamic
    endif
  endif

  ifeq ($(which_computer), cori)
    # -lquadmath needed for GNU; not sure about other compilers
    LIBRARIES += -lquadmath
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
  LIBRARY_LOCATIONS += /usr/lib64
  INCLUDE_LOCATIONS += /usr/include/Xm
  INCLUDE_LOCATIONS += /usr/include/

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
include $(AMREX_HOME)/Src/Extern/amrdata/Make.package

VPATH_LOCATIONS += $(HERE)
VPATH_LOCATIONS += $(AMREX_HOME)/Src/Base
VPATH_LOCATIONS += $(AMREX_HOME)/Src/Extern/amrdata

ifeq ($(USE_PROFDATA), TRUE)
  VPATH_LOCATIONS += $(AMRPROFPARSER_HOME)
  INCLUDE_LOCATIONS += $(AMRPROFPARSER_HOME)
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

BLProfParser.tab.H BLProfParser.tab.cpp:	BLProfParser.y
	cat BLProfParser.y $(SED0) $(SED1) > BLProfParserNC.y
	bison --defines=BLProfParser.tab.H --output=BLProfParser.tab.cpp \
	      BLProfParserNC.y
	rm BLProfParserNC.y

BLProfParser.lex.yy.cpp:	BLProfParser.tab.H BLProfParser.l
	flex --outfile=BLProfParser.lex.yy.cpp BLProfParser.l

endif

include $(AMREX_HOME)/Tools/GNUMake/Make.rules

### ------------------------------------------------------
### ------------------------------------------------------
