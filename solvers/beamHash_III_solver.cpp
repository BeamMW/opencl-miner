// BEAM OpenCL Miner
// Solvers
// Copyright 2020 The Beam Team	
// Copyright 2020 Wilke Trei

#include "beamSolvers.h"
#include "beam_hash_III.h"

/*
	Beam Hash III solver
*/

namespace beamMiner {

void beamHashIII_S::loadAndCompileKernel(cl::Context &context, cl::Device &device, uint32_t index) {

	// Source Code of the solver
	string progStr = string((const char*) __beam_hash_III_cl, __beam_hash_III_cl_len); 
	cl::Program::Sources source(1,std::make_pair(progStr.c_str(), progStr.length()+1));

	vector<cl::Device> devicesTMP;
	devicesTMP.push_back(device);

	// Building the program for the device
	cl::Program program(context, source);
	cl_int err;
	err = program.build(devicesTMP,"-DwgSize=256");

	// Create the Kernels
	kernels[index].push_back(cl::Kernel(program, "cleanUp", &err));
	kernels[index].push_back(cl::Kernel(program, "beamHashIII_seed", &err));
	kernels[index].push_back(cl::Kernel(program, "beamHashIII_R1", &err));
	kernels[index].push_back(cl::Kernel(program, "beamHashIII_R2", &err));
	kernels[index].push_back(cl::Kernel(program, "beamHashIII_R3", &err));
	kernels[index].push_back(cl::Kernel(program, "beamHashIII_R4", &err));
	kernels[index].push_back(cl::Kernel(program, "beamHashIII_R5", &err));
	kernels[index].push_back(cl::Kernel(program, "watch_counter", &err));
}


void beamHashIII_S::createBuffers(cl::Context &context, cl::Device &device, uint32_t index) {
	cl_int err;
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_ulong8) * 35717120, NULL, &err));
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_ulong8) * 35717120, NULL, &err));
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint) * 20480, NULL, &err));  
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint) * 324, NULL, &err));  
}


void beamHashIII_S::queueKernels(cl::CommandQueue * queue, uint32_t devInd,  cl::Event * cbEvent, clCallbackData * workData) {

	cl_ulong4 prePow;
	memcpy(&prePow, &(workData->wd.work[0]), 32);	

	cl_int err;

	for (uint32_t kInd=0; kInd < kernels[devInd].size(); kInd++) {
		// Set the buffers as arguments
		err = kernels[devInd][kInd].setArg(0, buffers[devInd][0]); 
		err = kernels[devInd][kInd].setArg(1, buffers[devInd][1]); 
		err = kernels[devInd][kInd].setArg(2, buffers[devInd][2]); 
		err = kernels[devInd][kInd].setArg(3, buffers[devInd][3]); 
		// Set the work as argument
		err = kernels[devInd][kInd].setArg(4, prePow); 
	}
	
	uint32_t  wgSize = 256;

	queue->enqueueNDRangeKernel(kernels[devInd][0], cl::NDRange(0), cl::NDRange(5120), cl::NDRange(wgSize), NULL, NULL);	// cleanUp
	queue->enqueueNDRangeKernel(kernels[devInd][1], cl::NDRange(0), cl::NDRange(33554432), cl::NDRange(wgSize), NULL, NULL);	// seed
	queue->enqueueNDRangeKernel(kernels[devInd][2], cl::NDRange(0), cl::NDRange(16384*wgSize), cl::NDRange(wgSize), NULL, NULL);	// Round 1
	queue->enqueueNDRangeKernel(kernels[devInd][3], cl::NDRange(0), cl::NDRange(16384*wgSize), cl::NDRange(wgSize), NULL, NULL);	// Round 2
	queue->enqueueNDRangeKernel(kernels[devInd][4], cl::NDRange(0), cl::NDRange(16384*wgSize), cl::NDRange(wgSize), NULL, NULL);	// Round 3
	queue->enqueueNDRangeKernel(kernels[devInd][5], cl::NDRange(0), cl::NDRange(16384*wgSize), cl::NDRange(wgSize), NULL, NULL);	// Round 4
	queue->enqueueNDRangeKernel(kernels[devInd][6], cl::NDRange(0), cl::NDRange(16384*wgSize), cl::NDRange(wgSize), NULL, NULL);	// Round 5

	results[devInd] = (uint32_t *) queue->enqueueMapBuffer(buffers[devInd][3], CL_FALSE, CL_MAP_READ, 0, sizeof(cl_uint4) * 81, NULL, cbEvent, NULL);	// Read the Results
}

} // End namespace beamMiner
