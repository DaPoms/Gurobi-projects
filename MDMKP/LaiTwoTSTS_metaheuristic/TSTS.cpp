/*************************************************************************************\
 *                                                                                    *
 * This procedure demonstrates the use of the two-stage tabu search algorithm for     * 
 * solving the classic multidemand multidimensional knapsack problem (MDMKP).         *
 * Reference: Xiangjing Lai, Jin-Kao Hao, Dong Yue. Two-stage solution-based tabu     *
 * search for the multidemand multidimensional knapsack problem.                      *
 * European Journal of Operational Research 274(1): 35-48, 2019                       *  
 *                                                                                    *
\*************************************************************************************/ 

/*************************************************************************************/
/****  0. Header files, data structures, and global varialbes  ***********************/
/*************************************************************************************/ 
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string.h>
#include <time.h>
#include <ctime>
#include <vector>
#include<math.h>
#include<ctype.h>
#include <string>
using namespace std;

typedef struct{
	int *X; 
	int *S;
	int *NS;
	int IN, ON;
	int *IW; 
	long int f; 
} Solution; // solutions
typedef struct Neighbor{
	int f; 
    int type;
    int IO; 
    int k; 
    int x;
    int x1;
    int y1; 
} Neighbor; // neighbors 

char* File_Name;
const char *outfilename;
double time_limit, time_to_target, AvgTime; 
double time_one_run, starting_time;  

int f, f_best;
double BestResult, AvgResult, WorResult;  
double sigmma;   
double BestResultK, AvgResultK, WorResultK;  
double sigmmaK;   
int Nhit;  
int *P;
//int *P1;  
int *B;        
int **R;  // weights matrix
//int **R1; 
int N, M, Q; // number of terms, number of dimensions
Solution SC;
Solution S_BEST; 
Solution G_BEST; 
Solution HG_BEST; 
int *W1, *W2, *W3; 
int *H1, *H2, *H3; 
long int L; 
int alpha  = 5000; 
int alpha1 = 5000; 
int lanbda = 100; 

/***********************************************************************************/
/***********************          1. Initializing           ************************/
/***********************************************************************************/ 
//1.1 Inputing 
void Initializing()
{
     int i,j, x1, x2; 
     int count=0; 

	 ifstream FIC;
	 ofstream FIC2; 
     FIC.open(File_Name);
     if ( FIC.fail() )
     {
           cout << "### Erreur open, File_Name " << File_Name << endl;
           exit(0);
     } 
	 FIC >> N >> M >> Q;
 
	 P = new int [N];
	 B = new int [M+Q]; 
	 
	 R = new int *[M+Q];
	 for(i=0;i<M+Q;i++) 
		 R[i] = new int [N];
		 		  
     while ( ! FIC.eof() )
     {
         for(i=0;i<N;i++) FIC >> P[i];
		 for(i=0;i<M;i++) 
		    for(j=0;j<N;j++)FIC >> R[i][j];
		 for(j=0;j<M;j++) FIC >> B[j];   
	       
		 for(i=M;i<M+Q;i++) 
		    for(j=0;j<N;j++)FIC >> R[i][j];  
		 for(j=M;j<M+Q;j++) FIC >> B[j];     
     }	  
      
     FIC.close();
}

void AssignMemery()
{
     int i, j, swap;
	 SC.X      = new int [N];
	 SC.S      = new int [N];
	 SC.NS     = new int [N]; 
	 SC.IW     = new int [M+Q];
	 S_BEST.X  = new int [N];
     S_BEST.S  = new int [N];
	 S_BEST.NS = new int [N];
	 S_BEST.IW = new int [M+Q]; 
	 HG_BEST.X  = new int [N];
     HG_BEST.S  = new int [N];
	 HG_BEST.NS = new int [N];
	 HG_BEST.IW = new int [M+Q]; 
	 G_BEST.X  = new int [N];
     G_BEST.S  = new int [N];
     G_BEST.NS = new int [N];
     G_BEST.IW = new int [M+Q];
     
	 W1 = new int [N];
	 W2 = new int [N];
	 W3 = new int [N]; 
	 H1 = new int [L]; 
     H2 = new int [L]; 
     H3 = new int [L];    
	 for(i=0;i<N;i++)
	 {
	 	W1[i] = (int) pow(i+1,1.9);
	 	W2[i] = (int) pow(i+1,2.1);
	 	W3[i] = (int) pow(i+1,2.3);
	 }
}
/*****************************************************************************/
/*****************          2. Initial Solution       ************************/
/*****************************************************************************/ 
void InitiaSol(Solution &S)
{
	int i,j,k;
	int r, p, count;
	int *C;
	int c1,c2;
	int assign, assign1;
	C = new int [N];
	for(i=0;i<N;i++) C[i] = i; 
	for(i=0;i<N;i++) S.X[i] = 0; 
	for(i=0;i<N;i++) S.S[i] = -1; 
	for(i=0;i<N;i++) S.NS[i] = -1; 
	for(i=0;i<M+Q;i++) S.IW[i] = 0;
	S.IN = 0;
	S.ON = N;
	count  = N;
	S.f = 0;  
	c1 = 0;
	c2 = 0; 
	for(k=0;k<N;k++)
	{
		r = rand()%count;
		p = C[r]; 
		
		
		assign = 0;
		for(i=0;i<M;i++) 
		{
			if(S.IW[i]+R[i][p] > B[i]) {assign = 1;  break; }
		}
		
		assign1 = 0; 
		for(i=M;i<M+Q;i++) 
		{
			if(S.IW[i]+R[i][p] < B[i]) {assign1 = 1;  break; }
		}
	
		if(assign1 == 1 || assign == 0)
		{
			S.X[p] = 1; 
			for(i=0;i<M+Q;i++) S.IW[i] += R[i][p]; 
			S.IN ++;
			S.ON --;
			S.S[c1] = p; c1++;
			S.f += P[p]; 
		}
		else
		{
			S.NS[c2] = p; c2++;
		}
	
		for(j=r+1;j<=count;j++) C[j-1] = C[j]; 
		count --; 
	}
//	printf("f=%d\n",S.f); 
    delete [] C;
}

/*****************************************************************************/
/************************    5. Certification of the solution   **************/
/*****************************************************************************/ 
int proof(Solution &S)
{
   int i,j; 
   int count,p; 
   int assign; 
  // printf("\n %d \n",S.f) ;
   //for(i=0;i<N;i++) printf("%d ",S.X[i]); printf("\n");  
   for(i=0; i<M+Q; i++) S.IW[i] = 0;
   for(i=0;i<M+Q;i++)
      for(j=0;j<N;j++) 
	    S.IW[i] += S.X[j]*R[i][j]; 
   
   assign = 0; 
   for(i=0;i<M;i++)
   {  
      if(S.IW[i]>B[i]) { assign = 1; printf("<= errer ! \n") ;}; 
   } 
   for(i=M;i<M+Q;i++)
   {  
      if(S.IW[i]<B[i]) { assign = 1; printf(">= errer ! \n") ; }
   } 
  
   count = 0;
   p=0;
   for(i=0;i<N;i++) if(S.X[i]==1) count++;
   for(i=0;i<N;i++) if(S.X[i]==1) p+=P[i];
//   printf("count = %d   p=%d \n",count,p); 
   return assign; 
}

/*****************************************************************************/
/*****************     4. Solution-based Tabu Search  procedures *************/
/*****************************************************************************/ 
//-----------------------------------------------------------------------------
// 4.1 Build initial information for the tabu lists
//-----------------------------------------------------------------------------
void Build_Information()
{
   int i;
   for(i=0;i<L;i++) 
	 {
	 	 H1[i]=0;
	     H2[i]=0; 
		 H3[i]=0; 
	 } 
}
//-----------------------------------------------------------------------------
// 4.2 Tabu Search with the union neighborhood of N1 and N2
//-----------------------------------------------------------------------------
void Tabu_Search(Solution &S) 
{
     int i, j, j1, k, k0, x, y, v, u, iter ;
     int Ncount; 
	 int num, num1; 
     int select, swap; 
     int non_improve = 0 ;  // the stop condition of TS
	 long int tabu_best_fc, best_fc, f_c ;
	 int num_tabu_best, num_best;  // the number of tabu neighbors and non-tabu neighbors
     Neighbor best[ 50 ];
     Neighbor tabu_best[ 50 ];
     int Hx1, Hx2, Hx3; 
     int Hx11, Hx22, Hx33; 
	 double current_time, starting_time; 
	 long int sum_penalty;  
	 long int delt; 
	 int flag;  
	 
     Build_Information();  
     Hx1 = 0;
     Hx2 = 0; 
     Hx3 = 0; 
     for(i=0;i<N;i++)
     {
     	Hx1 += W1[i]*(S.X[i]); 
     	Hx2 += W2[i]*(S.X[i]); 
     	Hx3 += W3[i]*(S.X[i]); 
	 }
	 
	 f = S.f; 
	 f_best = -999999; 
	 S_BEST.IN = S.IN;
	 S_BEST.ON = S.ON; 
	 S_BEST.f= S.f; 
	 for(i=0;i<N;i++)   S_BEST.S[i]  = S.S[i];
	 for(i=0;i<N;i++)   S_BEST.NS[i] = S.NS[i];
	 for(i=0;i<N;i++)   S_BEST.X[i]  = S.X[i]; 
	 for(i=0;i<M+Q;i++) S_BEST.IW[i] = S.IW[i]; 
	 
     iter = 0; 
	 starting_time = clock(); 
	 current_time = (double) (1.0*(clock() - starting_time)/CLOCKS_PER_SEC);  
	 flag  = 0; 
    // while( current_time < 0.5*time_limit || flag == 0 )
     while( non_improve < 2*alpha || flag == 0 )
        {
          tabu_best_fc = -999999999;  
          best_fc = -999999999;
          num_tabu_best = 0;  
          num_best = 0 ; 
		  num = 0; 
		 //1). the add neighborhodd
		 for(k = 0; k < S.ON; k ++) // add a term
         {     
		    j = S.NS[k];
			sum_penalty = 0; 
			for(i=0;i<M;i++)   if(S.IW[i] + R[i][j] > B[i]) sum_penalty += (S.IW[i] + R[i][j] - B[i]);   
			for(i=M;i<M+Q;i++) if(S.IW[i] + R[i][j] < B[i]) sum_penalty += (B[i] - S.IW[i] - R[i][j]);  
		    delt = -1.0*lanbda*sum_penalty;   
		    
			Hx11 = Hx1 + W1[j];
			Hx22 = Hx2 + W2[j];
			Hx33 = Hx3 + W3[j];
			f_c = S.f + P[j];  
		
		    if(H1[Hx11%L]==1 && H2[Hx22%L]==1 && H3[Hx33%L]==1)  // if it is tabu
            { 
               
			    if( f_c + delt > tabu_best_fc )
                {
                    tabu_best[ 0 ].x = j ; 
                    tabu_best[ 0 ].type = 1 ; 
                    tabu_best[ 0 ].IO = 0; 
                    tabu_best[ 0 ].k = k; 
                    tabu_best[ 0 ].f = f_c; 
                    tabu_best_fc = f_c + delt; 
                    num_tabu_best = 1 ;
                }
                else if( f_c + delt == tabu_best_fc && num_tabu_best < 50 )   
                {
                    tabu_best[ num_tabu_best ].x    = j;    
					tabu_best[ num_tabu_best ].type = 1;   
					tabu_best[ num_tabu_best ].IO = 0; 
					tabu_best[ num_tabu_best ].k = k; 
					tabu_best[ num_tabu_best ].f = f_c; 
                    num_tabu_best++ ;   
                }                               
              } 
			else  
            {  
             
                if( f_c + delt > best_fc )  
                {
                    best[ 0 ].x = j; 
                    best[ 0 ].type = 1;
                    best[ 0 ].IO = 0;
                    best[ 0 ].k = k;
                    best[ 0 ].f = f_c ;
                    best_fc  = f_c + delt; 
                    num_best = 1 ; 
                }
                else if( f_c + delt == best_fc && num_best < 50 )
                {
                    best[ num_best ].x = j;  
                    best[ num_best ].type = 1 ;
                    best[ num_best ].IO = 0; 
                    best[ num_best ].k = k; 
                    best[ num_best ].f = f_c;  
                    num_best++ ;  
                }
            } 	
		 } 
         //2) the drop neighborhood  
         for( k = 0; k < S.IN; k ++)  // drop a term
         {     
		    j = S.S[k];
		    sum_penalty = 0;  
			for(i=0;i<M;i++)   if(S.IW[i] - R[i][j] > B[i]) sum_penalty += (S.IW[i] - R[i][j] - B[i]); 
			for(i=M;i<M+Q;i++) if(S.IW[i] - R[i][j] < B[i]) sum_penalty += (B[i] - S.IW[i] + R[i][j]); 
		    delt = -1.0*lanbda*sum_penalty; 
		    
		    Hx11 = Hx1 - W1[j] ;
			Hx22 = Hx2 - W2[j] ;
			Hx33 = Hx3 - W3[j] ;
			
			f_c = S.f - P[j] ; 
			
		    if( H1[Hx11%L]==1 && H2[Hx22%L]==1 && H3[Hx33%L]==1 ) // if it is tabu
              { 
				if( f_c + delt > tabu_best_fc )
                {
                    tabu_best[ 0 ].x = j ; 
                    tabu_best[ 0 ].type = 1 ; 
                    tabu_best[ 0 ].IO = 1; 
                    tabu_best[ 0 ].k = k; 
                    tabu_best[ 0 ].f = f_c; 
                    tabu_best_fc = f_c + delt; 
                    num_tabu_best = 1 ;
                }
                else if( f_c + delt == tabu_best_fc && num_tabu_best < 50 )   
                {
                    tabu_best[ num_tabu_best ].x   = j;    
					tabu_best[ num_tabu_best ].type = 1;   
					tabu_best[ num_tabu_best ].IO = 1; 
					tabu_best[ num_tabu_best ].k = k; 
					tabu_best[ num_tabu_best ].f = f_c; 
                    num_tabu_best++ ;   
                }                               
              } 
			else  
            {   
                if( f_c + delt > best_fc )  
                {
                    best[ 0 ].x = j; 
                    best[ 0 ].type = 1;
                    best[ 0 ].IO = 1; 
                    best[ 0 ].k = k;
                    best[ 0 ].f = f_c ;
                    best_fc  = f_c + delt; 
                    num_best = 1 ; 
                }
                else if( f_c + delt == best_fc && num_best < 50 )
                {
                    best[ num_best ].x   = j ; 
                    best[ num_best ].type = 1 ;
                    best[ num_best ].IO = 1; 
                    best[ num_best ].k = k; 
                    best[ num_best ].f = f_c; 
                    num_best++ ;   
                }
            }              	
		 }  
		 
        //3) Evaluating the neighborhood N2 based candidate list  
          for(x = 0; x < S.IN; x++) 
			{ 
              for(y = 0; y < S.ON; y++)	 
			  {
			    sum_penalty = 0;   
			    for(i=0;i<M;i++) if(S.IW[i]+(R[i][S.NS[y]]-R[i][S.S[x]]) > B[i])   sum_penalty += (S.IW[i]+(R[i][S.NS[y]]-R[i][S.S[x]]) - B[i]) ; 
			    for(i=M;i<M+Q;i++) if(S.IW[i]+(R[i][S.NS[y]]-R[i][S.S[x]]) < B[i]) sum_penalty += (B[i]- S.IW[i] - (R[i][S.NS[y]]-R[i][S.S[x]])); 
			    delt = -1.0*lanbda*sum_penalty; 
			   
				f_c = S.f + (P[S.NS[y]]-P[S.S[x]]);
		        Hx11 = Hx1 + (W1[S.NS[y]]- W1[S.S[x]]) ;
			    Hx22 = Hx2 + (W2[S.NS[y]]- W2[S.S[x]]) ;
			    Hx33 = Hx3 + (W3[S.NS[y]]- W3[S.S[x]]) ; 
				
				if(  H1[Hx11%L]==1 && H2[Hx22%L]==1 && H3[Hx33%L]==1 ) // if it is tabu 
                   { 
                     
					   if( f_c + delt > tabu_best_fc )
                        {
                            tabu_best[ 0 ].x1 = x ; 
                            tabu_best[ 0 ].y1 = y ; 
                            tabu_best[ 0 ].type = 2 ; 
                            tabu_best[ 0 ].IO = -1;
                            tabu_best[ 0 ].f = f_c; 
                            tabu_best_fc = f_c + delt; 
                            num_tabu_best = 1 ;
                        }
                        else if( f_c + delt == tabu_best_fc && num_tabu_best < 50 )   
                        {
                            tabu_best[ num_tabu_best ].x1   = x;    
                            tabu_best[ num_tabu_best ].y1   = y;  
							tabu_best[ num_tabu_best ].type = 2;   
							tabu_best[ num_tabu_best ].IO = -1; 
							tabu_best[ num_tabu_best ].f = f_c;  
                            num_tabu_best++ ;   
                        }  
					                            
                   } 
			    else 
                   {   
						if( f_c + delt > best_fc )  
                        {
                            best[ 0 ].x1 = x; 
                            best[ 0 ].y1 = y;
                            best[ 0 ].type = 2;
                            best[ 0 ].IO = -1; 
                            best[ 0 ].f  = f_c; 
                            best_fc  = f_c + delt; 
                            num_best = 1 ; 
                        }
                        else if( f_c + delt == best_fc && num_best < 50 )
                        {
                            best[ num_best ].x1   = x ; 
                            best[ num_best ].y1   = y ;
                            best[ num_best ].type = 2 ;
                            best[ num_best ].IO = -1 ; 
                            best[ num_best ].f = f_c ; 
                            num_best++ ;  
                        }
                   }    
				               
			    }
            }	
     //4) moves
	 //if( ( num_tabu_best > 0 && tabu_best_fc > best_fc &&  tabu_best_fc > f_best ) || num_best == 0 )  // aspiration criterion 
	   if( num_best == 0 ) //aspiration criterion 
        {
					  
                     select = rand() % num_tabu_best ;  
                     f = tabu_best[select].f ;
                     if(tabu_best[select].type==2)  
                     {
                       u = tabu_best[ select ].x1; 
                       v = tabu_best[ select ].y1;  
                       
                       Hx1 += (W1[S.NS[v]]-W1[S.S[u]]); 
                       Hx2 += (W2[S.NS[v]]-W2[S.S[u]]); 
                       Hx3 += (W3[S.NS[v]]-W3[S.S[u]]); 
                       H1[Hx1%L] = 1; 
                       H2[Hx2%L] = 1; 
                       H3[Hx3%L] = 1; 
                       
                       for(i=0;i<M+Q;i++) 
                       {
                       	   S.IW[i] += (R[i][S.NS[v]] - R[i][S.S[u]]);   
					   } 
                       S.X[S.S[u]]  = 1 - S.X[S.S[u]]; 
                       S.X[S.NS[v]] = 1 - S.X[S.NS[v]];
                       
                       swap    = S.S[u] ; 
                       S.S[u]  = S.NS[v];
                       S.NS[v] = swap ;   
                       S.f = f;  
			    	}
			    	
			    	else if(tabu_best[select].type == 1)
			    	{
			    		j  = tabu_best[ select ].x; 
			    		k0 = tabu_best[ select ].k;  
			    	   
			    		if(tabu_best[ select ].IO == 1) 
			    		{
			    	      
						  Hx1 -= W1[j]; 
                          Hx2 -= W2[j]; 
                          Hx3 -= W3[j]; 
                          H1[Hx1%L] = 1; 
                          H2[Hx2%L] = 1; 
                          H3[Hx3%L] = 1; 
                       
                          for(i=0;i<M+Q;i++) 
                          {
                       	    S.IW[i] -= R[i][j];  
					      } 
					      
                          S.X[j] = 1 - S.X[j]; 
                          for(i=k0; i<S.IN; i++) S.S[i] = S.S[i+1];
                          S.IN -- ;
                          
                          S.NS[S.ON] = j ;
                          S.ON ++ ; 
                          S.f = f;    
						 
						}
						else if(tabu_best[ select ].IO == 0)
						{
						  Hx1 += W1[j]; 
                          Hx2 += W2[j]; 
                          Hx3 += W3[j]; 
                          H1[Hx1%L] = 1; 
                          H2[Hx2%L] = 1; 
                          H3[Hx3%L] = 1; 
                       
                          for(i=0;i<M+Q;i++) 
                          {
                       	     S.IW[i] += R[i][j];  
					      } 
                          S.X[j] = 1 - S.X[j]; 
                          for(i=k0;i<S.ON;i++) S.NS[i] = S.NS[i+1];
                          S.ON-- ; 
                          S.S[S.IN] = j; 
                          S.IN++ ;
                          S.f = f;  
						}	
					}
          } 
          else // non tabu 
            {
                     select = rand() % num_best ;  
                     f = best[select].f ; 
                     if(best[select].type == 2)  
                     {
                       u = best[ select ].x1; 
                       v = best[ select ].y1;  
                       
                       Hx1 += (W1[S.NS[v]]-W1[S.S[u]]); 
                       Hx2 += (W2[S.NS[v]]-W2[S.S[u]]); 
                       Hx3 += (W3[S.NS[v]]-W3[S.S[u]]); 
                       H1[Hx1%L] = 1; 
                       H2[Hx2%L] = 1; 
                       H3[Hx3%L] = 1; 
                       
                       for(i=0;i<M+Q;i++) 
                       {
                       	  S.IW[i] += (R[i][S.NS[v]] - R[i][S.S[u]]);  
					   } 
					   
                       S.X[S.S[u]]  = 1 - S.X[S.S[u]]; 
                       S.X[S.NS[v]] = 1 - S.X[S.NS[v]];
                       
                       swap    = S.S[u] ; 
                       S.S[u]  = S.NS[v];
                       S.NS[v] = swap ;  
					    
                       S.f = f;  
			    	}
			    	else if( best[select].type == 1 )
			    	{
			    		j  = best[ select ].x; 
			    		k0 = best[ select ].k; 
			    	    
			    		if(best[ select ].IO == 1)
			    		 {	
						  Hx1 -= W1[j]; 
                          Hx2 -= W2[j]; 
                          Hx3 -= W3[j]; 
                          H1[Hx1%L] = 1; 
                          H2[Hx2%L] = 1; 
                          H3[Hx3%L] = 1; 
                       
                          for(i=0;i<M+Q;i++) 
                          {
                       	    S.IW[i] -= R[i][j];  
					      } 
					      
                          S.X[j] = 1 - S.X[j]; 
                          for(i=k0;i<S.IN;i++) S.S[i] = S.S[i+1];
                          S.IN -- ;
                          
                          S.NS[S.ON] = j ;
                          S.ON ++ ; 
                          S.f = f;   
						 
					   	}
					   else if(best[ select ].IO == 0)
						 {
						 		 	
						  Hx1 += W1[j]; 
                          Hx2 += W2[j]; 
                          Hx3 += W3[j]; 
                          H1[Hx1%L] = 1; 
                          H2[Hx2%L] = 1; 
                          H3[Hx3%L] = 1; 
                       
                          for(i=0;i<M+Q;i++) 
                          {
                       	      S.IW[i] += R[i][j];  
					      } 
                          S.X[j] = 1 - S.X[j]; 
                          for(i=k0;i<S.ON;i++) S.NS[i] = S.NS[i+1];
                          S.ON -- ; 
                          S.S[S.IN] = j; 
                          S.IN ++ ;
                          S.f = f; 
					     }	
				   }
           } 
          //5. print
          iter ++ ;
		  current_time = (double) (1.0*(clock()-starting_time)/CLOCKS_PER_SEC); 
//		  printf(" f = %d \n", f); 
		  sum_penalty = 0; 
		  for(i=0;i<M;i++)   if(S.IW[i] > B[i])  sum_penalty += (S.IW[i] - B[i]);
		  for(i=M;i<M+Q;i++) if(S.IW[i] < B[i])  sum_penalty += (B[i] - S.IW[i]);
//		  printf("sum =%d\n",sum_penalty); 
		  if(sum_penalty == 0) flag = 1;  
          if( S.f >= f_best && sum_penalty == 0 )
           {   
              if( S.f > f_best  )
               {
                  f_best = S.f;
                  for( i = 0 ; i < N ; i ++ )
                    S_BEST.X[ i ] = S.X[ i ] ;
                  for( i = 0 ; i < N ; i ++ )
                    S_BEST.S[ i ] = S.S[ i ] ;
                  for( i = 0 ; i < N ; i ++ )
                    S_BEST.NS[ i ] = S.NS[ i ] ; 
                  for( i = 0 ; i < M+Q ; i ++ )
                    S_BEST.IW[ i ] = S.IW[ i ] ;  
                  S_BEST.IN = S.IN;
                  S_BEST.ON = S.ON; 
				  S_BEST.f = S.f; 
                //  printf("\n neighbor_move :  %8d       %d       %d       %lf", iter, S.f, f_best, current_time);
                  non_improve = 0 ;
				  time_to_target = current_time; 
               }  
              else if ( S.f == f_best )  non_improve ++ ;
           }  
           else non_improve ++ ;   
       }
       
     for(i=0;i<N;i++)  S.X[i]  = S_BEST.X[i]; 
	 for(i=0;i<N;i++)  S.S[i]  = S_BEST.S[i];
	 for(i=0;i<N;i++)  S.NS[i] = S_BEST.NS[i]; 
	 for(i=0;i<M+Q;i++)  S.IW[i] = S_BEST.IW[i]; 
	 S.f  = S_BEST.f;
     S.IN = S_BEST.IN;
     S.ON = S_BEST.ON; 
}
//-------------------------------------------------------------------------------------------------
// 4.3 Tabu Search with the constrained swap neighborhood N3 and the extended evaluation function
//-------------------------------------------------------------------------------------------------
void Swap_tabu_search(Solution &S, double X)
{
     int i, j, k, k0, x, y, v, u, iter ;
     int K;
     int count; 
     int assign, Ncount, Ncount1, Ncount2;
     double pho, eta; 
	 int num, num1; 
	 long int sum_penalty; 
	 long int delt, tabu_delt, non_delt; 
     int select, swap; 
     int non_improve = 0 ;  //the stop condition of TS
	 long int tabu_best_fc, best_fc, f_c ;
	 int num_tabu_best, num_best;  //the number of tabu neighbors and non-tabu neighbors
     Neighbor best[ 50 ];
     Neighbor tabu_best[ 50 ]; 
   
     int Hx1, Hx2, Hx3; 
     int Hx11, Hx22, Hx33; 
	 double current_time, starting_time; 
	 
	 eta = 0.3; 
	 pho = 0.3; 
     Build_Information();  
     Hx1 = 0;
     Hx2 = 0; 
     Hx3 = 0; 
     for(i=0;i<N;i++)
     {
     	Hx1 += W1[i]*(S.X[i]); 
     	Hx2 += W2[i]*(S.X[i]); 
     	Hx3 += W3[i]*(S.X[i]); 
	 }
	 
	 f = S.f; 
	 f_best = S.f; 
	 S_BEST.IN = S.IN;
	 S_BEST.ON = S.ON; 
	 S_BEST.f = S.f; 
	 for(i=0;i<N;i++) S_BEST.S[i]  = S.S[i];
	 for(i=0;i<N;i++) S_BEST.NS[i] = S.NS[i];
	 for(i=0;i<N;i++) S_BEST.X[i]  = S.X[i]; 
	 for(i=0;i<M+Q;i++) S_BEST.IW[i] = S.IW[i];  
	 
     iter = 0;  
     non_improve = 0; 
	 starting_time = clock();  
	 current_time = (double) (1.0*(clock()-starting_time)/CLOCKS_PER_SEC);  
     while( current_time < X )
        {
          tabu_best_fc = -99999999 ; 
          best_fc = -99999999 ;
          num_tabu_best = 0 ; 
          num_best = 0 ;
		  num = 0; 
		  num1 = 0; 
		 //2) Evaluating the neighborhood N3   
		  for(x = 0; x < S.IN; x++) 
		  {
             for(y = 0; y < S.ON; y++)  
			  {
			   
				f_c = f + (P[S.NS[y]]-P[S.S[x]]) ; 
			    if(f_c <= f_best) continue; 
			    
				sum_penalty = 0; 
			    for(i=0;i<M;i++)   if(S.IW[i]+(R[i][S.NS[y]]-R[i][S.S[x]]) > B[i])  sum_penalty += (S.IW[i] + R[i][S.NS[y]] - R[i][S.S[x]] - B[i]);
			    for(i=M;i<M+Q;i++) if(S.IW[i]+(R[i][S.NS[y]]-R[i][S.S[x]]) < B[i])  sum_penalty += (B[i] - S.IW[i] - R[i][S.NS[y]] + R[i][S.S[x]]);
			    delt = -1.0*lanbda*sum_penalty; 
			     
		        Hx11 = Hx1 + (W1[S.NS[y]]- W1[S.S[x]]) ;
			    Hx22 = Hx2 + (W2[S.NS[y]]- W2[S.S[x]]) ;
			    Hx33 = Hx3 + (W3[S.NS[y]]- W3[S.S[x]]) ; 
				
				if(  H1[Hx11%L]==1 && H2[Hx22%L]==1 && H3[Hx33%L]==1 ) // if it is tabu 
                   { 
                       num ++; 
					   if( f_c + delt > tabu_best_fc )
                        {
                            tabu_best[ 0 ].x1 = x ; 
                            tabu_best[ 0 ].y1 = y ; 
                            tabu_best[ 0 ].type = 2 ; 
                            tabu_best[ 0 ].f = f_c ; 
                            tabu_best_fc = f_c + delt ; 
                            tabu_delt = delt; 
                            num_tabu_best = 1 ;
                        }
                        else if( f_c + delt == tabu_best_fc && num_tabu_best < 50 )   
                        {
                            tabu_best[ num_tabu_best ].x1   = x;    
                            tabu_best[ num_tabu_best ].y1   = y;  
							tabu_best[ num_tabu_best ].type = 2;   
							tabu_best[ num_tabu_best ].f = f_c; 
                            num_tabu_best++ ;   
                        }  
					                            
                   } 
			    else 
                   {   
                        num1++;  
						if( f_c + delt > best_fc )  
                        {
                            best[ 0 ].x1 = x; 
                            best[ 0 ].y1 = y;
                            best[ 0 ].type = 2;
                            best[ 0 ].f = f_c; 
                            best_fc  = f_c + delt; 
                            non_delt = delt; 
                            num_best = 1 ; 
                        }
                        else if( f_c + delt == best_fc && num_best < 50 )
                        {
                            best[ num_best ].x1   = x ; 
                            best[ num_best ].y1   = y ;
                            best[ num_best ].type = 2 ;
                            best[ num_best ].f = f_c; 
                            num_best++ ;  
                        }
                   }    
			       
			    }
            }	 
     //4) moves
	   if(  num_best == 0 )  // aspiration criterion
        {
					   
                       select = rand() % num_tabu_best ; 
					   f = tabu_best[select].f ;
                       u = tabu_best[ select ].x1; 
                       v = tabu_best[ select ].y1;  
                       
                       Hx1 += (W1[S.NS[v]]-W1[S.S[u]]); 
                       Hx2 += (W2[S.NS[v]]-W2[S.S[u]]); 
                       Hx3 += (W3[S.NS[v]]-W3[S.S[u]]); 
                       H1[Hx1%L] = 1; 
                       H2[Hx2%L] = 1; 
                       H3[Hx3%L] = 1; 
                       
                       for(i=0;i<M+Q;i++) 
                       {
                       	    S.IW[i] += (R[i][S.NS[v]] - R[i][S.S[u]]);   
					   } 
                       S.X[S.S[u]] = 1 - S.X[S.S[u]]; 
                       S.X[S.NS[v]] = 1 - S.X[S.NS[v]];
                       
                       swap    = S.S[u] ; 
                       S.S[u]  = S.NS[v];
                       S.NS[v] = swap   ;   
                       S.f = f;   
          } 
           
          else // non tabu 
            {
                    
                     select = rand() % num_best ;  
                     f = best[ select ].f ; 
                     u = best[ select ].x1; 
                     v = best[ select ].y1;  
                       
                     Hx1 += (W1[S.NS[v]]-W1[S.S[u]]); 
                     Hx2 += (W2[S.NS[v]]-W2[S.S[u]]); 
                     Hx3 += (W3[S.NS[v]]-W3[S.S[u]]); 
                     H1[Hx1%L] = 1; 
                     H2[Hx2%L] = 1; 
                     H3[Hx3%L] = 1;  
                       
                     for(i=0;i<M+Q;i++) 
                     {
                       	 S.IW[i] += (R[i][S.NS[v]] - R[i][S.S[u]]);  
					 } 
					   
                     S.X[S.S[u]]  = 1 - S.X[S.S[u]]; 
                     S.X[S.NS[v]] = 1 - S.X[S.NS[v]];
                       
                     swap    = S.S[u] ; 
                     S.S[u]  = S.NS[v];
                     S.NS[v] = swap   ;  
					    
                     S.f = f;  
           } 
          //5. print
          iter ++ ;
		  current_time = (double) (1.0*(clock()-starting_time)/CLOCKS_PER_SEC); 
          if( f >= f_best  )
           {
           	  sum_penalty = 0; 
			  for(i=0;i<M;i++)   if(S.IW[i] > B[i])  sum_penalty += (S.IW[i] - B[i]);
			  for(i=M;i<M+Q;i++) if(S.IW[i] < B[i])  sum_penalty += (B[i] - S.IW[i]);
			  //printf("sum =%d\n",sum_penalty); 
              if( f > f_best && sum_penalty == 0 )
               {
                  f_best = f;
                  for( i = 0 ; i < N ; i ++ )
                    S_BEST.X[ i ] = S.X[ i ] ;
                  for( i = 0 ; i < N ; i ++ )
                    S_BEST.S[ i ] = S.S[ i ] ;
                  for( i = 0 ; i < N ; i ++ )
                    S_BEST.NS[ i ] = S.NS[ i ] ; 
                  for( i = 0 ; i < M+Q ; i ++ )
                    S_BEST.IW[ i ] = S.IW[ i ] ;  
                  S_BEST.IN = S.IN;
                  S_BEST.ON = S.ON; 
				  S_BEST.f = f; 
                 // printf("\n swap_move :  %8d     %d     %d     %d     %lf      %lf", iter, f, f_best, non_improve, pho, current_time);
                  non_improve = 0 ;
				  time_one_run = current_time; 
               }  
              else if ( f == f_best )  non_improve ++ ;
          }  
         else non_improve ++ ;   
       }
       
     for(i=0;i<N;i++)  S.X[i]  = S_BEST.X[i]; 
	 for(i=0;i<N;i++)  S.S[i]  = S_BEST.S[i];
	 for(i=0;i<N;i++)  S.NS[i] = S_BEST.NS[i]; 
	 for(i=0;i<M+Q;i++)  S.IW[i] = S_BEST.IW[i]; 
	 S.f  = S_BEST.f;
     S.IN = S_BEST.IN;
     S.ON = S_BEST.ON; 
} // With a fixed value of K.  

/*****************************************************************************/
/**************************     9.  Two-phase Search  ************************/
/*****************************************************************************/
void TwoPhaseSearch()
{
	int i,j;
	int k, k0; 
	int select1, select2; 
	double start_time, first_time; 
	
	G_BEST.f = -99999; 
	start_time = clock();
    InitiaSol(SC); 
	Tabu_Search(SC); // the search of the first pahse. 
    proof(SC); 
	first_time = (double)((clock()- start_time)/CLOCKS_PER_SEC); 
	Swap_tabu_search(SC,time_limit-first_time);  // the search of the second phase.
    proof(SC); 
	time_one_run += first_time; 
	if(SC.f > G_BEST.f) 
	{
		for(j=0;j<N;j++)   G_BEST.S[j]  = SC.S[j];
	    for(j=0;j<N;j++)   G_BEST.NS[j] = SC.NS[j]; 
	    for(j=0;j<N;j++)   G_BEST.X[j]  = SC.X[j];
	    for(j=0;j<M+Q;j++) G_BEST.IW[j] = SC.IW[j]; 
        G_BEST.f  = SC.f;
        G_BEST.IN = SC.IN;
        G_BEST.ON = SC.ON; 
        //time_one_run = (double)((clock()- start_time)/CLOCKS_PER_SEC);
	}
}

/*****************************************************************************/
/*************************     10. Outputing  results   **********************/
/*****************************************************************************/ 
//5.1 Output the best solution found
void OutSol(Solution &S, char *filename)
{
    int i;
    int r;
	FILE *fp; 
	char buff[80];
    sprintf(buff,"%s.sol",filename); 
    fp=fopen(buff,"a+");
    fprintf(fp,"N = %d  M = %d  f= %d\n", N, M, S.f); 
    for(i=0;i<N;i++)
    fprintf(fp,"%d\n", S.X[i]); 
	fclose(fp);
}
//5.2 Output the statistics information over multipe runs of algorithm
void Outresulting(char *filename, const char *outfile)
{
    int i,j;
    FILE *fp; 
   	char buff[80];
    sprintf(buff,"%s",outfile);   
    fp=fopen(buff,"a+");
    fprintf(fp,"%s  %d  %lf  %lf  %lf   %lf  %lf  %lf  %lf  %lf  %lf %d\n",filename, N, BestResult, AvgResult, WorResult, sigmma, BestResultK, AvgResultK, WorResultK, sigmmaK, AvgTime, Nhit);  
	fclose(fp);         
}
void Outresulting1(char *filename, char *outfile, double F[], double T[], int Number)
{
    int i,j;
    FILE *fp; 
   	char buff[80];
    sprintf(buff,"%s",outfile);   
    fp=fopen(buff,"a+");
    fprintf(fp,"%s  %d  %lf  %lf  %lf  %lf  %lf   %d\n",filename,N,BestResult,AvgResult,WorResult,sigmma,AvgTime,Nhit);  
    for(i=0;i<101;i++)
    {
    	 fprintf(fp,"%d  %lf  %lf \n",i,F[i]/Number, T[i]/Number); 
	}
	fclose(fp);         
}
//5.3 Computing the standard deviation
double Deviation(int arr[], int n)
{
    int i;
    double sum = 0, tmp = 0, x_avg;
    for(i = 0; i < n; ++i) sum += arr[i];
    x_avg = 1.0*sum / n;
    for(i = 0; i < n; ++i)
        tmp += (arr[i] - x_avg)*(arr[i] - x_avg);
    return sqrt(tmp/n); 
} 


// Daniel Tripoli methods (excluding main)
// I just remade the output to make a cleaner result, and also to ensure it outputs in one spot not 1 file for each result
void outputDecisionVars(const Solution& sol, string fileName)
{
    ofstream ansFile{"LaiTwo_DecisionVars", ios::app};
    ansFile << fileName << ": ";
     for(int j{0}; j < N; j++) // for loop required to traverse int * of decision vars
        ansFile << HG_BEST.X[j] << " ";
    ansFile << "\n";
    ansFile.close();
}

/*****************************************************************************/
/**************************   11. Main  Scheme     ***************************/
/*****************************************************************************/ 
int main(int argc, char **argv)
{ 
        int i,j ; 
        int Nruns;
        srand( time(NULL) ) ;
        Nruns = 30;   // number of runs
        //Nruns = 1;   // number of runs
        int Result[101];
        int ResultK[101];
        L = 2*pow(10,8);
        File_Name  = argv[1];
        time_limit =  1.0*atoi(argv[2]); // SHould be 60 for beasley's ct7 ("the time limit tmax for each run was set to 60 seconds for instances with n ≤ 250")
        outfilename = "TSTS.txt"; 
        BestResult = -99999999; 
        WorResult  = 99999999; 
        Nhit = 0; 
        AvgResult = 0.0; 
        AvgResultK = 0.0;
        AvgTime = 0.0; 
        
        Initializing(); 
        AssignMemery(); 
        
        for(i=0;i<Nruns;i++)
        {
        TwoPhaseSearch() ;  
        Result[i]  =  G_BEST.f; 
        ResultK[i] =  G_BEST.IN; 
        AvgResult  += G_BEST.f; 
        AvgResultK += G_BEST.IN; 
        
        AvgTime   += time_one_run;  
        if(G_BEST.f > BestResult)
        {    
            BestResult  = G_BEST.f; 
            BestResultK = G_BEST.IN; 
            Nhit = 1;  
            for(j=0;j<N;j++)   HG_BEST.S[j]  = G_BEST.S[j];
            for(j=0;j<N;j++)   HG_BEST.NS[j] = G_BEST.NS[j]; 
            for(j=0;j<N;j++)   HG_BEST.X[j]  = G_BEST.X[j] ;
            for(j=0;j<M;j++)   HG_BEST.IW[j] = G_BEST.IW[j]; 
            HG_BEST.f  = G_BEST.f;
            HG_BEST.IN = G_BEST.IN;
            HG_BEST.ON = G_BEST.ON;  
        }
        else if( abs(G_BEST.f - BestResult)< 1.0e-7)
        { 
            Nhit++; 
        }
        if(G_BEST.f < WorResult)
        {
            WorResult = G_BEST.f;     
            WorResultK = G_BEST.IN;         
        }
        
        }

        AvgResult /= Nruns; 
        AvgResultK /= Nruns; 
        AvgTime   /= Nruns; 
        sigmma = Deviation(Result, Nruns); 
        sigmmaK= Deviation(ResultK, Nruns); 
        Outresulting(File_Name,outfilename); 
        //OutSol(HG_BEST, File_Name);

        proof(HG_BEST);     
        outputDecisionVars(HG_BEST, File_Name);
        cout << "DONE!\n";
     return 1;	
}
