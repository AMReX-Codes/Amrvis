PRECISION = FLOAT
PRECISION = DOUBLE
PROFILE   = TRUE
PROFILE   = FALSE

# use mpKCC for the sp
COMP      = mpKCC

COMP      = KCC
DEBUG     = TRUE
DEBUG     = FALSE
DIM       = 2
DIM       = 3

USE_ARRAYVIEW = TRUE
USE_ARRAYVIEW = FALSE

USE_MPI=FALSE
USE_MPI=TRUE

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

############################################### mpi definitions
MPI_HOME =

ifeq ($(USE_MPI), TRUE)
DEFINES += -DBL_USE_MPI
ifeq ($(MACHINE), OSF1)
MPI_HOME = /usr/local/mpi
endif
ifeq ($(MACHINE), AIX)
MPI_HOME = /usr/lpp/ppe.poe
endif
endif

ifeq ($(USE_MPI), TRUE)
ifeq ($(MACHINE), OSF1)
INCLUDE_LOCATIONS += $(MPI_HOME)/include
LIBRARY_LOCATIONS += $(MPI_HOME)/lib/alpha/ch_p4
endif
ifeq ($(MACHINE), AIX)
INCLUDE_LOCATIONS += $(MPI_HOME)/include
LIBRARY_LOCATIONS += $(MPI_HOME)/lib
endif
endif

ifeq ($(USE_MPI), TRUE)
ifeq ($(USE_UPSHOT), TRUE)
LIBRARIES += -llmpi -lpmpi
endif
LIBRARIES += -lmpi
endif



DEFINES += -DBL_PARALLEL_IO
ifeq ($(COMP),KCC)
#DEFINES += -DBL_USE_NEW_HFILES
ifeq ($(KCC_VERSION),3.3)
CXXFLAGS+= --diag_suppress 837
CXXOPTF += -Olimit 2400
endif
endif

ifeq ($(COMP),g++)
DEFINES += -DBL_USE_NEW_HFILES
endif


############################################### x includes and libraries
ifeq ($(MACHINE), OSF1)
INCLUDE_LOCATIONS += /usr/include/X11 /usr/include/Xm /usr/include/X11/Xaw
LIBRARIES += -lXm -lXt -lX11
endif

ifeq ($(MACHINE), AIX)
#INCLUDE_LOCATIONS += /usr/include/X11 /usr/include/Xm /usr/include/X11/Xaw
INCLUDE_LOCATIONS += /usr/include/X11 /usr/include/Xm
LIBRARIES += -lXm -lXt -lX11
DEFINES += -D_ALL_SOURCE
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
ifeq ($(MACHINE), AIX)
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

FDEBF += -C -fpe0

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

