// BEAM OpenCL Miner
// Stratum interface class
// Copyright 2020 The Beam Team	
// Copyright 2020 Wilke Trei

#include <iostream>
#include <thread>
#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>
#include <bitset>
#include <limits>
#include <deque>
#include <random>

#include <boost/scoped_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "core/difficulty.h"
#include "core/uintBig.h"

#include "beamUtil.h"

#ifndef beamStratum_H 
#define beamStratum_H 

using namespace std;
using namespace boost::asio;
using boost::asio::ip::tcp;
namespace pt = boost::property_tree;

namespace beamMiner {


class beamStratum {
	private:

	// Definitions belonging to the physical connection
	boost::asio::io_service io_service;
	boost::scoped_ptr< boost::asio::ssl::stream<tcp::socket> > socket;
	tcp::resolver res;
	boost::asio::streambuf requestBuffer;
	boost::asio::streambuf responseBuffer;
	boost::asio::ssl::context context;

	// User Data
	string host;
	string port;
	string apiKey;
	bool debug = true;

	// Storage for received work
	int64_t workId;
	std::vector<uint8_t> serverWork;
	std::atomic<uint64_t> nonce;
	beam::Difficulty powDiff;
	std::vector<uint8_t> poolNonce;
	uint8_t extraNonce[4] = {0};

	// Stat
	uint64_t sharesAcc = 0;
	uint64_t sharesRej = 0;
	time_t t_start, t_current;

	//Stratum sending subsystem
	bool activeWrite = false;
	void queueDataSend(string);
	void syncSend(string);
	void activateWrite();
	void writeHandler(const boost::system::error_code&);	
	std::deque<string> writeRequests;

	// Stratum receiving subsystem
	void readStratum(const boost::system::error_code&);
	boost::mutex updateMutex;

	// Connection handling
	void connect();
	void handleConnect(const boost::system::error_code& err,  tcp::resolver::iterator);
	void handleHandshake(const boost::system::error_code& err);
	bool verifyCertificate(bool,boost::asio::ssl::verify_context& );

	// Solution Check & Submit
	std::vector<uint8_t> packBeamIII(std::vector<uint32_t> &);
	void Blake2B_BeamIII(WorkDescription *);
	static bool testSolution(const beam::Difficulty&, const std::vector<uint32_t>&, std::vector<uint8_t>&);
	void submitSolution(int64_t, uint64_t, const std::vector<uint8_t>&);

	// Fork Information
	solverType forcedSolver = None;
	uint64_t blockHeight = numeric_limits<uint64_t>::max();
	uint64_t forkHeight  = numeric_limits<uint64_t>::max();
	uint64_t forkHeight2 = numeric_limits<uint64_t>::max();

	public:
	beamStratum(string, string, string, bool);
	void startWorking();

	bool hasWork();
	void getWork(WorkDescription&, solverType*);

	void handleSolution(const WorkDescription&, std::vector<uint32_t>&);
	
};

}

#endif 



