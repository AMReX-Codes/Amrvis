PRECISION = FLOAT
PRECISION = DOUBLE
PROFILE   = TRUE
PROFILE   = FALSE

COMP      = KCC
DEBUG     = TRUE
DEBUG     = FALSE
DIM       = 2
DIM       = 3

USE_ARRAYVIEW = TRUE
USE_ARRAYVIEW = FALSE

USE_BSP=FALSE
USE_BSP=TRUE

USE_VOLRENDER = FALSE
USE_VOLRENDER = TRUE

USE_PARALLELVOLRENDER = TRUE
USE_PARALLELVOLRENDER = FALSE

PBOXLIB_HOME = ..
include ../mk/Make.defs

EBASE = amrvis
HERE = .

INCLUDE_LOCATIONS += $(HERE)
INCLUDE_LOCATIONS += ../pBoxLib_2 ../amrlib

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
endif

############################################### bsp definitions
ifeq ($(USE_BSP), TRUE)
DEFINES += -DBL_USE_BSP
BSP_HOME = $(VINCEDIR)/BSP
INCLUDE_LOCATIONS += $(BSP_HOME)/include
LIBRARY_LOCATIONS += $(BSP_HOME)/lib/$(BSP_MACHINE)
LIBRARY_LOCATIONS += $(BSP_HOME)/lib/$(BSP_MACHINE)/$(BSP_DEVICE)

ifeq ($(MACHINE), T3E)
DEFINES += -DBL_T3E_$(WHICHT3E)
endif

ifeq ($(DEBUG), TRUE)
LIBRARIES += -lbspcore_O0 -lbsplevel1_O0
else
ifeq ($(BSP_MACHINE), OSF1)
LIBRARIES += -lbspcore_O2 -lbsplevel1_O0
else
LIBRARIES += -lbspcore_O2 -lbsplevel1_O2
endif
endif

# exception library (for newest bsplib)
ifeq ($(BSP_MACHINE), OSF1)
LIBRARY_LOCATIONS += /usr/ccs/lib/cmplrs/cc
LIBRARIES += -lexc
endif

endif
# end if(USE_BSP == TRUE)


DEFINES += -DBL_PARALLEL_IO


############################################### x includes and libraries
ifeq ($(MACHINE), OSF1)
INCLUDE_LOCATIONS += /usr/include/X11 /usr/include/Xm /usr/include/X11/Xaw
LIBRARIES += -lXm -lXt -lX11
endif

ifeq ($(MACHINE), T3E)
ifeq ($(WHICHT3E), NERSC)
INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/X11
INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/Xm
INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/X11/Xaw
LIBRARY_LOCATIONS += /opt/ctl/cvt/ctv/lib
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
ifeq ($(USE_VOLRENDER), TRUE)
DEFINES += -DBL_VOLUMERENDER
VOLPACKDIR = $(VINCEDIR)/volpack
INCLUDE_LOCATIONS += $(VOLPACKDIR)
LIBRARY_LOCATIONS += $(VOLPACKDIR)/lib
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
FDEBF += -C -fpe2
FOPTF  = -fast -O5 -tune ev5
endif
endif

############################################### 3rd analyzer
#CXXDEBF = +K0 --link_command_prefix 3rd
#LIBRARIES += -ldnet_stub

CFLAGS += -g

XTRALIBS +=

include $(HERE)/Make.package

vpath %.cpp $(HERE) ../pBoxLib_2 ../amrlib
vpath %.F $(HERE)
vpath %.a $(LIBRARY_LOCATIONS)

all: $(executable)

include ../mk/Make.rules

