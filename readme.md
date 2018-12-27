# Beam Equihash 150/5 OpenCL Miner

```
Copyright 2018 The Beam Team
Copyright 2018 Wilke Trei 
```


# Usage
## One Liner Examples
```
Linux: ./beamMiner --server <hostName>:<portNumer> --key <apiKey> --devices <deviceList>
Windows: ./beamMiner --server <hostName>:<portNumer> --key <apiKey> --devices <deviceList>
```

## Parameters
### --server 
Passes the address and port of the node the miner will mine on to the miner.
The server address can be an IP or any other valid server address.- For example when the node
is running on the same computer and listens on port 17000 then use --server localhost:17000

### --key
Pass a valid API key from "stratum.api.keys" to the miner. Required to authenticate the miner at the node

### --devices (Optional)
Selects the devices to mine on. If not specified the miner will run on all devices found on the system. 
For example to mine on GPUs 0,1 and 3 but not number 2 use --devices 0,1,3
To list all devices that are present in the system and get their order start the miner with --devices -2 .
Then all devices will be listed, but none selected for mining. The miner closes when no devices were 
selected for mining or all selected miner fail in the compatibility check.

# Build status
[![Build Status](https://travis-ci.org/BeamMW/opencl-miner.svg?branch=master)](https://travis-ci.org/BeamMW/opencl-miner)
