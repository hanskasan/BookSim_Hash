// CSNL KAIST, Sept 2022

#include <limits>
#include <sstream>
#include <fstream>

#include "packet_reply_info.hpp"
#include "random_utils.hpp"
#include "mixmsgbatchratetrafficmanager.hpp"

MixMsgBatchRateTrafficManager::MixMsgBatchRateTrafficManager( const Configuration &config, 
					  const vector<Network *> & net )
: MsgBatchRateTrafficManager(config, net)
{
  /*
  _batch_time = new Stats( this, "batch_time", 1.0, 1000 );
  _stats["batch_time"] = _batch_time;
  
  string sent_packets_out_file = config.GetStr( "sent_packets_out" );
  if(sent_packets_out_file == "") {
    _sent_packets_out = NULL;
  } else {
    _sent_packets_out = new ofstream(sent_packets_out_file.c_str());
  }
  */

  // HANS: Get load
  _load = config.GetFloatArray("batch_injection_rate"); 
  if(_load.empty()) {
      _load.push_back(config.GetFloat("batch_injection_rate"));
  } 
  _load.resize(_classes, _load.back());

  // HANS: Read and write message size
  _read_request_message_size = config.GetIntArray("read_request_message_size");
  if(_read_request_message_size.empty()) {
      _read_request_message_size.push_back(config.GetInt("read_request_message_size"));
  }
  _read_request_message_size.resize(_classes, _read_request_message_size.back());

  _read_reply_message_size = config.GetIntArray("read_reply_message_size");
  if(_read_reply_message_size.empty()) {
      _read_reply_message_size.push_back(config.GetInt("read_reply_message_size"));
  }
  _read_reply_message_size.resize(_classes, _read_reply_message_size.back());

  _write_request_message_size = config.GetIntArray("write_request_message_size");
  if(_write_request_message_size.empty()) {
      _write_request_message_size.push_back(config.GetInt("write_request_message_size"));
  }
  _write_request_message_size.resize(_classes, _write_request_message_size.back());

  _write_reply_message_size = config.GetIntArray("write_reply_message_size");
  if(_write_reply_message_size.empty()) {
      _write_reply_message_size.push_back(config.GetInt("write_reply_message_size"));
  }
  _write_reply_message_size.resize(_classes, _write_reply_message_size.back());

  // HANS: Message size configuration
  string message_size_str = config.GetStr("message_size");
  if(message_size_str.empty()) {
      _message_size.push_back(vector<int>(1, config.GetInt("message_size")));
  } else {
      vector<string> message_size_strings = tokenize_str(message_size_str);
      for(size_t i = 0; i < message_size_strings.size(); ++i) {
          _message_size.push_back(tokenize_int(message_size_strings[i]));
      }
  }
  _message_size.resize(_classes, _message_size.back());

  /*
  string message_size_rate_str = config.GetStr("message_size_rate");
  if(message_size_rate_str.empty()) {
      int rate = config.GetInt("message_size_rate");
      assert(rate >= 0);
      for(int c = 0; c < _classes; ++c) {
          int size = _message_size[c].size();
          _message_size_rate.push_back(vector<int>(size, rate));
          _message_size_max_val.push_back(size * rate - 1);
      }
  } else {
      vector<string> message_size_rate_strings = tokenize_str(message_size_rate_str);
      message_size_rate_strings.resize(_classes, message_size_rate_strings.back());
      for(int c = 0; c < _classes; ++c) {
          vector<int> rates = tokenize_int(message_size_rate_strings[c]);
          rates.resize(_message_size[c].size(), rates.back());
          _message_size_rate.push_back(rates);
          int size = rates.size();
          int max_val = -1;
          for(int i = 0; i < size; ++i) {
              int rate = rates[i];
              assert(rate >= 0);
              max_val += rate;
          }
          _message_size_max_val.push_back(max_val);
      }
  }
  */
  for(int c = 0; c < _classes; ++c) {
      if(_use_read_write[c]) {
          _message_size[c] = 
              vector<int>(1, (_read_request_message_size[c] * _read_request_size[c] + _read_reply_message_size[c] * _read_reply_size[c] +
                              _write_request_message_size[c] * _write_request_size[c] + _write_reply_message_size[c] * _write_reply_size[c]) / 2);
          // _message_size_rate[c] = vector<int>(1, 1);
          // _message_size_max_val[c] = 0;
      } else {
          int sizes;

          vector<int> const & msize = _message_size[c];
          sizes = msize.size();
          assert(sizes == 1); // HANS: Just for now..
          // if(sizes == 1) {
          double message_size = (double)msize[0];

          vector<int> const & psize = _packet_size[c];
          sizes = psize.size();
          assert(sizes == 1); // HANS: Just for now..

          double packet_size = (double)psize[0];

          _message_size[c] = vector<int>(1, (message_size * packet_size));
      }
  }

  if(config.GetInt("injection_rate_uses_flits")) {
    for(int c = 0; c < _classes; ++c)
      _load[c] /= GetAverageMessageSize(c);
  }

  // HANS: Get injection rate and process
  vector<string> injection_process = config.GetStrArray("injection_process");
  injection_process.resize(_classes, injection_process.back());

  for(int c = 0; c < _classes; ++c){
    _injection_process[c] = InjectionProcess::New(injection_process[c], _nodes, _load[c], &config);
  }

  // HANS: Initialization
  _cur_mid = 0;

  // HANS: For requests and replies
  _message_seq_no.resize(_nodes);
}

MixMsgBatchRateTrafficManager::~MixMsgBatchRateTrafficManager( )
{
  delete _batch_time;
  if(_sent_packets_out) delete _sent_packets_out;
}

// HANS: Inherit _RetireFlit to enable reordering buffer
void MixMsgBatchRateTrafficManager::_RetireFlit( Flit *f, int dest )
{
  // Insert to reordering buffer
  // Push flit to its respective reordering queue
  // if ((f->src == 15) && (dest == 13))
  // if ((f->id == 6072) || (f->id == 6086))
    // cout << GetSimTime() << " - Push flit " << f->id << ", pid: " << f->pid << ", src: " << f->src << ", dest: " << f->dest << ", packet_seq: " << f->packet_seq << ", head: " << f->head << ", tail: " << f->tail << " | type: " << f->type << endl;

  // if (f->head){
  if (f->tail){
    // assert(f->dest == dest);
    f->rtime = GetSimTime();
  }

  int type = FindType(f->type);
#ifdef PACKET_GRAN_ORDER
  _reordering_vect[f->src][dest][type]->q.push(f);
#else
  auto search = _reordering_vect[f->src][dest][type]->q.find(f->mid);

  if (search != _reordering_vect[f->src][dest][type]->q.end()){ // Entry already exists
    search->second.second.push(f);
  } else { // Create new entry in the map
    priority_queue<Flit*, vector<Flit*>, Compare > temp;

    auto temp_pair = make_pair(f->mid, make_pair(0, temp));

    assert(_reordering_vect[f->src][dest][type]->q.insert(temp_pair).second);
    search = _reordering_vect[f->src][dest][type]->q.find(f->mid);
  }
#endif

#ifdef PACKET_GRAN_ORDER
  while ((!_reordering_vect[f->src][dest][type]->q.empty()) && (_reordering_vect[f->src][dest][type]->q.top()->packet_seq == _reordering_vect[f->src][dest][type]->recv)){
    Flit* temp = _reordering_vect[f->src][dest][type]->q.top();

    if (temp->tail)
      _reordering_vect[f->src][dest][type]->recv += 1;
#else
  while (search->second.second.top()->packet_seq <= (unsigned)search->second.first){
    Flit* temp = search->second.second.top();

    if (temp->tail)
      search->second.first += 1;
#endif

    _last_id = temp->id;
    _last_pid = temp->pid;
    TrafficManager::_RetireFlit(temp, dest);

#ifdef PACKET_GRAN_ORDER
    _reordering_vect[f->src][dest][type]->q.pop();
#else
    search->second.second.pop();
#endif
  }

  // HANS: Without reordering buffer
  // _last_id = f->id;
  // _last_pid = f->pid;
  // TrafficManager::_RetireFlit(f, dest);
}

int MixMsgBatchRateTrafficManager::IssueMessage( int source, int cl )
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
      if (_compute_nodes.count(source) > 0){
        if((_injection_process[cl]->test(source)) && (_message_seq_no[source] < _batch_size) && ((_max_outstanding <= 0) || (_requestsOutstanding[source] < _max_outstanding))) {
	        //coin toss to determine request type.
	        result = (RandomFloat() < 0.5) ? 2 : 1;

	        _requestsOutstanding[source]++;
        }
      }
    }
  } else { //normal
    if (_compute_nodes.count(source) > 0){
      if((_injection_process[cl]->test(source)) && (_message_seq_no[source] < _batch_size) && ((_max_outstanding <= 0) || (_requestsOutstanding[source] < _max_outstanding))) {
        result = GetNextMessageSize(cl);
        _requestsOutstanding[source]++;
      }
    }
  }
  // if(result != 0) {
  if(result > 0) {
    _message_seq_no[source]++;
  }
  return result;
}

void MixMsgBatchRateTrafficManager::GenerateMessage( int source, int stype, int cl, int time )
{
    assert(stype!=0);
    int message_destination;

    Flit::FlitType message_type = Flit::ANY_TYPE;
    int message_size = GetNextMessageSize(cl); //in packets
    int packet_size = _GetNextPacketSize(cl); //in flits
    // THO: workaround glitch in compute-memory traffic
    if (stype > 0)
      message_destination = _traffic_pattern[cl]->dest(source);
    bool record = false;
    // bool watch = gWatchOut && (_packets_to_watch.count(pid) > 0); // HANS: Disabled for now
    bool watch = false;
    if(_use_read_write[cl]){
        if(stype > 0) {
            if (stype == 1) {
                message_type = Flit::READ_REQUEST;
                message_size = _read_request_message_size[cl];
                packet_size = _read_request_size[cl];
            } else if (stype == 2) {
                message_type = Flit::WRITE_REQUEST;
                message_size = _write_request_message_size[cl];
                packet_size = _write_request_size[cl];

            } else {
                ostringstream err;
                err << "Invalid packet type: " << message_type;
                Error( err.str( ) );
            }
        } else {
            PacketReplyInfo* rinfo = _repliesPending[source].front();
            if (rinfo->type == Flit::READ_REQUEST) {//read reply
                message_type = Flit::READ_REPLY;
                message_size = _read_reply_message_size[cl];
                packet_size = _read_reply_size[cl];
            } else if(rinfo->type == Flit::WRITE_REQUEST) {  //write reply
                message_type = Flit::WRITE_REPLY;
                message_size = _write_reply_message_size[cl];
                packet_size = _write_reply_size[cl];
            } else {
                ostringstream err;
                err << "Invalid packet type: " << rinfo->type;
                Error( err.str( ) );
            }
            message_destination = rinfo->source;
            time = rinfo->time;
            record = rinfo->record;
            _repliesPending[source].pop_front();
            rinfo->Free();
        }
    }

    if ((message_destination < 0) || (message_destination >= _nodes)) {
        ostringstream err;
        err << "Incorrect message destination " << message_destination
            << " for stype " << message_type;
        Error( err.str( ) );
    }

    if ( ( _sim_state == running ) ||
         ( ( _sim_state == draining ) && ( time < _drain_time ) ) ) {
        record = _measure_stats[cl];
    }

    int subnetwork = ((message_type == Flit::ANY_TYPE) ? 
                      RandomInt(_subnets-1) :
                      _subnet[message_type]);
  
    int mid = _cur_mid++;
    assert(_cur_mid);

    for (int i = 0; i < message_size; ++i) {
      int pid = _cur_pid++;
      assert(_cur_pid);

      if ( watch ) { 
        *gWatchOut << GetSimTime() << " | "
                   << "node" << source << " | "
                   << "Enqueuing packet " << pid
                   << " at time " << time
                   << "." << endl;
      }

      for ( int j = 0; j < packet_size; ++j ) {
          Flit * f  = Flit::New();
          f->id     = _cur_id++;
          assert(_cur_id);
          f->pid    = pid;
          f->mid    = mid;
          f->watch  = watch | (gWatchOut && (_flits_to_watch.count(f->id) > 0));
          f->subnetwork = subnetwork;
          f->src    = source;
          f->ctime  = time;
          f->record = record;
          f->cl     = cl;

          _total_in_flight_flits[f->cl].insert(make_pair(f->id, f));
          if(record) {
              _measured_in_flight_flits[f->cl].insert(make_pair(f->id, f));
          }

          if(gTrace){
              cout<<"New Flit "<<f->src<<endl;
          }
          f->type = message_type;

          if ( i == 0 ) { // Head packet
              f->msg_head = true;
          }

          if ( j == 0 ) { // Head flit
              f->head = true;
              //packets are only generated to nodes smaller or equal to limit
              f->dest = message_destination;            
          } else {
              f->head = false;
              f->dest = -1;
          }

#ifdef PACKET_GRAN_ORDER
          int type = FindType(f->type);
          f->packet_seq = _reordering_vect[source][message_destination][type]->send;
#else
          f->packet_seq = i;
#endif

          // HANS: For debugging
          // if (message_destination == 2)
            // cout << GetSimTime() << " - Generate flit from " << source << " to " << message_destination << " sequence " << f->packet_seq << ", mID: " << f->mid << ", pID: " << f->pid << " ID: " << f->id << " | Type: " << f->type << endl;

          // HANS: For debugging
          int pkt_watch_id = -1;
          if (f->pid == pkt_watch_id) f->watch = true;

          switch( _pri_type ) {
          case class_based:
              f->pri = _class_priority[cl];
              assert(f->pri >= 0);
              break;
          case age_based:
              f->pri = numeric_limits<int>::max() - time;
              assert(f->pri >= 0);
              break;
          case sequence_based:
              // f->pri = numeric_limits<int>::max() - _packet_seq_no[source];
              f->pri = numeric_limits<int>::max() - _message_seq_no[source];
              assert(f->pri >= 0);
              break;
          default:
              f->pri = 0;
          }
          if (i == ( message_size - 1 ) ) { // Tail packet
              f->msg_tail = true;
          }
          if ( j == ( packet_size - 1 ) ) { // Tail flit
              f->tail = true;

              // HANS: Additionals for packet reordering
#ifdef PACKET_GRAN_ORDER
              int type = FindType(f->type);
              _reordering_vect[source][message_destination][type]->send += 1;
#endif
              
          } else {
              f->tail = false;
          }

          f->vc  = -1;

          if ( f->watch ) { 
              *gWatchOut << GetSimTime() << " | "
                         << "node" << source << " | "
                         << "Enqueuing flit " << f->id
                         << " (message " << f->mid
                         << ", packet " << f->pid
                         << ") at time " << time
                         << "." << endl;
          }

          // HANS: For debugging
          // if (source == 12)
            // cout << GetSimTime() << " - Generate flit at source: " << f->src << ", fID: " << f->id << ", pID: " << f->pid << ", mID: " << f->mid << " | Packet_Head: " << f->head << ", Packet_Tail: " << f->tail << ", Msg_Head: " << f->msg_head << ", Msg_Tail: " << f->msg_tail << " | Type: " << f->type << " | Prio: " << f->pri << endl;

          _partial_packets[source][cl].push_back( f );
      }
    }
}

void MixMsgBatchRateTrafficManager::_Inject(){

    for ( int input = 0; input < _nodes; ++input ) {
        for ( int c = 0; c < _classes; ++c ) {
            // Potentially generate packets for any (input,class)
            // that is currently empty
            if ( _partial_packets[input][c].empty() ) {
                bool generated = false;
                while( !generated && ( _qtime[input][c] <= _time ) ) {
                    int stype = IssueMessage( input, c );
	  
                    if ( stype != 0 ) { //generate a packet
                        GenerateMessage( input, stype, c, 
                                         _include_queuing==1 ? 
                                         _qtime[input][c] : _time );
                        generated = true;
                    }
                    // only advance time if this is not a reply packet
                    if(!_use_read_write[c] || (stype >= 0)){
                        ++_qtime[input][c];
                    }
                }
	
                if ( ( _sim_state == draining ) && 
                     ( _qtime[input][c] > _drain_time ) ) {
                    _qdrained[input][c] = true;
                }
            }
        }
    }
}

void MixMsgBatchRateTrafficManager::_ClearStats( )
{
  TrafficManager::_ClearStats();
  _batch_time->Clear( );
}

bool MixMsgBatchRateTrafficManager::_SingleSim( )
{
  int batch_index = 0;
  while(batch_index < _batch_count) {
    // _packet_seq_no.assign(_nodes, 0);
    _message_seq_no.assign(_nodes, 0);
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
        if (_compute_nodes.count(i) > 0){
          if (_message_seq_no[i] < _batch_size) {
	          batch_complete = false;
	          break;
          }
	      }
      }
      // if(_sent_packets_out) {
	      // *_sent_packets_out << _packet_seq_no << endl;
      // }
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

void MixMsgBatchRateTrafficManager::_UpdateOverallStats() {
  TrafficManager::_UpdateOverallStats();
  _overall_min_batch_time += _batch_time->Min();
  _overall_avg_batch_time += _batch_time->Average();
  _overall_max_batch_time += _batch_time->Max();
}
  
string MixMsgBatchRateTrafficManager::_OverallStatsCSV(int c) const
{
  ostringstream os;
  os << TrafficManager::_OverallStatsCSV(c) << ','
     << _overall_min_batch_time / (double)_total_sims << ','
     << _overall_avg_batch_time / (double)_total_sims << ','
     << _overall_max_batch_time / (double)_total_sims;
  return os.str();
}

void MixMsgBatchRateTrafficManager::WriteStats(ostream & os) const
{
  TrafficManager::WriteStats(os);
  os << "batch_time = " << _batch_time->Average() << ";" << endl;
}    

void MixMsgBatchRateTrafficManager::DisplayStats(ostream & os) const {
  TrafficManager::DisplayStats();
  os << "Minimum batch duration = " << _batch_time->Min() << endl;
  os << "Average batch duration = " << _batch_time->Average() << endl;
  os << "Maximum batch duration = " << _batch_time->Max() << endl;
}

void MixMsgBatchRateTrafficManager::DisplayOverallStats(ostream & os) const {
  TrafficManager::DisplayOverallStats(os);
  os << "Overall min batch duration = " << _overall_min_batch_time / (double)_total_sims
     << " (" << _total_sims << " samples)" << endl
     << "Overall avg batch duration = " << _overall_avg_batch_time / (double)_total_sims
     << " (" << _total_sims << " samples)" << endl
     << "Overall max batch duration = " << _overall_max_batch_time / (double)_total_sims
     << " (" << _total_sims << " samples)" << endl;
}

int MixMsgBatchRateTrafficManager::GetNextMessageSize(int cl) const
{
  assert(cl >= 0 && cl < _classes);

    vector<int> const & msize = _message_size[cl];
    int sizes = msize.size();

    assert(sizes == 1); // HANS: For now

    // if(sizes == 1) { // HANS: Commented out just to prevent compiler warning
        return msize[0];
    // }

    /*
    vector<int> const & mrate = _message_size_rate[cl];
    int max_val = _message_size_max_val[cl];

    int pct = RandomInt(max_val);

    for(int i = 0; i < (sizes - 1); ++i) {
        int const limit = mrate[i];
        if(limit > pct) {
            return msize[i];
        } else {
            pct -= limit;
        }
    }
    assert(mrate.back() > pct);
    return msize.back();
    */
}

double MixMsgBatchRateTrafficManager::GetAverageMessageSize(int cl) const
{
    int sizes;

    vector<int> const & msize = _message_size[cl];
    sizes = msize.size();
    assert(sizes == 1); // HANS: Just for now..
    // if(sizes == 1) {
    double message_size = (double)msize[0];
    // }
    
    // vector<int> const & psize = _packet_size[cl];
    // sizes = psize.size();
    // assert(sizes == 1); // HANS: Just for now..

    // if(sizes == 1) {
    // double packet_size = (double)psize[0];
    // }

    // return message_size * packet_size;
    return message_size;
    
    /*
    vector<int> const & prate = _packet_size_rate[cl];
    int sum = 0;
    for(int i = 0; i < sizes; ++i) {
        sum += psize[i] * prate[i];
    }
    return (double)sum / (double)(_packet_size_max_val[cl] + 1);
    */
}