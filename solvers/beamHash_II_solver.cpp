// BEAM OpenCL Miner
// Solvers
// Copyright 2020 The Beam Team	
// Copyright 2020 Wilke Trei

#include "beamSolvers.h"
#include "beam_hash_II.h"



namespace beamMiner {

/*
	Beam Hash I solver
*/

void beamHashI_S::loadAndCompileKernel(cl::Context &context, cl::Device &device, uint32_t index) {

	// Source Code of the solver
	string progStr = string((const char*) __beam_hash_II_cl, __beam_hash_II_cl_len); 
	cl::Program::Sources source(1,std::make_pair(progStr.c_str(), progStr.length()+1));

	vector<cl::Device> devicesTMP;
	devicesTMP.push_back(device);

	// Building the program for the device
	cl::Program program(context, source);
	cl_int err;
	err = program.build(devicesTMP,"");

	// Create the Kernels
	kernels[index].push_back(cl::Kernel(program, "clearCounter", &err));

	kernels[index].push_back(cl::Kernel(program, "round0", &err));
	kernels[index].push_back(cl::Kernel(program, "round1", &err));

	kernels[index].push_back(cl::Kernel(program, "round0_BH2", &err));
	kernels[index].push_back(cl::Kernel(program, "round1_BH2", &err));
		
	kernels[index].push_back(cl::Kernel(program, "round2", &err));
	kernels[index].push_back(cl::Kernel(program, "round3", &err));
	kernels[index].push_back(cl::Kernel(program, "round4", &err));
	kernels[index].push_back(cl::Kernel(program, "round5", &err));

	kernels[index].push_back(cl::Kernel(program, "combine", &err));
}


void beamHashI_S::createBuffers(cl::Context &context, cl::Device &device, uint32_t index) {
	cl_int err;

	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 71303168, NULL, &err));
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 71303168, NULL, &err)); 
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 71303168, NULL, &err)); 
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint2) * 71303168, NULL, &err)); 

	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 256, NULL, &err));   
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint) * 49152, NULL, &err));  
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint) * 324, NULL, &err));  
}


void beamHashI_S::queueKernels(cl::CommandQueue * queue, uint32_t devInd,  cl::Event * cbEvent, clCallbackData * workData) {

	cl_ulong4 work;
	cl_ulong nonce;

	memcpy(&work, &(workData->wd.work[0]), 32);
	memcpy(&nonce, &(workData->wd.nonce), 8);	

	cl_int err;

	kernels[devInd][0].setArg(0, buffers[devInd][5]); 
	kernels[devInd][0].setArg(1, buffers[devInd][6]);

	// Kernel arguments for round0
	kernels[devInd][1].setArg(0, buffers[devInd][0]); 
	kernels[devInd][1].setArg(1, buffers[devInd][2]); 
	kernels[devInd][1].setArg(2, buffers[devInd][5]); 
	kernels[devInd][1].setArg(3, work); 
	kernels[devInd][1].setArg(4, nonce); 

	// Kernel arguments for round1
	kernels[devInd][2].setArg(0, buffers[devInd][0]); 
	kernels[devInd][2].setArg(1, buffers[devInd][2]); 
	kernels[devInd][2].setArg(2, buffers[devInd][1]); 
	kernels[devInd][2].setArg(3, buffers[devInd][3]); 	// Index tree will be stored here
	kernels[devInd][2].setArg(4, buffers[devInd][5]); 

	// Kernel arguments for round0-BH2
	kernels[devInd][3].setArg(0, buffers[devInd][0]); 
	kernels[devInd][3].setArg(1, buffers[devInd][2]); 
	kernels[devInd][3].setArg(2, buffers[devInd][5]); 
	kernels[devInd][3].setArg(3, work); 
	kernels[devInd][3].setArg(4, nonce); 

	// Kernel arguments for round1-BH2
	kernels[devInd][4].setArg(0, buffers[devInd][0]); 
	kernels[devInd][4].setArg(1, buffers[devInd][2]); 
	kernels[devInd][4].setArg(2, buffers[devInd][1]); 
	kernels[devInd][4].setArg(3, buffers[devInd][3]); 	// Index tree will be stored here
	kernels[devInd][4].setArg(4, buffers[devInd][5]); 

	// Kernel arguments for round2
	kernels[devInd][5].setArg(0, buffers[devInd][1]); 
	kernels[devInd][5].setArg(1, buffers[devInd][0]);	// Index tree will be stored here 
	kernels[devInd][5].setArg(2, buffers[devInd][5]); 

	// Kernel arguments for round3
	kernels[devInd][6].setArg(0, buffers[devInd][0]); 
	kernels[devInd][6].setArg(1, buffers[devInd][1]); 	// Index tree will be stored here 
	kernels[devInd][6].setArg(2, buffers[devInd][5]); 

	// Kernel arguments for round4
	kernels[devInd][7].setArg(0, buffers[devInd][1]); 
	kernels[devInd][7].setArg(1, buffers[devInd][2]); 	// Index tree will be stored here 
	kernels[devInd][7].setArg(2, buffers[devInd][5]);  

	// Kernel arguments for round5
	kernels[devInd][8].setArg(0, buffers[devInd][2]); 
	kernels[devInd][8].setArg(1, buffers[devInd][4]); 	// Index tree will be stored here 
	kernels[devInd][8].setArg(2, buffers[devInd][5]);  

	// Kernel arguments for Combine
	kernels[devInd][9].setArg(0, buffers[devInd][0]); 
	kernels[devInd][9].setArg(1, buffers[devInd][1]); 	
	kernels[devInd][9].setArg(2, buffers[devInd][2]); 
	kernels[devInd][9].setArg(3, buffers[devInd][3]); 	
	kernels[devInd][9].setArg(4, buffers[devInd][4]); 
	kernels[devInd][9].setArg(5, buffers[devInd][5]); 	
	kernels[devInd][9].setArg(6, buffers[devInd][6]);
	
	uint32_t  wgSize = 256;

	queue->enqueueNDRangeKernel(kernels[devInd][0], cl::NDRange(0), cl::NDRange(12288), cl::NDRange(256), NULL, NULL); 
	
	queue->enqueueNDRangeKernel(kernels[devInd][1], cl::NDRange(0), cl::NDRange(22369536), cl::NDRange(256), NULL, NULL);
	queue->enqueueNDRangeKernel(kernels[devInd][2], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL); 
	
	queue->enqueueNDRangeKernel(kernels[devInd][5], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL); 
	queue->enqueueNDRangeKernel(kernels[devInd][6], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL); 
	queue->enqueueNDRangeKernel(kernels[devInd][7], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
	queue->enqueueNDRangeKernel(kernels[devInd][8], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
	queue->enqueueNDRangeKernel(kernels[devInd][9], cl::NDRange(0), cl::NDRange(4096), cl::NDRange(16), NULL, NULL); 
	results[devInd] = (uint32_t *) queue->enqueueMapBuffer(buffers[devInd][6], CL_FALSE, CL_MAP_READ, 0, sizeof(cl_uint4) * 81, NULL, cbEvent, NULL);	// Read the Results
}

/*
	Beam Hash II solver
*/


void beamHashII_S::loadAndCompileKernel(cl::Context &context, cl::Device &device, uint32_t index) {

	// Source Code of the solver
	string progStr = string((const char*) __beam_hash_II_cl, __beam_hash_II_cl_len); 
	cl::Program::Sources source(1,std::make_pair(progStr.c_str(), progStr.length()+1));

	vector<cl::Device> devicesTMP;
	devicesTMP.push_back(device);

	// Building the program for the device
	cl::Program program(context, source);
	cl_int err;
	err = program.build(devicesTMP,"");

	// Create the Kernels
	kernels[index].push_back(cl::Kernel(program, "clearCounter", &err));

	kernels[index].push_back(cl::Kernel(program, "round0", &err));
	kernels[index].push_back(cl::Kernel(program, "round1", &err));

	kernels[index].push_back(cl::Kernel(program, "round0_BH2", &err));
	kernels[index].push_back(cl::Kernel(program, "round1_BH2", &err));
		
	kernels[index].push_back(cl::Kernel(program, "round2", &err));
	kernels[index].push_back(cl::Kernel(program, "round3", &err));
	kernels[index].push_back(cl::Kernel(program, "round4", &err));
	kernels[index].push_back(cl::Kernel(program, "round5", &err));

	kernels[index].push_back(cl::Kernel(program, "combine", &err));
}


void beamHashII_S::createBuffers(cl::Context &context, cl::Device &device, uint32_t index) {
	cl_int err;

	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 71303168, NULL, &err));
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 71303168, NULL, &err)); 
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 71303168, NULL, &err)); 
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint2) * 71303168, NULL, &err)); 

	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 256, NULL, &err));   
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint) * 49152, NULL, &err));  
	buffers[index].push_back(cl::Buffer(context, CL_MEM_READ_WRITE,  sizeof(cl_uint) * 324, NULL, &err));  
}


void beamHashII_S::queueKernels(cl::CommandQueue * queue, uint32_t devInd,  cl::Event * cbEvent, clCallbackData * workData) {

	cl_ulong4 work;
	cl_ulong nonce;

	memcpy(&work, &(workData->wd.work[0]), 32);
	memcpy(&nonce, &(workData->wd.nonce), 8);	

	cl_int err;

	kernels[devInd][0].setArg(0, buffers[devInd][5]); 
	kernels[devInd][0].setArg(1, buffers[devInd][6]);

	// Kernel arguments for round0
	kernels[devInd][1].setArg(0, buffers[devInd][0]); 
	kernels[devInd][1].setArg(1, buffers[devInd][2]); 
	kernels[devInd][1].setArg(2, buffers[devInd][5]); 
	kernels[devInd][1].setArg(3, work); 
	kernels[devInd][1].setArg(4, nonce); 

	// Kernel arguments for round1
	kernels[devInd][2].setArg(0, buffers[devInd][0]); 
	kernels[devInd][2].setArg(1, buffers[devInd][2]); 
	kernels[devInd][2].setArg(2, buffers[devInd][1]); 
	kernels[devInd][2].setArg(3, buffers[devInd][3]); 	// Index tree will be stored here
	kernels[devInd][2].setArg(4, buffers[devInd][5]); 

	// Kernel arguments for round0-BH2
	kernels[devInd][3].setArg(0, buffers[devInd][0]); 
	kernels[devInd][3].setArg(1, buffers[devInd][2]); 
	kernels[devInd][3].setArg(2, buffers[devInd][5]); 
	kernels[devInd][3].setArg(3, work); 
	kernels[devInd][3].setArg(4, nonce); 

	// Kernel arguments for round1-BH2
	kernels[devInd][4].setArg(0, buffers[devInd][0]); 
	kernels[devInd][4].setArg(1, buffers[devInd][2]); 
	kernels[devInd][4].setArg(2, buffers[devInd][1]); 
	kernels[devInd][4].setArg(3, buffers[devInd][3]); 	// Index tree will be stored here
	kernels[devInd][4].setArg(4, buffers[devInd][5]); 

	// Kernel arguments for round2
	kernels[devInd][5].setArg(0, buffers[devInd][1]); 
	kernels[devInd][5].setArg(1, buffers[devInd][0]);	// Index tree will be stored here 
	kernels[devInd][5].setArg(2, buffers[devInd][5]); 

	// Kernel arguments for round3
	kernels[devInd][6].setArg(0, buffers[devInd][0]); 
	kernels[devInd][6].setArg(1, buffers[devInd][1]); 	// Index tree will be stored here 
	kernels[devInd][6].setArg(2, buffers[devInd][5]); 

	// Kernel arguments for round4
	kernels[devInd][7].setArg(0, buffers[devInd][1]); 
	kernels[devInd][7].setArg(1, buffers[devInd][2]); 	// Index tree will be stored here 
	kernels[devInd][7].setArg(2, buffers[devInd][5]);  

	// Kernel arguments for round5
	kernels[devInd][8].setArg(0, buffers[devInd][2]); 
	kernels[devInd][8].setArg(1, buffers[devInd][4]); 	// Index tree will be stored here 
	kernels[devInd][8].setArg(2, buffers[devInd][5]);  

	// Kernel arguments for Combine
	kernels[devInd][9].setArg(0, buffers[devInd][0]); 
	kernels[devInd][9].setArg(1, buffers[devInd][1]); 	
	kernels[devInd][9].setArg(2, buffers[devInd][2]); 
	kernels[devInd][9].setArg(3, buffers[devInd][3]); 	
	kernels[devInd][9].setArg(4, buffers[devInd][4]); 
	kernels[devInd][9].setArg(5, buffers[devInd][5]); 	
	kernels[devInd][9].setArg(6, buffers[devInd][6]);
	
	uint32_t  wgSize = 256;

	queue->enqueueNDRangeKernel(kernels[devInd][0], cl::NDRange(0), cl::NDRange(12288), cl::NDRange(256), NULL, NULL); 
	
	queue->enqueueNDRangeKernel(kernels[devInd][3], cl::NDRange(0), cl::NDRange(2796032), cl::NDRange(256), NULL, NULL);
	queue->enqueueNDRangeKernel(kernels[devInd][4], cl::NDRange(0), cl::NDRange(2097152), cl::NDRange(256), NULL, NULL);
	
	queue->enqueueNDRangeKernel(kernels[devInd][5], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL); 
	queue->enqueueNDRangeKernel(kernels[devInd][6], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL); 
	queue->enqueueNDRangeKernel(kernels[devInd][7], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
	queue->enqueueNDRangeKernel(kernels[devInd][8], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
	queue->enqueueNDRangeKernel(kernels[devInd][9], cl::NDRange(0), cl::NDRange(4096), cl::NDRange(16), NULL, NULL); 
	results[devInd] = (uint32_t *) queue->enqueueMapBuffer(buffers[devInd][6], CL_FALSE, CL_MAP_READ, 0, sizeof(cl_uint4) * 81, NULL, cbEvent, NULL);	// Read the Results
}

} // End namespace beamMiner
