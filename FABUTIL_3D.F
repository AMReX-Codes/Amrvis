c ::: SCCS stuff "@(#)FABUTIL_3D.F	3.1\t6/25/93"

#define   FORT_CINTERP   cntrp3d
#define   FORT_PCINTERP  pcntrp3d

#include "REAL.H"
#include "CONSTANTS.H"


c ::: --------------------------------------------------------------
      subroutine FORT_CINTERP (fine,floi1,floi2,floi3,fhii1,fhii2,fhii3,
     $  fblo,fbhi,nvar,lratio, crse,clo,chi,cblo,cbhi,fslo,fshi, cslope,
     $ clen,fslope,fdat,flen,voff)

      integer floi1,floi2,floi3, fhii1,fhii2,fhii3
      integer fblo(3), fbhi(3)
      integer cblo(3), cbhi(3)
      integer fslo(3), fshi(3)
      integer lratio, nvar, clen, flen, clo, chi
      REAL_T fine(floi1 :fhii1 ,floi2 :fhii2 ,floi3 :fhii3, nvar)
      REAL_T crse(clo:chi, nvar)
      REAL_T cslope(clo:chi, 3)
      REAL_T fslope(flen, 3)
      REAL_T fdat(flen)
      REAL_T voff(flen)

c ::: NOTE: data must be sent in so that 
c ::: cslope(1,*) and crse(1,*) are associated with
c ::: the same cell

c ::: ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
c ::: cinterp:   conservative interpolation from coarse grid to
c ::: subregion of fine grid defined by (fblo,fbhi)
c ::: 
c ::: Inputs/Outputs
c ::: fine        <=>  (modify) fine grid array
c ::: flo,fhi      =>  (const)  index limits of fine grid
c ::: fblo,fbhi    =>  (const)  subregion of fine grid to get values
c ::: nvar         =>  (const)  number of variables in state vector
c ::: lratio       =>  (const)  refinement ratio between levels
c ::: 
c ::: crse         =>  (const)  coarse grid data widended by 1 zone
c ::: and unrolled
c ::: clo,chi      =>  (const)  one dimensional limits of crse grid
c ::: cslo,cshi    =>  (const)  coarse grid index limits where
c ::: slopes are to be defined. This is
c ::: the projection of (fblo,fbhi) down
c ::: to the coarse level 
c ::: fslo,fshi    =>  (const)  fine grid index limits where
c ::: slopes are needed.  This is the
c ::: refinement of (cslo,cshi) and
c ::: contains but may not be identical
c ::: to (fblo,fbhi).
c ::: cslope       =>  (modify) temp array coarse grid slopes
c ::: clen         =>  (const)  length of coarse gtid slopes
c ::: fslope       =>  (modify) temp array for fine grid slope
c ::: flen         =>  (const)  length of fine grid slope array
c ::: fdat         =>  (const)  temp array for fine grid data
c ::: ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

c ::: local var
      integer n, fn
      integer i, ic, ioff
      integer j, jc, joff
      integer k, kc, koff
      integer ist, jst, kst
      REAL_T hafrat, volratio
      REAL_T cen, forw, back, slp
      REAL_T xoff, yoff, zoff
      integer ncbx, ncby, ncbz
      integer ncsx, ncsy, ncsz
      integer islo, jslo, kslo
      integer icc, istart, iend
      integer lenx, leny, lenz, maxlen

      hafrat = half*float(lratio-1)
      volratio = one/float(lratio)

      ncbx = cbhi(1)-cblo(1)+1
      ncby = cbhi(2)-cblo(2)+1
      ncbz = cbhi(3)-cblo(3)+1
      ncsx = ncbx+2
      ncsy = ncby+2
      ncsz = ncbz+2
      ist = 1
      jst = ncsx
      kst = ncsx*ncsy
      islo = cblo(1)-1
      jslo = cblo(2)-1
      kslo = cblo(3)-1
      lenx = fbhi(1)-fblo(1)+1
      leny = fbhi(2)-fblo(2)+1
      lenz = fbhi(3)-fblo(3)+1
      maxlen = max(lenx,leny,lenz)
      if (maxlen .eq. lenx) then
          do 100 i = fblo(1), fbhi(1) 
              fn = i-fslo(1)+1
              ioff = mod(fn-1,lratio)
              voff(fn) = float(ioff)-hafrat
100       continue
      elseif (maxlen .eq. leny) then
          do 110 j = fblo(2), fbhi(2) 
              fn = j-fslo(2)+1
              joff = mod(fn-1,lratio)
              voff(fn) = float(joff)-hafrat
110       continue
      else
          do 120 k = fblo(3), fbhi(3) 
              fn = k-fslo(3)+1
              koff = mod(fn-1,lratio)
              voff(fn) = float(koff)-hafrat
120       continue
      endif
      do 130 n = 1, nvar 

c ::: ::::: compute slopes in x direction
          do 140 i = 1, clen 
              cen = half*(crse(i+ist,n)-crse(i-ist,n))
              forw = crse(i+ist,n)-crse(i,n)
              back = crse(i,n)-crse(i-ist,n)
              slp = sign(one,cen)*min(abs(cen),abs(forw),abs(back))
              cslope(i,1)=cvmgp(slp,zero,forw*back)
140       continue
c ::: ::::: compute slopes in y direction
          do 150 i = 1, clen 
              cen = half*(crse(i+jst,n)-crse(i-jst,n))
              forw = crse(i+jst,n)-crse(i,n)
              back = crse(i,n)-crse(i-jst,n)
              slp = sign(one,cen)*min(abs(cen),abs(forw),abs(back))
              cslope(i,2)=cvmgp(slp,zero,forw*back)
150       continue
c ::: ::::: compute slopes in z direction
          do 160 i = 1, clen 
              cen = half*(crse(i+kst,n)-crse(i-kst,n))
              forw = crse(i+kst,n)-crse(i,n)
              back = crse(i,n)-crse(i-kst,n)
              slp = sign(one,cen)*min(abs(cen),abs(forw),abs(back))
              cslope(i,3)=cvmgp(slp,zero,forw*back)
160       continue

          if (maxlen .eq. lenx) then
              do 170 kc = cblo(3), cbhi(3) 
                  do 180 jc = cblo(2), cbhi(2) 

c ::: ..,.......::::: strip out a fine grid slope vector
                      do 190 ioff = 1, lratio 
                          icc = clo + ist + jst*(jc-jslo) + kst*(kc-
     $                    kslo)
                          istart = ioff
                          iend = ioff + (ncbx-1)*lratio
                          do 200 fn = istart, iend, lratio 
                              fslope(fn,1) = cslope(icc,1)
                              fslope(fn,2) = cslope(icc,2)
                              fslope(fn,3) = cslope(icc,3)
                              fdat(fn) = crse(icc,n)
                              icc = icc + ist
200                       continue
190                   continue

                      do 210 koff = 0, lratio-1 
                          k = lratio*kc + koff
                          if (k .lt. fblo(3)) then
                              goto 210
c                             --- next ---
                          endif
                          if (k .gt. fbhi(3)) then
                              goto 211
c                             --- break ---
                          endif
                          zoff = float(koff)-hafrat
                          do 220 joff = 0, lratio-1 
                              j = lratio*jc + joff
                              if (j .lt. fblo(2)) then
                                  goto 220
c                                 --- next ---
                              endif
                              if (j .gt. fbhi(2)) then
                                  goto 221
c                                 --- break ---
                              endif
                              yoff = float(joff)-hafrat

                              do 230 i = fblo(1), fbhi(1) 
                                  fn = i-fslo(1)+1
                                  fine(i,j,k,n) = fdat(fn) + volratio* 
     $                            (voff(fn)*fslope(fn,1)+yoff*fslope(fn,
     $                            2)+ zoff*fslope(fn,3))
230                           continue
220                       continue
221                       continue
210                   continue
211                   continue
180               continue
170           continue
          elseif (maxlen .eq. leny) then
              do 240 kc = cblo(3), cbhi(3) 
                  do 250 ic = cblo(1), cbhi(1) 

c ::: ..,.......::::: strip out a fine grid slope vector
                      do 260 joff = 1, lratio 
                          icc = clo + ist*(ic-islo) + jst + kst*(kc-
     $                    kslo)
                          istart = joff
                          iend = joff + (ncby-1)*lratio
                          do 270 fn = istart, iend, lratio 
                              fslope(fn,1) = cslope(icc,1)
                              fslope(fn,2) = cslope(icc,2)
                              fslope(fn,3) = cslope(icc,3)
                              fdat(fn) = crse(icc,n)
                              icc = icc + jst
270                       continue
260                   continue

                      do 280 koff = 0, lratio-1 
                          k = lratio*kc + koff
                          if (k .lt. fblo(3)) then
                              goto 280
c                             --- next ---
                          endif
                          if (k .gt. fbhi(3)) then
                              goto 281
c                             --- break ---
                          endif
                          zoff = float(koff)-hafrat
                          do 290 ioff = 0, lratio-1 
                              i = lratio*ic + ioff
                              if (i .lt. fblo(1)) then
                                  goto 290
c                                 --- next ---
                              endif
                              if (i .gt. fbhi(1)) then
                                  goto 291
c                                 --- break ---
                              endif
                              xoff = float(ioff)-hafrat

                              do 300 j = fblo(2), fbhi(2) 
                                  fn = j-fslo(2)+1
                                  fine(i,j,k,n) = fdat(fn) + volratio* 
     $                            (xoff*fslope(fn,1)+voff(fn)*fslope(fn,
     $                            2)+ zoff*fslope(fn,3))
300                           continue
290                       continue
291                       continue
280                   continue
281                   continue
250               continue
240           continue
          else
              do 310 jc = cblo(2), cbhi(2) 
                  do 320 ic = cblo(1), cbhi(1) 

c ::: ..,.......::::: strip out a fine grid slope vector
                      do 330 koff = 1, lratio 
                          icc = clo + ist*(ic-islo) + jst*(jc-jslo) + 
     $                    kst
                          istart = koff
                          iend = koff + (ncbz-1)*lratio
                          do 340 fn = istart, iend, lratio 
                              fslope(fn,1) = cslope(icc,1)
                              fslope(fn,2) = cslope(icc,2)
                              fslope(fn,3) = cslope(icc,3)
                              fdat(fn) = crse(icc,n)
                              icc = icc + kst
340                       continue
330                   continue

                      do 350 joff = 0, lratio-1 
                          j = lratio*jc + joff
                          if (j .lt. fblo(2)) then
                              goto 350
c                             --- next ---
                          endif
                          if (j .gt. fbhi(2)) then
                              goto 351
c                             --- break ---
                          endif
                          yoff = float(joff)-hafrat
                          do 360 ioff = 0, lratio-1 
                              i = lratio*ic + ioff
                              if (i .lt. fblo(1)) then
                                  goto 360
c                                 --- next ---
                              endif
                              if (i .gt. fbhi(1)) then
                                  goto 361
c                                 --- break ---
                              endif
                              xoff = float(ioff)-hafrat

                              do 370 k = fblo(3), fbhi(3) 
                                  fn = k-fslo(3)+1
                                  fine(i,j,k,n) = fdat(fn) + volratio* 
     $                            (xoff*fslope(fn,1)+yoff*fslope(fn,2)+ 
     $                            voff(fn)*fslope(fn,3))
370                           continue
360                       continue
361                       continue
350                   continue
351                   continue
320               continue
310           continue
          endif
130   continue

      return
      end


c ::: --------------------------------------------------------------
      subroutine FORT_PCINTERP (fine,floi1,floi2,floi3,fhii1,fhii2,fhii3,
     $ fblo, fbhi,lrat,nvar, crse,cloi1,cloi2,cloi3,chii1,chii2,chii3,
     $ cblo, cbhi,temp,tloi,thii)

      integer floi1,floi2,floi3
      integer fhii1,fhii2,fhii3
      integer cloi1,cloi2,cloi3
      integer chii1,chii2,chii3

      integer fblo(3), fbhi(3)
      integer cblo(3), cbhi(3)
      integer lrat, nvar, tloi, thii
      REAL_T fine(floi1 :fhii1 ,floi2 :fhii2 ,floi3 :fhii3, nvar)
      REAL_T crse(cloi1 :chii1 ,cloi2 :chii2 ,cloi3 :chii3, nvar)
      REAL_T temp(tloi:thii)
c ::: ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
c ::: pcntrp:  use piecewise constant interpolation to define
c ::: values on the subregion of the fine FAB defined
c ::: by (fblo,fbhi).
c ::: 
c ::: Inputs/Outputs
c ::: fine        <=>  (modify) fab to get interpolated values
c ::: flo,fhi      =>  (const)  index limits of fine
c ::: fblo,fbhi    =>  (const)  subregion of fine grid to get values
c ::: crse         =>  (const)  fab holding coarse grid values
c ::: clo,chi      =>  (const)  index limits of src
c ::: cblo,cbhi    =>  (const)  subregion of coarse grid holding values
c ::: temp         =>  (modify) temporary space for vectorization
c ::: tlo,thi      =>  (const)  index limits of temp space
c ::: ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
c ::: local var
      integer i,j,k,ic,jc,kc,ioff,n
      integer ixproj,ii,ll
      ixproj(ii,ll) = (ii + ll*iabs(ii))/ll - iabs(ii)

      do 130 k = fblo(3), fbhi(3) 
          kc = ixproj(k,lrat)
          do 140 j = fblo(2), fbhi(2) 
              jc = ixproj(j,lrat)
              do 150 n = 1, nvar 
                  do 160 ioff = 0, lrat-1 
                      do 170 ic = cblo(1),cbhi(1) 
                          i = lrat*ic + ioff
                          temp(i) = crse(ic,jc,kc,n)
170                   continue
160               continue
                  do 180 i = fblo(1), fbhi(1) 
                      fine(i,j,k,n) = temp(i)
180               continue
150           continue
140       continue
130   continue

      return
      end
