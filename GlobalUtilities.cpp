// ---------------------------------------------------------------
// GlobalUtilities.cpp
// ---------------------------------------------------------------
#include <GlobalUtilities.H>
#include <AMReX_FArrayBox.H>
#include <AMReX_FabConv.H>
#include <AMReX_VisMF.H>
#include <fstream>
#include <iostream>
using std::ifstream;
using std::ofstream;
using std::cout;
using std::cerr;
using std::endl;

#include <PltApp.H>
#include <AMReX_ParallelDescriptor.H>

extern void PrintProfParserBatchUsage(std::ostream &os);

using namespace amrex;

const int DEFAULTMAXPICTURESIZE = 600000;

int boundaryWidth;
int skipPltLines;
int boxColor;
int maxPictureSize;
Amrvis::FileType fileType;
bool bAnimation;
bool bAnnotated;
bool bCacheAnimFrames;
Vector<string> comlinefilename;
string initialDerived;
string initialFormat;
string initialPalette;
string initialLightingFile;
bool startWithValueModel;
int fileCount;
int sleepTime;
bool givenBox;
bool givenBoxSlice;
bool createSWFData;
bool makeSWFLight;
bool lowBlack;
bool usePerStreams;
bool dumpSlices;
bool sliceAllVars;
bool givenFilename;
Box comlinebox;
bool verbose;
Vector< std::list<int> > dumpSliceList;
bool specifiedMinMax;
Real specifiedMin;
Real specifiedMax;
bool useMaxLevel(false);
int  maxLevel(-1);
int  maxPaletteIndex;
bool SGIrgbfile(false);
int  fabIOSize;
bool bShowBody(true);
Real bodyOpacity(0.05);
bool givenInitialPlanes(false);
IntVect ivInitialPlanes;
bool givenInitialPlanesReal(false);
Vector< Real > ivInitialPlanesReal;
AVGlobals::ENUserVectorNames givenUserVectorNames(AVGlobals::enUserNone);
Vector<string> userVectorNames(BL_SPACEDIM);
bool newPltSet(false);

const char *FileTypeString[] = {
  "invalidtype", "fab", "multifab", "newplt", "profdata"
};


// -------------------------------------------------------------------
const AVGlobals::ENUserVectorNames &AVGlobals::GivenUserVectorNames() {
  return givenUserVectorNames;
}


// -------------------------------------------------------------------
const Vector<string> &AVGlobals::UserVectorNames() {
  return userVectorNames;
}


// -------------------------------------------------------------------
const string &AVGlobals::GetInitialDerived() {
  return initialDerived;
}


// -------------------------------------------------------------------
void AVGlobals::SetInitialDerived(const string &initialderived) {
  initialDerived = initialderived;
}


// -------------------------------------------------------------------
const string &AVGlobals::GetPaletteName() {
  return initialPalette;
}


// -------------------------------------------------------------------
const string &AVGlobals::GetLightingFileName() {
  return initialLightingFile;
}


// -------------------------------------------------------------------
void AddSlices(int dir, char *sliceset) {
  int rangeStart(-1), rangeEnd(-1), slice(-1);
  bool bRangeSpecified(false);
  dumpSlices = true;

  for(int ipos(1); ipos <= strlen(sliceset); ++ipos) { // skip first char for neg
    if(sliceset[ipos] == '-') {  // we have a range of values specified
      bRangeSpecified = true;
      rangeStart = atoi(sliceset);
      rangeEnd   = atoi(sliceset + ipos + 1);
      sliceset[ipos] = '\0';
      break;
    }
  }

  list<int>::iterator dsliter;
  if(bRangeSpecified) {
    if(BL_SPACEDIM == 2 && dir == Amrvis::ZDIR) {
      slice = 0;
      dsliter = find(dumpSliceList[dir].begin(), dumpSliceList[dir].end(), slice);
      if(dsliter == dumpSliceList[dir].end()) {
        dumpSliceList[dir].push_back(slice);
      }
    } else {
      for(slice = rangeStart; slice <= rangeEnd; ++slice) {
        dsliter = find(dumpSliceList[dir].begin(),dumpSliceList[dir].end(),slice);
        if(dsliter == dumpSliceList[dir].end()) {
          dumpSliceList[dir].push_back(slice);
        }
      }
    }
  } else {
    slice = atoi(sliceset);
    if(BL_SPACEDIM == 2 && dir == Amrvis::ZDIR) {
      slice = 0;
    }
    dsliter =  find(dumpSliceList[dir].begin(),dumpSliceList[dir].end(),slice);
    if(dsliter == dumpSliceList[dir].end()) {
      dumpSliceList[dir].push_back(slice);
    }
  }
}


// -------------------------------------------------------------------
bool AVGlobals::ReadLightingFile(const string &lightdefaultsFile,
                      Real &ambientDef, Real &diffuseDef, Real &specularDef,
                      Real &shinyDef,
                      Real &minRayOpacityDef, Real &maxRayOpacityDef)
{
  char defaultString[Amrvis::LINELENGTH], cRealIn[Amrvis::LINELENGTH];
  // try to find the defaultsFile
  char buffer[Amrvis::BUFSIZE];

  ifstream defs(lightdefaultsFile.c_str());

  if(defs.fail()) {
    if(ParallelDescriptor::IOProcessor()) {
      cout << "Cannot find lighting defaults file:  " << lightdefaultsFile << endl;
    }
    return false;
  } else {
    if(ParallelDescriptor::IOProcessor()) {
      cout << "Reading lighting defaults from:  " << lightdefaultsFile << endl;
    }

    ws(defs);
    defs.getline(buffer, Amrvis::BUFSIZE, '\n');

    while( ! defs.eof()) {
      sscanf(buffer,"%s", defaultString);

      if(defaultString[0] != '#') {  // a comment starts with #
        if(strcmp(defaultString,"ambient") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          ambientDef = atof(cRealIn);
        } else if(strcmp(defaultString, "diffuse") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          diffuseDef = atof(cRealIn);
        } else if(strcmp(defaultString, "specular") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          specularDef = atof(cRealIn);
        } else if(strcmp(defaultString, "shiny") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          shinyDef = atof(cRealIn);
        } else if(strcmp(defaultString, "minRayOpacity") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          minRayOpacityDef = atof(cRealIn);
        } else if(strcmp(defaultString, "maxRayOpacity") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          maxRayOpacityDef = atof(cRealIn);
        }
      }
      defs.getline(buffer, Amrvis::BUFSIZE, '\n');
    }
  }
  return true;
}


// -------------------------------------------------------------------
bool AVGlobals::WriteLightingFile(const string &lightdefaultsFile,
                       const Real &ambientDef, const Real &diffuseDef,
		       const Real &specularDef, const Real &shinyDef,
                       const Real &minRayOpacityDef, const Real &maxRayOpacityDef)
{

  // the format of this file is:
  //ambient 0.42
  //diffuse 0.41
  //specular 0.40
  //shiny 12.0
  //minRayOpacity 0.04
  //maxRayOpacity 0.96

  if(ParallelDescriptor::IOProcessor()) {
    ofstream defs(lightdefaultsFile.c_str());
    if(defs.fail()) {
      cerr << "***** Error writing lighting file:  filename = "
	   << lightdefaultsFile << endl;
      return false;
    } else {
      defs << "ambient " << ambientDef << endl;
      defs << "diffuse " << diffuseDef << endl;
      defs << "specular " << specularDef << endl;
      defs << "shiny " << shinyDef << endl;
      defs << "minRayOpacity " << minRayOpacityDef << endl;
      defs << "maxRayOpacity " << maxRayOpacityDef << endl;
      defs.close();
    }
  }
  return true;
}


// -------------------------------------------------------------------
void AVGlobals::GetDefaults(const string &defaultsFile) {
  char buffer[Amrvis::BUFSIZE];
  char defaultString[Amrvis::LINELENGTH];
  char tempString[Amrvis::LINELENGTH];
  char tempStringSDim[BL_SPACEDIM][Amrvis::LINELENGTH];
  int  tempInt;

  // standard defaults
  PltApp::SetDefaultPalette("Palette");
  PltApp::SetDefaultLightingFile("amrvis.lighting");
  initialPalette = "Palette";
  PltApp::SetInitialDerived("density");
  PltApp::SetInitialScale(1);
  PltApp::SetInitialFormatString("%7.5f");
  PltApp::SetDefaultShowBoxes(true);
  PltApp::SetInitialWindowHeight(500);
  PltApp::SetInitialWindowWidth(850);
  PltApp::SetReserveSystemColors(24);
  Dataset::SetInitialColor(true);
  maxPictureSize = DEFAULTMAXPICTURESIZE;
  boundaryWidth = 0;
  skipPltLines = 0;
  maxPaletteIndex = 255;  // dont clip the top palette index (default)
  boxColor = -1;  // invalid
  fileType = Amrvis::NEWPLT;  // default
  fabIOSize = 0;
  lowBlack = false;
  usePerStreams = false;
  bShowBody = true;
  startWithValueModel = false;


  // try to find the defaultsFile

  string fullDefaultsFile;
  const string homePath = getenv("HOME");

  std::vector<string> fullDefaultsFileList;
#ifdef AMRVIS_CONFIG_FILEPATH
  string configFilepath(AMRVIS_CONFIG_FILEPATH);
  fullDefaultsFileList.push_back(configFilepath + "/" + defaultsFile);
#endif
  fullDefaultsFileList.push_back("./" + defaultsFile);
  fullDefaultsFileList.push_back(homePath + "/" + defaultsFile);
  fullDefaultsFileList.push_back(homePath + "/." + defaultsFile);

  ifstream defs;

  bool fileFound = false;

  for (string const& fileLoc : fullDefaultsFileList){

      defs.clear();  // must do this to clear the fail bit
      defs.open(fileLoc.c_str());
      if(defs.fail()) {
        if(ParallelDescriptor::IOProcessor()) {
          cout << "Cannot find amrvis defaults file:  " << fileLoc << endl;
        }
      } else {
        fullDefaultsFile = fileLoc;
        fileFound = true;
        break;
      }
  }

  if (!fileFound) {
    if(ParallelDescriptor::IOProcessor()) {
        cout << "Cannot find amrvis defaults file:  " << fullDefaultsFile << endl;
        cout << "Using standard defaults." << endl;
    }
    return;
  }

  if(ParallelDescriptor::IOProcessor()) {
    cout << "Reading defaults from:  " << fullDefaultsFile << endl;
  }


  ws(defs);
  defs.getline(buffer, Amrvis::BUFSIZE, '\n');

  while( ! defs.eof()) {
    sscanf(buffer,"%s", defaultString);

    if(defaultString[0] != '#') {  // a comment starts with #
      if(strcmp(defaultString,"palette") == 0) {
        sscanf(buffer, "%s%s",defaultString, tempString);
        PltApp::SetDefaultPalette(tempString);
        initialPalette = tempString;
      }
      else if(strcmp(defaultString,"lightingfile") == 0) {
        sscanf(buffer, "%s%s",defaultString, tempString);
        PltApp::SetDefaultLightingFile(tempString);
        initialLightingFile = tempString;
      }
      else if(strcmp(defaultString, "initialderived") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        PltApp::SetInitialDerived(tempString);
	initialDerived = tempString;
      }
      else if(strcmp(defaultString, "initialscale") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        PltApp::SetInitialScale(tempInt);
      }
      else if(strcmp(defaultString, "maxmenuitems") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        PltApp::SetInitialMaxMenuItems(tempInt);
      }
      else if(strcmp(defaultString, "numberformat") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        PltApp::SetInitialFormatString(tempString);
      }
      else if(strcmp(defaultString, "windowheight") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        PltApp::SetInitialWindowHeight(tempInt);
      }
      else if(strcmp(defaultString, "windowwidth") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        PltApp::SetInitialWindowWidth(tempInt);
      }
      else if(strcmp(defaultString, "maxpixmapsize") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        maxPictureSize = tempInt;
      }
      else if(strcmp(defaultString, "reservesystemcolors") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        PltApp::SetReserveSystemColors(tempInt);
      }
      else if(strcmp(defaultString, "extrapalettewidth") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        PltApp::SetExtraPaletteWidth(tempInt);
      }
      else if(strcmp(defaultString, "showboxes") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        if(*tempString == 't' || *tempString == 'T') {
          PltApp::SetDefaultShowBoxes(true);
        } else {
          PltApp::SetDefaultShowBoxes(false);
        }
      }
      else if(strcmp(defaultString, "showbody") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        if(*tempString == 'f' || *tempString == 'F') {
          bShowBody = false;
        }
      }
      else if(strcmp(defaultString, "ppm") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        if(*tempString == 't' || *tempString == 'T') {
          AVGlobals::ClearSGIrgbFile();
        }
      }
      else if(strcmp(defaultString, "rgb") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        if(*tempString == 't' || *tempString == 'T') {
          AVGlobals::SetSGIrgbFile();
        }
      }
      else if(strcmp(defaultString, "boundarywidth") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        boundaryWidth = tempInt;
      }
      else if(strcmp(defaultString, "maxlev") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        maxLevel = tempInt;
        useMaxLevel = true;
        if(maxLevel < 0) {
          cerr << "Error in defaults file:  invalid parameter for maxLevel:  "
               << maxLevel << endl;
          maxLevel = -1;
          useMaxLevel = false;
        }
      }
      else if(strcmp(defaultString, "sleep") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        sleepTime = tempInt;
      }
      else if(strcmp(defaultString, "skippltlines") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        skipPltLines = tempInt;
      }
      else if(strcmp(defaultString, "boxcolor") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        boxColor = tempInt;
      }
      else if(strcmp(defaultString, "datasetinitialcolor") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        if(*tempString == 't' || *tempString == 'T') {
          Dataset::SetInitialColor(true);
        } else {
          Dataset::SetInitialColor(false);
	}
	VisMF::SetUsePersistentIFStreams(usePerStreams);
      }
     else if(strcmp(defaultString, "filetype") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
	if(strcmp(tempString, "fab") == 0) {
          fileType = Amrvis::FAB;
	} else if(strcmp(tempString, "multifab") == 0) {
          fileType = Amrvis::MULTIFAB;
	} else if(strcmp(tempString, "newplt") == 0) {
          fileType = Amrvis::NEWPLT;
#ifdef BL_USE_PROFPARSER
	} else if(strcmp(tempString, "profdata") == 0) {
          fileType = Amrvis::PROFDATA;
#endif
	} else {  // error
	  cerr << "Error in defaults file:  invalid parameter for filetype:  "
	       << tempString << endl;
	}
      }
      else if(strcmp(defaultString, "cliptoppalette") == 0) {
        maxPaletteIndex = 254;  // clip the top palette index
      }
      else if(strcmp(defaultString, "fixdenormals") == 0) {
        RealDescriptor::SetFixDenormals();
      }
      else if(strcmp(defaultString, "lowblack") == 0) {
        lowBlack = true;
      }
#ifdef BL_OPTIO
      else if(strcmp(defaultString, "useperstreams") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        if(*tempString == 't' || *tempString == 'T') {
          usePerStreams = true;
        } else {
          usePerStreams = false;
	}
	VisMF::SetUsePersistentIFStreams(usePerStreams);
      }
#endif
      else if(strcmp(defaultString, "valuemodel") == 0) {
        startWithValueModel = true;
      }
#if (BL_SPACEDIM == 3)
      else if(strcmp(defaultString, "initplanes") == 0) {
	int tempX, tempY, tempZ;
        sscanf(buffer, "%s%d%d%d", defaultString, &tempX, &tempY, &tempZ);
        ivInitialPlanes.setVal(Amrvis::XDIR, tempX);
        ivInitialPlanes.setVal(Amrvis::YDIR, tempY);
        ivInitialPlanes.setVal(Amrvis::ZDIR, tempZ);
        givenInitialPlanes = true;
      }
      else if(strcmp(defaultString, "initplanesreal") == 0) {
        float tempX, tempY, tempZ;
        sscanf(buffer, "%s%e%e%e", defaultString, &tempX, &tempY, &tempZ);
        ivInitialPlanesReal.resize(BL_SPACEDIM);
        ivInitialPlanesReal[Amrvis::XDIR] = tempX;
        ivInitialPlanesReal[Amrvis::YDIR] = tempY;
        ivInitialPlanesReal[Amrvis::ZDIR] = tempZ;
        givenInitialPlanesReal = true;
      }
#endif
      else if(strcmp(defaultString, "setvelnames") == 0) {
#if (BL_SPACEDIM == 2)
        sscanf(buffer, "%s%s%s", defaultString,
	       tempStringSDim[0], tempStringSDim[1]);
#else
        sscanf(buffer, "%s%s%s%s", defaultString,
	       tempStringSDim[0], tempStringSDim[1], tempStringSDim[2]);
#endif
        givenUserVectorNames = enUserVelocities;
	for(int ii(0); ii < BL_SPACEDIM; ++ii) {
          userVectorNames[ii] = tempStringSDim[ii];
	}
        if(ParallelDescriptor::IOProcessor()) {
	  cout << "Using velnames:  ";
	  for(int ii(0); ii < BL_SPACEDIM; ++ii) {
            cout << userVectorNames[ii] << "  ";
	  }
	  cout << endl;
	}
      }
      else if(strcmp(defaultString, "setmomnames") == 0) {
#if (BL_SPACEDIM == 2)
        sscanf(buffer, "%s%s%s", defaultString,
	       tempStringSDim[0], tempStringSDim[1]);
#else
        sscanf(buffer, "%s%s%s%s", defaultString,
	       tempStringSDim[0], tempStringSDim[1], tempStringSDim[2]);
#endif
        givenUserVectorNames = enUserMomentums;
	for(int ii(0); ii < BL_SPACEDIM; ++ii) {
          userVectorNames[ii] = tempStringSDim[ii];
	}
        if(ParallelDescriptor::IOProcessor()) {
	  cout << "Using momnames:  ";
	  for(int ii(0); ii < BL_SPACEDIM; ++ii) {
            cout << userVectorNames[ii] << "  ";
	  }
	  cout << endl;
	}
      } else {
        if(ParallelDescriptor::IOProcessor()) {
          cout << "bad default argument:  " << defaultString << endl;
	}
      }
    }

    ws(defs);
    defs.getline(buffer, Amrvis::BUFSIZE, '\n');
  }
  defs.close();

}  // end GetDefaults


// -------------------------------------------------------------------
void PrintUsage(char *exname) {
 if(ParallelDescriptor::IOProcessor()) {
  cout << '\n';
  cout << exname << " [<options>]  [<filename(s)>]" << '\n';
  cout << '\n';


  cout << "  -help              print help and exit." << '\n';
  cout << "  file type flags:   -fab [-fb], -multifab [-mf], -profdata, -newplt (default)" << '\n';
  cout << "  -v                 verbose." << '\n';
  cout << "  -maxpixmapsize n   specify maximum allowed picture size in pixels."
       << '\n';
  cout << "  -subdomain _box_   specify subdomain box (on finest level)." << '\n';
  cout << "                     _box_ format:  lox loy loz hix hiy hiz." << '\n';
  cout << "  -skippltlines n    skip n lines at head of the plt file." << '\n';
  cout << "  -boxcolor n        set volumetric box color value [0,255]." << '\n';
#if(BL_SPACEDIM != 3)
  cout << "  -a                 load files as an animation." << '\n';
  cout << "  -aa                load files as an animation with annotations." << '\n';
  cout << "  -anc               load files as an animation, dont cache frames." << '\n';
#endif
  //cout << "  -sleep  n          specify sleep time (for attaching parallel debuggers)." << '\n';
  cout << "  -setvelnames xname yname (zname)   specify velocity names for" << '\n';
  cout << "                                     drawing vector plots." << '\n';
  cout << "  -setmomnames xname yname (zname)   specify momentum names for" << '\n';
  cout << "                                     drawing vector plots." << '\n';
  cout << "  -fabiosize nbits   write fabs with nbits (valid values are 1 (ascii), 8 or 32." << '\n';
  cout << "                     the default is native (usually 64)." << '\n';
  cout << "  -maxlev n          specify the maximum drawn level." << '\n';
  cout << "  -palette palname   set the initial palette." << '\n';
  cout << "  -lightingfile name set the initial lighting parameter file." << '\n';
  cout << "  -maxmenuitems n    set the max menu items per column to n." << '\n';
  cout << "  -initialderived dername   set the initial derived to dername." << '\n';
  cout << "  -initialscale n    set the initial scale to n." << '\n';
  cout << "  -showboxes tf      show boxes (the value of tf is true or false)." << '\n';
  cout << "  -showbody tf       show cartGrid body as body cells (def is true)." << '\n';
  cout << "  -numberformat fmt  set the initial format to fmt (ex:  %4.2f)." << '\n';
  cout << "  -lowblack          sets the lowest color in the palette to black." << '\n';
#ifdef BL_OPTIO
  cout << "  -useperstreams tf  use vismf persistent streams." << '\n';
#endif
  cout << "  -cliptoppalette    do not use the top palette index (for exceed)." << '\n';
  cout << "  -fixdenormals      always fix denormals when reading fabs." << '\n';
  cout << "  -ppm               output rasters using PPM file format." << '\n';
  cout << "  -rgb               output rasters using RGB file format." << '\n';
  cout << "  -useminmax min max       use min and max as the global min max values" << '\n';

#ifdef BL_VOLUMERENDER
  cout << "  -valuemodel        start with the value model for rendering." << '\n';
#endif
#if (BL_SPACEDIM == 3)
  cout << "  -initplanes xp yp zp     set initial planes" << '\n';
#endif

  cout << '\n';
  cout << "-------------------------------------------------------- batch functions" << '\n';
  cout << "  -xslice n          write a fab slice at x = n (n at the finest level)." << '\n';
  cout << "  -yslice n          write a fab slice at y = n (n at the finest level)." << '\n';
  cout << "  -zslice n          write a fab slice at z = n (n at the finest level)." << '\n';
  cout << "  -sliceallvars      write all fab variables instead of just initialderived." << '\n';
  cout << "  -boxslice _box_    write a fab on the box (box at the finest level)." << '\n';
  cout << "                     _box_ format:  lox loy (loz) hix hiy (hiz)." << '\n';
  cout << "                     example:  -boxslice 0 0 0 120 42 200." << '\n';
#ifdef BL_VOLUMERENDER
  cout << "  -makeswf_light     make volume rendering data using the" << '\n';
  cout << "                     current transfer function and write data" << '\n';
  cout << "                     to a file, using the lighting model." << '\n'
       << "                     note:  works in batch mode." << '\n';
  cout << "  -makeswf_value     same as above, with value model rendering." << '\n';
#endif
  cout << "------------------------------------------------------------------------" << '\n';

  cout << '\n';

#ifdef BL_USE_PROFPARSER
  cout << "----------------------------------------- amrproparser functions" << '\n';
  PrintProfParserBatchUsage(cout);
  cout << "----------------------------------------------------------------" << '\n';
#endif

  cout << "  <filename(s)>      must be included if box is specified." << '\n';
  cout << endl;

 }

  exit(0);
}


// -------------------------------------------------------------------
void AVGlobals::ParseCommandLine(int argc, char *argv[]) {
  char clsx[32], clsy[32];
  char clbx[32], clby[32];

#if (BL_SPACEDIM == 3)
  char clsz[32];
  char clbz[32];
  char clPlaneX[32], clPlaneY[32], clPlaneZ[32];
  bool givenInitialPlanesOnComline(false);
  char clPlaneXReal[32], clPlaneYReal[32], clPlaneZReal[32];
  bool givenInitialPlanesRealOnComline(false);
#endif

  givenFilename = false;
  givenBox = false;
  givenBoxSlice = false;
  createSWFData = false;
  makeSWFLight = false;
  dumpSlices = false;
  sliceAllVars = false;
  verbose = false;
  fileCount = 0;
  sleepTime = 0;
  bAnimation = false;
  bAnnotated = false;
  bCacheAnimFrames = false;
  specifiedMinMax = false;
  specifiedMin = 0.0;
  specifiedMax = 1.0;
  comlinefilename.resize(argc);  // slightly larger than eventual fileCount
  dumpSliceList.resize(3);  // always use 3 (zslice in 2d is image plane)

  int i;
  for(i = 1; i <= argc - 1; ++i) {
    if(strcmp(argv[i], "-maxpixmapsize") == 0) {
      if(argc-1<i+1 || atoi(argv[i+1]) == 0) {
        PrintUsage(argv[0]);
      } else {
	maxPictureSize = atoi(argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-bw") == 0) {
      if(argc-1<i+1 || atoi(argv[i+1]) < 0) {
        PrintUsage(argv[0]);
      } else {
	boundaryWidth = atoi(argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-maxlev") == 0) {
      int imaxlev = atoi(argv[i+1]);
      if(argc-1 < i+1) {
        PrintUsage(argv[0]);
      } else if(imaxlev < 0) {
        cerr << "Error:  invalid parameter for maxlev:  " << imaxlev << endl;
      } else {
        maxLevel = imaxlev;
        useMaxLevel = true;
      }
      ++i;
    } else if(strcmp(argv[i], "-sleep") == 0) {
      sleepTime = atoi(argv[i+1]);
      ++i;
    } else if(strcmp(argv[i], "-fabiosize") == 0) {
      int iSize = atoi(argv[i+1]);
      if(iSize == 1 || iSize == 8 || iSize == 32) {
        fabIOSize = iSize;
      } else {
	cerr << "Warning:  -fabiosize must be 1, 8 or 32.  Defaulting to native."
	     << endl;
        fabIOSize = 0;
      }
      ++i;
    } else if(strcmp(argv[i], "-skippltlines") == 0) {
      if(argc-1<i+1 || atoi(argv[i+1]) < 0) {
        PrintUsage(argv[0]);
      } else {
	skipPltLines = atoi(argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-boxcolor") == 0) {
      if(argc-1<i+1 || atoi(argv[i+1]) < 0) {
        PrintUsage(argv[0]);
      } else {
	boxColor = atoi(argv[i+1]);
      }
      ++i;
#    if (BL_SPACEDIM != 3)
    } else if(strcmp(argv[i],"-a") == 0) {
      bAnimation = true;
      bAnnotated = false;
      bCacheAnimFrames = true;
    } else if(strcmp(argv[i],"-aa") == 0) {
      bAnimation = true;
      bAnnotated = true;
      bCacheAnimFrames = true;
    } else if(strcmp(argv[i],"-anc") == 0) {
      bAnimation = true;
      bCacheAnimFrames = false;
      bAnnotated = false;
#   endif
    } else if(strcmp(argv[i],"-fab") == 0) {
      fileType = Amrvis::FAB;
    } else if(strcmp(argv[i],"-fb") == 0) {
      fileType = Amrvis::FAB;
    } else if(strcmp(argv[i],"-multifab") == 0) {
      fileType = Amrvis::MULTIFAB;
    } else if(strcmp(argv[i],"-mf") == 0) {
      fileType = Amrvis::MULTIFAB;
    } else if(strcmp(argv[i],"-newplt") == 0) {
      fileType = Amrvis::NEWPLT;
      newPltSet = true;
#ifdef BL_USE_PROFPARSER
    } else if(strcmp(argv[i],"-profdata") == 0) {
      fileType = Amrvis::PROFDATA;
#endif
    } else if(strcmp(argv[i],"-v") == 0) {
      verbose = true;
    } else if(strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "-subdomain") == 0) {
#    if (BL_SPACEDIM == 2 || BL_SPACEDIM == 1)
      if(argc-1<i+1 || ! strcpy(clsx, argv[i+1])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+2 || ! strcpy(clsy, argv[i+2])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+3 || ! strcpy(clbx, argv[i+3])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+4 || ! strcpy(clby, argv[i+4])) {
        PrintUsage(argv[0]);
      }
      i += 4;
      givenBox = true;
#    else
      if(argc-1<i+1 || ! strcpy(clsx, argv[i+1])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+2 || ! strcpy(clsy, argv[i+2])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+3 || ! strcpy(clsz, argv[i+3])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+4 || ! strcpy(clbx, argv[i+4])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+5 || ! strcpy(clby, argv[i+5])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+6 || ! strcpy(clbz, argv[i+6])) {
        PrintUsage(argv[0]);
      }
      i += 6;
      givenBox = true;
#   endif

    } else if(strcmp(argv[i], "-makeswf_light") == 0) {
        createSWFData = true;
	makeSWFLight = true;
    } else if(strcmp(argv[i], "-makeswf_value") == 0) {
        createSWFData = true;
	makeSWFLight = false;
    } else if(strcmp(argv[i], "-valuemodel") == 0) {
        startWithValueModel = true;
    } else if(strcmp(argv[i], "-lowblack") == 0) {
        lowBlack = true;
#ifdef BL_OPTIO
    } else if(strcmp(argv[i], "-useperstreams") == 0) {
      if(*argv[i+1] == 't' || *argv[i+1] == 'T') {
        usePerStreams = true;
      } else if(*argv[i+1] == 'f' || *argv[i+1] == 'F') {
        usePerStreams = false;
      } else {
        PrintUsage(argv[0]);
      }
      VisMF::SetUsePersistentIFStreams(usePerStreams);
      ++i;
#endif
    } else if(strcmp(argv[i], "-setvelnames") == 0) {
      if(argc-1<i+BL_SPACEDIM) {
        PrintUsage(argv[0]);
      } else {
        givenUserVectorNames = enUserVelocities;
	for(int ii(0); ii < BL_SPACEDIM; ++ii) {
          userVectorNames[ii] = argv[i+1 + ii];
	}
        if(ParallelDescriptor::IOProcessor()) {
	  cout << "Using velnames:  ";
	  for(int ii(0); ii < BL_SPACEDIM; ++ii) {
	    cout << userVectorNames[ii] << "  ";
	  }
	  cout << endl;
	}
      }
      i += BL_SPACEDIM;
    } else if(strcmp(argv[i], "-setmomnames") == 0) {
      if(argc-1<i+BL_SPACEDIM) {
        PrintUsage(argv[0]);
      } else {
        givenUserVectorNames = enUserMomentums;
	for(int ii(0); ii < BL_SPACEDIM; ++ii) {
          userVectorNames[ii] = argv[i+1 + ii];
	}
        if(ParallelDescriptor::IOProcessor()) {
	  cout << "Using momnames:  ";
	  for(int ii(0); ii < BL_SPACEDIM; ++ii) {
	    cout << userVectorNames[ii] << "  ";
	  }
	  cout << endl;
	}
      }
      i += BL_SPACEDIM;
    } else if(strcmp(argv[i], "-useminmax") == 0) {
      specifiedMinMax = true;
      if(argc-1<i+2) {
        PrintUsage(argv[0]);
      } else {
        specifiedMin = atof(argv[i+1]);
        specifiedMax = atof(argv[i+2]);
        if(ParallelDescriptor::IOProcessor()) {
	  cout << "******* using min max = " << specifiedMin
	       << "  " << specifiedMax << endl;
	}
      }
      i += 2;
    } else if(strcmp(argv[i],"-help") == 0) {
      PrintUsage(argv[0]);
    } else if(strcmp(argv[i], "-xslice") == 0) {
      if(argc-1<i+1) {
        PrintUsage(argv[0]);
      } else {
	AddSlices(Amrvis::XDIR, argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-yslice") == 0) {
      if(argc-1<i+1) {
        PrintUsage(argv[0]);
      } else {
	AddSlices(Amrvis::YDIR, argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-zslice") == 0) {
      if(argc-1<i+1) {
        PrintUsage(argv[0]);
      } else {
	AddSlices(Amrvis::ZDIR, argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-boxslice") == 0) {
#if (BL_SPACEDIM == 1)
      if(argc-1<i+1 || ! strcpy(clsx, argv[i+1])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+2 || ! strcpy(clbx, argv[i+2])) {
        PrintUsage(argv[0]);
      }
      i += 2;
      givenBoxSlice = true;
#elif (BL_SPACEDIM == 2)
      if(argc-1<i+1 || ! strcpy(clsx, argv[i+1])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+2 || ! strcpy(clsy, argv[i+2])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+3 || ! strcpy(clbx, argv[i+3])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+4 || ! strcpy(clby, argv[i+4])) {
        PrintUsage(argv[0]);
      }
      i += 4;
      givenBoxSlice = true;
#else
      if(argc-1<i+1 || ! strcpy(clsx, argv[i+1])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+2 || ! strcpy(clsy, argv[i+2])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+3 || ! strcpy(clsz, argv[i+3])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+4 || ! strcpy(clbx, argv[i+4])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+5 || ! strcpy(clby, argv[i+5])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+6 || ! strcpy(clbz, argv[i+6])) {
        PrintUsage(argv[0]);
      }
      i += 6;
      givenBoxSlice = true;
#endif
#if (BL_SPACEDIM == 3)
    } else if(strcmp(argv[i], "-initplanes") == 0) {
      if(argc-1<i+1 || ! strcpy(clPlaneX, argv[i+1])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+2 || ! strcpy(clPlaneY, argv[i+2])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+3 || ! strcpy(clPlaneZ, argv[i+3])) {
        PrintUsage(argv[0]);
      }
      i += 3;
      givenInitialPlanes = true;
      givenInitialPlanesOnComline = true;
    } else if(strcmp(argv[i], "-initplanesreal") == 0) {
      if(argc-1<i+1 || ! strcpy(clPlaneXReal, argv[i+1])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+2 || ! strcpy(clPlaneYReal, argv[i+2])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+3 || ! strcpy(clPlaneZReal, argv[i+3])) {
        PrintUsage(argv[0]);
      }
      i += 3;
      givenInitialPlanesReal = true;
      givenInitialPlanesRealOnComline = true;
#endif
    } else if(strcmp(argv[i],"-palette") == 0) {
      PltApp::SetDefaultPalette(argv[i+1]);
      initialPalette = argv[i+1];
      ++i;
    } else if(strcmp(argv[i],"-lightingfile") == 0) {
      PltApp::SetDefaultLightingFile(argv[i+1]);
      initialLightingFile = argv[i+1];
      ++i;
    } else if(strcmp(argv[i], "-initialderived") == 0) {
      PltApp::SetInitialDerived(argv[i+1]);
      initialDerived = argv[i+1];
      ++i;
    } else if(strcmp(argv[i], "-initialscale") == 0) {
      int tempiscale = atoi(argv[i+1]);
      if(argc-1 < i+1 || tempiscale < 1) {
        PrintUsage(argv[0]);
      }
      PltApp::SetInitialScale(tempiscale);
      ++i;
    } else if(strcmp(argv[i], "-maxmenuitems") == 0) {
      int tempimaxmenuitems = atoi(argv[i+1]);
      if(argc-1 < i+1 || tempimaxmenuitems < 1) {
        PrintUsage(argv[0]);
      }
      PltApp::SetInitialMaxMenuItems(tempimaxmenuitems);
      ++i;
    } else if(strcmp(argv[i], "-numberformat") == 0) {
      PltApp::SetInitialFormatString(argv[i+1]);
      ++i;
    } else if(strcmp(argv[i], "-showboxes") == 0) {
      if(*argv[i+1] == 't' || *argv[i+1] == 'T') {
        PltApp::SetDefaultShowBoxes(true);
      } else if(*argv[i+1] == 'f' || *argv[i+1] == 'F') {
        PltApp::SetDefaultShowBoxes(false);
      } else {
        PrintUsage(argv[0]);
      }
      ++i;
    } else if(strcmp(argv[i], "-showbody") == 0) {
      if(*argv[i+1] == 't' || *argv[i+1] == 'T') {
        bShowBody = true;
      } else if(*argv[i+1] == 'f' || *argv[i+1] == 'F') {
        bShowBody = false;
      } else {
        PrintUsage(argv[0]);
      }
      ++i;
    } else if(strcmp(argv[i], "-bodyopacity") == 0) {
      if(argc-1<i+1) {
        PrintUsage(argv[0]);
      } else {
        bodyOpacity = atof(argv[i+1]);
        if(ParallelDescriptor::IOProcessor()) {
	  cout << "******* using bodyOpacity = " << bodyOpacity << endl;
	}
      }
      ++i;
    } else if(strcmp(argv[i], "-cliptoppalette") == 0) {
      maxPaletteIndex = 254;  // clip the top palette index
    } else if(strcmp(argv[i], "-fixdenormals") == 0) {
      RealDescriptor::SetFixDenormals();
    } else if(strcmp(argv[i], "-ppm") == 0) {
	SGIrgbfile = false;
    } else if(strcmp(argv[i], "-rgb") == 0) {
	SGIrgbfile = true;
    } else if(strcmp(argv[i], "-sliceallvars") == 0) {
      sliceAllVars = true;
    } else if(i < argc) {
      if(fileType == Amrvis::MULTIFAB) {
        // delete the _H from the filename if it is there
	string tempfilename(argv[i]);
	int len(tempfilename.length());
	if(len >= 3) {
	  std::size_t found(tempfilename.find("_H", len - 2));
	  if(found != std::string::npos) {
	    len -= 2;
	  }
	}
        comlinefilename[fileCount] = tempfilename.substr(0, len);
      } else {
        comlinefilename[fileCount] = argv[i];
	// if filetypes not set, see if it is a fab file or profdie
	if( ! newPltSet && fileType != Amrvis::MULTIFAB && fileType != Amrvis::FAB) {
	  int len(comlinefilename[fileCount].length());
	  if(len >= 5) {  // a.fab or larger
	    {
	      std::size_t found(comlinefilename[fileCount].find(".fab", len - 4));
	      if(found != std::string::npos) {
	        fileType = Amrvis::FAB;
	      }
	    }
#ifdef BL_USE_PROFPARSER
	    {
	      if(IsProfDirName(comlinefilename[fileCount])) {
	        fileType = Amrvis::PROFDATA;
                if(ParallelDescriptor::IOProcessor()) {
		  cout << "Setting fileType to Amrvis::PROFDATA." << endl;
		}
	      }
	    }
#endif
	  }
	}
      }
      ++fileCount;
      givenFilename = true;
    } else {
      PrintUsage(argv[0]);
    }
  }  // end for(i...)

  if(givenBox && ! givenFilename) {
    PrintUsage(argv[0]);
  }

  if(givenBox || givenBoxSlice) {
#if (BL_SPACEDIM == 1)
      if(atoi(clsx) > atoi(clbx)) {
        if(ParallelDescriptor::IOProcessor()) {
          cout << "A sub-region box must be specified as:\n\t <small x> "
	       << "<big x>\n" << endl;
	}
        exit(0);
      }
      comlinebox.setSmall(Amrvis::XDIR, atoi(clsx));
      comlinebox.setBig(Amrvis::XDIR, atoi(clbx));
#elif (BL_SPACEDIM == 2)
      if(atoi(clsx) > atoi(clbx) || atoi(clsy) > atoi(clby)) {
        if(ParallelDescriptor::IOProcessor()) {
          cout << "A sub-region box must be specified as:\n\t <small x> <small y> "
	       << "<big x> <big y>\n" << endl;
	}
        exit(0);
      }
      comlinebox.setSmall(Amrvis::XDIR, atoi(clsx));
      comlinebox.setSmall(Amrvis::YDIR, atoi(clsy));
      comlinebox.setBig(Amrvis::XDIR, atoi(clbx));
      comlinebox.setBig(Amrvis::YDIR, atoi(clby));
#else
      if(atoi(clsx) > atoi(clbx) || atoi(clsy) > atoi(clby) ||
	 atoi(clsz) > atoi(clbz))
      {
        if(ParallelDescriptor::IOProcessor()) {
          cout << "A sub-region box must be specified as:\n\t<small x> <small y>"
	       << " <small z> <big x> <big y> <big z>" << endl;
	}
        exit(0);
      }
      comlinebox.setSmall(Amrvis::XDIR, atoi(clsx));
      comlinebox.setSmall(Amrvis::YDIR, atoi(clsy));
      comlinebox.setSmall(Amrvis::ZDIR, atoi(clsz));
      comlinebox.setBig(Amrvis::XDIR,   atoi(clbx));
      comlinebox.setBig(Amrvis::YDIR,   atoi(clby));
      comlinebox.setBig(Amrvis::ZDIR,   atoi(clbz));
#endif
  }

#if (BL_SPACEDIM == 3)
  if(givenInitialPlanesOnComline) {
    ivInitialPlanes.setVal(Amrvis::XDIR, atoi(clPlaneX));
    ivInitialPlanes.setVal(Amrvis::YDIR, atoi(clPlaneY));
    ivInitialPlanes.setVal(Amrvis::ZDIR, atoi(clPlaneZ));
  }
  if(givenInitialPlanesRealOnComline) {
    ivInitialPlanesReal.resize(BL_SPACEDIM);
    ivInitialPlanesReal[Amrvis::XDIR] = atof(clPlaneXReal);
    ivInitialPlanesReal[Amrvis::YDIR] = atof(clPlaneYReal);
    ivInitialPlanesReal[Amrvis::ZDIR] = atof(clPlaneZReal);
  }
#endif


  if(fileType == Amrvis::INVALIDTYPE) {
    amrex::Abort("Error:  invalid file type.  Exiting.");
  } else {
    if(ParallelDescriptor::IOProcessor()) {
      if(verbose) {
        if(ParallelDescriptor::IOProcessor()) {
          cout << ">>>>>>> Setting file type to "
               << FileTypeString[fileType] << "." << endl << endl;
	}
      }
    }
  }

}  // end ParseCommandLine


// -------------------------------------------------------------------
void AVGlobals::SetSGIrgbFile() { SGIrgbfile = true; }
void AVGlobals::ClearSGIrgbFile() { SGIrgbfile = false; }
bool AVGlobals::IsSGIrgbFile() { return SGIrgbfile; }

void AVGlobals::SetAnimation() { bAnimation = true; }
bool AVGlobals::IsAnimation()  { return bAnimation; }
void AVGlobals::SetAnnotated() { bAnnotated = true; }
bool AVGlobals::IsAnnotated()  { return bAnnotated; }
bool AVGlobals::CacheAnimFrames()  { return bCacheAnimFrames; }

Box AVGlobals::GetBoxFromCommandLine() { return comlinebox; }

void AVGlobals::SetMaxPictureSize(int maxpicsize) { maxPictureSize = maxpicsize; }
int AVGlobals::MaxPictureSize() { return maxPictureSize; }

int AVGlobals::MaxPaletteIndex() { return maxPaletteIndex; }

void AVGlobals::SetVerbose() { verbose = true; }
bool AVGlobals::Verbose()    { return verbose; }

int AVGlobals::GetFileCount() { return fileCount; }
int AVGlobals::SleepTime() { return sleepTime; }

int  AVGlobals::GetMaxLevel() { return maxLevel; }
bool AVGlobals::UseMaxLevel() { return useMaxLevel; }

void AVGlobals::SetShowBody(const bool bsb) { bShowBody = bsb; }
bool AVGlobals::GetShowBody()  { return bShowBody; }
Real AVGlobals::GetBodyOpacity()  { return bodyOpacity; }

const string &AVGlobals::GetComlineFilename(int i) { return comlinefilename[i]; }
Amrvis::FileType AVGlobals::GetDefaultFileType()   { return fileType;    }

bool AVGlobals::GivenBox()    { return givenBox;     }
bool AVGlobals::GivenBoxSlice() { return givenBoxSlice;     }
bool AVGlobals::CreateSWFData() { return createSWFData;  }
bool AVGlobals::MakeSWFLight() { return makeSWFLight; }
bool AVGlobals::StartWithValueModel() { return startWithValueModel; }
bool AVGlobals::LowBlack() { return lowBlack; }
bool AVGlobals::DumpSlices() { return dumpSlices;   }
bool AVGlobals::SliceAllVars() { return sliceAllVars; }

bool AVGlobals::GivenFilename() { return givenFilename; }

void AVGlobals::SetBoundaryWidth(int width) { boundaryWidth = width; }
int AVGlobals::GetBoundaryWidth() { return boundaryWidth;  }

void AVGlobals::SetSkipPltLines(int nlines) { skipPltLines = nlines; }
int AVGlobals::GetSkipPltLines() { return skipPltLines; }

void AVGlobals::SetBoxColor(int boxcolor) { boxColor = boxcolor; }
int AVGlobals::GetBoxColor() { return boxColor; }

Vector< list<int> > &AVGlobals::GetDumpSlices() { return dumpSliceList; }

int  AVGlobals::GetFabOutFormat() { return fabIOSize;  }

bool AVGlobals::UseSpecifiedMinMax() { return specifiedMinMax; }
void AVGlobals::SetSpecifiedMinMax(Real specifiedmin, Real specifiedmax) {
  specifiedMin = specifiedmin;
  specifiedMax = specifiedmax;
}

void AVGlobals::GetSpecifiedMinMax(Real &specifiedmin, Real &specifiedmax) {
  specifiedmin = specifiedMin;
  specifiedmax = specifiedMax;
}

bool AVGlobals::GivenInitialPlanes() { return givenInitialPlanes; }
IntVect AVGlobals::GetInitialPlanes() { return ivInitialPlanes; }

bool AVGlobals::GivenInitialPlanesReal() { return givenInitialPlanesReal; }
Vector <Real>  AVGlobals::GetInitialPlanesReal() { return ivInitialPlanesReal; }

// -------------------------------------------------------------------
/*int AVGlobals::CRRBetweenLevels(int fromlevel, int tolevel,
                                const Vector<int> &refratios)
{
  BL_ASSERT(fromlevel >= 0);
  BL_ASSERT(tolevel >= fromlevel);
  BL_ASSERT(tolevel <= refratios.size());
  int level, rr = 1;
  for(level = fromlevel; level < tolevel; ++level) {
    rr *= refratios[level];
  }
  return rr;
}
*/

// -------------------------------------------------------------------
int AVGlobals::DetermineMaxAllowableLevel(const Box &finestbox,
                                          int finestlevel,
                                          int maxpoints,
					  const Vector<int> &refratios)
{
  BL_ASSERT(finestlevel >= 0);
  BL_ASSERT(maxpoints >= 0);
  BL_ASSERT(finestbox.ok());

  Box levelDomain(finestbox);
  int maxallowablelevel(finestlevel);
  int max1DLength(exp2(15) - 1);  // ---- a pixmap cannot exceed this length
  unsigned long boxpoints;
  while(maxallowablelevel > 0) {
#   if (BL_SPACEDIM == 1)
      boxpoints = static_cast<unsigned long>(levelDomain.length(Amrvis::XDIR));
#   elif (BL_SPACEDIM == 2)
      boxpoints = static_cast<unsigned long>(levelDomain.length(Amrvis::XDIR)) *
                  static_cast<unsigned long>(levelDomain.length(Amrvis::YDIR));
#   else
      unsigned long tempLength;
      boxpoints  = static_cast<unsigned long>(levelDomain.length(Amrvis::XDIR)) *
                   static_cast<unsigned long>(levelDomain.length(Amrvis::YDIR));
      tempLength = static_cast<unsigned long>(levelDomain.length(Amrvis::YDIR)) *
                   static_cast<unsigned long>(levelDomain.length(Amrvis::ZDIR));
      boxpoints  = max(boxpoints, tempLength);
      tempLength = static_cast<unsigned long>(levelDomain.length(Amrvis::XDIR)) *
                   static_cast<unsigned long>(levelDomain.length(Amrvis::ZDIR));
      boxpoints = max(boxpoints, tempLength);
#   endif

      bool tooLong(false);
      for(int sd(0); sd < BL_SPACEDIM; ++sd) {
        if(levelDomain.length(sd) > max1DLength) {
	  tooLong = true;
	}
      }

    if(boxpoints > maxpoints || tooLong) {  // ---- try next coarser level
      --maxallowablelevel;
      levelDomain = finestbox;
      levelDomain.coarsen(CRRBetweenLevels(maxallowablelevel, finestlevel,
                                           refratios));
    } else {
      break;
    }
  }
  int imaxlev(maxallowablelevel);
  if(useMaxLevel) {
    imaxlev = std::min(maxLevel, maxallowablelevel);
    if(verbose) {
      if(ParallelDescriptor::IOProcessor()) {
        std::cout << "-------- using maxLevel = " << imaxlev << std::endl;
      }
    }
  }
  return(imaxlev);
}


// -------------------------------------------------------------------
// this function strips slashes to try to find the true file name
//   so:  /d/e/f/pltname/  would return pltname
// -------------------------------------------------------------------
string AVGlobals::StripSlashes(const string &inString) {
  string sTemp;
  string::size_type startString, endString;
  endString = inString.find_last_not_of('/');
  startString = inString.find_last_of('/', endString);
  if(startString == string::npos) {  // no slashes found
    startString = 0;
  } else {                           // skip over the last one found
    ++startString;
  }
  sTemp = inString.substr(startString, (endString - startString + 1));

  return sTemp;
}


// -------------------------------------------------------------------
bool AVGlobals::IsProfDirName(const string &pdname) {
  std::size_t found(pdname.find("bl_prof", 0));
  return(found != std::string::npos);
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
