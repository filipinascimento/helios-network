//
//  CXNetworkCentrality.h
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 8/27/13.
//  Copyright (c) 2013 Filipi Nascimento Silva. All rights reserved.
//

#ifndef CXNetwork_CXNetworkCentrality_h
#define CXNetwork_CXNetworkCentrality_h

#include "CXNetwork.h"
#include "CXBasicArrays.h"
#include "CXSimpleQueue.h"

CXBool CXNetworkCalculateCentrality(const CXNetwork* network,CXFloatArray* centrality, CXOperationControl* operationControl);
CXBool CXNetworkCalculateStressCentrality(const CXNetwork* network,CXFloatArray* centrality, CXOperationControl* operationControl);


#endif
