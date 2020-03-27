#define bucketSize 8720 

#ifndef wgSize 
#define wgSize 256
#endif

#define SIPROUND 				\
    v0 += v1; v2 += v3; 			\
    v1 = rotate(v1, 13UL); 			\
    v3 = rotate(v3, 16UL); 			\
    v1 ^= v0; v3 ^= v2; 			\
    ((uint2*)&v0)[0] = ((uint2*)&v0)[0].yx; 	\
    v2 += v1; v0 += v3; 			\
    v1 = rotate(v1, 17UL); 			\
    v3 = rotate(v3, 21UL); 			\
    v1 ^= v2; v3 ^= v0; 			\
    ((uint2*)&v2)[0] = ((uint2*)&v2)[0].yx; 

inline ulong sipHash24(ulong4 prePow, ulong nonce) {
	ulong v0 = prePow.s0, v1 = prePow.s1, v2 = prePow.s2, v3 = prePow.s3 ^ nonce;
	
	SIPROUND; SIPROUND;
	v0 ^= nonce;
	v2 ^= 0xff;
	SIPROUND; SIPROUND; SIPROUND; SIPROUND;
	return (v0 ^ v1 ^ v2  ^ v3);
}

inline ulong mixer (ulong8 input) {
	ulong result;

	result = rotate(input.s0, 29UL);
	result += rotate(input.s1, 58UL);
	result += rotate(input.s2, 23UL);
	result += rotate(input.s3, 52UL);
	result += rotate(input.s4, 17UL);
	result += rotate(input.s5, 46UL);
	result += rotate(input.s6, 11UL);
	result += rotate(input.s7, 40UL);

	return rotate(result, 24UL);
} 

ulong8 shift24(ulong8 input) {
	ulong8 tmp = (input >> 24);
	ulong8 tmp2 = (input << 40);

	tmp.s0123 |= tmp2.s1234;
	tmp.s456 |= tmp2.s567;

	return tmp;
}

ulong8 shift56(ulong8 input) {
	ulong8 tmp = (input >> 56);
	ulong8 tmp2 = (input << 8);

	tmp.s0123 |= tmp2.s1234;
	tmp.s456 |= tmp2.s567;

	return tmp;
}


/*
	Kernel Clearing all counters
*/
__attribute__((reqd_work_group_size(wgSize, 1, 1)))
__kernel void cleanUp(	__global ulong8 * buffer0,
			__global ulong8 * buffer1,
			__global uint4 * counters,
			__global uint * results,
			ulong4 prePow) {

	uint gId = get_global_id(0);
	counters[gId] = (uint4) 0;

	if (get_global_id(0) == 0) {
		results[0] = 0;
	}
}

/*
	Kernel for round 0 (seed)
	Writing to buffer0
*/
__attribute__((reqd_work_group_size(wgSize, 1, 1)))
__kernel void beamHashIII_seed (__global ulong8 * buffer0,
				__global ulong8 * buffer1,
				__global uint * counters,
				__global uint * results,
				ulong4 prePow) {

	uint gId = get_global_id(0);

	ulong8 elem;
	elem.s0 = sipHash24(prePow, (ulong) (gId << 3)+0);
	elem.s1 = sipHash24(prePow, (ulong) (gId << 3)+1);
	elem.s2 = sipHash24(prePow, (ulong) (gId << 3)+2);
	elem.s3 = sipHash24(prePow, (ulong) (gId << 3)+3);
	elem.s4 = sipHash24(prePow, (ulong) (gId << 3)+4);
	elem.s5 = sipHash24(prePow, (ulong) (gId << 3)+5);
	elem.s6 = sipHash24(prePow, (ulong) (gId << 3)+6); 
	elem.s7 = (ulong) gId;

	//

	// Mixing for round 1

	elem.s0 = mixer(elem);

	//if ((get_global_id(0) == 7579378) || (get_global_id(0) == 24760)) printf("PostMix: %d %lu \n", get_global_id(0), elem.s0 );

	uint bucket = (uint) elem.s0 & 0xFFF;
	uint pos = atomic_inc(&counters[bucket]);
	pos = min(pos, (uint) (bucketSize-1));
	pos += bucket * bucketSize;

	buffer0[pos] = elem;	


}

/*
	Kernel for round 1 
	Reading from buffer0
	Writing to buffer1
*/
__attribute__((reqd_work_group_size(wgSize, 1, 1)))
__kernel void beamHashIII_R1 (	__global ulong8 * buffer0,
				__global ulong8 * buffer1,
				__global uint * counters,
				__global uint * results,
				ulong4 prePow) {

	uint bucket  = get_group_id(0);
	uint locSize = get_local_size(0);
	uint lId     = get_local_id(0);

	uint mask  = bucket & 0x3;
	bucket     = bucket >> 2;

	uint inputOfs = 0;
	uint outputOfs = 4096;

	uint inLim = counters[inputOfs+bucket];

	__local uint match[1024];
	__local uint table[2560];
	__local uint inCounter[1];

	for (uint i=lId; i<1024; i+=locSize) {
		match[i] = 0xFFF;
	} 

	inCounter[0] = 0;

	barrier(CLK_LOCAL_MEM_FENCE);

	for (uint i=lId; i<inLim; i+=locSize) {
		ulong8 input = buffer0[mad24((uint) bucketSize, bucket, i)];

		if (((input.s0 >> 12) & 0x3) == mask) {
			uint inPos = atomic_inc(&inCounter[0]);
			inPos = min(inPos, (uint) 2560);
	
			uint slot = (input.s0 >> 14) & 0x3FF;
			uint ret = atomic_xchg(&match[slot], inPos);
			
			table[inPos] = ret | (i << 16);
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	for (uint i=lId; i<inCounter[0]; i+=locSize) {
		uint elemPos0 = table[i] >> 16;
		uint nextElem = table[i] & 0xFFF;

		while (nextElem != 0xFFF) {
			// Loop iteration through table
			uint elemPos1 = table[nextElem] >> 16;
			nextElem = table[nextElem] & 0xFFF;

			// Fetch the matched elements
			ulong8 stepRow0 = buffer0[mad24((uint) bucketSize, bucket, elemPos0)];
			ulong8 stepRow1 = buffer0[mad24((uint) bucketSize, bucket, elemPos1)];

			// xoring the work bits
			stepRow0.s0123 ^= stepRow1.s0123;
			stepRow0.s456  ^= stepRow1.s456;

			// Sorting & Serializing the index tree
			ulong2 indexTree = (ulong2) (stepRow0.s7, stepRow1.s7); 
			indexTree = (stepRow0.s7 < stepRow1.s7) ? indexTree.s01 : indexTree.s10; 

			stepRow0.s7 = indexTree.s0 | (indexTree.s1 << 25);
			//stepRow0.s7 = (ulong) 4089119  | (((ulong) 24478574) << 25);

			// Shifting away the just matched bits
			stepRow0 = shift24(stepRow0);

			// Mix for round 2
			stepRow0.s0 = mixer(stepRow0);

			// Bucket sort for round 2
			uint bucket = (uint) stepRow0.s0 & 0xFFF;
			uint pos = atomic_inc(&counters[outputOfs+bucket]);
			pos = min(pos, (uint) (bucketSize-1));
			pos += bucket * bucketSize;
			
			buffer1[pos] = stepRow0;	
		}
	} 
}

/*
	Kernel for round 2 
	Reading from buffer1
	Writing to buffer0
*/
__attribute__((reqd_work_group_size(wgSize, 1, 1)))
__kernel void beamHashIII_R2 (	__global ulong8 * buffer0,
				__global ulong8 * buffer1,
				__global uint * counters,
				__global uint * results,
				ulong4 prePow) {

	uint bucket  = get_group_id(0);
	uint locSize = get_local_size(0);
	uint lId     = get_local_id(0);

	uint mask  = bucket & 0x3;
	bucket     = bucket >> 2;

	uint inputOfs = 4096;
	uint outputOfs = 8192;

	uint inLim = counters[inputOfs+bucket];

	__local uint match[1024];
	__local uint table[2560];
	__local uint inCounter[1];

	for (uint i=lId; i<1024; i+=locSize) {
		match[i] = 0xFFF;
	} 

	inCounter[0] = 0;

	barrier(CLK_LOCAL_MEM_FENCE);

	for (uint i=lId; i<inLim; i+=locSize) {
		ulong8 input = buffer1[mad24((uint) bucketSize, bucket, i)];

		if (((input.s0 >> 12) & 0x3) == mask) {
			uint inPos = atomic_inc(&inCounter[0]);
			inPos = min(inPos, (uint) 2560);
	
			uint slot = (input.s0 >> 14) & 0x3FF;
			uint ret = atomic_xchg(&match[slot], inPos);
			
			table[inPos] = ret | (i << 16);
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	for (uint i=lId; i<inCounter[0]; i+=locSize) {
		uint elemPos0 = table[i] >> 16;
		uint nextElem = table[i] & 0xFFF;

		while (nextElem != 0xFFF) {
			// Loop iteration through table
			uint elemPos1 = table[nextElem] >> 16;
			nextElem = table[nextElem] & 0xFFF;

			// Fetch the matched elements
			ulong8 stepRow0 = buffer1[mad24((uint) bucketSize, bucket, elemPos0)];
			ulong8 stepRow1 = buffer1[mad24((uint) bucketSize, bucket, elemPos1)];

			// xoring the work bits 0 to 424
			stepRow0.s0123 ^= stepRow1.s0123;
			stepRow0.s45  ^= stepRow1.s45;
			stepRow0.s6 ^= (stepRow1.s6 & 0xFFFFFFFFFFUL);

			// Sorting the index tree
			ulong2 indexTree; 
			indexTree.s0 = (stepRow0.s7 << 24) | (stepRow0.s6 >> 40);
			indexTree.s1 = (stepRow1.s7 << 24) | (stepRow1.s6 >> 40);
			indexTree = ((indexTree.s0 & 0x1FFFFFF) < (indexTree.s1 & 0x1FFFFFF)) ? indexTree.s01 : indexTree.s10; 


			//if (get_global_id(0) == 0) printf("R1 out: \n%lu %lu %lu %lu \n\n", (indexTree.s0 & 0x1FFFFFF), ((indexTree.s0 >> 25) & 0x1FFFFFF), (indexTree.s1 & 0x1FFFFFF), ((indexTree.s1 >> 25) & 0x1FFFFFF)),

			// Shifting away the just matched bits
			stepRow0.s6 &= 0xFFFFFFFFFFUL;
			stepRow0.s7 = 0;
			stepRow0 = shift24(stepRow0);

			// Serializing the index tree
			stepRow0.s6 |= (indexTree.s0 << 16);	
			stepRow0.s7 = (indexTree.s0 >> 48) | (indexTree.s1 << 2);			

			// Mix for round 3
			stepRow0.s0 = mixer(stepRow0);

			// Bucket sort for round 3
			uint bucket = (uint) stepRow0.s0 & 0xFFF;
			uint pos = atomic_inc(&counters[outputOfs+bucket]);
			pos = min(pos, (uint) (bucketSize-1));
			pos += bucket * bucketSize;
		
			buffer0[pos] = stepRow0;
		}
	} 
}


/*
	Kernel for round 3 
	Reading from buffer0
	Writing to buffer1
*/
__attribute__((reqd_work_group_size(wgSize, 1, 1)))
__kernel void beamHashIII_R3 (	__global ulong8 * buffer0,
				__global ulong8 * buffer1,
				__global uint * counters,
				__global uint * results,
				ulong4 prePow) {

	uint bucket  = get_group_id(0);
	uint locSize = get_local_size(0);
	uint lId     = get_local_id(0);

	uint mask  = bucket & 0x3;
	bucket     = bucket >> 2;

	uint inputOfs = 8192;
	uint outputOfs = 12288;

	uint inLim = counters[inputOfs+bucket];

	__local uint match[1024];
	__local uint table[2560];
	__local uint inCounter[1];

	for (uint i=lId; i<1024; i+=locSize) {
		match[i] = 0xFFF;
	} 

	inCounter[0] = 0;

	barrier(CLK_LOCAL_MEM_FENCE);

	for (uint i=lId; i<inLim; i+=locSize) {
		ulong8 input = buffer0[mad24((uint) bucketSize, bucket, i)];

		if (((input.s0 >> 12) & 0x3) == mask) {
			uint inPos = atomic_inc(&inCounter[0]);
			inPos = min(inPos, (uint) 2560);
	
			uint slot = (input.s0 >> 14) & 0x3FF;
			uint ret = atomic_xchg(&match[slot], inPos);
			
			table[inPos] = ret | (i << 16);
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	for (uint i=lId; i<inCounter[0]; i+=locSize) {
		uint elemPos0 = table[i] >> 16;
		uint nextElem = table[i] & 0xFFF;

		while (nextElem != 0xFFF) {
			// Loop iteration through table
			uint elemPos1 = table[nextElem] >> 16;
			nextElem = table[nextElem] & 0xFFF;

			// Fetch the matched elements
			ulong8 stepRow0 = buffer0[mad24((uint) bucketSize, bucket, elemPos0)];
			ulong8 stepRow1 = buffer0[mad24((uint) bucketSize, bucket, elemPos1)];

			// xoring the work bits 0 to 400
			stepRow0.s0123 ^= stepRow1.s0123;
			stepRow0.s45  ^= stepRow1.s45;
			stepRow0.s6 ^= (stepRow1.s6 & 0xFFFFUL);

			// Sorting the index tree
			ulong4 indexTree; 
			indexTree.s01 = stepRow0.s67;
			indexTree.s23 = stepRow1.s67;
			indexTree = (((indexTree.s0 >> 16) & 0x1FFFFFF) < ((indexTree.s2 >> 16) & 0x1FFFFFF)) ? indexTree : indexTree.s2301; 
			indexTree.s02 = (indexTree.s02 >> 16);


			//if (get_global_id(0) == 0) printf("R2 out: \n%d %d %d %d \n\n", (indexTree.s0 & 0x1FFFFFF), ((indexTree.s0 >> 25) & 0x7FFFFF) | ((indexTree.s1 & 0x3) << 23), ((indexTree.s1 >> 2) & 0x1FFFFFF), ((indexTree.s1 >> 27) & 0x1FFFFFF));
		

			// Shifting away the just matched bits
			stepRow0.s6 &= 0xFFFFUL;
			stepRow0.s7 = 0;
			stepRow0 = shift24(stepRow0);

			// Serializing the index tree (Low part)
			stepRow0.s5 |= (indexTree.s0 << 56);
			stepRow0.s6 = (indexTree.s0 >> 8);	
			stepRow0.s6 |= (indexTree.s1 << 40);
			stepRow0.s7 = (indexTree.s1 >> 24) | (indexTree.s2 << 28);	

			// Mix for round 4
			stepRow0.s0 = mixer(stepRow0);

			// Drop of 64 bits after mix
			stepRow0.s4 &= 0x00FFFFFFFFFFFFFFUL;
			stepRow0.s4 |= (stepRow0.s5 & 0xFF00000000000000UL); 
			stepRow0.s56 = stepRow0.s67;

			// Add the missing index tree bits (high half)
			stepRow0.s7 = (indexTree.s2 >> 36);
			stepRow0.s7 |= (indexTree.s3 << 12);

			// Bucket sort for round 4
			uint bucket = (uint) stepRow0.s0 & 0xFFF;
			uint pos = atomic_inc(&counters[outputOfs+bucket]);
			pos = min(pos, (uint) (bucketSize-1));
			pos += bucket * bucketSize;
		
			buffer1[pos] = stepRow0;
		}
	} 
}


/*
	Kernel for round 4 
	Reading from buffer1
	Writing to buffer0
*/
__attribute__((reqd_work_group_size(wgSize, 1, 1)))
__kernel void beamHashIII_R4 (	__global ulong8 * buffer0,
				__global ulong8 * buffer1,
				__global uint * counters,
				__global uint * results,
				ulong4 prePow) {

	uint bucket  = get_group_id(0);
	uint locSize = get_local_size(0);
	uint lId     = get_local_id(0);

	uint mask  = bucket & 0x3;
	bucket     = bucket >> 2;

	uint inputOfs = 12288;
	uint outputOfs = 16384;

	uint inLim = counters[inputOfs+bucket];

	__local uint match[1024];
	__local uint table[2560];
	__local uint inCounter[1];

	for (uint i=lId; i<1024; i+=locSize) {
		match[i] = 0xFFF;
	} 

	inCounter[0] = 0;

	barrier(CLK_LOCAL_MEM_FENCE);

	for (uint i=lId; i<inLim; i+=locSize) {
		ulong8 input = buffer1[mad24((uint) bucketSize, bucket, i)];

		if (((input.s0 >> 12) & 0x3) == mask) {
			uint inPos = atomic_inc(&inCounter[0]);
			inPos = min(inPos, (uint) 2560);
	
			uint slot = (input.s0 >> 14) & 0x3FF;
			uint ret = atomic_xchg(&match[slot], inPos);
			
			table[inPos] = ret | (i << 16);
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	for (uint i=lId; i<inCounter[0]; i+=locSize) {
		uint elemPos0 = table[i] >> 16;
		uint nextElem = table[i] & 0xFFF;

		while (nextElem != 0xFFF) {
			// Loop iteration through table
			uint elemPos1 = table[nextElem] >> 16;
			nextElem = table[nextElem] & 0xFFF;

			// Fetch the matched elements
			ulong8 stepRow0 = buffer1[mad24((uint) bucketSize, bucket, elemPos0)];
			ulong8 stepRow1 = buffer1[mad24((uint) bucketSize, bucket, elemPos1)];

			// xoring the work bits 0 to 312
			stepRow0.s0123 ^= stepRow1.s0123;
			stepRow0.s4 ^= stepRow1.s4 & 0xFFFFFFFFFFFFFFUL;

			// Sorting the index tree
			ulong8 indexTree; 
			indexTree.lo = stepRow0.hi;
			indexTree.hi = stepRow1.hi;
			indexTree.s0 = (indexTree.s0 >> 56) | (indexTree.s1 << 8);
			indexTree.s4 = (indexTree.s4 >> 56) | (indexTree.s5 << 8);
			indexTree = ((indexTree.s0 & 0x1FFFFFF) < (indexTree.s4 & 0x1FFFFFF)) ? indexTree : indexTree.s45670123; 


			// Shifting away the just matched bits
			stepRow0.s4 &= 0xFFFFFFFFFFFFFFUL;
			stepRow0.s5 = 0;
			stepRow0.s67 = (ulong2) 0;
			stepRow0 = shift24(stepRow0);

			// Serializing the index tree (trucated to 512 bit)
			stepRow0.s4 |= (indexTree.s0 << 32);
			stepRow0.s5 = (indexTree.s1 >> 24);	
			stepRow0.s5 |= (indexTree.s2 << 40);
			stepRow0.s6 = (indexTree.s2 >> 24);	
			stepRow0.s6 |= (indexTree.s3 << 40);
			stepRow0.s7 = (indexTree.s3 >> 24);
	
			stepRow0.s7 |= (indexTree.s4 << 40);			

			// Mix for round 5
			stepRow0.s0 = mixer(stepRow0);

			// Drop all bits except the needed matchbits (48) and store the index tree
			indexTree.s0 = (indexTree.s0 << 56);
			indexTree.s0 |= (stepRow0.s0 & 0xFFFFFFFFFFFF);

			// Serialize the high bits properly
			indexTree.s5 = (indexTree.s5 >> 56) | (indexTree.s6 << 8);
			indexTree.s6 = (indexTree.s6 >> 56) | (indexTree.s7 << 8);
			indexTree.s7 = (indexTree.s7 >> 56) | (indexTree.s7 << 8);

			stepRow0 = indexTree;

			// Bucket sort for round 5
			uint bucket = (uint) stepRow0.s0 & 0xFFF;
			uint pos = atomic_inc(&counters[outputOfs+bucket]);
			pos = min(pos, (uint) (bucketSize-1));
			pos += bucket * bucketSize;
		
			buffer0[pos] = stepRow0;
		}
	} 
}


/*
	Kernel for round 5 
	Reading from buffer0
	Writing results
*/
__attribute__((reqd_work_group_size(wgSize, 1, 1)))
__kernel void beamHashIII_R5 (	__global ulong8 * buffer0,
				__global ulong8 * buffer1,
				__global uint * counters,
				__global uint * results,
				ulong4 prePow) {

	uint bucket  = get_group_id(0);
	uint locSize = get_local_size(0);
	uint lId     = get_local_id(0);

	uint mask  = bucket & 0x3;
	bucket     = bucket >> 2;

	uint inputOfs = 16384;
	uint inLim = counters[inputOfs+bucket];

	__local uint match[1024];
	__local uint table[2560];
	__local uint inCounter[1];

	__global ulong2 * resultsUL = (__global ulong2 *) results;

	for (uint i=lId; i<1024; i+=locSize) {
		match[i] = 0xFFF;
	} 

	inCounter[0] = 0;

	barrier(CLK_LOCAL_MEM_FENCE);

	for (uint i=lId; i<inLim; i+=locSize) {
		ulong8 input = buffer0[mad24((uint) bucketSize, bucket, i)];

		if (((input.s0 >> 12) & 0x3) == mask) {
			uint inPos = atomic_inc(&inCounter[0]);
			inPos = min(inPos, (uint) 2560);
	
			uint slot = (input.s0 >> 14) & 0x3FF;
			uint ret = atomic_xchg(&match[slot], inPos);
			
			table[inPos] = ret | (i << 16);
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	for (uint i=lId; i<inCounter[0]; i+=locSize) {
		uint elemPos0 = table[i] >> 16;
		uint nextElem = table[i] & 0xFFF;

		while (nextElem != 0xFFF) {
			// Loop iteration through table
			uint elemPos1 = table[nextElem] >> 16;
			nextElem = table[nextElem] & 0xFFF;

			// Fetch the matched elements
			ulong8 stepRow0 = buffer0[mad24((uint) bucketSize, bucket, elemPos0)];
			ulong8 stepRow1 = buffer0[mad24((uint) bucketSize, bucket, elemPos1)];

			// Check if the bits match in full length
			if ((stepRow0.s0 & 0xFFFFFFFFFFFFUL) == (stepRow1.s0 & 0xFFFFFFFFFFFFUL)) {

				// We have a solution!
				uint pos = atomic_inc(&results[0]);

				stepRow0 = shift56(stepRow0);
				stepRow1 = shift56(stepRow1);

				bool smaller = (stepRow1.s0 & 0x1FFFFFF) < (stepRow0.s0 & 0x1FFFFFF);
				if (smaller) {
					stepRow0 ^= stepRow1;
					stepRow1 ^= stepRow0;
					stepRow0 ^= stepRow1;
				}	

				resultsUL[1 + 8*pos + 0] = stepRow0.s01; 
				resultsUL[1 + 8*pos + 1] = stepRow0.s23; 
				resultsUL[1 + 8*pos + 2] = stepRow0.s45; 
				resultsUL[1 + 8*pos + 3] = stepRow0.s67; 

				resultsUL[1 + 8*pos + 4] = stepRow1.s01; 
				resultsUL[1 + 8*pos + 5] = stepRow1.s23; 
				resultsUL[1 + 8*pos + 6] = stepRow1.s45; 
				resultsUL[1 + 8*pos + 7] = stepRow1.s67; 

				//printf("GPU: %d %d \n", (uint) (stepRow0.s0 & 0x1FFFFFF), (uint) ((stepRow0.s0 >> 25) & 0x1FFFFFF));
			}
		}
	} 
}

__attribute__((reqd_work_group_size(wgSize, 1, 1)))
__kernel void watch_counter    (__global ulong8 * buffer0,
				__global ulong8 * buffer1,
				__global uint * counters,
				__global uint * results,
				ulong4 prePow) {

	if (get_global_id(0) == 0) {
		uint sum=0;
		uint mine=(1 << 24);
		uint maxe=0;
		
		for (uint i=0; i<4096; i++) {
			sum += counters[16384+i];	
			mine = min(mine, counters[16384+i]);
			maxe = max(maxe, counters[16384+i]);	
			//printf("%d %d \n", i, counters[i]);	

			//if (counters[i] < 13000) printf("%d %d \n", i, counters[i]);	
			//if (counters[i] > 16000) printf("%d %d \n", i, counters[i]);
		}
		printf("Counters: %u %u %u %u %u | %u %u \n", (uint) prePow.s0, sum, counters[16384+0], counters[16384+1], counters[16384+4095], mine, maxe);
	}

}

