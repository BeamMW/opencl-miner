// BEAM OpenCL Miner
// OpenCL Host Interface
// Copyright 2018 The Beam Team	
// Copyright 2018 Wilke Trei

#include "clHost.h"

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



// Detect the OpenCL hardware on this system
void clHost::detectPlatFormDevices(vector<int32_t> selDev, bool allowCPU) {
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
				cl_command_queue_properties queue_prop = 0;  
				devices.push_back(nDev[di]);
				currentWork.push_back(clCallbackData());
				deviceContext.push_back(pl);
				events.push_back(cl::Event());
				paused.push_back(false);
				solutionCnt.push_back(0);
				queues.push_back(cl::CommandQueue(contexts[pl], devices[devices.size()-1], queue_prop, NULL)); 
			}

			curDiv++; 
		}
	}

	if (devices.size() == 0) {
		cout << "No compatible OpenCL devices found or all are deselected. Closing beamMiner." << endl;
		exit(0);
	}

	BeamHashIII.setup(devices, contexts, deviceContext);
}


// Setup function called from outside
void clHost::setup(beamStratum* stratumIn, vector<int32_t> devSel) {
	stratum = stratumIn;
	detectPlatFormDevices(devSel, false);
}



// this function will sumit the solutions done on GPU, then fetch new work and restart mining
void clHost::callbackFunc(cl_int err , void* data){
	clCallbackData* workInfo = (clCallbackData*) data;
	uint32_t gpu = workInfo->gpuIndex;

	beamSolver * activeSolver;
	switch (workInfo->currentSolver) {
		case BeamIII:
			activeSolver = &BeamHashIII;
			break;

		case None: 
			paused[gpu] = true;
			return;
	}

	uint32_t * results = activeSolver->getResults(gpu);
	
	// Read the number of solutions of the last iteration
	uint32_t solutions = results[0];
	//cout << solutions << endl;
	solutions = min<uint32_t>(solutions, 10);

	for (uint32_t  i=0; i<solutions; i++) {
		vector<uint32_t> indexes;
		indexes.assign(32,0);
		memcpy(indexes.data(), &results[4 + 32*i], sizeof(uint32_t) * 32);

		stratum->handleSolution(workInfo->wd,indexes);
	}

	solutionCnt[gpu] += solutions;

	// Get new work and resume working
	if (stratum->hasWork()) {

		activeSolver->unmapResult(&queues[gpu], gpu);

		// Get a new set of work from the stratum interface
		solverType nextSolver;
		stratum->getWork(workInfo->wd, &nextSolver);
		
		if (nextSolver != workInfo->currentSolver) {
			activeSolver->stop(gpu); 
			workInfo->currentSolver = nextSolver;

			switch (workInfo->currentSolver) {
				case BeamIII:
					activeSolver = &BeamHashIII;
					break;

				case None: 
					paused[gpu] = true;
					return;
			}

			activeSolver->createBuffers(contexts[deviceContext[gpu]], devices[gpu], gpu);
		}
		
		activeSolver->queueKernels(&queues[gpu], gpu, &events[gpu], workInfo);
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
		//queueKernels(i, &currentWork[i]);

		solverType nextSolver;
		stratum->getWork(currentWork[i].wd, &nextSolver);

		beamSolver * activeSolver;		
		switch (nextSolver) {
			case BeamIII:
				activeSolver = &BeamHashIII;
				break;

			case None: 
				paused[i] = true;
				break;
		}

		currentWork[i].currentSolver = nextSolver;

		activeSolver->createBuffers(contexts[deviceContext[i]], devices[i], i);
		activeSolver->queueKernels(&queues[i], i, &events[i], &currentWork[i]);
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

		/* 

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

		} */
	} 
}


} 	// end namespace



