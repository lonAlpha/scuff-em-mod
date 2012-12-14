/* Copyright (C) 2005-2011 M. T. Homer Reid
 *
 * This file is part of SCUFF-EM.
 *
 * SCUFF-EM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * SCUFF-EM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * scuff-test-Ewald.cc -- a test program for libscuff's routines for
 *                     -- evaluating the periodic Green's function
 * 
 * homer reid          -- 11/2005 -- 12/2012
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define HAVE_READLINE
#ifdef HAVE_READLINE
 #include <readline/readline.h>
 #include <readline/history.h>
#else
 #include "readlineReplacement.h"
#endif

#include <complex>

#include <libhrutil.h>

#include <libscuff.h>
#include <libscuffInternals.h>

#define NSUM 8
#define NFIRSTROUND 1
#define NMAX 10000

using namespace scuff;

/***************************************************************/
/***************************************************************/
/***************************************************************/
namespace scuff{
void ComputeG1(double *R, cdouble k,
               double *kBloch, double **LBV,
               double E, int *pnCells, cdouble *Sum);

void ComputeG2(double *R, cdouble k,
               double *kBloch, double **LBV,
               double E, int *pnCells, cdouble *Sum);

void ComputeGBFFirst9(double *R, cdouble k, 
                      int LDim, double *kBloch, double *LBV[2], 
                      cdouble *Sum);

void AddGBFContribution(double R[3], cdouble k, double kBloch[2],
                        double Lx, double Ly, cdouble *Sum);
}

/***************************************************************/
/* compute \sum_L e^{iK\dot L} G(r+L)                          */
/* where G(r)=exp(i*Beta*|r|) / (4*pi*|r|)                     */
/***************************************************************/
void ComputeGBF(cdouble k, double *kBloch, double **LBV, double *R,
                double AbsTol, double RelTol, int *pnCells, cdouble *Sum)
{ 
  int nx, ny;
  double RelDelta, AbsDelta;
  cdouble LastSum[NSUM];
  double MaxRelDelta, MaxAbsDelta;
  double Delta, AbsSum;
  int i, NN, ConvergedIters;
  int nCells=0;
  double *L1=LBV[0]; 
  double *L2=LBV[1];

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  memset(Sum,0,NSUM*sizeof(cdouble));
  for (nx=-NFIRSTROUND; nx<=NFIRSTROUND; nx++)
   for (ny=-NFIRSTROUND; ny<=NFIRSTROUND; ny++, nCells++)
    { 
#if 1
      if ( (abs(nx)<=1) && (abs(ny)<=1) )
       continue; // skip the innermost 9 grid cells 
#endif

      AddGBFContribution(R, k, kBloch,
                         nx*L1[0] + ny*L2[0],
                         nx*L1[1] + ny*L2[1],
                         Sum);
    };
        
  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  memcpy(LastSum,Sum,NSUM*sizeof(cdouble));
  ConvergedIters=0;
  for(NN=NFIRSTROUND+1; ConvergedIters<3 && NN<=NMAX; NN++)
   {  
     for(nx=-NN; nx<=NN; nx++)
      for(ny=-NN; ny<=NN; ny++)
       { 
         if ( (abs(nx)<NN) && (abs(ny)<NN) )
          continue;

         AddGBFContribution(R, k, kBloch,
                            nx*L1[0] + ny*L2[0],
                            nx*L1[1] + ny*L2[1], Sum);

         nCells++;
        };

     /*--------------------------------------------------------------*/
     /* convergence analysis ----------------------------------------*/
     /*--------------------------------------------------------------*/
     MaxAbsDelta=MaxRelDelta=0.0;
     for(i=0; i<NSUM; i++)
      { Delta=abs(Sum[i]-LastSum[i]);
        if ( Delta>MaxAbsDelta )
         MaxAbsDelta=Delta;
        AbsSum=abs(Sum[i]);
        if ( AbsSum>0.0 && (Delta > MaxRelDelta*AbsSum) )
         MaxRelDelta=Delta/AbsSum;
      };
     if ( MaxAbsDelta<AbsTol || MaxRelDelta<RelTol )
      ConvergedIters++;
     else
      ConvergedIters=0;

     memcpy(LastSum,Sum,NSUM*sizeof(cdouble));

   };

  if (pnCells) *pnCells=nCells; 

}

/***************************************************************/
/***************************************************************/
/***************************************************************/
int main(int argc, char *argv[]) 
{ 
  /***************************************************************/
  /* process command-line arguments ******************************/
  /***************************************************************/
  char *GeoFileName=0;
  double LBV1[2];		int nLBV1=0;
  double LBV2[2]; 		int nLBV2=0;
  /* name        type    #args  max_instances  storage    count  description*/
  OptStruct OSArray[]=
   { {"geometry", PA_STRING,  1, 1, (void *)&GeoFileName,   0,  ".scuffgeo file"},
     {"LBV1",     PA_DOUBLE,  2, 1, (void *)LBV1,      &nLBV1,  "lattice basis vector 1"},
     {"LBV2",     PA_DOUBLE,  2, 1, (void *)LBV2,      &nLBV2,  "lattice basis vector 2"},
     {0,0,0,0,0,0,0}
   };
  ProcessOptions(argc, argv, OSArray);

  /***************************************************************/ 
  /* if a geometry was specified, set the lattice vectors from it*/
  /***************************************************************/
  double LBV[2][2];
  double *LBVP[2] = { LBV[0], LBV[1] };
  int LatticeDimension;
  if ( nLBV1!=0 && nLBV2==0 )
   { LBV[0][0] = LBV1[0];
     LBV[0][1] = LBV1[1];
     LatticeDimension=1;
   }
  else if (nLBV1!=0 && nLBV2!=0)
   { LBV[0][0] = LBV1[0];
     LBV[0][1] = LBV1[1];
     LBV[1][0] = LBV2[0];
     LBV[1][1] = LBV2[1];
     LatticeDimension=2;
   }
  else if (GeoFileName!=0)
   { 
     RWGGeometry *G = new RWGGeometry(GeoFileName);
     LatticeDimension = G->NumLatticeBasisVectors;
     if (LatticeDimension>=1)
      { LBV[0][0] = G->LatticeBasisVectors[0][0];
        LBV[0][1] = G->LatticeBasisVectors[0][1];
      };
     if (LatticeDimension>=2)
      { LBV[1][0] = G->LatticeBasisVectors[1][0];
        LBV[1][1] = G->LatticeBasisVectors[1][1];
      }
   }
  else // default to a square lattice with side length 1
   { LatticeDimension=2;
     LBV[0][0] = 1.0;    LBV[0][1] = 0.0;
     LBV[1][0] = 0.0;    LBV[1][1] = 1.0;
   };

  srand48(time(0));
  SetDefaultCD2SFormat("(%15.8e,%15.8e)");

  /***************************************************************/
  /* enter command loop ******************************************/
  /***************************************************************/
  using_history();
  read_history(0);
  int nt, NumTokens;
  char *Tokens[50];
  char *p;

  double R[3];
  cdouble k;
  double kBloch[2];
  double E;
  int RealSumTerms, RecipSumTerms, BFSumTerms;
  double RealSumTime, RecipSumTime, BFSumTime;
  int SkipBF;
  int NumTimes=100;
  for(;;)
   { 
     /*--------------------------------------------------------------*/
     /*- print prompt and get input string --------------------------*/
     /*--------------------------------------------------------------*/
     printf("\n");
     printf(" options: --x      xx \n");
     printf("          --y      xx \n");
     printf("          --z      xx \n");
     printf("          --k      xx \n");
     printf("          --kBloch xx xx \n");
     printf("          --E      xx\n");
     printf("          --SkipBF\n");
     printf("          --quit\n");
     p=readline("enter options: ");
     if (!p) break;
     add_history(p);
     write_history(0);

     /*--------------------------------------------------------------*/
     /*- set default variable values --------------------------------*/
     /*--------------------------------------------------------------*/
     R[0] = -1.0 + 2.0*drand48();
     R[1] = -1.0 + 2.0*drand48();
     R[2] = -1.0 + 2.0*drand48();
     k = 1.0;
     kBloch[0] = kBloch[1] = 0.0;
     E = -1.0;
     SkipBF=0;

     /*--------------------------------------------------------------*/
     /* parse input string                                          -*/
     /*--------------------------------------------------------------*/
     NumTokens=Tokenize(p,Tokens,50);
     for(nt=0; nt<NumTokens; nt++)
      if ( !strcasecmp(Tokens[nt],"--x") )
       sscanf(Tokens[nt+1],"%le",R+0);
     for(nt=0; nt<NumTokens; nt++)
      if ( !strcasecmp(Tokens[nt],"--y") )
       sscanf(Tokens[nt+1],"%le",R+1);
     for(nt=0; nt<NumTokens; nt++)
      if ( !strcasecmp(Tokens[nt],"--z") )
       sscanf(Tokens[nt+1],"%le",R+2);
     for(nt=0; nt<NumTokens; nt++)
      if ( !strcasecmp(Tokens[nt],"--k") )
       S2CD(Tokens[nt+1],&k);
     for(nt=0; nt<NumTokens; nt++)
      if ( !strcasecmp(Tokens[nt],"--kBloch") )
       { sscanf(Tokens[nt+1],"%le",kBloch+0);
         sscanf(Tokens[nt+2],"%le",kBloch+1);
       };
     for(nt=0; nt<NumTokens; nt++)
      if ( !strcasecmp(Tokens[nt],"--E") )
       sscanf(Tokens[nt+1],"%le",&E);
     for(nt=0; nt<NumTokens; nt++)
      if ( !strcasecmp(Tokens[nt],"--SkipBF") )
       SkipBF=1;
     for(nt=0; nt<NumTokens; nt++)
      if ( !strcasecmp(Tokens[nt],"--quit") )
       exit(1);
       
     free(p);

     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     if (E==-1.0)
      E=sqrt( M_PI / (LBV[0][0]*LBV[1][1] - LBV[0][1]*LBV[1][0]) );

     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     printf("--x %g --y %g --z %g --k %s --kBloch %e %e --E %e \n",
              R[0],R[1],R[2],z2s(k),kBloch[0],kBloch[1],E);

     /*--------------------------------------------------------------*/
     /*- do the computation using the ewald summation method         */
     /*--------------------------------------------------------------*/
     cdouble Sum=0.0;
     cdouble G1[8], G2[8], GF9[8], GHR[8];
     Tic();
     for(int nt=0; nt<NumTimes; nt++)
      { ComputeG1(R, k, kBloch, LBVP, E, &RecipSumTerms, G1);
        Sum += G1[0];
      };
     RecipSumTime = Toc() / NumTimes;

     Sum=0.0;
     Tic();
     for(int nt=0; nt<NumTimes; nt++)
      { ComputeG2(R, k, kBloch, LBVP, E, &RealSumTerms, G2);
        Sum += G2[0];
      };
     RealSumTime = Toc() / NumTimes;

     ComputeGBFFirst9(R, k, 2, kBloch, LBVP, GF9);

     for(int n=0; n<8; n++)
      GHR[n] = G1[n] + G2[n] - GF9[n];

     printf("Reciprocal space sum: %10i terms (%.1f us, %.1f ns/term)\n",
             RecipSumTerms,RecipSumTime*1e6,RecipSumTime*1e9/RecipSumTerms);
     printf("Real space sum:       %10i terms (%.1f us, %.1f ns/term)\n",
             RealSumTerms,RealSumTime*1e6,RealSumTime*1e9/RealSumTerms);

     /*--------------------------------------------------------------*/
     /*- do the computation using the brute-force method ------------*/
     /*--------------------------------------------------------------*/
     cdouble GBF[8];
     if (SkipBF==0)
      { 
        Tic();
        ComputeGBF(k, kBloch, LBVP, R, 1.0e-2, 1.0e-2, &BFSumTerms, GBF);
        BFSumTime=Toc();
      }
     else
      GBF[0]=0.0;

     /*--------------------------------------------------------------*/
     /*- print results ----------------------------------------------*/
     /*--------------------------------------------------------------*/
     printf("\n");
     printf("**Ewald sum: \n");
     printf("  G1 = %s \n",CD2S(G1[0]));
     printf("+ G2 = %s \n",CD2S(G2[0]));
     printf("- F9 = %s \n",CD2S(GF9[0]));
     printf("------------------------------------------\n");
     printf("     = %s \n",CD2S(GHR[0]));
     printf("\n");

     printf("**BF sum: \n");
     printf("     = %s \n",CD2S(GBF[0]));

     printf("\n");
     printf("**RD: %e \n",RD(GBF[0],GHR[0]));


   }; // for(;;)

}
