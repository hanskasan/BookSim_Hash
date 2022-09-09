// CSNL KAIST, Sept 2022

#ifndef _BATCHRATETRAFFICMANAGER_HPP_
#define _BATCHRATETRAFFICMANAGER_HPP_

#include <iostream>

#include "config_utils.hpp"
#include "stats.hpp"
#include "trafficmanager.hpp"

// HANS: Additionals
#include "globals.hpp"

class BatchRateTrafficManager : public TrafficManager {

protected:

  int _max_outstanding;
  int _batch_size;
  int _batch_count;
  int _last_id;
  int _last_pid;

  // HANS: Additionals
  int _active_nodes;

  Stats * _batch_time;
  double _overall_min_batch_time;
  double _overall_avg_batch_time;
  double _overall_max_batch_time;

  ostream * _sent_packets_out;

  virtual void _RetireFlit( Flit *f, int dest );

  virtual int _IssuePacket( int source, int cl );
  virtual void _ClearStats( );
  virtual bool _SingleSim( );

  virtual void _UpdateOverallStats( );

  virtual string _OverallStatsCSV(int c = 0) const;

public:

  BatchRateTrafficManager( const Configuration &config, const vector<Network *> & net );
  virtual ~BatchRateTrafficManager( );

  virtual void WriteStats( ostream & os = cout ) const;
  virtual void DisplayStats( ostream & os = cout ) const;
  virtual void DisplayOverallStats( ostream & os = cout ) const;

};

#endif
