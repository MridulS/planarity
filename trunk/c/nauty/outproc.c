/***********************************************************************
 Author: John Boyer
 Date: 16 May 2001, 2 Jan 2004, 7 Feb 2009, 4 Oct 2009, 21 Jan 2010,
       17 Jul 2010

 This file contains functions that connect McKay's makeg program output
 with planarity-related graph algorithm implementations.
 ***********************************************************************/

/*
Planarity-Related Graph Algorithms Project
Copyright (c) 1997-2010, John M. Boyer
All rights reserved. Includes a reference implementation of the following:

* John M. Boyer. "Simplified O(n) Algorithms for Planar Graph Embedding,
  Kuratowski Subgraph Isolation, and Related Problems". Ph.D. Dissertation,
  University of Victoria, 2001.

* John M. Boyer and Wendy J. Myrvold. "On the Cutting Edge: Simplified O(n)
  Planarity by Edge Addition". Journal of Graph Algorithms and Applications,
  Vol. 8, No. 3, pp. 241-273, 2004.

* John M. Boyer. "A New Method for Efficiently Generating Planar Graph
  Visibility Representations". In P. Eades and P. Healy, editors,
  Proceedings of the 13th International Conference on Graph Drawing 2005,
  Lecture Notes Comput. Sci., Volume 3843, pp. 508-511, Springer-Verlag, 2006.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

* Neither the name of the Planarity-Related Graph Algorithms Project nor the names
  of its contributors may be used to endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define EXTDEFS
#define MAXN 16
#include "naututil.h"
extern int g_maxn, g_mine, g_maxe, g_mod, g_res;
extern char g_command;
extern char quietMode;

#include <stdlib.h>

#include "outproc.h"
#include "testFramework.h"
#include "../graphColorVertices.h"

int runTest(FILE *msgfile, char command);

testResultFrameworkP testFramework = NULL;
int errorFound = 0;

int unittestMode = 0;
unsigned long unittestNumGraphs, unittestNumOKs;

/***********************************************************************
 WriteMatrixGraph()

 Writes a graph, giving n in decimal then the adj. matrix in hexadecimal
 ***********************************************************************/

void WriteMatrixGraph(char *filename, graph *g, int n)
{
	FILE *Outfile = fopen(filename, "wt");
	register int i;

	fprintf(Outfile, "%d\n", n);

	for (i = 0; i < n; ++i)
		fprintf(Outfile, "%04X\n", g[i]);

	fclose(Outfile);
}

/***********************************************************************
 TransferGraph() - edges from makeg graph g are added to theGraph.
 ***********************************************************************/

int  TransferGraph(graphP theGraph, graph *g, int n)
{
int  i, j, ErrorCode;
unsigned long PO2;

	 gp_ReinitializeGraph(theGraph);

     for (i = 0, ErrorCode = OK; i < n-1 && ErrorCode==OK; i++)
     {
          theGraph->G[i].v = i;

		  PO2 = 1 << (MAXN - 1);
		  PO2 >>= i+1;

          for (j = i+1; j < n; j++)
          {
             if (g[i] & PO2)
             {
                 ErrorCode = gp_AddEdge(theGraph, i, 0, j, 0);
                 if (ErrorCode != OK)
                 {
                     // If we only stopped because the graph contains too
                     // many edges, then we determine whether or not this is
                     // a real error as described below
                     if (ErrorCode == NONEMBEDDABLE)
                     {
                    	 // In the default case of this implementation, the graph's
                    	 // arc capacity is set to accommodate a complete graph,
                    	 // so if there was a failure to add an edge, then some
                    	 // corruption has occurred and we report an error.
                    	 if (gp_GetArcCapacity(theGraph)/2 == (n*(n-1)/2))
                    		 ErrorCode = NOTOK;

                    	 // Many of the algorithms require only a small sampling
                    	 // of edges to be successful.  For example, planar embedding
                    	 // and Kuratowski subgraph isolation required only 3n-5
                    	 // edges.  K3,3 search requires only 3n edges from the input
                    	 // graph.  If a user modifies the test code to exploit this
                    	 // lower limit, then we permit the failure to add an edge since
                    	 // the failure to add an edge is expected.
                    	 else
                    		 ErrorCode = OK;
                     }
                     break;
                 }
             }

			 PO2 >>= 1;
          }
     }

	 return ErrorCode;
}

/***********************************************************************
 ***********************************************************************/

void outprocTest(FILE *f, graph *g, int n)
{
	if (errorFound)
		return;

	if (testFramework == NULL)
	{
		testFramework = tf_AllocateTestFramework(g_command, n, g_maxe);
		if (testFramework == NULL)
		{
			fprintf(f, "\rUnable to create the test framework.\n");
			errorFound++;
		}
	}

	if (errorFound)
		return;

	// Copy from the nauty graph to the test graph(s)
	if (TransferGraph(testFramework->algResults[0].origGraph, g, n) != OK)
	{
		fprintf(f, "\rFailed to initialize with generated graph in errorMatrix.txt\n");
		WriteMatrixGraph("errorMatrix.txt", g, n);
		errorFound++;
	}

	if (errorFound)
		return;

	if (g_command == 'a')
	{
		int i, len;

		for (i=1, len=strlen(commands); i < len; i++)
		{
			gp_ReinitializeGraph(testFramework->algResults[i].origGraph);
			if (gp_CopyAdjacencyLists(testFramework->algResults[i].origGraph,
					                  testFramework->algResults[0].origGraph) != OK)
			{
				fprintf(f, "\rFailed to copy adjacency lists\n");
				errorFound++;
				return;
			}
		}
	}

	// Run the test(s)
	if (g_command == 'a')
	{
		int i, len;

		for (i=0, len=strlen(commands); i < len; i++)
			if (runTest(f, commands[i]) != OK)
			{
				fprintf(f, "See error.txt and errorMatrix.txt\n");
				gp_Write(testFramework->algResults[0].origGraph, "error.txt", WRITE_ADJLIST);
				WriteMatrixGraph("errorMatrix.txt", g, n);
				break;
			}
	}
	else
	{
		if (runTest(f, g_command) != OK)
		{
			fprintf(f, "See error.txt and errorMatrix.txt\n");
			gp_Write(testFramework->algResults[0].origGraph, "error.txt", WRITE_ADJLIST);
			WriteMatrixGraph("errorMatrix.txt", g, n);
		}
	}

	if (quietMode == 'n')
	{
#ifndef DEBUG
		// In release mode, print numbers less often for faster results
		if (testFramework->algResults[0].result.numGraphs % 379 == 0)
#endif
		{
			fprintf(f, "\r%lu ", testFramework->algResults[0].result.numGraphs);
			fflush(f);
		}
	}
}

int runTest(FILE *msgfile, char command)
{
	int Result = OK;
	testResultP testResult = tf_GetTestResult(testFramework, command);
	graphP theGraph = testResult->theGraph;
	graphP origGraph = testResult->origGraph;

	// Increment the main graph counter
	if (++testResult->result.numGraphs == 0)
	{
		fprintf(msgfile, "\rExceeded maximum number of supported graphs\n");
		errorFound = 1;
		return NOTOK;
	}

	// Now copy from the origGraph into theGraph on which the work will be done
	if ((Result = gp_CopyGraph(theGraph, origGraph)) != OK)
	{
		fprintf(msgfile, "\rFailed to copy graph #%lu\n", testResult->result.numGraphs);
		errorFound++;
		return NOTOK;
	}

	// Run the command on theGraph and check the integrity of the result
	if (command == 'c')
	{
		if ((Result = gp_ColorVertices(theGraph)) != OK)
			Result = NOTOK;
		else
		{
			if (gp_ColorVerticesIntegrityCheck(theGraph, origGraph) != OK)
			{
				fprintf(msgfile, "\rIntegrity check failed on graph #%lu.\n", testResult->result.numGraphs);
				Result = NOTOK;
			}
			if (Result == OK)
			{
				if (gp_GetNumColorsUsed(theGraph) >= 6)
					Result = NONEMBEDDABLE;
			}
		}
	}
	else if (strchr(commands, command))
	{
		int embedFlags = EMBEDFLAGS_PLANAR;

		switch (command)
		{
			case 'p' : embedFlags = EMBEDFLAGS_PLANAR; break;
			case 'd' : embedFlags = EMBEDFLAGS_DRAWPLANAR; break;
			case 'o' : embedFlags = EMBEDFLAGS_OUTERPLANAR; break;
			case '2' : embedFlags = EMBEDFLAGS_SEARCHFORK23; break;
			case '3' : embedFlags = EMBEDFLAGS_SEARCHFORK33; break;
			case '4' : embedFlags = EMBEDFLAGS_SEARCHFORK4; break;
		}

		Result = gp_Embed(theGraph, embedFlags);

		if (Result == OK || Result == NONEMBEDDABLE)
		{
			gp_SortVertices(theGraph);

			if (gp_TestEmbedResultIntegrity(theGraph, origGraph, Result) != Result)
			{
				Result = NOTOK;
				fprintf(msgfile, "\rIntegrity check failed on graph #%lu.\n", testResult->result.numGraphs);
			}
		}
	}
	else Result = NOTOK;

	// Increment the counters (note that origGraph is used to get
	// the number of edges M since theGraph may be a subgraph)
	testResult->edgeResults[origGraph->M].numGraphs++;
	if (Result == OK)
	{
		testResult->result.numOKs++;
		testResult->edgeResults[origGraph->M].numOKs++;
	}
	else if (Result == NONEMBEDDABLE)
		;
	else
	{
		errorFound++;
		fprintf(msgfile, "\rFailed to runTest() on graph #%lu.\n",
				testResult->result.numGraphs);
	}

	return Result == OK || Result == NONEMBEDDABLE ? OK : NOTOK;
}

/***********************************************************************
 ***********************************************************************/

void getMessages(char command, char **pMsgAlg, char **pMsgOK, char **pMsgNoEmbed)
{
	switch (command)
	{
		case 'p' : *pMsgAlg="Planarity"; *pMsgOK="Planar"; *pMsgNoEmbed="Not Planar"; break;
		case 'd' : *pMsgAlg="Planar Drawing"; *pMsgOK="Planar"; *pMsgNoEmbed="Not Planar"; break;
		case 'o' : *pMsgAlg="Outerplanarity"; *pMsgOK="Embedded"; *pMsgNoEmbed="Obstructed"; break;
		case '2' : *pMsgAlg="K2,3 Search"; *pMsgOK="no K2,3"; *pMsgNoEmbed="with K2,3"; break;
		case '3' : *pMsgAlg="K3,3 Search"; *pMsgOK="no K3,3"; *pMsgNoEmbed="with K3,3"; break;
		case '4' : *pMsgAlg="K4 Search"; *pMsgOK="no K4"; *pMsgNoEmbed="with K4"; break;
		case 'c' : *pMsgAlg="Vertex Coloring"; *pMsgOK="<=5 colors"; *pMsgNoEmbed=">5 colors"; break;
		default  : *pMsgAlg = *pMsgOK = *pMsgNoEmbed = NULL; break;
	}
}

/***********************************************************************
 ***********************************************************************/

void printStats(FILE *msgfile, testResultP testResult)
{
	char *msgAlg, *msgOK, *msgNoEmbed;
	int j;
	unsigned long numGraphs, numOKs, numNoEmbeds;

	getMessages(testResult->command, &msgAlg, &msgOK, &msgNoEmbed);

	fprintf(msgfile, "Begin Stats for Algorithm %s\n", msgAlg);
	fprintf(msgfile, "Status=%s\n", errorFound?"ERROR":"SUCCESS");

	fprintf(msgfile, "maxn=%d, mine=%d, maxe=%d\n", g_maxn, g_mine, g_maxe);
	if (g_mod > 1)
		fprintf(msgfile, "mod=%d, res=%d\n", g_mod, g_res);

	fprintf(msgfile, "# Edges  %10s  %10s  %10s\n", "# Graphs", msgOK, msgNoEmbed);
	fprintf(msgfile, "-------  ----------  ----------  ----------\n");
	for (j = g_mine; j <= g_maxe; j++)
	{
		numGraphs = testResult->edgeResults[j].numGraphs;
		numOKs = testResult->edgeResults[j].numOKs;
		numNoEmbeds = numGraphs - numOKs;
		fprintf(msgfile, "%7d  %10lu  %10lu  %10lu\n", j, numGraphs, numOKs, numNoEmbeds);
	}

	numGraphs = testResult->result.numGraphs;
	numOKs = testResult->result.numOKs;
	numNoEmbeds = numGraphs - numOKs;
	fprintf(msgfile, "TOTALS   %10lu  %10lu  %10lu\n", numGraphs, numOKs, numNoEmbeds);

	fprintf(msgfile, "End Stats for Algorithm %s\n", msgAlg);
}

/***********************************************************************
 Test_PrintStats() - called by makeg to print the final stats.
 ***********************************************************************/

void Test_PrintStats(FILE *msgfile)
{
	if (quietMode == 'n')
		fprintf(msgfile, "\r%lu \n", testFramework->algResults[0].result.numGraphs);

	if (unittestMode)
	{
		unittestNumGraphs = testFramework->algResults[0].result.numGraphs;
		unittestNumOKs = testFramework->algResults[0].result.numOKs;
	}
	//else
	if (g_command == 'a')
	{
		int i;
		for (i=0; i < testFramework->algResultsSize; i++)
			printStats(msgfile, &testFramework->algResults[i]);
	}
	else
	{
		printStats(msgfile, &testFramework->algResults[0]);
	}

	tf_FreeTestFramework(&testFramework);
	errorFound = 0;
}
