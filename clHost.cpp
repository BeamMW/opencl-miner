// BEAM OpenCL Miner
// OpenCL Host Interface
// Copyright 2018 The Beam Team	
// Copyright 2018 Wilke Trei

#include "clHost.h"
#include "./kernels/equihash_150_5_inc.h"

namespace beamMiner {

// Helper functions to split a string
inline vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while(getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


inline vector<string> split(const string &s, char delim) {
    vector<string> elems;
    return split(s, delim, elems);
}


// Helper function that tests if a OpenCL device supports a certain CL extension
inline bool hasExtension(cl::Device &device, string extension) {
	string info;
	device.getInfo(CL_DEVICE_EXTENSIONS, &info);
	vector<string> extens = split(info, ' ');

	for (int i=0; i<extens.size(); i++) {
		if (extens[i].compare(extension) == 0) 	return true;
	} 
	return false;
}


// This is a bit ugly c-style, but the OpenCL headers are initially for c and
// support c-style callback functions (no member functions) only.
// This function will be called every time a GPU is done with its current work
void CL_CALLBACK CCallbackFunc(cl_event ev, cl_int err , void* data) {
	clHost* self = static_cast<clHost*>(((clCallbackData*) data)->host);
	self->callbackFunc(err,data);
}


// Function to load the OpenCL kernel and prepare our device for mining
void clHost::loadAndCompileKernel(cl::Device &device, uint32_t pl, bool use3G) {
	cout << "   Loading and compiling Beam OpenCL Kernel" << endl;

	// reading the kernel
	string progStr = string(__equihash_150_5_cl, __equihash_150_5_cl_len); 

	/* ifstream file("./kernels/equihash_150_5.cl");
	string progStr(istreambuf_iterator<char>(file),(istreambuf_iterator<char>())); */
	cl::Program::Sources source(1,std::make_pair(progStr.c_str(), progStr.length()+1));

	// Create a program object and build it
	vector<cl::Device> devicesTMP;
	devicesTMP.push_back(device);

	cl::Program program(contexts[pl], source);
	cl_int err;
	if (!use3G) {
		err = program.build(devicesTMP,"");
	} else {
		err = program.build(devicesTMP,"-DMEM3G");
	}

	// Check if the build was Ok
	if (!err) {
		cout << "   Build sucessfull. " << endl;

		// Store the device and create a queue for it
		cl_command_queue_properties queue_prop = 0;  
		devices.push_back(device);
		queues.push_back(cl::CommandQueue(contexts[pl], devices[devices.size()-1], queue_prop, NULL)); 

		// Reserve events, space for storing results and so on
		events.push_back(cl::Event());
		results.push_back(NULL);
		currentWork.push_back(clCallbackData());
		paused.push_back(true);
		is3G.push_back(use3G);
		solutionCnt.push_back(0);

		// Create the kernels
		vector<cl::Kernel> newKernels;	
		newKernels.push_back(cl::Kernel(program, "clearCounter", &err));
		newKernels.push_back(cl::Kernel(program, "round0", &err));
		newKernels.push_back(cl::Kernel(program, "round1", &err));
		newKernels.push_back(cl::Kernel(program, "round2", &err));
		newKernels.push_back(cl::Kernel(program, "round3", &err));
		newKernels.push_back(cl::Kernel(program, "round4", &err));
		newKernels.push_back(cl::Kernel(program, "round5", &err));
		if (use3G) {
			newKernels.push_back(cl::Kernel(program, "combine3G", &err));
			newKernels.push_back(cl::Kernel(program, "repack", &err));
			newKernels.push_back(cl::Kernel(program, "move", &err));
		} else {
			newKernels.push_back(cl::Kernel(program, "combine", &err));
		}
		kernels.push_back(newKernels);

		// Create the buffers
		vector<cl::Buffer> newBuffers;	
		
		if (!use3G) {
			newBuffers.push_back(cl::Buffer(contexts[pl], CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 71303168, NULL, &err));
			newBuffers.push_back(cl::Buffer(contexts[pl], CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 71303168, NULL, &err)); 
			newBuffers.push_back(cl::Buffer(contexts[pl], CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 71303168, NULL, &err)); 
			newBuffers.push_back(cl::Buffer(contexts[pl], CL_MEM_READ_WRITE,  sizeof(cl_uint2) * 71303168, NULL, &err)); 
		} else {

			newBuffers.push_back(cl::Buffer(contexts[pl], CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 69599232, NULL, &err));
			newBuffers.push_back(cl::Buffer(contexts[pl], CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 69599232, NULL, &err)); 
			newBuffers.push_back(cl::Buffer(contexts[pl], CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 52199424, NULL, &err)); 
			newBuffers.push_back(cl::Buffer(contexts[pl], CL_MEM_READ_WRITE,  sizeof(cl_uint2) * 1, NULL, &err)); 
		}

		newBuffers.push_back(cl::Buffer(contexts[pl], CL_MEM_READ_WRITE,  sizeof(cl_uint4) * 256, NULL, &err));   
		newBuffers.push_back(cl::Buffer(contexts[pl], CL_MEM_READ_WRITE,  sizeof(cl_uint) * 49152, NULL, &err));  
		newBuffers.push_back(cl::Buffer(contexts[pl], CL_MEM_READ_WRITE,  sizeof(cl_uint) * 324, NULL, &err));  
		buffers.push_back(newBuffers);		
			
	} else {
		cout << "   Program build error, device will not be used. " << endl;
		// Print error msg so we can debug the kernel source
		cout << "   Build Log: "     << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devicesTMP[0]) << endl;
	}
}


// Detect the OpenCL hardware on this system
void clHost::detectPlatFormDevices(vector<int32_t> selDev, bool allowCPU, bool force3G) {
	// read the OpenCL platforms on this system
	cl::Platform::get(&platforms);  

	// this is for enumerating the devices
	uint32_t curDiv = 0;
	uint32_t selNum = 0;
	
	for (int pl=0; pl<platforms.size(); pl++) {
		// Create the OpenCL contexts, one for each platform
		cl_context_properties properties[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[pl](), 0};  
		cl::Context context;
		if (allowCPU) { 
			context = cl::Context(CL_DEVICE_TYPE_ALL, properties);
		} else {
			context = cl::Context(CL_DEVICE_TYPE_GPU, properties);
		}
		contexts.push_back(context);

		// Read the devices of this platform
		vector< cl::Device > nDev = context.getInfo<CL_CONTEXT_DEVICES>();  
		for (uint32_t di=0; di<nDev.size(); di++) {
			
			// Print the device name
			string name;
			if ( hasExtension(nDev[di], "cl_amd_device_attribute_query") ) {
				nDev[di].getInfo(0x4038,&name);			// on AMD this gives more readable result
			} else {
				nDev[di].getInfo(CL_DEVICE_NAME, &name); 	// for all other GPUs
			}

			// Get rid of strange characters at the end of device name
			if (isalnum((int) name.back()) == 0) {
				name.pop_back();
			} 
			
			cout << "Found device " << curDiv << ": " << name << endl;

			// Check if the device should be selected
			bool pick = false;

			if (selDev[0] == -1) pick = true;
			if (selNum < selDev.size()) {
				if (curDiv == selDev[selNum]) {
					pick = true;
					selNum++;

					
				}				
			}
			

			if (pick) {
				// Check if the CPU / GPU has enough memory
				uint64_t deviceMemory = nDev[di].getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
				uint64_t needed_4G = 7* ((uint64_t) 570425344) + 4096 + 196608 + 1296;
				uint64_t needed_3G = 4* ((uint64_t) 556793856) + ((uint64_t) 835190784) + 4096 + 196608 + 1296;

				cout << "   Device reports " << deviceMemory / (1024*1024) << "MByte total memory" << endl;

				if ( hasExtension(nDev[di], "cl_amd_device_attribute_query") ) {
					uint64_t freeDeviceMemory;
				 	nDev[di].getInfo(0x4039, &freeDeviceMemory);  // CL_DEVICE_GLOBAL_FREE_MEMORY_AMD
					freeDeviceMemory *= 1024;
					cout << "   Device reports " << freeDeviceMemory / (1024*1024) << "MByte free memory (AMD)" << endl;
					deviceMemory = min<uint64_t>(deviceMemory, freeDeviceMemory);
				}
				

				if ((deviceMemory > needed_4G) && (!force3G)) {
					cout << "   Memory check for 4G kernel passed" << endl;
					loadAndCompileKernel(nDev[di], pl, false);
				} else if (deviceMemory > needed_3G) {
					cout << "   Memory check for 3G kernel passed" << endl;
					loadAndCompileKernel(nDev[di], pl, true);
				}  else {
					cout << "   Memory check failed, required minimum memory: " << needed_3G/(1024*1024) << endl;
				}
			} else {
				cout << "   Device will not be used, it was not included in --devices parameter." << endl;
			}

			curDiv++; 
		}
	}

	if (devices.size() == 0) {
		cout << "No compatible OpenCL devices found or all are deselected. Closing beamMiner." << endl;
		exit(0);
	}
}


// Setup function called from outside
void clHost::setup(beamStratum* stratumIn, vector<int32_t> devSel,  bool allowCPU, bool force3G) {
	stratum = stratumIn;
	detectPlatFormDevices(devSel, allowCPU, force3G);
}


// Function that will catch new work from the stratum interface and then queue the work on the device
void clHost::queueKernels(uint32_t gpuIndex, clCallbackData* workData) {
	cl_ulong4 work;	
	cl_ulong nonce;

	// Get a new set of work from the stratum interface
	stratum->getWork(workData->wd, (uint8_t *) &work);
	nonce = workData->wd.nonce;

	if (!is3G[gpuIndex]) {		// Starting the 4G kernels
		// Kernel arguments for cleanCounter
		kernels[gpuIndex][0].setArg(0, buffers[gpuIndex][5]); 
		kernels[gpuIndex][0].setArg(1, buffers[gpuIndex][6]);

		// Kernel arguments for round0
		kernels[gpuIndex][1].setArg(0, buffers[gpuIndex][0]); 
		kernels[gpuIndex][1].setArg(1, buffers[gpuIndex][2]); 
		kernels[gpuIndex][1].setArg(2, buffers[gpuIndex][5]); 
		kernels[gpuIndex][1].setArg(3, work); 
		kernels[gpuIndex][1].setArg(4, nonce); 

		// Kernel arguments for round1
		kernels[gpuIndex][2].setArg(0, buffers[gpuIndex][0]); 
		kernels[gpuIndex][2].setArg(1, buffers[gpuIndex][2]); 
		kernels[gpuIndex][2].setArg(2, buffers[gpuIndex][1]); 
		kernels[gpuIndex][2].setArg(3, buffers[gpuIndex][3]); 	// Index tree will be stored here
		kernels[gpuIndex][2].setArg(4, buffers[gpuIndex][5]); 

		// Kernel arguments for round2
		kernels[gpuIndex][3].setArg(0, buffers[gpuIndex][1]); 
		kernels[gpuIndex][3].setArg(1, buffers[gpuIndex][0]);	// Index tree will be stored here 
		kernels[gpuIndex][3].setArg(2, buffers[gpuIndex][5]); 

		// Kernel arguments for round3
		kernels[gpuIndex][4].setArg(0, buffers[gpuIndex][0]); 
		kernels[gpuIndex][4].setArg(1, buffers[gpuIndex][1]); 	// Index tree will be stored here 
		kernels[gpuIndex][4].setArg(2, buffers[gpuIndex][5]); 

		// Kernel arguments for round4
		kernels[gpuIndex][5].setArg(0, buffers[gpuIndex][1]); 
		kernels[gpuIndex][5].setArg(1, buffers[gpuIndex][2]); 	// Index tree will be stored here 
		kernels[gpuIndex][5].setArg(2, buffers[gpuIndex][5]);  

		// Kernel arguments for round5
		kernels[gpuIndex][6].setArg(0, buffers[gpuIndex][2]); 
		kernels[gpuIndex][6].setArg(1, buffers[gpuIndex][4]); 	// Index tree will be stored here 
		kernels[gpuIndex][6].setArg(2, buffers[gpuIndex][5]);  

		// Kernel arguments for Combine
		kernels[gpuIndex][7].setArg(0, buffers[gpuIndex][0]); 
		kernels[gpuIndex][7].setArg(1, buffers[gpuIndex][1]); 	
		kernels[gpuIndex][7].setArg(2, buffers[gpuIndex][2]); 
		kernels[gpuIndex][7].setArg(3, buffers[gpuIndex][3]); 	
		kernels[gpuIndex][7].setArg(4, buffers[gpuIndex][4]); 
		kernels[gpuIndex][7].setArg(5, buffers[gpuIndex][5]); 	
		kernels[gpuIndex][7].setArg(6, buffers[gpuIndex][6]);

		cl_int err;
		// Queue the kernels
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][0], cl::NDRange(0), cl::NDRange(12288), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][1], cl::NDRange(0), cl::NDRange(22369536), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][2], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
		queues[gpuIndex].flush();
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][3], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][4], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][5], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][6], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][7], cl::NDRange(0), cl::NDRange(4096), cl::NDRange(16), NULL, NULL);
	} else {	// Starting the 3G kernels
		// Kernel arguments for cleanCounter
		kernels[gpuIndex][0].setArg(0, buffers[gpuIndex][5]); 
		kernels[gpuIndex][0].setArg(1, buffers[gpuIndex][6]);

		// Kernel arguments for round0
		kernels[gpuIndex][1].setArg(0, buffers[gpuIndex][0]); 
		kernels[gpuIndex][1].setArg(1, buffers[gpuIndex][5]); 
		kernels[gpuIndex][1].setArg(2, work); 
		kernels[gpuIndex][1].setArg(3, nonce); 
		kernels[gpuIndex][1].setArg(4, (cl_uint) 0); 

		 // Kernel arguments for round1
		kernels[gpuIndex][2].setArg(0, buffers[gpuIndex][0]); 
		kernels[gpuIndex][2].setArg(1, buffers[gpuIndex][1]); 
		kernels[gpuIndex][2].setArg(2, buffers[gpuIndex][2]);  // Index tree will be stored here 
		kernels[gpuIndex][2].setArg(3, buffers[gpuIndex][5]); 
		kernels[gpuIndex][2].setArg(4, (cl_uint) 0); 

		// Kernel arguments for round2
		kernels[gpuIndex][3].setArg(0, buffers[gpuIndex][1]); 
		kernels[gpuIndex][3].setArg(1, buffers[gpuIndex][0]);	// Index tree will be stored here 
		kernels[gpuIndex][3].setArg(2, buffers[gpuIndex][5]); 

		// Kernel arguments for move
		kernels[gpuIndex][9].setArg(0, buffers[gpuIndex][2]); 
		kernels[gpuIndex][9].setArg(1, buffers[gpuIndex][1]); 	

		// Kernel arguments for repack
		kernels[gpuIndex][8].setArg(0, buffers[gpuIndex][1]); 
		kernels[gpuIndex][8].setArg(1, buffers[gpuIndex][0]); 
		kernels[gpuIndex][8].setArg(2, buffers[gpuIndex][2]); 	// Index tree will be stored here 	

		// Kernel arguments for round3
		kernels[gpuIndex][4].setArg(0, buffers[gpuIndex][0]); 
		kernels[gpuIndex][4].setArg(1, buffers[gpuIndex][1]); 	// Index tree will be stored here 
		kernels[gpuIndex][4].setArg(2, buffers[gpuIndex][5]); 

		// Kernel arguments for round4
		kernels[gpuIndex][5].setArg(0, buffers[gpuIndex][1]); 
		kernels[gpuIndex][5].setArg(1, buffers[gpuIndex][0]); 	// Index tree will be stored here 
		kernels[gpuIndex][5].setArg(2, buffers[gpuIndex][5]);  

		// Kernel arguments for round5
		kernels[gpuIndex][6].setArg(0, buffers[gpuIndex][0]); 
		kernels[gpuIndex][6].setArg(1, buffers[gpuIndex][4]); 	// Index tree will be stored here 
		kernels[gpuIndex][6].setArg(2, buffers[gpuIndex][5]);  

		// Kernel arguments for Combine
		kernels[gpuIndex][7].setArg(0, buffers[gpuIndex][1]); 
		kernels[gpuIndex][7].setArg(1, buffers[gpuIndex][2]); 	
		kernels[gpuIndex][7].setArg(2, buffers[gpuIndex][4]);  
		kernels[gpuIndex][7].setArg(3, buffers[gpuIndex][5]); 	
		kernels[gpuIndex][7].setArg(4, buffers[gpuIndex][6]);

		

		cl_int err;
		// Queue the kernels
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][0], cl::NDRange(0), cl::NDRange(12288), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][1], cl::NDRange(0), cl::NDRange(22369536), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][2], cl::NDRange(0), cl::NDRange(8388608), cl::NDRange(256), NULL, NULL);
		queues[gpuIndex].flush();
		
		kernels[gpuIndex][1].setArg(4, (cl_uint) 1); 
		kernels[gpuIndex][2].setArg(4, (cl_uint) 1); 
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][1], cl::NDRange(0), cl::NDRange(22369536), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][2], cl::NDRange(0), cl::NDRange(8388608), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][3], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][9], cl::NDRange(0), cl::NDRange(34799616), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][8], cl::NDRange(0), cl::NDRange(69599232), cl::NDRange(256), NULL, NULL);
		queues[gpuIndex].flush();
		
		
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][4], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][5], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][6], cl::NDRange(0), cl::NDRange(16777216), cl::NDRange(256), NULL, NULL);
		err = queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][7], cl::NDRange(0), cl::NDRange(4096), cl::NDRange(16), NULL, NULL); 
	}

	
}


// this function will sumit the solutions done on GPU, then fetch new work and restart mining
void clHost::callbackFunc(cl_int err , void* data){
	clCallbackData* workInfo = (clCallbackData*) data;
	uint32_t gpu = workInfo->gpuIndex;

	// Read the number of solutions of the last iteration
	uint32_t solutions = results[gpu][0];
	for (uint32_t  i=0; i<solutions; i++) {
		vector<uint32_t> indexes;
		indexes.assign(32,0);
		memcpy(indexes.data(), &results[gpu][4 + 32*i], sizeof(uint32_t) * 32);

		stratum->handleSolution(workInfo->wd,indexes);
	}

	solutionCnt[gpu] += solutions;

	// Get new work and resume working
	if (stratum->hasWork()) {
		queues[gpu].enqueueUnmapMemObject(buffers[gpu][6], results[gpu], NULL, NULL);
		queueKernels(gpu, &currentWork[gpu]);
		results[gpu] = (unsigned *) queues[gpu].enqueueMapBuffer(buffers[gpu][6], CL_FALSE, CL_MAP_READ, 0, sizeof(cl_uint4) * 81, NULL, &events[gpu], NULL);
		events[gpu].setCallback(CL_COMPLETE, &CCallbackFunc, (void*) &currentWork[gpu]);
		queues[gpu].flush();
	} else {
		paused[gpu] = true;
		cout << "Device will be paused, waiting for new work" << endl;
	}
}


void clHost::startMining() {

	// Start mining initially
	for (int i=0; i<devices.size(); i++) {	
		paused[i] = false;

		currentWork[i].gpuIndex = i;
		currentWork[i].host = (void*) this;
		queueKernels(i, &currentWork[i]);

		results[i] = (unsigned *) queues[i].enqueueMapBuffer(buffers[i][6], CL_FALSE, CL_MAP_READ, 0, sizeof(cl_uint4) * 81, NULL, &events[i], NULL);
		events[i].setCallback(CL_COMPLETE, &CCallbackFunc, (void*) &currentWork[i]);
		queues[i].flush();
	}


	// While the mining is running print some statistics
	while (restart) {
		this_thread::sleep_for(std::chrono::seconds(15));

		// Print performance stats (roughly)
		cout << "Performance: ";
		uint32_t totalSols = 0;
		for (int i=0; i<devices.size(); i++) {
			uint32_t sol = solutionCnt[i];
			solutionCnt[i] = 0;
			totalSols += sol;
			cout << fixed << setprecision(2) << (double) sol / 15.0 << " sol/s ";
			
		}

		if (devices.size() > 1) cout << "| Total: " << setprecision(2) << (double) totalSols / 15.0 << " sol/s ";
		cout << endl;

		// Check if there are paused devices and restart them
		for (int i=0; i<devices.size(); i++) {
			if (paused[i] && stratum->hasWork()) {
				paused[i] = false;
				
				// Same as above
				queueKernels(i, &currentWork[i]);

				results[i] = (unsigned *) queues[i].enqueueMapBuffer(buffers[i][6], CL_FALSE, CL_MAP_READ, 0, sizeof(cl_uint4) * 81, NULL, &events[i], NULL);
				events[i].setCallback(CL_COMPLETE, &CCallbackFunc, (void*) &currentWork[i]);
				queues[i].flush();
			}

		}
	}
}


} 	// end namespace



