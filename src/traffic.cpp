// $Id$

/*
 Copyright (c) 2007-2015, Trustees of The Leland Stanford Junior University
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this 
 list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <iostream>
#include <sstream>
#include <ctime>
#include <algorithm>
#include "random_utils.hpp"
#include "traffic.hpp"

TrafficPattern::TrafficPattern(int nodes)
: _nodes(nodes)
{
  if(nodes <= 0) {
    cout << "Error: Traffic patterns require at least one node." << endl;
    exit(-1);
  }
}

void TrafficPattern::reset()
{

}

TrafficPattern * TrafficPattern::New(string const & pattern, int nodes, 
				     Configuration const * const config)
{
  string pattern_name;
  string param_str;
  size_t left = pattern.find_first_of('(');
  if(left == string::npos) {
    pattern_name = pattern;
  } else {
    pattern_name = pattern.substr(0, left);
    size_t right = pattern.find_last_of(')');
    if(right == string::npos) {
      param_str = pattern.substr(left+1);
    } else {
      param_str = pattern.substr(left+1, right-left-1);
    }
  }
  vector<string> params = tokenize_str(param_str);
  
  TrafficPattern * result = NULL;
  if(pattern_name == "bitcomp") {
    result = new BitCompTrafficPattern(nodes);
  } else if(pattern_name == "transpose") {
    result = new TransposeTrafficPattern(nodes);
  } else if(pattern_name == "bitrev") {
    result = new BitRevTrafficPattern(nodes);
  } else if(pattern_name == "shuffle") {
    result = new ShuffleTrafficPattern(nodes);
  } else if(pattern_name == "randperm") {
    int perm_seed = -1;
    if(params.empty()) {
      if(config) {
	if(config->GetStr("perm_seed") == "time") {
	  perm_seed = int(time(NULL));
	  cout << "SEED: perm_seed=" << perm_seed << endl;
	} else {
	  perm_seed = config->GetInt("perm_seed");
	}
      } else {
        cout << "Error: Missing parameter for random permutation traffic pattern: " << pattern << endl;
        exit(-1);
      }
    } else {
      perm_seed = atoi(params[0].c_str());
    }
    result = new RandomPermutationTrafficPattern(nodes, perm_seed);
  } else if(pattern_name == "uniform") {
    result = new UniformRandomTrafficPattern(nodes);
  } else if(pattern_name == "uniform_fat") {
    int fat_ratio = config->GetInt("fat_ratio");
    result = new UniformRandomFatTrafficPattern(nodes, fat_ratio);
  } else if(pattern_name == "uniform_sel") {
    result = new UniformRandomSelectiveTrafficPattern(nodes);
  } else if (pattern_name == "randperm_sel") {
    result = new PermRandomSelectiveTrafficPattern(nodes);
  } else if (pattern_name == "groupperm") {
    result = new PermGroupSelectiveTrafficPattern(nodes);
  } else if (pattern_name == "adversarial_sel") {
    result = new AdversarialRandomSelectiveTrafficPattern(nodes);
  } else if(pattern_name == "modulo_worst") {
    result = new ModuloWorstTrafficPattern(nodes);
  } else if(pattern_name == "rand_hotspot") {
    result = new UniformRandomHotspotTrafficPattern(nodes);
  } else if (pattern_name == "background_uniform") {
    result = new UniformRandomBackgroundTrafficPattern(nodes);
  } else if(pattern_name == "uniform_interrouter") {
    result = new UniformRandomInterRouterTrafficPattern(nodes);
  } else if(pattern_name == "modulo_worst") {
    result = new ModuloWorstTrafficPattern(nodes);
  } else if(pattern_name == "endpoint") {
    result = new EndpointTrafficPattern(nodes);
  } else if(pattern_name == "endpoint_fat") {
    int fat_ratio = config->GetInt("fat_ratio");
    result = new EndpointFatTrafficPattern(nodes, fat_ratio);
  } else if(pattern_name == "background") {
    vector<int> excludes = tokenize_int(params[0]);
    result = new UniformBackgroundTrafficPattern(nodes, excludes);
  } else if(pattern_name == "diagonal") {
    result = new DiagonalTrafficPattern(nodes);
  } else if(pattern_name == "asymmetric") {
    result = new AsymmetricTrafficPattern(nodes);
  } else if(pattern_name == "taper64") {
    result = new Taper64TrafficPattern(nodes);
  } else if (pattern_name == "bad_flatfly") { // HANS: Adversarial traffic for 1D flattened butterfly
    bool missing_params = false;
    int k = -1;
    if(params.size() < 1) {
      if(config) {
        k = config->GetInt("k");
      } else {
        missing_params = true;
      }
    } else {
      k = atoi(params[0].c_str());
    }
    int n = -1;
    if(params.size() < 2) {
      if(config) {
        n = config->GetInt("n");
      } else {
        missing_params = true;
      }
    } else {
      n = atoi(params[1].c_str());
    }
    if(missing_params) {
      cout << "Error: Missing parameters for flattened butterfly bad permutation traffic pattern: " << pattern << endl;
      exit(-1);
    }
    result = new BadFlatflyTrafficPattern(nodes, k, n);
  } else if(pattern_name == "bad_dragon") {
    bool missing_params = false;
    int k = -1;
    if(params.size() < 1) {
      if(config) {
        k = config->GetInt("k");
      } else {
        missing_params = true;
      }
    } else {
      k = atoi(params[0].c_str());
    }
    int n = -1;
    if(params.size() < 2) {
      if(config) {
        n = config->GetInt("n");
      } else {
        missing_params = true;
      }
    } else {
      n = atoi(params[1].c_str());
    }
    if(missing_params) {
      cout << "Error: Missing parameters for dragonfly bad permutation traffic pattern: " << pattern << endl;
      exit(-1);
    }
    result = new BadPermDFlyTrafficPattern(nodes, k, n);
  } else if((pattern_name == "tornado") || (pattern_name == "neighbor") ||
	    (pattern_name == "badperm_yarc")) {
    bool missing_params = false;
    int k = -1;
    if(params.size() < 1) {
      if(config) {
        k = config->GetInt("k");
      } else {
        missing_params = true;
      }
    } else {
      k = atoi(params[0].c_str());
    }
    int n = -1;
    if(params.size() < 2) {
      if(config) {
        n = config->GetInt("n");
      } else {
        missing_params = true;
      }
    } else {
      n = atoi(params[1].c_str());
    }
    int xr = -1;
    if(params.size() < 3) {
      if(config) {
        xr = config->GetInt("xr");
      } else {
        missing_params = true;
      }
    } else {
      xr = atoi(params[2].c_str());
    }
    if(missing_params) {
      cout << "Error: Missing parameters for digit permutation traffic pattern: " << pattern << endl;
      exit(-1);
    }
    if(pattern_name == "tornado") {
      result = new TornadoTrafficPattern(nodes, k, n, xr);
    } else if(pattern_name == "neighbor") {
      result = new NeighborTrafficPattern(nodes, k, n, xr);
    } else if(pattern_name == "badperm_yarc") {
      result = new BadPermYarcTrafficPattern(nodes, k, n, xr);
    }
  } else if(pattern_name == "hotspot") {
    if(params.empty()) {
      params.push_back("-1");
    } 
    vector<int> hotspots = tokenize_int(params[0]);
    for(size_t i = 0; i < hotspots.size(); ++i) {
      if(hotspots[i] < 0) {
        hotspots[i] = RandomInt(nodes - 1);
      }
    }
    vector<int> rates;
    if(params.size() >= 2) {
      rates = tokenize_int(params[1]);
      rates.resize(hotspots.size(), rates.back());
    } else {
      rates.resize(hotspots.size(), 1);
    }
    result = new HotSpotTrafficPattern(nodes, hotspots, rates);
  } else {
    cout << "Error: Unknown traffic pattern: " << pattern << endl;
    exit(-1);
  }
  return result;
}

PermutationTrafficPattern::PermutationTrafficPattern(int nodes)
  : TrafficPattern(nodes)
{
  
}

BitPermutationTrafficPattern::BitPermutationTrafficPattern(int nodes)
  : PermutationTrafficPattern(nodes)
{
  if((nodes & -nodes) != nodes) {
    cout << "Error: Bit permutation traffic patterns require the number of "
	 << "nodes to be a power of two." << endl;
    exit(-1);
  }
}

BitCompTrafficPattern::BitCompTrafficPattern(int nodes)
  : BitPermutationTrafficPattern(nodes)
{
  
}

int BitCompTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  int const mask = _nodes - 1;
  return ~source & mask;
}

TransposeTrafficPattern::TransposeTrafficPattern(int nodes)
  : BitPermutationTrafficPattern(nodes), _shift(0)
{
  while(nodes >>= 1) {
    ++_shift;
  }
  if(_shift % 2) {
    cout << "Error: Transpose traffic pattern requires the number of nodes to "
	 << "be an even power of two." << endl;
    exit(-1);
  }
  _shift >>= 1;
}

int TransposeTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  int const mask_lo = (1 << _shift) - 1;
  int const mask_hi = mask_lo << _shift;
  return (((source >> _shift) & mask_lo) | ((source << _shift) & mask_hi));
}

BitRevTrafficPattern::BitRevTrafficPattern(int nodes)
  : BitPermutationTrafficPattern(nodes)
{
  
}

int BitRevTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  int result = 0;
  for(int n = _nodes; n > 1; n >>= 1) {
    result = (result << 1) | (source % 2);
    source >>= 1;
  }
  return result;
}

ShuffleTrafficPattern::ShuffleTrafficPattern(int nodes)
  : BitPermutationTrafficPattern(nodes)
{

}

int ShuffleTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  int const shifted = source << 1;
  return ((shifted & (_nodes - 1)) | bool(shifted & _nodes));
}

DigitPermutationTrafficPattern::DigitPermutationTrafficPattern(int nodes, int k,
							       int n, int xr)
  : PermutationTrafficPattern(nodes), _k(k), _n(n), _xr(xr)
{
  
}

TornadoTrafficPattern::TornadoTrafficPattern(int nodes, int k, int n, int xr)
  : DigitPermutationTrafficPattern(nodes, k, n, xr)
{

}

int TornadoTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));

  int offset = 1;
  int result = 0;

  for(int n = 0; n < _n; ++n) {
    result += offset *
      (((source / offset) % (_xr * _k) + ((_xr * _k + 1) / 2 - 1)) % (_xr * _k));
    offset *= (_xr * _k);
  }
  return result;
}

NeighborTrafficPattern::NeighborTrafficPattern(int nodes, int k, int n, int xr)
  : DigitPermutationTrafficPattern(nodes, k, n, xr)
{

}

int NeighborTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));

  int offset = 1;
  int result = 0;
  
  for(int n = 0; n < _n; ++n) {
    result += offset *
      (((source / offset) % (_xr * _k) + 1) % (_xr * _k));
    offset *= (_xr * _k);
  }
  return result;
}

RandomPermutationTrafficPattern::RandomPermutationTrafficPattern(int nodes, 
								 int seed)
  : TrafficPattern(nodes)
{
  _dest.resize(nodes);
  randomize(seed);

  // HANS: For debugging
  cout << "Random permutation src-dest pair" << endl;
  for (int iter = 0; iter < nodes; iter++){
    cout << "Source: " << iter << ", Dest: " << _dest[iter] << endl;
  }
}

void RandomPermutationTrafficPattern::randomize(int seed)
{
  vector<long> save_x;
  vector<double> save_u;
  SaveRandomState(save_x, save_u);
  RandomSeed(seed);

  _dest.assign(_nodes, -1);

  for(int i = 0; i < _nodes; ++i) {
    int ind = RandomInt(_nodes - 1 - i);

    int j = 0;
    int cnt = 0;
    while((cnt < ind) || (_dest[j] != -1)) {
      if(_dest[j] == -1) {
        ++cnt;
      }
      ++j;
      assert(j < _nodes);
    }

    _dest[j] = i;
  }

  RestoreRandomState(save_x, save_u); 
}

int RandomPermutationTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  assert((_dest[source] >= 0) && (_dest[source] < _nodes));
  return _dest[source];
}

RandomTrafficPattern::RandomTrafficPattern(int nodes)
  : TrafficPattern(nodes)
{

}

UniformRandomTrafficPattern::UniformRandomTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{

}

int UniformRandomTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  return RandomInt(_nodes - 1);
}

UniformRandomFatTrafficPattern::UniformRandomFatTrafficPattern(int nodes, int fat_ratio)
  : RandomTrafficPattern(nodes)
{
  _physical_nodes = _nodes / fat_ratio;
}

int UniformRandomFatTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _physical_nodes));

  /*
  int random = source;

  while (random == source){
    random = RandomInt(_physical_nodes - 1);
  }

  return random;
  */

 return RandomInt(_physical_nodes - 1);
}

// THO: Select destination using _active_nodes (UR)
UniformRandomSelectiveTrafficPattern::UniformRandomSelectiveTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{
}

int UniformRandomSelectiveTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  // assert(_compute_nodes.count(source));
  
  // THO: Select destination only from _memory_nodes
  int rand_dest = RandomInt(_nodes-1);
  while(_memory_nodes.count(rand_dest) == 0) {
    rand_dest = RandomInt(_nodes-1);
  }

  return rand_dest;

  // // HANS: Select destination only from _memory_nodes
  // int rand = RandomInt(_memory_nodes.size() - 1);
  // set<int>::iterator temp = _memory_nodes.begin();
  // for (int iter = 0; iter < rand; iter++){
  //   temp++;
  // }

  // return *temp;
}

// THO: Select destination using _active_nodes (AD)
AdversarialRandomSelectiveTrafficPattern::AdversarialRandomSelectiveTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{
}

int AdversarialRandomSelectiveTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  assert((_nodes % gK) == 0);
  // assert(_compute_nodes.count(source));

  int const src_router = source/gK;
  // Change this dest_router if running clustered
  int const dest_router = (src_router + 1) * gK;

  int dest = (dest_router + RandomInt(gK - 1)) % _nodes;

  // while(_compute_nodes.count(dest) != 0) {
  //   dest = (dest_router + RandomInt(gK - 1)) % _nodes;
  // }

  return dest;
}


// THO: PERM but send to other routers (*All equal nodes*)
// Unlike randperm, all channels must be used (This traffic won't always work)
// PermGroupSelectiveTrafficPattern::PermGroupSelectiveTrafficPattern(int nodes)
//   : RandomTrafficPattern(nodes)
// {
// }

// int PermGroupSelectiveTrafficPattern::dest(int source)
// {
//   assert((source >= 0) && (source < _nodes));
//   assert((_nodes % gK) == 0);

//   // int src_router = source/gK;

//   if (_dest.empty()) {
//     for(int src = 0; src < _nodes; src++) {
//       int rand_dest = RandomInt(_nodes-1);
//       int src_router = src/gK;
//       // cout << "Condition: >= "
//       // cout << "rand_dest = " << rand_dest << endl;
//       // Destination mustn't connect to the same router as source
//       while(((rand_dest >= (src_router*gK)) && (rand_dest <= (src_router*gK + gK - 1))) || (std::find(_dest.begin(), _dest.end(), rand_dest) != _dest.end())) {
//         rand_dest = RandomInt(_nodes-1);
//         // cout << "rand_dest = " << rand_dest << endl;
//       }
//       cout << "source " << src << " push " << rand_dest << endl;
//       _dest.push_back(rand_dest);
//     }
//     for(int src = 0; src < _nodes; src++) {
//       cout << "source = " << src << ", dest = " << _dest[src] << endl;
//     }
//   }

//   return _dest[source];
// }


// THO: PERM but each source has a list of destinations instead of only 1 (*All equal nodes*)
//      *This traffic is not completely load balanced
PermGroupSelectiveTrafficPattern::PermGroupSelectiveTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{
}


int PermGroupSelectiveTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  assert((_nodes % gK) == 0);

  if (_dest_vec.empty()) {
    cout << "GroupPerm trafic: " << endl;
    _dest_vec.resize(_nodes);
    for (int src = 0; src < _nodes; src++) {
      int _list_size = 5; // Fixed number of destination
      // int _list_size = RandomInt(_nodes - 1);  // Random number of destination
      while (_list_size == 0) {
        _list_size = RandomInt(_nodes - 1);
      }
      // // Use this part to have duplicate elements (traffic has weights)
      // for (int i = 0; i < _list_size; i++) {
      //   int temp_dest = RandomInt(_nodes - 1);
      //   _dest_vec[src].push_back(temp_dest);
      // }
      // Use this part to make sure there is no duplicate elements
      // if (_cyclic_dest.empty()) {
      //   for (int i = 0; i < _nodes; i++)
      //     _cyclic_dest.push_back(i);
      // }
      while (_dest_vec[src].size() < _list_size) {
        if (_cyclic_dest.empty()) {
          for (int i = 0; i < _nodes; i++)
            _cyclic_dest.push_back(i);
        }

        int temp_dest = RandomInt(_cyclic_dest.size() - 1);
        _dest_vec[src].push_back(_cyclic_dest[temp_dest]);
        int size_before = _dest_vec[src].size();
        std::sort(_dest_vec[src].begin(), _dest_vec[src].end());
        vector<int>::iterator ip;
        ip = std::unique(_dest_vec[src].begin(), _dest_vec[src].begin() + _dest_vec[src].size());
        _dest_vec[src].resize(std::distance(_dest_vec[src].begin(), ip));
        int size_after = _dest_vec[src].size();
        if (size_after == size_before) {
          swap(_cyclic_dest[temp_dest], _cyclic_dest[_cyclic_dest.size() - 1]);
          _cyclic_dest.pop_back();
          cout << "  Pop!!!";
        }
      }
      cout << "  Nodes " << src << ":";
      for (int j = 0; j < _dest_vec[src].size(); j++) {
        cout << " " << _dest_vec[src][j];
      }
      cout << endl;
    }
  }

  int rand_dest = RandomInt(_dest_vec[source].size() - 1);

  // if (source == 1)
  //   cout << "Source 1 -> Dest " << _dest_vec[source][rand_dest] << endl;

  return _dest_vec[source][rand_dest];
}


// THO: Select destination using _compute_nodes (PERM)
// Unlike randperm, there will be endpoint congestion here
// *Note: Since compute nodes > memory nodes, there will always be endpoint congestion for requests
PermRandomSelectiveTrafficPattern::PermRandomSelectiveTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{
}

int PermRandomSelectiveTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  assert((_nodes % gK) == 0);
  assert(_compute_nodes.count(source));

  if (_dest.empty()) {
    _dest.resize(_nodes);
    for(int src = 0; src < _nodes; src++) {
      // Memory nodes will not send out requests
      if(_compute_nodes.count(source) == 0) {
        _dest[src] = -1;
      }
      else {
        int rand_dest = RandomInt(_nodes-1);
        // Destination mustn't be compute nodes (must be memory nodes)
        while(_compute_nodes.count(rand_dest) != 0) {

          rand_dest = RandomInt(_nodes-1);
        }
        _dest[src] = rand_dest;
      }
    }
  }

  assert(_dest[source] != -1);
  return _dest[source];
}


// THO: Worst case for Source and Destination hashing (*All equal nodes*)
ModuloWorstTrafficPattern::ModuloWorstTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{
}

int ModuloWorstTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  assert((_nodes % gK) == 0);

  int const src_router = source/gK;
  int const dest_router = (source % gK) * gK;
  int dest = (dest_router + src_router) % _nodes;

  return dest;
}

// THO: Hotspot traffic (Evaluate separately Hotspot vs. background)
UniformRandomHotspotTrafficPattern::UniformRandomHotspotTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{
}

int UniformRandomHotspotTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  
  int rand_dest = RandomInt(_nodes-1);
  // Background (not send to hs_dest)
  if (!_hs_send_all && _hs_srcs.count(source)==0) {
    assert(0);
    while(_hs_dests.count(rand_dest) != 0) {
      rand_dest = RandomInt(_nodes-1);
    }
  }
  // Hotspot (send to hs_dest)
  else {
    while(_hs_dests.count(rand_dest) == 0) {
      rand_dest = RandomInt(_nodes-1);
    }
  }

  // cout << "[HOTSPOT] Source: " << source << ", Destination: " << rand_dest << endl;

  return rand_dest;
}

// THO: Hotspot traffic (Evaluate traffics altogether)
UniformRandomBackgroundTrafficPattern::UniformRandomBackgroundTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{
}

int UniformRandomBackgroundTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  
  int rand_dest = RandomInt(_nodes-1);
  while(_hs_dests.count(rand_dest) != 0 || _hs_srcs.count(rand_dest) != 0) {
    rand_dest = RandomInt(_nodes-1);
  }

  // cout << "[BACKGROUND] Source: " << source << ", Destination: " << rand_dest << endl;
  return rand_dest;
}

EndpointTrafficPattern::EndpointTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{
}

int EndpointTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));

  int dest_router = 0;

  int dest = ((dest_router * gK) + RandomInt(gK - 1)) % _nodes;
  // int dest = ((dest_router * gK) + (source % gK)) % _nodes;

  // cout << "Source: " << source << ", Dest: " << dest << endl;

  return dest;
}


EndpointFatTrafficPattern::EndpointFatTrafficPattern(int nodes, int fat_ratio)
  : RandomTrafficPattern(nodes)
{
  _fat_ratio = fat_ratio;
  _physical_nodes = nodes / fat_ratio;
}

int EndpointFatTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _physical_nodes));

  int dest_router = 0;
  // int dest = ((dest_router * _fat_ratio) + RandomInt(_fat_ratio - 1)) % _nodes;
  int dest = ((dest_router * _fat_ratio) + (source % (gC / _fat_ratio))) % _nodes;

  return dest;
}

// THO: Hotspot traffic (Evaluate separately Hotspot vs. background)
UniformRandomHotspotTrafficPattern::UniformRandomHotspotTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{
}

int UniformRandomHotspotTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  
  int rand_dest = RandomInt(_nodes-1);
  // Background (not send to hs_dest)
  if (!_hs_send_all && _hs_srcs.count(source)==0) {
    assert(0);
    while(_hs_dests.count(rand_dest) != 0) {
      rand_dest = RandomInt(_nodes-1);
    }
  }
  // Hotspot (send to hs_dest)
  else {
    while(_hs_dests.count(rand_dest) == 0) {
      rand_dest = RandomInt(_nodes-1);
    }
  }

  // cout << "[HOTSPOT] Source: " << source << ", Destination: " << rand_dest << endl;

  return rand_dest;
}

// THO: Hotspot traffic (Evaluate traffics altogether)
UniformRandomBackgroundTrafficPattern::UniformRandomBackgroundTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{
}

int UniformRandomBackgroundTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  
  int rand_dest = RandomInt(_nodes-1);
  while(_hs_dests.count(rand_dest) != 0 || _hs_srcs.count(rand_dest) != 0) {
    rand_dest = RandomInt(_nodes-1);
  }

  // cout << "[BACKGROUND] Source: " << source << ", Destination: " << rand_dest << endl;
  return rand_dest;
}

UniformBackgroundTrafficPattern::UniformBackgroundTrafficPattern(int nodes, vector<int> excluded_nodes)
  : RandomTrafficPattern(nodes)
{
  for(size_t i = 0; i < excluded_nodes.size(); ++i) {
    int const node = excluded_nodes[i];
    assert((node >= 0) && (node < _nodes));
    _excluded.insert(node);
  }
}

int UniformBackgroundTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));

  int result;

  do {
    result = RandomInt(_nodes - 1);
  } while(_excluded.count(result) > 0);

  return result;
}

DiagonalTrafficPattern::DiagonalTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{

}

int DiagonalTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  return ((RandomInt(2) == 0) ? ((source + 1) % _nodes) : source);
}

AsymmetricTrafficPattern::AsymmetricTrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{

}

int AsymmetricTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  int const half = _nodes / 2;
  return (source % half) + (RandomInt(1) ? half : 0);
}

Taper64TrafficPattern::Taper64TrafficPattern(int nodes)
  : RandomTrafficPattern(nodes)
{
  if(nodes != 64) {
    cout << "Error: Tthe Taper64 traffic pattern requires the number of nodes "
	 << "to be exactly 64." << endl;
    exit(-1);
  }
}

int Taper64TrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  if(RandomInt(1)) {
    return ((64 + source + 8 * (RandomInt(2) - 1) + (RandomInt(2) - 1)) % 64);
  } else {
    return RandomInt(_nodes - 1);
  }
}

// HANS: Adversarial traffic for 1D flattened butterfly
BadFlatflyTrafficPattern::BadFlatflyTrafficPattern(int nodes, int k, int n)
  : DigitPermutationTrafficPattern(nodes, k, n, 1)
{
  // assert(n == 1); // This is not a worst-case traffic for multi-dimensional flattened butterfly
}

int BadFlatflyTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  assert((_nodes % _k) == 0);

  int const c = _nodes/_k;
  int const src_router = source/c;
  int const dest_router = (src_router + 1) % _k;

  return ((dest_router * c) + RandomInt(c - 1));
}

BadPermDFlyTrafficPattern::BadPermDFlyTrafficPattern(int nodes, int k, int n)
  : DigitPermutationTrafficPattern(nodes, k, n, 1)
{
  
}

int BadPermDFlyTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));

  int const grp_size_routers = 2 * _k;
  int const grp_size_nodes = grp_size_routers * _k;

  return ((RandomInt(grp_size_nodes - 1) + ((source / grp_size_nodes) + 1) * grp_size_nodes) % _nodes);
}

BadPermYarcTrafficPattern::BadPermYarcTrafficPattern(int nodes, int k, int n, 
						     int xr)
  : DigitPermutationTrafficPattern(nodes, k, n, xr)
{

}

int BadPermYarcTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));
  int const row = source / (_xr * _k);
  return RandomInt((_xr * _k) - 1) * (_xr * _k) + row;
}

HotSpotTrafficPattern::HotSpotTrafficPattern(int nodes, vector<int> hotspots, 
					     vector<int> rates)
  : TrafficPattern(nodes), _hotspots(hotspots), _rates(rates), _max_val(-1)
{
  assert(!_hotspots.empty());
  size_t const size = _hotspots.size();
  _rates.resize(size, _rates.empty() ? 1 : _rates.back());
  for(size_t i = 0; i < size; ++i) {
    int const hotspot = _hotspots[i];
    assert((hotspot >= 0) && (hotspot < _nodes));
    int const rate = _rates[i];
    assert(rate > 0);
    _max_val += rate;
  }
}

int HotSpotTrafficPattern::dest(int source)
{
  assert((source >= 0) && (source < _nodes));

  if(_hotspots.size() == 1) {
    return _hotspots[0];
  }

  int pct = RandomInt(_max_val);

  for(size_t i = 0; i < (_hotspots.size() - 1); ++i) {
    int const limit = _rates[i];
    if(limit > pct) {
      return _hotspots[i];
    } else {
      pct -= limit;
    }
  }
  assert(_rates.back() > pct);
  return _hotspots.back();
}
