// CSNL KAIST, Sept 2022

#include <limits>
#include <sstream>
#include <fstream>

#include "packet_reply_info.hpp"
#include "random_utils.hpp"
#include "batchratetrafficmanager.hpp"

BatchRateTrafficManager::BatchRateTrafficManager( const Configuration &config, 
					  const vector<Network *> & net )
: TrafficManager(config, net), _last_id(-1), _last_pid(-1), 
   _overall_min_batch_time(0), _overall_avg_batch_time(0), 
   _overall_max_batch_time(0)
{

  _max_outstanding = config.GetInt ("max_outstanding_requests");  

  _batch_size = config.GetInt( "batch_size" );
  _batch_count = config.GetInt( "batch_count" );

  _batch_time = new Stats( this, "batch_time", 1.0, 1000 );
  _stats["batch_time"] = _batch_time;
  
  string sent_packets_out_file = config.GetStr( "sent_packets_out" );
  if(sent_packets_out_file == "") {
    _sent_packets_out = NULL;
  } else {
    _sent_packets_out = new ofstream(sent_packets_out_file.c_str());
  }

  // HANS: Get load
  _load = config.GetFloatArray("batch_injection_rate"); 
  if(_load.empty()) {
      _load.push_back(config.GetFloat("batch_injection_rate"));
  }
  _load.resize(_classes, _load.back());

  if(config.GetInt("injection_rate_uses_flits")) {
    for(int c = 0; c < _classes; ++c)
      _load[c] /= _GetAveragePacketSize(c);
  }

  // HANS: Get injection rate and process
  vector<string> injection_process = config.GetStrArray("injection_process");
  injection_process.resize(_classes, injection_process.back());

  for(int c = 0; c < _classes; ++c){
    _injection_process[c] = InjectionProcess::New(injection_process[c], _nodes, _load[c], &config);
  }
}

BatchRateTrafficManager::~BatchRateTrafficManager( )
{
  delete _batch_time;
  if(_sent_packets_out) delete _sent_packets_out;
}

void BatchRateTrafficManager::_RetireFlit( Flit *f, int dest )
{
  if (f->head){
    assert(f->dest == dest);
    f->rtime = GetSimTime();
  }

  int type = FindType(f->type);
  _reordering_vect[f->src][dest][type]->q.push(f);

  while ((!_reordering_vect[f->src][dest][type]->q.empty()) && (_reordering_vect[f->src][dest][type]->q.top()->packet_seq == _reordering_vect[f->src][dest][type]->recv)){
    Flit* temp = _reordering_vect[f->src][dest][type]->q.top();

    if (temp->tail){
      _reordering_vect[f->src][dest][type]->recv += 1;
    }

    _last_id = temp->id;
    _last_pid = temp->pid;
    TrafficManager::_RetireFlit(temp, dest);

    _reordering_vect[f->src][dest][type]->q.pop();
  }

  // HANS: Without reordering buffer
  // _last_id = f->id;
  // _last_pid = f->pid;
  // TrafficManager::_RetireFlit(f, dest);
}

int BatchRateTrafficManager::_IssuePacket( int source, int cl )
{
  int result = 0;
  if(_use_read_write[cl]) { //read write packets
    //check queue for waiting replies.
    //check to make sure it is on time yet
    if(!_repliesPending[source].empty()) {
      if(_repliesPending[source].front()->time <= _time) {
	      result = -1;
      }
    } else {
      // HANS
      if (_active_nodes.count(source) > 0){
        if((_injection_process[cl]->test(source)) && (_packet_seq_no[source] < _batch_size) && ((_max_outstanding <= 0) || (_requestsOutstanding[source] < _max_outstanding))) {
        
	        //coin toss to determine request type.
	        result = (RandomFloat() < 0.5) ? 2 : 1;

	        _requestsOutstanding[source]++;
        }
      }
    }
  } else { //normal
    if (_active_nodes.count(source) > 0){
      if((_injection_process[cl]->test(source)) && (_packet_seq_no[source] < _batch_size) && ((_max_outstanding <= 0) || (_requestsOutstanding[source] < _max_outstanding))) {
        result = _GetNextPacketSize(cl);
        _requestsOutstanding[source]++;
      }
    }
  }
  // if(result != 0) {
  if(result > 0) {

    _packet_seq_no[source]++;
  }
  return result;
}

void BatchRateTrafficManager::_ClearStats( )
{
  TrafficManager::_ClearStats();
  _batch_time->Clear( );
}

bool BatchRateTrafficManager::_SingleSim( )
{
  int batch_index = 0;
  while(batch_index < _batch_count) {
    _packet_seq_no.assign(_nodes, 0);
    _last_id = -1;
    _last_pid = -1;
    _sim_state = running;
    int start_time = _time;
    bool batch_complete;
    cout << "Sending batch " << batch_index + 1 << " (" << _batch_size << " packets)..." << endl;
    do {
      _Step();
      batch_complete = true;
      for(int i = 0; i < _nodes; ++i) {
      // HANS: Additionals
        if (_active_nodes.count(i) > 0){
          if (_packet_seq_no[i] < _batch_size) {
	          batch_complete = false;
	          break;
          }
	      }
      }
      if(_sent_packets_out) {
	*_sent_packets_out << _packet_seq_no << endl;
      }
    } while(!batch_complete);
    cout << "Batch injected. Time used is " << _time - start_time << " cycles." << endl;

    int sent_time = _time;
    cout << "Waiting for batch to complete..." << endl;

    int empty_steps = 0;
    
    bool packets_left = false;
    for(int c = 0; c < _classes; ++c) {
      packets_left |= !_total_in_flight_flits[c].empty();
    }
    
    while( packets_left ) { 
      _Step( ); 
      
      ++empty_steps;
      
      if ( empty_steps % 1000 == 0 ) {
	_DisplayRemaining( ); 
	cout << ".";
      }
      
      packets_left = false;
      for(int c = 0; c < _classes; ++c) {
	packets_left |= !_total_in_flight_flits[c].empty();
      }
    }
    cout << endl;
    cout << "Batch received. Time used is " << _time - sent_time << " cycles." << endl
	 << "Last packet was " << _last_pid << ", last flit was " << _last_id << "." << endl;

    _batch_time->AddSample(_time - start_time);

    cout << _sim_state << endl;

    UpdateStats();
    DisplayStats();
        
    ++batch_index;
  }
  _sim_state = draining;
  _drain_time = _time;
  return 1;
}

void BatchRateTrafficManager::_UpdateOverallStats() {
  TrafficManager::_UpdateOverallStats();
  _overall_min_batch_time += _batch_time->Min();
  _overall_avg_batch_time += _batch_time->Average();
  _overall_max_batch_time += _batch_time->Max();
}
  
string BatchRateTrafficManager::_OverallStatsCSV(int c) const
{
  ostringstream os;
  os << TrafficManager::_OverallStatsCSV(c) << ','
     << _overall_min_batch_time / (double)_total_sims << ','
     << _overall_avg_batch_time / (double)_total_sims << ','
     << _overall_max_batch_time / (double)_total_sims;
  return os.str();
}

void BatchRateTrafficManager::WriteStats(ostream & os) const
{
  TrafficManager::WriteStats(os);
  os << "batch_time = " << _batch_time->Average() << ";" << endl;
}    

void BatchRateTrafficManager::DisplayStats(ostream & os) const {
  TrafficManager::DisplayStats();
  os << "Minimum batch duration = " << _batch_time->Min() << endl;
  os << "Average batch duration = " << _batch_time->Average() << endl;
  os << "Maximum batch duration = " << _batch_time->Max() << endl;
}

void BatchRateTrafficManager::DisplayOverallStats(ostream & os) const {
  TrafficManager::DisplayOverallStats(os);
  os << "Overall min batch duration = " << _overall_min_batch_time / (double)_total_sims
     << " (" << _total_sims << " samples)" << endl
     << "Overall avg batch duration = " << _overall_avg_batch_time / (double)_total_sims
     << " (" << _total_sims << " samples)" << endl
     << "Overall max batch duration = " << _overall_max_batch_time / (double)_total_sims
     << " (" << _total_sims << " samples)" << endl;
}