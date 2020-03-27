

#include "core/difficulty.h"

#ifndef beamUtil_h
#define beamUtil_h

namespace beamMiner {

	enum solverType {All, None, BeamI, BeamII, BeamIII};

	struct WorkDescription 	{
		solverType solver;
		int64_t workId;
		uint64_t nonce;
		uint64_t work[4];
		beam::Difficulty powDiff;
	};

	struct clCallbackData {
		void* host;
		uint32_t gpuIndex;
		solverType currentSolver=None;
		WorkDescription wd;
	};

}


#endif
