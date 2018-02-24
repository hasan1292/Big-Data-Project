/*
Copyright (c) 2014-2015 Xiaowei Zhu, Tsinghua University

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "core/graph.hpp"
#include <string> 
#include <iostream>
#include <fstream>


/*
Main functions
*/
int main(int argc, char ** argv) {

	if (argc<3) {
		fprintf(stderr, "usage: lp [path] [total no. of lables] [total no. of iterations] [memory budget in GB] \n");
		exit(-1);
	}
	std::string path = argv[1];
	int LabelsNum = atoi(argv[2]);
	int IterationsNum = atoi(argv[3]);
	long memory_bytes = (argc>=5)?atol(argv[4])*1024l*1024l*1024l:8l*1024l*1024l*1024l;
	
	printf("Number of Labels = %d\n",LabelsNum);
	printf("Number of Iterations = %d\n",IterationsNum);
	printf("Memory Budget = %d GB\n",atoi(argv[4]));	

	Graph graph(path);
	graph.set_memory_bytes(memory_bytes);
	BigVector<double> ProbabilityOfVertices(graph.path+"/ProbabilityOfVertices", graph.vertices);
	BigVector<double [20]> TempLabelGrid(graph.path+"/TempLabelGrid", graph.vertices);
	BigVector<VertexId> LabelsOfVertices(graph.path+"/LabelsOfVertices", graph.vertices);
	BigVector<VertexId> OutDegreesOfVertices(graph.path+"/OutDegreesOfVerticess", graph.vertices);
	long vertex_data_bytes = (long) graph.vertices * (sizeof(double) + sizeof(double) + sizeof(VertexId));
	graph.set_vertex_data_bytes(vertex_data_bytes);

	double begin_time = get_time();

	//Out Degrees Calculation
	OutDegreesOfVertices.fill(0);
	graph.stream_edges<VertexId>(
		[&](Edge & e){
			write_add(&OutDegreesOfVertices[e.source], 1);
			return 0;
		}, nullptr, 0, 0
	);

	//Probabilities calculation
	graph.hint(ProbabilityOfVertices);
	graph.stream_vertices<VertexId>(
		[&](VertexId i){
			if(OutDegreesOfVertices[i] != 0)
				ProbabilityOfVertices[i] = 1.f / (OutDegreesOfVertices[i]);
			else
				ProbabilityOfVertices[i] = 0.f;
			return 0;
		}, nullptr, 0
	);
	printf("Degrees and Probability computation of each Vertex was completed in %.2f seconds\n", get_time() - begin_time);
	

	for (int i=0;i<graph.vertices;i++){
		for (int j=0;j<LabelsNum;j++){
			TempLabelGrid[i][j] = 0.f;
		}
	}

	double begin_time1 = get_time();

	//Assigning every vertex in graph with label -999. -999 means its not visited or given any label yet.
	LabelsOfVertices.fill(-999);
	srand((unsigned)time(0)); 
	for (int i=0;i<LabelsNum;i++){
		int RandomIndex = rand();
		RandomIndex = RandomIndex % graph.vertices;
		LabelsOfVertices[RandomIndex] = i;
	}

	printf("%d number of Labels were randomly allocated to vertices/nodes in %.2f seconds.\n",LabelsNum, get_time() - begin_time1);

	double begin_timeX = get_time();

	//main algo
	for (int k=0;k<IterationsNum;k++) {
		double begin_time2 = get_time();
		printf("Iteration no. %d in process...\t",k+1);

		graph.hint(TempLabelGrid);
		//process edges
		graph.stream_edges<VertexId>(
			[&](Edge & e){
				if ((LabelsOfVertices[e.source]>=0))
					if (LabelsOfVertices[e.target]<0)
						write_add(&TempLabelGrid[e.target][LabelsOfVertices[e.source]], TempLabelGrid[e.target][LabelsOfVertices[e.source]]+ProbabilityOfVertices[e.source]);
				return 0;
			}, nullptr, 0, 1
		);

		//proces vertices
		graph.stream_vertices<VertexId>(
			[&](VertexId i){
				if (LabelsOfVertices[i]<0){
					float maxCount = 0;
					int label = 0;
					for (int j=0;j<LabelsNum;j++){
						if (TempLabelGrid[i][j]>maxCount){
							maxCount = TempLabelGrid[i][j];
							label = j;
						} 
					}
					if (maxCount>0){
						write_add(&LabelsOfVertices[i], label+999);
					}
				}
				return 0;
			}
		);

		printf("Completed in %.2f seconds\n", get_time() - begin_time2);

	}


	printf("Time taken by %d number of iterations inside label propagation algorithm =  %.2f seconds\n", IterationsNum, get_time() - begin_timeX);
	
	double begin_time3 = get_time();
	
	//calculate the FrequencyofLabels
	int FrequencyofLabels [LabelsNum];
	for (int j=0;j<LabelsNum;j++){
		FrequencyofLabels[j] = 0;
	}
	for (int j=0;j<graph.vertices;j++){
		if (LabelsOfVertices[j]>=0){
			FrequencyofLabels[LabelsOfVertices[j]]+=1;
		}
	}

	graph.stream_vertices<VertexId>(
		[&](VertexId i){
			return 0;
		}, nullptr, 0,
		[&](std::pair<VertexId,VertexId> vid_range){
			LabelsOfVertices.load(vid_range.first, vid_range.second);
		},
		[&](std::pair<VertexId,VertexId> vid_range){
			LabelsOfVertices.save();
		}
	);
	for (int j=0;j<LabelsNum;j++){
		printf("Number of vertices of Label %d = %d\n",j, FrequencyofLabels[j]);
	}
	printf("Time taken for calculating number of nodes/vertices of each label =  %.2f seconds\n",  get_time() - begin_time3);
	printf("Total time taken is whole execution =  %.2f seconds\n",  get_time() - begin_time);
	
	
	
}
