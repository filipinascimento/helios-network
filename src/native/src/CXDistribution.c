//
//  CXDistribution.c
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 20/02/14.
//  Copyright (c) 2014 Filipi Nascimento Silva. All rights reserved.
//

#include "CXDistribution.h"


/** Builds a sampling helper from the provided probability table. */
CXDistribution* CXCreateDistribution(const CXFloat* probabilities, const CXFloat* data, CXSize count){
	CXIndex i;
	CXDistribution* distribution = calloc(1, sizeof(CXDistribution));
	
	distribution->count=count;
	distribution->distribution = calloc(count, sizeof(CXFloat));
	distribution->cumulative = calloc(count, sizeof(double));
	
	
	memcpy(distribution->distribution, probabilities, count*sizeof(CXFloat));
	
	if(data){
		distribution->data = calloc(count, sizeof(CXFloat));
		memcpy(distribution->data, data, count*sizeof(CXFloat));
	}
	
	double cumValue = 0.0;
	for(i=0;i<count;i++){
		cumValue += distribution->distribution[i];
		distribution->cumulative[i] =cumValue;
	}
	
	for(i=0;i<count;i++){
		distribution->distribution[i]/=cumValue;
		distribution->cumulative[i]/=cumValue;
	}
	
	CXIndex m = 0;
	CXSize n = (CXInteger)CXNextPowerOfTwo(count);
	CXIndex p2n = ilog2(n);
	
	distribution->tree = calloc(n-1, sizeof(double));
	
	for(m=0;m<n-1;m++){
		CXInteger lm = ilog2(m+1);
		CXInteger jm = p2n - lm-1;
		CXInteger im = m - (ipow2(lm)-1);
		CXInteger Kij = im*ipow2(jm+1) + ipow2(jm) - 1;
		if(Kij<count){
			distribution->tree[m] = distribution->cumulative[Kij];
		}else{
			distribution->tree[m] = 1.0;
		}
	}
	/*printf("-----\nGenerated Tree:\n----\n");
	for(m=0;m<n-1;m++){
		CXInteger lm = ilog2(m+1);
		CXInteger jm = p2n - lm-1;
		CXInteger im = m - (ipow2(lm)-1);
		CXInteger Kij = im*ipow2(jm+1) + ipow2(jm) - 1;
		printf("%"CXIntegerScan" ",Kij);
	}
	printf("\n-----\n");*/
	return distribution;
}


/** Releases memory owned by the distribution helper. */
void CXDestroyDistribution(CXDistribution* distribution){
	distribution->count=0;
	free(distribution->cumulative);
	free(distribution->distribution);
	free(distribution->tree);
	free(distribution->data);
	free(distribution);
}


// void CXTestDistribution(){
// 	const CXInteger nnn = 1000;
	
// 	CXFloat probs[nnn];
// 	CXInteger freqs[nnn];
	
// 	CXInteger m;
// 	for(m=0;m<nnn;m++){
// 		probs[m] = expf(-(m-500.0)*(m-500.0)/200.0/200.0);
// 		freqs[m] = 0;
// 	}
	
// 	CXDistribution* distrib = CXCreateDistribution(probs, NULL, nnn);
	
// 	CXRandomSeedDev();
	
// 	CXSize reps = 50000000;
// 	for(m=0;m<reps;m++){
// 		CXInteger index = CXDistributionRandomIndex(distrib);
// 		freqs[index]++;
// 	}
	
// 	for(m=0;m<distrib->count;m++){
// 		printf("%g\t%10g\n", m/(double)(distrib->count-1), freqs[m]/(double)reps);
// 	}
// }
