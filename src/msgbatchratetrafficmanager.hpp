// CSNL KAIST, Sept 2022

#ifndef _MSGBATCHRATETRAFFICMANAGER_HPP_
#define _MSGBATCHRATETRAFFICMANAGER_HPP_

#include <iostream>

#include "config_utils.hpp"
#include "stats.hpp"
#include "trafficmanager.hpp"

// HANS: Additionals
#include "globals.hpp"

class MsgBatchRateTrafficManager : public TrafficManager {

protected:

  int _max_outstanding;
  int _batch_size;
  int _batch_count;
  int _last_id;
  int _last_pid;

  // HANS: Configure message size
  vector<vector<int> > _message_size;
  // vector<vector<int> > _message_size_rate;
  // vector<int>          _message_size_max_val;

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

  int IssueMessage( int source, int cl );
  void GenerateMessage( int source, int size, int cl, int time );

  virtual void _Inject( );
  virtual void _RetireFlit( Flit *f, int dest );
  virtual void _ClearStats( );
  virtual bool _SingleSim( );

  virtual void _UpdateOverallStats( );

  virtual string _OverallStatsCSV(int c = 0) const;

  int GetNextMessageSize(int cl) const;
  double GetAverageMessageSize(int cl) const;

public:

  MsgBatchRateTrafficManager( const Configuration &config, const vector<Network *> & net );
  virtual ~MsgBatchRateTrafficManager( );

  virtual void WriteStats( ostream & os = cout ) const;
  virtual void DisplayStats( ostream & os = cout ) const;
  virtual void DisplayOverallStats( ostream & os = cout ) const;

};

#endif
