//
//  CXDistribution.h
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 20/02/14.
//  Copyright (c) 2014 Filipi Nascimento Silva. All rights reserved.
//

#ifndef CXNetwork_CXDistribution_h
#define CXNetwork_CXDistribution_h
#include "CXCommons.h"



typedef struct{
	CXFloat* distribution;
	CXFloat* data;
	CXSize count;
	double* tree;
	double* cumulative;
} CXDistribution;


/**
 * Builds a discrete distribution helper from parallel arrays of probabilities
 * and optional payload values. Probabilities are expected to sum to ~1.0.
 */
CXDistribution* CXCreateDistribution(const CXFloat* probabilities, const CXFloat* data, CXSize count);
/** Releases buffers allocated for the distribution. */
void CXDestroyDistribution(CXDistribution* distribution);



/**
 * Resolves a probability `choice` in the range [0.0,1.0) to an index in the
 * distribution. Values outside the range are clamped to the nearest boundary.
 */
CX_INLINE CXInteger CXDistributionIndexForChoice(CXDistribution* distribution,double choice){
	if(CXUnlikely(choice>=1.0)){
		return distribution->count-1;
	}else if(CXUnlikely(choice<0.0)){
		return 0;
	}
	CXSize n = (CXInteger)CXNextPowerOfTwo(distribution->count);
	double* tree = distribution->tree;
	CXIndex current = 0;
	while (current<n-1) {
		if(choice<tree[current]){
			current = (current+1)*2 - 1;
		}else{
			current = (current+1)*2;
		}
	}
	return current-(n-1);
}


/** Resolves a probability `choice` into the payload value, if provided. */
CX_INLINE CXFloat CXDistributionValueForChoice(CXDistribution* distribution,double choice){
	CXInteger index = CXDistributionIndexForChoice(distribution,choice);
	if(distribution->data){
		return distribution->data[index];
	}else{
		return (CXFloat)index;
	}
}

/** Picks a random index using the distribution's probability table. */
CX_INLINE CXInteger CXDistributionRandomIndex(CXDistribution* distribution){
	return CXDistributionIndexForChoice(distribution, CXRandomFloat());
}

/** Picks a random payload value using the distribution's probability table. */
CX_INLINE CXFloat CXDistributionRandomValue(CXDistribution* distribution){
	return CXDistributionValueForChoice(distribution, CXRandomFloat());
}

// void CXTestDistribution();

#endif
