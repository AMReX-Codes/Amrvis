PRECISION = FLOAT
PRECISION = DOUBLE
PROFILE   = TRUE
PROFILE   = FALSE

COMP      = KCC
DEBUG     = FALSE
DEBUG     = TRUE
DIM       = 2
DIM       = 3

include ../mk/Make.defs

#USE_ARRAYVIEW = TRUE
ifeq ($(PROFILE), FALSE)
ifeq ($(DEBUG), TRUE)
USE_ARRAYVIEW = FALSE
endif
endif
USE_ARRAYVIEW = FALSE

USE_BSP=FALSE
USE_BSP=TRUE

EBASE = amrvis
HERE = .

ifeq ($(USE_BSP), TRUE)
DEFINES += -DBL_USE_BSP
ifeq ($(BSP_MACHINE), OSF1)
BSP_HOME = /usr/people/vince/Parallel/BSP/BSP
endif
ifeq ($(MACHINE), T3E)
ifeq ($(WHICHT3E), NERSC)
DEFINES += -DBL_T3E_NERSC
BSP_HOME = /u1/vince/BSP
endif
ifeq ($(WHICHT3E), NAVO)
DEFINES += -DBL_T3E_NAVO
BSP_HOME = /home/Cvince/BSP
endif
endif
endif


ifeq ($(MACHINE), OSF1)
INCLUDE_LOCATIONS += /usr/include/X11 /usr/include/Xm /usr/include/X11/Xaw
endif

ifeq ($(MACHINE), T3E)
ifeq ($(WHICHT3E), NERSC)
INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/X11
INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/Xm
INCLUDE_LOCATIONS += /opt/ctl/cvt/3.1.0.0/include/X11/Xaw
LIBRARY_LOCATIONS += /opt/ctl/cvt/ctv/lib
endif
endif

INCLUDE_LOCATIONS += $(HERE)
INCLUDE_LOCATIONS += ../pBoxLib_2 ../amrlib

###### arrayview
#USE_ARRAYVIEW = TRUE
ifeq (USE_ARRAYVIEW, TRUE)
DEFINES += -DBL_USE_ARRAYVIEW
#ARRAYVIEWDIR = /usr/people/vince/Visualization/ArrayView
ARRAYVIEWDIR = .
INCLUDE_LOCATIONS += $(ARRAYVIEWDIR)
#LIBRARY_LOCATIONS += $(ARRAYVIEWDIR)
#LIBRARIES += -larrayview$(DIM)d.$(machineSuffix)
endif

ifeq ($(DIM), 3)
DEFINES += -DBL_VOLUMERENDER
VINCEDIR = /usr/people/vince
VOLPACKDIR = $(VINCEDIR)/Visualization/VolPack/volpack-1.0b3
INCLUDE_LOCATIONS += $(VOLPACKDIR)
LIBRARY_LOCATIONS += $(VOLPACKDIR)/lib
LIBRARIES += -lvolpack
#DEFINES += -DVOLUMEBOXES
endif

ifeq ($(MACHINE), OSF1)
LIBRARIES += -lXm -lXt -lX11
endif

ifeq ($(MACHINE), T3E)
ifeq ($(WHICHT3E), NERSC)
LIBRARIES += -lXm -lSM -lICE -lXt -lX11
endif
endif

#DEFINES += -DSCROLLBARERROR
#DEFINES += -DFIXDENORMALS

ifeq ($(USE_BSP), TRUE)
INCLUDE_LOCATIONS += $(BSP_HOME)/include
LIBRARY_LOCATIONS += $(BSP_HOME)/lib/$(BSP_MACHINE)
LIBRARY_LOCATIONS += $(BSP_HOME)/lib/$(BSP_MACHINE)/$(BSP_DEVICE)
endif

DEFINES += -DBL_PARALLEL_IO

ifeq ($(USE_BSP), TRUE)
ifeq ($(DEBUG), TRUE)
LIBRARIES += -lbspcore_O0 -lbsplevel1_O0
else
ifeq ($(BSP_MACHINE), OSF1)
LIBRARIES += -lbspcore_O2 -lbsplevel1_O0
else
LIBRARIES += -lbspcore_O2 -lbsplevel1_O2
endif
endif
endif
#
# exception library (for newest bsplib)
#
ifeq ($(BSP_MACHINE), OSF1)
LIBRARY_LOCATIONS += /usr/ccs/lib/cmplrs/cc
LIBRARIES += -lexc
endif


# if we are using float override FOPTF which sets -real_size 64
ifeq ($(PRECISION), FLOAT)
ifeq ($(MACHINE), OSF1)
FDEBF += -C -fpe2
FOPTF    = -fast -O5 -tune ev5
endif
endif

###### 3rd analyzer
#CXXDEBF = +K0 --link_command_prefix 3rd
#LIBRARIES += -ldnet_stub

###### threads
#DEFINES  += -DUSETHREADS -D_EXC_NO_EXCEPTIONS_
#DEFINES  += -DUSETHREADS
#LIBRARIES += -lpthreads -lmach

XTRALIBS +=

include $(HERE)/Make.package

vpath %.cpp $(HERE) ../pBoxLib_2 ../amrlib
vpath %.F $(HERE)
vpath %.a $(LIBRARY_LOCATIONS)

all: $(executable)

pAmrvistar:
	gtar -cvf pAmrvis.tar \
	GNUmakefile Make.package \
	*.H *.cpp *.F \
	Palette vpramps.dat amrvis.defaults

include ../mk/Make.rules

