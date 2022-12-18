// CSNL KAIST, Sept 2022

#ifndef _MIXMSGBATCHRATETRAFFICMANAGER_HPP_
#define _MIXMSGBATCHRATETRAFFICMANAGER_HPP_

#include <iostream>

#include "config_utils.hpp"
#include "stats.hpp"
#include "trafficmanager.hpp"
#include "msgbatchratetrafficmanager.hpp"

// HANS: Additionals
#include "globals.hpp"

class MixMsgBatchRateTrafficManager : public MsgBatchRateTrafficManager {

protected:
  // HANS: Configure message size
  vector<vector<int> > _message_size;
  // vector<vector<int> > _message_size_rate;
  // vector<int>          _message_size_max_val;

  int    _big_message_size;
  int   _small_message_size;
  float _big_message_ratio;

  vector<int> _read_request_message_size;
  vector<int> _read_reply_message_size;
  vector<int> _write_request_message_size;
  vector<int> _write_reply_message_size;

  // HANS: Additionals
  int _cur_mid;

  Stats * _batch_time;
  double _overall_min_batch_time;
  double _overall_avg_batch_time;
  double _overall_max_batch_time;

  ostream * _sent_packets_out;

  // HANS: For requests and replies
  vector<int> _message_seq_no;

  // int IssueMessage( int source, int cl );
  // void GenerateMessage( int source, int size, int cl, int time );

  // virtual void _Inject( );
  // virtual void _RetireFlit( Flit *f, int dest );
  // virtual void _ClearStats( );
  // virtual bool _SingleSim( );

  // virtual void _UpdateOverallStats( );

  // virtual string _OverallStatsCSV(int c = 0) const;

  virtual int GetNextMessageSize(int cl) const;
  virtual double GetAverageMessageSize(int cl) const;

public:

  MixMsgBatchRateTrafficManager( const Configuration &config, const vector<Network *> & net );
  virtual ~MixMsgBatchRateTrafficManager( );

  // virtual void WriteStats( ostream & os = cout ) const;
  // virtual void DisplayStats( ostream & os = cout ) const;
  // virtual void DisplayOverallStats( ostream & os = cout ) const;

};

#endif
