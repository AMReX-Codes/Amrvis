PRECISION = FLOAT
PRECISION = DOUBLE
PROFILE   = TRUE
PROFILE   = FALSE

# use mpKCC for the sp
COMP      = mpKCC

COMP      = g++
COMP      = KCC
DEBUG     = FALSE
DEBUG     = TRUE
DIM       = 2
DIM       = 3
NAMESPACE = FALSE
NAMESPACE = TRUE

STRICTLY  = FALSE
STRICTLY  = TRUE

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
INCLUDE_LOCATIONS += ../pBoxLib_2

############################################### define vince's home dir
ifeq ($(MACHINE), OSF1)
  VINCEDIR = /usr/people/vince
endif
ifeq ($(MACHINE), T3E)
  ifeq ($(WHICHT3E), NERSC)
    VINCEDIR = /u1/vince
  endif
  ifeq ($(WHICHT3E), NAVO)
    VINCEDIR = /home/Cvince
  endif
  ifeq ($(WHICHT3E), ARSC)
    VINCEDIR = /u2/wes/beckner
  endif
endif


DEFINES += -DBL_PARALLEL_IO

ifeq ($(MACHINE),OSF1)
  ifeq ($(COMP),KCC)
    CXXFLAGS += --diag_suppress 837
  endif
endif

############################################### x includes and libraries
ifeq ($(MACHINE), OSF1)
  LIBRARIES += -lXm -lXt -lX11
endif

ifeq ($(MACHINE), Linux)
  INCLUDE_LOCATIONS += /usr/X11R6/include
  LIBRARY_LOCATIONS += /usr/X11R6/lib
  LIBRARIES += -lXm -lXp -lXt -lXext -lSM -lICE -lXpm -lX11
endif

ifeq ($(MACHINE), AIX)
  INCLUDE_LOCATIONS += /usr/include/X11
  INCLUDE_LOCATIONS += /usr/include/Xm
  #INCLUDE_LOCATIONS += /usr/include/X11/Xaw
  LIBRARIES += -lXm -lXt -lX11
  DEFINES += -D_ALL_SOURCE
endif

ifeq ($(MACHINE), T3E)
  ifeq ($(WHICHT3E), NERSC)
    INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/X11
    INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/Xm
    INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/X11/Xaw
    LIBRARY_LOCATIONS += /opt/ctl/cvt/cvt/lib
    LIBRARIES += -lXm -lSM -lICE -lXt -lX11
  endif
  ifeq ($(WHICHT3E), ARSC)
    INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/X11
    INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/Xm
    INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/X11/Xaw
    LIBRARY_LOCATIONS += /opt/ctl/cvt/cvt/lib
    LIBRARIES += -lXm -lSM -lICE -lXt -lX11
  endif
endif


############################################### arrayview
ifeq (USE_ARRAYVIEW, TRUE)
  DEFINES += -DBL_USE_ARRAYVIEW
  #ARRAYVIEWDIR = $(VINCEDIR)/ArrayView
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
    USE_VOLRENDER = FALSE
  endif
  ifeq ($(USE_VOLRENDER), TRUE)
    DEFINES += -DBL_VOLUMERENDER
    #VOLPACKDIR = ../../volpack/volpack_cpp
    #DEFINES += -DBL_VOLPACK_NEW
    VOLPACKDIR = ../../volpack/volpack-1.0b3
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
    #FDEBF += -fpe0
    FOPTF  = -fast -O5 -tune ev5
  endif
endif

#FDEBF += -fpe1
#FOPTF  = -fpe1

############################################### 3rd analyzer
#CXXDEBF = +K0 --link_command_prefix 3rd
#LIBRARIES += -ldnet_stub

#CFLAGS += -g

#XTRALIBS += 

include $(HERE)/Make.package
include $(TOP)/pBoxLib_2/Make.package

vpath %.cpp $(HERE) ../pBoxLib_2
vpath %.H $(HERE) ../pBoxLib_2
vpath %.F $(HERE)
vpath %.a $(LIBRARY_LOCATIONS)

all: $(executable)

include ../mk/Make.rules
