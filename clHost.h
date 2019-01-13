// BEAM OpenCL Miner
// OpenCL Host Interface
// Copyright 2018 The Beam Team	
// Copyright 2018 Wilke Trei

#include <CL/cl.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <cstdlib>
#include <climits>

#include "beamStratum.h"

namespace beamMiner {

struct clCallbackData {
	void* host;
	uint32_t gpuIndex;
	beamStratum::WorkDescription wd;
};

class clHost {
	private:
	// OpenCL 
	vector<cl::Platform> platforms;  
	vector<cl::Context> contexts;
	vector<cl::CommandQueue> queues;
	vector<cl::Device> devices;
	vector<cl::Event> events;
	vector<unsigned*> results;

	vector< vector<cl::Buffer> > buffers;
	vector< vector<cl::Kernel> > kernels;

	vector<bool> is3G;

	// Statistics
	vector<int> solutionCnt;
	vector<uint32_t> lastHashrates;

	// To check if a mining thread stoped and we must resume it
	vector<bool> paused;

	// Callback data
	vector<clCallbackData> currentWork;
	bool restart = true;


	// Functions
	void detectPlatFormDevices(vector<int32_t>, bool, bool);
	void loadAndCompileKernel(cl::Device &, uint32_t, bool);
	void queueKernels(uint32_t, clCallbackData*);
	
	// The connector
	beamStratum* stratum;

	public:
	
	void setup(beamStratum*, vector<int32_t>, bool, bool);
	void startMining();	
	void callbackFunc(cl_int, void*);
	
	std::string getAPIResponse();
	void startAPI();
};

}
