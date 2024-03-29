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

#ifndef _ROUTER_HPP_
#define _ROUTER_HPP_

#include <string>
#include <vector>

#include "timed_module.hpp"
#include "flit.hpp"
#include "credit.hpp"
#include "flitchannel.hpp"
#include "channel.hpp"
#include "config_utils.hpp"

typedef Channel<Credit> CreditChannel;

class Router : public TimedModule {

protected:

  static int const STALL_BUFFER_BUSY;
  static int const STALL_BUFFER_CONFLICT;
  static int const STALL_BUFFER_FULL;
  static int const STALL_BUFFER_RESERVED;
  static int const STALL_CROSSBAR_CONFLICT;

  int _id;
  
  int _inputs;
  int _outputs;
  
  int _classes;

  int _input_speedup;
  int _output_speedup;
  
  double _internal_speedup;
  double _partial_internal_cycles;

  int _crossbar_delay;
  int _credit_delay;
  
  vector<FlitChannel *>   _input_channels;
  vector<CreditChannel *> _input_credits;
  vector<FlitChannel *>   _output_channels;
  vector<CreditChannel *> _output_credits;
  vector<bool>            _channel_faults;

#ifdef TRACK_FLOWS
  vector<vector<int> > _received_flits;
  vector<vector<int> > _stored_flits;
  vector<vector<int> > _sent_flits;
  vector<vector<int> > _outstanding_credits;
  vector<vector<int> > _active_packets;
#endif

#ifdef TRACK_STALLS
  vector<int> _buffer_busy_stalls;
  vector<int> _buffer_conflict_stalls;
  vector<int> _buffer_full_stalls;
  vector<int> _buffer_reserved_stalls;
  vector<int> _crossbar_conflict_stalls;
#endif

  virtual void _InternalStep() = 0;

  // HANS: Additionals for hashing
  mutable int _roundrobin;

public:
  Router( const Configuration& config,
	  Module *parent, const string & name, int id,
	  int inputs, int outputs );

  static Router *NewRouter( const Configuration& config,
			    Module *parent, const string & name, int id,
			    int inputs, int outputs );

  virtual void AddInputChannel( FlitChannel *channel, CreditChannel *backchannel );
  virtual void AddOutputChannel( FlitChannel *channel, CreditChannel *backchannel );
 
  inline FlitChannel * GetInputChannel( int input ) const {
    assert((input >= 0) && (input < _inputs));
    return _input_channels[input];
  }
  inline FlitChannel * GetOutputChannel( int output ) const {
    assert((output >= 0) && (output < _outputs));
    return _output_channels[output];
  }

  virtual void ReadInputs( ) = 0;
  virtual void Evaluate( );
  virtual void WriteOutputs( ) = 0;

  void OutChannelFault( int c, bool fault = true );
  bool IsFaultyOutput( int c ) const;

  inline int GetID( ) const {return _id;}

  // Tho: Hash supporting variables
  mutable int inc_hash;
  mutable int ran_hash;
  mutable vector<bool> uplink_occupy;
  mutable vector<pair<int, bool> > uplink_register;
  mutable vector<int>              src_avai;
  mutable vector<int>              pattern;
  mutable vector<vector<int> >     dest_list;   // List of destination for each source, use vector for searching
  // inline int RouterRandom(int k) const {return RandomInt(k);}

  // THO: Spreading
  mutable vector<set<int> >     uplink_spread_dest;
  mutable vector<vector<int> >  uplink_spread_list;
  mutable vector<int>           spread_degree;
  mutable vector<int>           spread_pid;
  mutable int                   packet_cnt;
  mutable vector<bool>          switch_flag;

  mutable vector<int>           flit_count;
  int                           random_offset;
  int                           random_src_offset;
  mutable vector<pair<int, int> >  committed_packet;
  
  // HANS: Additionals for hashing
  inline void IncrementRROffset( int max ) const { 
    _roundrobin = (_roundrobin + 1) % max;
  }
  inline int GetRROffset( ) const{ return _roundrobin; }


  virtual int GetUsedCredit(int o) const = 0;
  virtual int GetBufferOccupancy(int i) const = 0;

  // HANS: Additionals
  virtual int GetUsedCreditVC(int o, int v) const = 0;
  virtual bool IsVCFull(int o, int v) const = 0;
  virtual int GetInjectedPacket(int o) const = 0;

#ifdef TRACK_BUFFERS
  virtual int GetUsedCreditForClass(int output, int cl) const = 0;
  virtual int GetBufferOccupancyForClass(int input, int cl) const = 0;
#endif

#ifdef TRACK_FLOWS
  inline vector<int> const & GetReceivedFlits(int c) const {
    assert((c >= 0) && (c < _classes));
    return _received_flits[c];
  }
  inline vector<int> const & GetStoredFlits(int c) const {
    assert((c >= 0) && (c < _classes));
    return _stored_flits[c];
  }
  inline vector<int> const & GetSentFlits(int c) const {
    assert((c >= 0) && (c < _classes));
    return _sent_flits[c];
  }
  inline vector<int> const & GetOutstandingCredits(int c) const {
    assert((c >= 0) && (c < _classes));
    return _outstanding_credits[c];
  }

  inline vector<int> const & GetActivePackets(int c) const {
    assert((c >= 0) && (c < _classes));
    return _active_packets[c];
  }

  inline void ResetFlowStats(int c) {
    assert((c >= 0) && (c < _classes));
    _received_flits[c].assign(_received_flits[c].size(), 0);
    _sent_flits[c].assign(_sent_flits[c].size(), 0);
  }
#endif

  virtual vector<int> UsedCredits() const = 0;
  virtual vector<int> FreeCredits() const = 0;
  virtual vector<int> MaxCredits() const = 0;

#ifdef TRACK_STALLS
  inline int GetBufferBusyStalls(int c) const {
    assert((c >= 0) && (c < _classes));
    return _buffer_busy_stalls[c];
  }
  inline int GetBufferConflictStalls(int c) const {
    assert((c >= 0) && (c < _classes));
    return _buffer_conflict_stalls[c];
  }
  inline int GetBufferFullStalls(int c) const {
    assert((c >= 0) && (c < _classes));
    return _buffer_full_stalls[c];
  }
  inline int GetBufferReservedStalls(int c) const {
    assert((c >= 0) && (c < _classes));
    return _buffer_reserved_stalls[c];
  }
  inline int GetCrossbarConflictStalls(int c) const {
    assert((c >= 0) && (c < _classes));
    return _crossbar_conflict_stalls[c];
  }

  inline void ResetStallStats(int c) {
    assert((c >= 0) && (c < _classes));
    _buffer_busy_stalls[c] = 0;
    _buffer_conflict_stalls[c] = 0;
    _buffer_full_stalls[c] = 0;
    _buffer_reserved_stalls[c] = 0;
    _crossbar_conflict_stalls[c] = 0;
  }
#endif

  inline int NumInputs() const {return _inputs;}
  inline int NumOutputs() const {return _outputs;}

  // HANS: Additonals
  virtual int GetChanUtil(int output) { return 0; }

  // HANS: For non-repeating random number
  virtual void RandomizeHash() const {}
  virtual int GetLastRandomizingTime() const { return -1; }
  virtual int GetRandomNumber(int src) const { return -1; }
  virtual int GetRandomNumberFromSet(int src) const { return -1; }

  // HANS: Additional information for hashing
  virtual int GetContributorsToDest(int dest_router) const { return 0; };

  virtual int GetHashed(int input) const { return 0; };
  virtual void ModifyHashFunc(int input, int new_val) const {};

  // THO: CYCLIC RANDOM GENERATOR
  virtual int GenerateCyclic(int n, vector<int>& src_avai) const {return -1;}
  virtual char CRC8(const char *data,int length) const {return 0;}
  virtual uint16_t crc16(const uint8_t* data, size_t length) const {return 0;}
  virtual uint16_t crc16_int(uint32_t data) const {return 0;}

};

#endif
