// CSNL KAIST, Sept 2022

#ifndef _FATMSGBATCHRATETRAFFICMANAGER_HPP_
#define _FATMSGBATCHRATETRAFFICMANAGER_HPP_

#include <iostream>

#include "config_utils.hpp"
#include "stats.hpp"
#include "msgbatchratetrafficmanager.hpp"

// HANS: Additionals
#include "globals.hpp"

class FatMsgBatchRateTrafficManager : public MsgBatchRateTrafficManager {

protected:
  int _physical_nodes;

  int IssueMessage( int physical_source, int cl );
  void GenerateMessage( int physical_source, int size, int cl, int time );

  virtual void _Inject( );
  virtual void _RetireFlit( Flit *f, int dest );

public:

  FatMsgBatchRateTrafficManager( const Configuration &config, const vector<Network *> & net );
  virtual ~FatMsgBatchRateTrafficManager( );

  // virtual void WriteStats( ostream & os = cout ) const;
  // virtual void DisplayStats( ostream & os = cout ) const;
  // virtual void DisplayOverallStats( ostream & os = cout ) const;

private:
  virtual void OriRetireFlit( Flit *f, int dest );

};

#endif