### ------------------------------------------------------
### GNUmakefile for Amrvis
### ------------------------------------------------------
PRECISION = FLOAT
PRECISION = DOUBLE
PROFILE   = TRUE
PROFILE   = FALSE

COMP      = g++
FCOMP     = gfortran

DEBUG     = TRUE
DEBUG     = FALSE

DIM       = 3
DIM       = 2

USE_ARRAYVIEW = TRUE
USE_ARRAYVIEW = FALSE

USE_MPI=TRUE
USE_MPI=FALSE

USE_VOLRENDER = FALSE
USE_VOLRENDER = TRUE

USE_PARALLELVOLRENDER = TRUE
USE_PARALLELVOLRENDER = FALSE

BOXLIB_HOME = ../BoxLib

include $(BOXLIB_HOME)/Tools/C_mk/Make.defs

EBASE = amrvis
HERE = .

INCLUDE_LOCATIONS += $(HERE)
INCLUDE_LOCATIONS += $(BOXLIB_HOME)/Src/C_BaseLib
INCLUDE_LOCATIONS += $(BOXLIB_HOME)/Src/Extern/amrdata

DEFINES += -DBL_PARALLEL_IO

############################################### x includes and libraries

ifeq ($(__gcc_major_version),4)
  ifeq ($(__gcc_minor_version),9)
    LIBRARIES += -lquadmath
  endif
endif

ifeq ($(MACHINE), OSF1)
  LIBRARIES += -lXm -lXt -lX11
endif

ifeq ($(WHICHLINUX), ATLAS)
  LIBRARY_LOCATIONS += /usr/X11R6/lib64
endif 

ifeq ($(MACHINE), Linux)
  LIBRARY_LOCATIONS += /usr/lib64
  INCLUDE_LOCATIONS += /usr/include/Xm
  INCLUDE_LOCATIONS += /usr/include/

  LIBRARIES += -lXm -lXp -lXt -lXext -lSM -lICE -lXpm -lX11
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

# JFG: this line is needed on hive
# LIBRARY_LOCATIONS += /usr/X11R6/lib64

############################################### arrayview
ifeq (USE_ARRAYVIEW, TRUE)
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
include $(BOXLIB_HOME)/Src/C_BaseLib/Make.package
include $(BOXLIB_HOME)/Src/Extern/amrdata/Make.package

VPATH_LOCATIONS += $(HERE)
VPATH_LOCATIONS += $(BOXLIB_HOME)/Src/C_BaseLib
VPATH_LOCATIONS += $(BOXLIB_HOME)/Src/Extern/amrdata

vpath %.cpp $(VPATH_LOCATIONS)
vpath %.H   $(VPATH_LOCATIONS)
vpath %.F   $(VPATH_LOCATIONS)
vpath %.f   $(VPATH_LOCATIONS)
vpath %.f90 $(VPATH_LOCATIONS)
vpath %.a   $(LIBRARY_LOCATIONS)

all: $(executable)

include $(BOXLIB_HOME)/Tools/C_mk/Make.rules
### ------------------------------------------------------
### ------------------------------------------------------
