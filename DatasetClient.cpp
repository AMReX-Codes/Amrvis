// ---------------------------------------------------------------
// DatasetClient.cpp
// ---------------------------------------------------------------
#include <climits>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include <DatasetClient.H>
#include <AMReX_Box.H>
#include <AMReX_FArrayBox.H>
#include <AMReX_MultiFab.H>
#ifdef BL_ARRAYVIEW_TAGBOX
#include <AMReX_TagBox.H>
#endif

const int MAXBUFSIZE  = 1024;
const int PORTOFFSET  = 5000;
const char *defaultFormat = "%7.5e";
const char *defaultLabel = " ";

using namespace amrex;

// -------------------------------------------------------------------
bool CreateSocket(int &newsocket) {
  int                       sockfd;
  struct sockaddr_in        serveraddr;
  char const               *serverhost = "localhost";
  struct hostent           *serverhostp;

  int GETUID_SERVER_PORT = getuid() + PORTOFFSET;  // use to contact the server

  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {    // create socket
    perror("Bad client socket create");
    return false;
  }
  //cout << "=== after opening socket." << std::endl;

  // set up the socket structures
  bzero((char *) &serveraddr, sizeof(struct sockaddr_in));
  serveraddr.sin_family = AF_INET;
  if((serverhostp = gethostbyname(serverhost)) == (struct hostent *) NULL) {
    std::cerr << "gethostbyname on " << serverhost << " failed" << std::endl;
    return false;
  }
  u_long sAddr(serveraddr.sin_addr.s_addr);
  bcopy(serverhostp->h_addr, (char *) &sAddr,
        serverhostp->h_length);
  serveraddr.sin_port = htons(GETUID_SERVER_PORT);

  // connect to the server
  if(connect(sockfd, (sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
    perror ("Bad client connect");
    return false;
  }
  //cout << "=== connection successful." << std::endl;

  newsocket = sockfd;
  return true;
}


// -------------------------------------------------------------------
bool SendString(int sockfd, const char *sendstring) {
  int count;
  char ptrbuffer[MAXBUFSIZE];

  if(send(sockfd, sendstring, strlen(sendstring), 0) < 0) {
    perror("Bad client sendstring send");
    return false;
  }

  // wait for acknowledgment
  if((count = recv(sockfd, ptrbuffer, MAXBUFSIZE, 0)) < 0) {
    perror("Bad sendstring ack.");
    return false;
  }
  ptrbuffer[count] = '\0';
  //cout << "<<< received sendstring ack:  " << ptrbuffer << std::endl;
  return true;
}


// -------------------------------------------------------------------
bool SendRealArray(int sockfd, Real *data[], int nvar,    // size nvar
                   const int *lodim, const int *hidim)    // size BL_SPACEDIM
{
  int                        count;
  char                        buffer[MAXBUFSIZE];
  char                        ptrbuffer[MAXBUFSIZE];

  IntVect ivlo(lodim);
  IntVect ivhi(hidim);
  Box dataBox(ivlo, ivhi);

  // --------------------------------------------------- send the box
  //cout << ">>> sending box." << std::endl;

  std::ostringstream bufferstream;

  bufferstream << dataBox;

  if(send(sockfd, bufferstream.str().c_str(), bufferstream.str().size(), 0) < 0) {
    perror("Bad client box send");
    return false;
  }

  // wait for acknowledgment
  if((count = recv(sockfd, buffer, MAXBUFSIZE, 0)) < 0) {
    perror("Bad box ack.");
    return false;
  }
  buffer[count] = '\0';
  //cout << "<<< received box ack:  " << buffer << std::endl;

  // --------------------------------------------------- send nComp
  //cout << ">>> sending nComp." << std::endl;
  sprintf(buffer, "%d", nvar);
  if(send(sockfd, buffer, strlen(buffer), 0) < 0) {
    perror("Bad client nComp send");
    return false;
  }

  // wait for acknowledgment
  if((count = recv(sockfd, buffer, MAXBUFSIZE, 0)) < 0) {
    perror("Bad nComp ack.");
    return false;
  }
  buffer[count] = '\0';
  //cout << "<<< received nComp ack:  " << buffer << std::endl;

  // --------------------------------------------------- send the data.
  //cout << ">>> sending data." << std::endl;

  long t_long = sizeof(Real) * dataBox.numPts();
  BL_ASSERT(t_long < INT_MAX);
  int totalDataBytes = int(t_long);
  int totalBytesSent, dataBytesRemaining;
  int dataBufferSize;
  char *getDataHere, *dataComponentStartingAddress;

  for(int dataComponent = 0; dataComponent < nvar; ++dataComponent) {
    //cout << "dataComponent = " << dataComponent << std::endl;
    totalBytesSent = 0;
    dataBytesRemaining = totalDataBytes;
    dataComponentStartingAddress = (char *) (data[dataComponent]);

    while(totalBytesSent < totalDataBytes) {  // send a chunk of data
      getDataHere = dataComponentStartingAddress + totalBytesSent;
      dataBufferSize = dataBytesRemaining;
      if((count = write(sockfd, getDataHere, dataBufferSize)) < 0) {
        perror("Bad client data send");
        return false;
      }
      //cout << "  bytes sent = " << count << std::endl;
      totalBytesSent        += count;
      dataBytesRemaining -= count;
    }  // end while
  }  // end for

  // --------------------------------------------------- send the pointer
  std::ostringstream ptrbufferstream;

  ptrbufferstream << data[0];
  if(send(sockfd, ptrbufferstream.str().c_str(), ptrbufferstream.str().size(), 0) < 0) {
    perror("Bad client data ptr send");
    return false;
  }

  // wait for acknowledgment
  if((count = recv(sockfd, ptrbuffer, MAXBUFSIZE, 0)) < 0) {
    perror("Bad data ptr ack.");
    return false;
  }
  ptrbuffer[count] = '\0';
  //cout << "<<< received data ptr ack:  " << ptrbuffer << std::endl;

  // --------------------------------------------------- done sending data

  return true;
}  // end SendRealArray




// -------------------------------------------------------------------
// pointer to fab interface
// -------------------------------------------------------------------
// -------------------------------------------------------------------
bool ArrayView(amrex::FArrayBox *debugFab) {
  return(ArrayViewFab(debugFab));
}


// -------------------------------------------------------------------
bool ArrayViewFab(amrex::FArrayBox *debugFab) {
  return( ArrayViewFabFormatLabel(debugFab, defaultFormat, "Fab") );
}


// -------------------------------------------------------------------
bool ArrayViewFabFormatLabel(amrex::FArrayBox *debugFab, const char *format,
                             const char *label)
{
  bool returnValue;
  int nvar = debugFab->nComp();
  if(nvar < 1) {
    std::cerr << "Error in ArrayView:  fab nComp < 1:  fab->nComp = " << nvar << std::endl;
    return false;
  }
  if( ! debugFab->box().ok()) {
    std::cerr << "Error in ArrayView:  bad fab box = " << debugFab->box() << std::endl;
    return false;
  }

  Real **dataArray = new Real*[nvar];
  for(int d = 0; d < nvar; ++d) {  // build the array of real pointers
    dataArray[d] = debugFab->dataPtr(d);  // dont assume contiguous
  }
  returnValue = ArrayViewRealPtrArrayNVarDims(dataArray, nvar,
                                debugFab->box().smallEnd().getVect(),
                                debugFab->box().bigEnd().getVect(), format, label);
  delete [] dataArray;
  return returnValue;
}




// -------------------------------------------------------------------
// pointer to MultiFab interface
// -------------------------------------------------------------------
// -------------------------------------------------------------------
bool ArrayViewMultiFab(amrex::MultiFab *debugMultiFab) {
  return (ArrayViewMultiFabFormatLabel(debugMultiFab, defaultFormat, "MultiFab"));
}


// -------------------------------------------------------------------
bool ArrayViewMultiFabElement(amrex::MultiFab *debugMultiFab, int element) {
  return( ArrayViewMultiFabElementFormatLabel(debugMultiFab, element,
                                              defaultFormat, "MultiFab element") );
}


// -------------------------------------------------------------------
bool ArrayViewMultiFabElementFormatLabel(amrex::MultiFab *debugMultiFab, int element,
                                         const char *format, const char *label)
{
  if( ! debugMultiFab->ok()) {
    std::cerr << "Error in ArrayViewMultiFabComp:  MultiFab is not ok()." << std::endl;
    return false;
  }
  if(element < 0 || element >= debugMultiFab->size()) {
    std::cerr << "Error in ArrayViewMultiFabElement:  element index is not" << std::endl;
    std::cerr << "  within range of MultiFab.length()." << std::endl;
    std::cerr << "  MultiFab.length() = " << debugMultiFab->size() << std::endl;
    std::cerr << "  Requested element = " << element << std::endl;
    return false;
  }

  return ( ArrayViewFabFormatLabel(&((*debugMultiFab)[element]), format, label) );
}



#ifdef BL_ARRAYVIEW_TAGBOX
// -------------------------------------------------------------------
// pointer to TagBox interface
// -------------------------------------------------------------------
// -------------------------------------------------------------------
bool ArrayViewTagBox(TagBox *debugTagBox) {
  bool returnValue;
  int nvar = debugTagBox->nComp();
  if(nvar < 1) {
    std::cerr << "Error in ArrayView:  fab nComp < 1:  fab->nComp = " << nvar << std::endl;
    return false;
  }
  if( ! debugTagBox->box().ok()) {
    std::cerr << "Error in ArrayView:  bad fab box = " << debugTagBox->box() << std::endl;
    return false;
  }

  // create a temp fab and put the TagBox values (ints) into it
  FArrayBox *debugFab = new FArrayBox(debugTagBox->box(), nvar);
  for(int nv = 0; nv < nvar; ++nv) {
    Real *debugFabPtr    = debugFab->dataPtr(nv);
    int  *debugTagBoxPtr = debugTagBox->dataPtr(nv);
    for(int i = 0; i < debugTagBox->box().numPts() ; ++i) {
      debugFabPtr[i] = (Real) debugTagBoxPtr[i];
    }
  }

  Real **dataArray = new Real*[nvar];
  for(int d = 0; d < nvar; ++d) {  // build the array of real pointers
    dataArray[d] = debugFab->dataPtr(d);  // dont assume contiguous
  }
  returnValue = ArrayViewRealPtrArrayNVarDims(dataArray, nvar,
                                debugFab->box().smallEnd().getVect(),
                                debugFab->box().bigEnd().getVect(),
                                "%3.0f", " TagBox ");
  delete [] dataArray;
  delete debugFab;
  return returnValue;
}


// -------------------------------------------------------------------
// pointer to TagBoxArray interface
// -------------------------------------------------------------------
// -------------------------------------------------------------------
bool ArrayViewTagBoxArray(TagBoxArray *debugTagBoxArray) {
  bool returnValue;
  int nvar = debugTagBoxArray->nComp();
  if(nvar < 1) {
    std::cerr << "Error in ArrayView:  fab nComp < 1:  fab->nComp = " << nvar << std::endl;
    return false;
  }
  if( ! debugTagBoxArray->ok()) {
    std::cerr << "Error in ArrayView:  bad TagBoxArray." << std::endl;
    return false;
  }

  // create a temp fab and put the TagBoxArray values (ints) into it
  MultiFab *debugMultiFab = new MultiFab(debugTagBoxArray->boxArray(),
                                         nvar, debugTagBoxArray->nGrow());
  for(int nfab = 0; nfab < debugTagBoxArray->length(); ++nfab) {
    FArrayBox &debugFab    = (*debugMultiFab)[nfab];
    TagBox    &debugTagBox = (*debugTagBoxArray)[nfab];
    for(int nv = 0; nv < nvar; ++nv) {
      Real *debugFabPtr    = debugFab.dataPtr(nv);
      int  *debugTagBoxPtr = debugTagBox.dataPtr(nv);
      for(int i = 0; i < debugTagBox.box().numPts() ; ++i) {
        debugFabPtr[i] = (Real) debugTagBoxPtr[i];
      }
    }
  }

  returnValue =  ArrayViewMultiFabFormatLabel(debugMultiFab,
                                              "%3.0f", " TagBoxArray ");
  delete debugMultiFab;
  return returnValue;
}
#endif



// -------------------------------------------------------------------
// pointer to real interface
// -------------------------------------------------------------------
// -------------------------------------------------------------------
bool ArrayViewReal(Real *data, const int *lodim, const int *hidim) {
  return ( ArrayViewRealFormatLabel(data, lodim, hidim,
                                    defaultFormat, "Real Array") );
}


// -------------------------------------------------------------------
bool ArrayViewRealFormatLabel(Real *data, const int *lodim, const int *hidim,
                         const char *format, const char *label)
{
  return ( ArrayViewRealNVarFormatLabel(data, 1, lodim, hidim, format, label) );
}


// -------------------------------------------------------------------
bool ArrayViewRealNVar(Real *data, int nvar, const int *lodim, const int *hidim) {
  return ( ArrayViewRealNVarFormatLabel(data, nvar, lodim, hidim,
                                   defaultFormat, "Real Array") );
}


// -------------------------------------------------------------------
bool ArrayViewRealNVarFormatLabel(Real *data, int nvar,
                         const int *lodim, const int *hidim,  // size BL_SPACEDIM
                         const char *format, const char *label)
{
  bool returnValue;

  if(data == NULL) {
    std::cerr << "Error in ArrayView:  data pointer == NULL" << std::endl;
    return false;
  }
  if(nvar < 1) {
    std::cerr << "Error in ArrayView:  nComp < 1:  nvar = " << nvar << std::endl;
    return false;
  }

  Real **dataArray = new Real*[nvar];
  long npts = 1; 
  for(int sd = 0; sd < BL_SPACEDIM; ++sd) {
    npts *= (hidim[sd] - lodim[sd] + 1);
  }

  char *tempCharPtr;
  for(int d = 0; d < nvar; ++d) {  // build the array of real pointers
    tempCharPtr  = ((char *) data);
    tempCharPtr += d * npts * sizeof(Real);
    dataArray[d] = (Real *) tempCharPtr;
  }
  returnValue = ArrayViewRealPtrArrayNVarDims(dataArray, nvar,
                                              lodim, hidim, format, label);
  delete [] dataArray;
  return returnValue;
}




#if (BL_SPACEDIM == 2)

// -------------------------------------------------------------------
bool ArrayViewRealDims(Real *data, int xlo, int ylo, int xhi, int yhi) {
  return ( ArrayViewRealDimsFormatLabel(data, xlo, ylo, xhi, yhi,
                                        defaultFormat, "Real data") );
}

// -------------------------------------------------------------------
bool ArrayViewRealDimsFormatLabel(Real *data, int xlo, int ylo, int xhi, int yhi,
                       const char *format, const char *label)
{
  return ( ArrayViewRealNVarDimsFormatLabel(data, 1, xlo, ylo, xhi, yhi,
                                            format, label) );
}

// -------------------------------------------------------------------
bool ArrayViewRealNVarDims(Real *data, int nvar,
                           int xlo, int ylo, int xhi, int yhi)
{
  return ( ArrayViewRealNVarDimsFormatLabel(data, nvar, xlo, ylo, xhi, yhi,
                                            defaultFormat, "Real data") );
}

// -------------------------------------------------------------------
bool ArrayViewRealNVarDimsFormatLabel(Real *data, int nvar,
                           int xlo, int ylo, int xhi, int yhi,
                           const char *format, const char *label)
{
  int lodims[BL_SPACEDIM], hidims[BL_SPACEDIM];
  if(data == NULL) {
    std::cerr << "Error in ArrayView:  data pointer == NULL" << std::endl;
    return false;
  }
  if(nvar < 1) {
    std::cerr << "Error in ArrayView:  nComp < 1:  nvar = " << nvar << std::endl;
    return false;
  }

  if(xlo > xhi) {
    std::cerr << "Error in ArrayView:  xlo > xhi:  " << xlo << " > " << xhi << std::endl;
    return false;
  }
  if(ylo > yhi) {
    std::cerr << "Error in ArrayView:  ylo > yhi:  " << ylo << " > " << yhi << std::endl;
    return false;
  }
  lodims[0] = xlo;
  lodims[1] = ylo;
  hidims[0] = xhi;
  hidims[1] = yhi;
  return( ArrayViewRealNVarFormatLabel(data, nvar, lodims, hidims, format, label) );
}


#else


// -------------------------------------------------------------------
bool ArrayViewRealDims(Real *data, int xlo, int ylo, int zlo,
                       int xhi, int yhi, int zhi)
{
  return ( ArrayViewRealDimsFormatLabel(data, xlo, ylo, zlo, xhi, yhi, zhi,
                                        defaultFormat, "Real data") );
}


// -------------------------------------------------------------------
bool ArrayViewRealDimsFormatLabel(Real *data,
                                  int xlo, int ylo, int zlo,
                                  int xhi, int yhi, int zhi,
                                  const char *format, const char *label)
{
  return ( ArrayViewRealNVarDimsFormatLabel(data, 1, xlo, ylo, zlo, xhi, yhi, zhi,
                                            format, label) );
}


// -------------------------------------------------------------------
bool ArrayViewRealNVarDims(Real *data, int nvar,
                           int xlo, int ylo, int zlo,
                           int xhi, int yhi, int zhi)
{
  return ( ArrayViewRealNVarDimsFormatLabel(data, nvar,
                                            xlo, ylo, zlo, xhi, yhi, zhi,
                                            defaultFormat, "Real data") );
}


// -------------------------------------------------------------------
bool ArrayViewRealNVarDimsFormatLabel(Real *data, int nvar,
                                      int xlo, int ylo, int zlo,
                                      int xhi, int yhi, int zhi,
                                      const char *format, const char *label)
{
  int lodims[BL_SPACEDIM], hidims[BL_SPACEDIM];

  if(data == NULL) {
    std::cerr << "Error in ArrayView:  data pointer == NULL" << std::endl;
    return false;
  }
  if(nvar < 1) {
    std::cerr << "Error in ArrayView:  nComp < 1:  nvar = " << nvar << std::endl;
    return false;
  }
  if(xlo > xhi) {
    std::cerr << "Error in ArrayView:  xlo > xhi:  " << xlo << " > " << xhi << std::endl;
    return false;
  }
  if(ylo > yhi) {
    std::cerr << "Error in ArrayView:  ylo > yhi:  " << ylo << " > " << yhi << std::endl;
    return false;
  }
  if(zlo > zhi) {
    std::cerr << "Error in ArrayView:  zlo > zhi:  " << zlo << " > " << zhi << std::endl;
    return false;
  }
  lodims[0] = xlo;
  lodims[1] = ylo;
  lodims[2] = zlo;
  hidims[0] = xhi;
  hidims[1] = yhi;
  hidims[2] = zhi;

  return( ArrayViewRealNVarFormatLabel(data, nvar, lodims, hidims, format, label) );
}

#endif


// -------------------------------------------------------------------
bool ArrayViewRealPtrArrayNVarDims(Real *data[], int nvar,    // size nvar
                         const int *lodim, const int *hidim,  // size BL_SPACEDIM
                         const char *format, const char *label)
{
  int         sockfd;

  if( ! CreateSocket(sockfd)) {
    return false;
  }

  // --------------------------------------------------- send data label
  if( ! SendString(sockfd, label)) {
    return false;
  }

  // --------------------------------------------------- send format
  if( ! SendString(sockfd, format)) {
    return false;
  }

  // --------------------------------------------------- send isMultiFab
  if( ! SendString(sockfd, "false")) {  // not a MultiFab
    return false;
  }

  // --------------------------------------------------- send nElements
  // dont send nElements

  // --------------------------------------------------- send the data
  return (SendRealArray(sockfd, data, nvar, lodim, hidim));

}  // end of function



// -------------------------------------------------------------------
bool ArrayViewMultiFabFormatLabel(amrex::MultiFab *multifab, const char *format,
                                  const char *label)
{
  int  sockfd;
  char buffer[MAXBUFSIZE];

  if( ! CreateSocket(sockfd)) {
    return false;
  }

  // --------------------------------------------------- send data label
  if( ! SendString(sockfd, label)) {
    return false;
  }

  // --------------------------------------------------- send format
  if( ! SendString(sockfd, format)) {
    return false;
  }

  // --------------------------------------------------- send isMultiFab
  if( ! SendString(sockfd, "true")) {  // this is a MultiFab
    return false;
  }

  // --------------------------------------------------- send nElements
  //cout << ">>> sending nElements." << std::endl;
  sprintf(buffer, "%d", multifab->size());
  if( ! SendString(sockfd, buffer)) {
    return false;
  }

  // --------------------------------------------------- send the data
  for(int element(0); element < multifab->size(); ++element) {
    // construct dataArray for this element
    FArrayBox &fab = (*multifab)[element];
    int nvar = fab.nComp();
    Real **dataArray = new Real*[nvar];
    for(int d(0); d < nvar; ++d) {  // build the array of Real *
      dataArray[d] = fab.dataPtr(d);  // dont assume contiguous
    }

    if( ! SendRealArray(sockfd, dataArray, nvar,
                        (fab.box()).loVect(), (fab.box()).hiVect()))
    {
      return false;
    }
    delete [] dataArray;
  }

  return true;

}  // end of function
// -------------------------------------------------------------------
// -------------------------------------------------------------------
