// BEAM OpenCL Miner
// OpenCL Host Interface
// Copyright 2020 The Beam Team	
// Copyright 2020 Wilke Trei

#include <CL/cl.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <cstdlib>
#include <climits>

#include "beamSolvers.h"
#include "beamStratum.h"

#ifndef beamMiner_H 
#define beamMiner_H 

namespace beamMiner {


class clHost {
	private:
	// OpenCL 
	vector<cl::Platform> platforms;  
	vector<cl::Context> contexts;
	vector<cl::Device> devices;
	vector<cl::Event> events;
	vector<cl::CommandQueue> queues;
	vector< uint32_t > deviceContext;

	// Statistics
	vector<int> solutionCnt;

	// To check if a mining thread stoped and we must resume it
	vector<bool> paused;

	// Callback data
	vector<clCallbackData> currentWork;
	bool restart = true;

	// Functions
	void detectPlatFormDevices(vector<int32_t>, bool);
	
	// The connector
	beamStratum* stratum;

	beamHashI_S   BeamHashI;
	beamHashII_S  BeamHashII;
	beamHashIII_S BeamHashIII;

	public:
	
	void setup(beamStratum*, vector<int32_t>);
	void startMining();	
	void callbackFunc(cl_int, void*);
};

}

#endif
