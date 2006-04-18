PRECISION = FLOAT
PRECISION = DOUBLE
PROFILE   = TRUE
PROFILE   = FALSE
COMP	  = xlC
FCOMP     = xlf
COMP      = CC
FCOMP     = ftn
COMP      = Intel
FCOMP     = Intel
COMP      = g++
FCOMP     = g77
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

PBOXLIB_HOME = ..
TOP = $(PBOXLIB_HOME)

include ../mk/Make.defs

EBASE = amrvis
HERE = .

INCLUDE_LOCATIONS += $(HERE)
INCLUDE_LOCATIONS += ../BoxLib

DEFINES += -DBL_PARALLEL_IO

############################################### x includes and libraries
ifeq ($(MACHINE), OSF1)
  LIBRARIES += -lXm -lXt -lX11
endif

#USE_XT3=TRUE
ifeq ($(USE_XT3),TRUE)
  CXX = pgCC
  FC  = pgf77
  fC  = pgf77
  FOPTF = -O
  CXXOPTF = -O
  CPPFLAGS += -DBL_PGI
  LIBRARY_LOCATIONS += /usr/X11R6/lib64
# LIBRARIES += -lpgf90 -lpgf90_rpm1 -lpgf902 -lpgf90rtl -lpgftnrtl 
  LIBRARIES += -lpgftnrtl 
endif

# Joe Grcar 10/29/05: per Vince, these 4 lines are needed on davinci
# INCLUDE_LOCATIONS += /usr/common/graphics/openmotif/include
# INCLUDE_LOCATIONS += /usr/common/graphics/openmotif/include/Xm
# LIBRARY_LOCATIONS += /usr/X11R6/lib 
# LIBRARY_LOCATIONS += /usr/common/graphics/openmotif/lib

ifeq ($(MACHINE), Darwin)
  INCLUDE_LOCATIONS += /usr/X11R6/include
  LIBRARY_LOCATIONS += /usr/X11R6/lib
# LIBRARIES += -lXm -lXp -lXt -lXext -lSM -lICE -lXpm -lX11
  LIBRARIES += -lXm -lXp -lXt                         -lX11
  LDFLAGS   += -bind_at_load
endif
ifeq ($(MACHINE), Linux)
  INCLUDE_LOCATIONS += /usr/X11R6/include
  LIBRARY_LOCATIONS += /usr/X11R6/lib
  LIBRARIES += -lXm -lXp -lXt -lXext -lSM -lICE -lXpm -lX11
  # Joe Grcar 1/9/03: per Vince, the following line is needed on battra
  # so I have left it here in commented-out form.
  # LIBRARIES += -LlibXm.so.2.1
endif

ifeq ($(MACHINE), AIX)
  INCLUDE_LOCATIONS += /usr/include/X11
  INCLUDE_LOCATIONS += /usr/include/Xm
  #INCLUDE_LOCATIONS += /usr/include/X11/Xaw
  LIBRARIES += -lXm -lXt -lX11
  DEFINES += -D_ALL_SOURCE
endif

ifeq ($(MACHINE),CRAYX1)
  INCLUDE_LOCATIONS += /opt/ctl/motif/2.1.0.0/include
  LIBRARY_LOCATIONS += /opt/ctl/motif/2.1.0.0/lib
  LIBRARIES += -lXm
  LIBRARIES += -lXp
  LIBRARIES += -lXext
  LIBRARIES += -lSM
  LIBRARIES += -lICE
  LIBRARIES += -lXt
  LIBRARIES += -lX11
endif

ifeq ($(MACHINE), CYGWIN_NT)
  #INCLUDE_LOCATIONS += /cygdrive/c/usr/X11R6/include
  #LIBRARY_LOCATIONS += /cygdrive/c/usr/X11R6/lib
  INCLUDE_LOCATIONS += /usr/X11R6/include
  LIBRARY_LOCATIONS += /usr/X11R6/lib
  #CXXFLAGS += -fpermissive
  #CXXFLAGS += -x c++
#  LDFLAGS += -noinhibit-exec
  LIBRARIES += -lXm -lXt -lSM -lICE -lXpm -lX11
endif

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
    #VOLPACKDIR = ../../volpack-1.0b3
    #VOLPACKDIR = $(PBOXLIB_HOME)/volpack
    VOLPACKDIR = ../volpack
    #VOLPACKDIR = ../../volpack.test
    #VOLPACKDIR = ../../volpack
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
    #FDEBF += -fpe0
    FOPTF  = -fast -O5 -tune ev5
  endif
endif

#FDEBF += -fpe1
#FOPTF  = -fpe1

#XTRALIBS += 

include $(HERE)/Make.package
#include $(PBOXLIB_HOME)/pBoxLib_2/Make.package
include $(PBOXLIB_HOME)/BoxLib/Make.package

#vpath %.cpp $(HERE) ../pBoxLib_2
#vpath %.H $(HERE) ../pBoxLib_2
vpath %.cpp $(HERE) ../BoxLib
vpath %.H $(HERE) ../BoxLib
vpath %.F $(HERE) ../BoxLib
vpath %.f $(HERE) ../BoxLib
vpath %.a $(LIBRARY_LOCATIONS)

all: $(executable)

include ../mk/Make.rules
