// -------------------------------------------------------------------
// XYPlotDataList.cpp 
// -------------------------------------------------------------------
#include "XYPlotDataList.H"
#include <cfloat>

#define STRDUP(xx) (strcpy(new char[strlen(xx)+1], (xx)))

// -------------------------------------------------------------------
XYPlotDataList::XYPlotDataList(const string &_derived, int max_level,
			       int _gridline,
			       const Array<int> &ratio_list,
			       const Array<Real> &d_X,
			       const Array<char *> &intersect_point,
			       Real offset_x)
  : dataSets(max_level+1),
    upXi(max_level),
    ratios(ratio_list),
    dX(d_X),
    intersectPoint(intersect_point),
    lloY(max_level+1),
    hhiY(max_level+1),
    maxLevel(max_level),
    gridline(_gridline),
    offsetX(offset_x),
    derived(_derived)
{
  cout << "_here 000:  max_level = " << max_level << endl;
  int idx;

  emptyQ   = 1;
  updatedQ = 0;
  curLevel = 0;
  copied_from = NULL;

  idx = 0;
  while(true) {
  cout << "_here 000.1:  making a new dataSet:  idx maxLevel = "
       << idx << "  " << maxLevel << endl;
    dataSets[idx] = new List< XYPlotDataListLink *>();
    if(idx == maxLevel) {
      break;
    }
    upXi[idx] = new List<int>();
    ++idx;
  }
}


// -------------------------------------------------------------------
XYPlotDataList::XYPlotDataList(XYPlotDataList *src)
  : dataSets(src->dataSets),
    upXi(src->upXi),
    ratios(src->ratios),
    dX(src->dX),
    intersectPoint(src->intersectPoint),
    lloY(src->lloY),
    hhiY(src->hhiY),
    numPoints(src->numPoints),
    maxLevel(src->maxLevel),
    curLevel(src->curLevel),
    gridline(src->gridline),
    updatedQ(src->updatedQ),
    emptyQ(src->updatedQ),
    startX(src->startX),
    endX(src->endX),
    offsetX(src->offsetX),
    derived(src->derived) 
{
  cout << "_here 001" << endl;
  if(src->copied_from) {
    copied_from = src->copied_from;
  } else {
    copied_from = src;
  }
}


// -------------------------------------------------------------------
XYPlotDataList::~XYPlotDataList() {
  if(copied_from == NULL) {
    int idx;
    for(idx = 0; idx <= maxLevel; ++idx) {
      for(ListIterator<XYPlotDataListLink *> li(*dataSets[idx]); li; ++li) {
	delete (*li)->data;
	delete *li;
      }
      delete dataSets[idx];
      delete intersectPoint[idx];
    }
    for(idx = 0; idx != maxLevel; ++idx) {
      delete upXi[idx];
    }
  }
}


// -------------------------------------------------------------------
void XYPlotDataList::AddFArrayBox(FArrayBox &fab, int which_dir, int level) {
  int length = fab.length()[which_dir];
  int startXi = fab.smallEnd()[which_dir];
  Real *data = new Real[length];
  Real *FABdata = fab.dataPtr();
  for(int ii(0); ii != length; ++ii) {
    data[ii] = FABdata[ii];
  }
  addLink(new XYPlotDataListLink(data, startXi, length), level);
}


// -------------------------------------------------------------------
void XYPlotDataList::addLink(XYPlotDataListLink *linky, int level) {
  cout << "_here 002:  level = " << level << endl;
  BL_ASSERT(level <= maxLevel);
  emptyQ = 0;
  updatedQ = 0;
  linky->endXi = linky->startXi + linky->length;

  ListIterator<XYPlotDataListLink *> curLevLI(*dataSets[level]);

  if(level == 0) {
    linky->down = NULL;

    while(true) {
      if( ! curLevLI) {
	dataSets[0]->append(linky);
	break;
      }
      if((*curLevLI)->startXi > linky->startXi) {
	dataSets[0]->addBefore(curLevLI, linky);
	break;
      }
      ++curLevLI;
    }
  cout << "_here 002.1:  returning" << endl;
    return;
  }

  int temp;

  // ASSUMPTION: DATA IS CELL CENTERED.  If the box we are adding begins
  // more than halfway through the cell on the level below, then we will
  // consider the cell below to be "visible".
  temp = linky->startXi / ratios[level-1] +
    (((linky->startXi % ratios[level-1]) * 2 > ratios[level-1]) ? 1 : 0);
  
  // Insertion into sorted location.
  ListIterator<int> XiLI(*upXi[level-1]);
  while(true) {
    // If we have reached the end of the list, append to the end.
    if( ! curLevLI) {
      dataSets[level]->append(linky);
      upXi[level-1]->append(temp);
      break;
    }

    // If the current box in the list begins after the box we are adding,
    // we have found the position in the list, so stop.
    if((*curLevLI)->startXi > linky->startXi) {
      dataSets[level]->addBefore(curLevLI, linky);
      upXi[level-1]->addBefore(XiLI, temp);
      break;
    }

    ++curLevLI;
    ++XiLI;
  }

  temp = linky->endXi / ratios[level-1];

  linky->nDown = temp + 
    (((linky->endXi % ratios[level-1]) * 2 > ratios[level-1])  ? 1 : 0);

  ListIterator<XYPlotDataListLink *> preLevLI(*dataSets[level-1]);
  cout << "_here 002.2" << endl;
  BL_ASSERT(preLevLI);
  XYPlotDataListLink *down = *preLevLI;
  while(down->endXi < temp && ++preLevLI) {
    down = *preLevLI;
  }
  linky->down = down;
}


// -------------------------------------------------------------------
void XYPlotDataList::UpdateStats(void) {

  // Find the number of points, and the extremes for each level.
  if(updatedQ) {
    return;
  }
  numPoints = 0;

  BL_ASSERT(dataSets[0]->firstElement());

  {
    ListIterator<XYPlotDataListLink *> li(*dataSets[0]);
    int startXi = (*li)->startXi;
    int endXi = (*li)->endXi;
    while(++li) {
      if((*li)->startXi < startXi) {
        startXi = (*li)->startXi;
      }
      if((*li)->endXi > endXi) {
        endXi = (*li)->endXi;
      }
    }
    startX = offsetX + dX[0] * startXi;
    endX = offsetX + dX[0] * endXi;
  }

  lloY[0] =  DBL_MAX;
  hhiY[0] = -DBL_MAX;

  for(int idx(0); idx <= maxLevel; ++idx) {
    for(ListIterator<XYPlotDataListLink *> li(*dataSets[idx]); li; ++li) {
      Real *ptr = (*li)->data;
      numPoints += (*li)->length;
      for(int idx2((*li)->length); idx2 != 0; --idx2) {
	if(*ptr < lloY[idx]) {
	  lloY[idx] = *ptr;
	}
	if(*ptr > hhiY[idx]) {
	  hhiY[idx] = *ptr;
	}
	++ptr;
      }
    }
    if(idx != maxLevel) {
      lloY[idx+1] = lloY[idx];
      hhiY[idx+1] = hhiY[idx];
    }
  }
  updatedQ = 1;
}


// -------------------------------------------------------------------
XYPlotDataListIterator::XYPlotDataListIterator (XYPlotDataList *alist)
  : list(alist),
    XiLI(alist->curLevel),
    linkLI(alist->curLevel+1),
    maxLevel(alist->curLevel)
{
  curLevel = 0;

  for(int idx = 0; idx <= maxLevel; ++idx) {
    linkLI[idx] = new ListIterator<XYPlotDataListLink *> (*list->dataSets[idx]);
  }

  for(int idx = 0; idx != maxLevel; ++idx) {
    XiLI[idx] = new ListIterator<int> (*list->upXi[idx]);
  }

  if( ! *linkLI[0]) {
    curLink = NULL;
    return;
  }
  curLink = **linkLI[0];

  int temp;
  while(true) {
    curXi = curLink->startXi;
    nextXi = curLink->endXi;
    if(curLevel != maxLevel && *XiLI[curLevel] &&
       ((temp = **XiLI[curLevel]) < nextXi))
    {
      nextXi = temp;
      nextLink = **linkLI[curLevel+1];
      ++(*XiLI[curLevel]);
      ++(*linkLI[curLevel+1]);
    } else {
      nextLink = NULL;
      break;
    }
    
    if(curXi < nextXi) {
      break;
    }
    ++curLevel;
    curLink = nextLink;
  }
  data = curLink->data;
  xval = (0.5 + curXi) * list->dX[curLevel] + list->offsetX;
  yval = *data;
}


// -------------------------------------------------------------------
XYPlotDataListIterator::~XYPlotDataListIterator(){
  int idx;
  for(idx = 0; idx <= maxLevel; ++idx) {
    delete linkLI[idx];
  }
  for(idx = 0; idx != maxLevel; ++idx) {
    delete XiLI[idx];
  }
}


// -------------------------------------------------------------------
XYPlotDataListIterator &XYPlotDataListIterator::operator++() {
  if(++curXi < nextXi) {
    ++data;
    xval += list->dX[curLevel];
    yval = *data;
    return *this;
  }

  int temp;
  do {
    if(nextLink) {
      curXi = nextLink->startXi;
      curLink = nextLink;
      ++curLevel;
    } else {
      if(curLevel != 0) {
	--curLevel;
	curXi = curLink->nDown;
	curLink = curLink->down;
      } else {
	while(curLink != **linkLI[0]) {
	  ++(*linkLI[0]);
	}
	++(*linkLI[0]);
	if( ! *linkLI[0]) {
	  curLink = NULL;
	  return *this;
	}
	curLink = **linkLI[0];
	curXi = curLink->startXi;
      }
    }
    nextXi = curLink->endXi;
    if(curLevel != maxLevel && *XiLI[curLevel] &&
       ((temp = **XiLI[curLevel]) < nextXi))
    {
      nextXi = temp;
      nextLink = **linkLI[curLevel+1];
      ++(*XiLI[curLevel]);
      ++(*linkLI[curLevel+1]);
    } else {
      nextLink = NULL;
    }
  } while(curXi >= nextXi);
  
  data = curLink->data + (curXi - curLink->startXi);
  xval = (0.5 + curXi) * list->dX[curLevel] + list->offsetX;
  yval = *data;
  return *this;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
