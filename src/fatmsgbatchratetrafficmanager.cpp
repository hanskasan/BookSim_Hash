// CSNL KAIST, Sept 2022

#include <limits>
#include <sstream>
#include <fstream>

#include "packet_reply_info.hpp"
#include "random_utils.hpp"
#include "fatmsgbatchratetrafficmanager.hpp"

FatMsgBatchRateTrafficManager::FatMsgBatchRateTrafficManager( const Configuration &config, 
					  const vector<Network *> & net )
: MsgBatchRateTrafficManager(config, net)
{
  _physical_nodes = gPhyNodes;

  // Injection queues
  _qtime.resize(_physical_nodes);
  _qdrained.resize(_physical_nodes);

  for ( int p = 0; p < _physical_nodes; ++p ) {
      _qtime[p].resize(_classes);
      _qdrained[p].resize(_classes);
  }

  _message_seq_no.resize(_physical_nodes);
  _requestsOutstanding.resize(_physical_nodes);
  _repliesPending.resize(_physical_nodes);

  // For reordering
  _reordering_vect.resize(_physical_nodes);

    for (int i = 0; i < _physical_nodes; i++){
        _reordering_vect[i].resize(_physical_nodes);

        for (int j = 0; j < _physical_nodes; j++){
            _reordering_vect[i][j].resize(_flit_types);

            for (int k = 0; k < _flit_types; k++)
                _reordering_vect[i][j][k] = new ReorderInfo();
        }
    }

  // HANS: Manual compute and memory node selection
  _compute_nodes.clear();
  _memory_nodes.clear();
  
  vector<int> temp_comp = {0, 1, 2};
  vector<int> temp_mem  = {3};

  set<int> temp_set_comp(temp_comp.begin(), temp_comp.end());
  set<int> temp_set_mem (temp_mem.begin(),  temp_mem.end());
  _compute_nodes = temp_set_comp;
  _memory_nodes  = temp_set_mem;

  cout << "Compute nodes are: ";
  for (set<int>::iterator i = _compute_nodes.begin(); i != _compute_nodes.end(); i++) {
    cout << *i << " ";
  }
  cout << endl;
  cout << "Memory nodes are: ";
  for (set<int>::iterator i = _memory_nodes.begin(); i != _memory_nodes.end(); i++) {
    cout << *i << " ";
  }
  cout << endl;

  // Load
  _load = config.GetFloatArray("batch_injection_rate"); 
  if(_load.empty()) {
      _load.push_back(config.GetFloat("batch_injection_rate"));
  } 
  _load.resize(_classes, _load.back());

  if(config.GetInt("injection_rate_uses_flits")) {
    for(int c = 0; c < _classes; ++c){
      _load[c] /= GetAverageMessageSize(c);
      _load[c] *= gC;
    }
  }

  // HANS: Get injection rate and process
  vector<string> injection_process = config.GetStrArray("injection_process");
  injection_process.resize(_classes, injection_process.back());

  for(int c = 0; c < _classes; ++c){
    _injection_process[c] = InjectionProcess::New(injection_process[c], _nodes, _load[c], &config);
  }

  if(config.GetInt("injection_rate_uses_flits"))
    cout << "Running with load (considering the packet size): " << _load[0] << endl;
}

FatMsgBatchRateTrafficManager::~FatMsgBatchRateTrafficManager( )
{
}

int FatMsgBatchRateTrafficManager::IssueMessage( int physical_source, int cl )
{
  assert((physical_source >= 0) && (physical_source < _physical_nodes));
  int result = 0;
  if(_use_read_write[cl]) { //read write packets
    //check queue for waiting replies.
    //check to make sure it is on time yet
    if(!_repliesPending[physical_source].empty()) {
      if(_repliesPending[physical_source].front()->time <= _time) {
	      result = -1;
      }
    } else {
      if (_compute_nodes.count(physical_source) > 0){
        if((_injection_process[cl]->test(physical_source)) && (_message_seq_no[physical_source] < _batch_size) && ((_max_outstanding <= 0) || (_requestsOutstanding[physical_source] < _max_outstanding))) {
	        //coin toss to determine request type.
	        result = (RandomFloat() < _write_fraction[cl]) ? 2 : 1;

	        _requestsOutstanding[physical_source]++;
        }
      }
    }
  } else { //normal
    if (_compute_nodes.count(physical_source) > 0){
      if((_injection_process[cl]->test(physical_source)) && (_message_seq_no[physical_source] < _batch_size) && ((_max_outstanding <= 0) || (_requestsOutstanding[physical_source] < _max_outstanding))) {
        result = GetNextMessageSize(cl);
        _requestsOutstanding[physical_source]++;
      }
    }
  }
  // if(result != 0) {
  if(result > 0) {
    _message_seq_no[physical_source]++;
  }
  return result;
}

void FatMsgBatchRateTrafficManager::GenerateMessage( int physical_source, int stype, int cl, int time )
{
    assert((physical_source >= 0) && (physical_source < _physical_nodes));
    assert(stype!=0);

    Flit::FlitType message_type = Flit::ANY_TYPE;
    int message_size = GetNextMessageSize(cl); //in packets
    int packet_size = _GetNextPacketSize(cl); //in flits
    int message_destination = _traffic_pattern[cl]->dest(physical_source);
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
            PacketReplyInfo* rinfo = _repliesPending[physical_source].front();
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
            _repliesPending[physical_source].pop_front();
            rinfo->Free();
        }
    }

    if ((message_destination < 0) || (message_destination >= _physical_nodes)) {
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

      // Distribute packets across the fat node's input port
      int temp_source = physical_source * gC + RandomInt(gC - 1);

      if ( watch ) { 
        *gWatchOut << GetSimTime() << " | "
                   << "fat_node" << physical_source << " | " 
                   << "port" << temp_source << " | "
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
          f->src    = temp_source;
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
            //   f->dest = message_destination * gC + (f->src % gC);            
              f->dest = message_destination * gC + RandomInt(gC - 1);            
          } else {
              f->head = false;
              f->dest = -1;
          }

          // HANS: For debugging
        //   if (f->id == 354){
        //   if (f->pid == 100){
        //   if (f->mid == 4){
            // f->watch = true;
        //   }

          int type = FindType(f->type);
          f->packet_seq = _reordering_vect[physical_source][message_destination][type]->send;

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
              f->pri = numeric_limits<int>::max() - _message_seq_no[physical_source];
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
              int type = FindType(f->type);
              _reordering_vect[physical_source][message_destination][type]->send += 1;
              
          } else {
              f->tail = false;
          }

          f->vc  = -1;

          if ( f->watch ) { 
              *gWatchOut << GetSimTime() << " | "
                         << "node" << f->src << " | "
                         << "Enqueuing flit " << f->id
                         << " (message " << f->mid
                         << ", packet " << f->pid
                         << ") at time " << time
                         << "." << endl;
          }

          _partial_packets[f->src][cl].push_back( f );
      }
    }
}

void FatMsgBatchRateTrafficManager::_Inject(){

    for ( int phy = 0; phy < _physical_nodes; ++phy ) {
        for ( int c = 0; c < _classes; ++c ) {
            // Potentially generate packets for any (phy,class)
            // that is currently empty

            bool is_empty = true;
            for ( int n = 0; n < gC; ++n ){
                if (!_partial_packets[phy * gC + n][c].empty())
                    is_empty = false;
            }

            if ( is_empty ) {
                bool generated = false;
                while( !generated && ( _qtime[phy][c] <= _time ) ) {
                    int stype = IssueMessage( phy, c );
	  
                    if ( stype != 0 ) { //generate a packet
                        GenerateMessage( phy, stype, c, 
                                         _include_queuing==1 ? 
                                         _qtime[phy][c] : _time );
                        generated = true;
                    }
                    // only advance time if this is not a reply packet
                    if(!_use_read_write[c] || (stype >= 0)){
                        ++_qtime[phy][c];
                    }
                }
	
                if ( ( _sim_state == draining ) && 
                     ( _qtime[phy][c] > _drain_time ) ) {
                    _qdrained[phy][c] = true;
                }
            }
        }
    }
}

// HANS: Inherit _RetireFlit to enable reordering buffer
void FatMsgBatchRateTrafficManager::_RetireFlit( Flit *f, int dest )
{
  int physical_source = f->src / gC;
  int physical_dest = dest / gC;

  if (f->head){
    assert((f->dest / gC) == physical_dest);
    f->rtime = GetSimTime();
  }

  int type = FindType(f->type);
  _reordering_vect[physical_source][physical_dest][type]->q.push(f);

  while ((!_reordering_vect[physical_source][physical_dest][type]->q.empty()) && (_reordering_vect[physical_source][physical_dest][type]->q.top()->packet_seq == _reordering_vect[physical_source][physical_dest][type]->recv)){
    Flit* temp = _reordering_vect[physical_source][physical_dest][type]->q.top();

    if (temp->tail){
      _reordering_vect[physical_source][physical_dest][type]->recv += 1;
    }

    _last_id = temp->id;
    _last_pid = temp->pid;
    
    OriRetireFlit(temp, dest);

    _reordering_vect[physical_source][physical_dest][type]->q.pop();
  }
}

void FatMsgBatchRateTrafficManager::OriRetireFlit( Flit *f, int dest )
{
    int physical_dest = dest / gC;

    _deadlock_timer = 0;

    assert(_total_in_flight_flits[f->cl].count(f->id) > 0);
    _total_in_flight_flits[f->cl].erase(f->id);
  
    if(f->record) {
        assert(_measured_in_flight_flits[f->cl].count(f->id) > 0);
        _measured_in_flight_flits[f->cl].erase(f->id);
    }

    if ( f->watch ) { 
        *gWatchOut << GetSimTime() << " | "
                   << "node" << dest << " | "
                   << "Retiring flit " << f->id 
                   << " (packet " << f->pid
                   << ", src = " << f->src 
                   << ", dest = " << f->dest
                   << ", hops = " << f->hops
                   << ", flat = " << f->atime - f->itime
                   << ")." << endl;
    }

    if ( f->head && ( f->dest / gC != physical_dest ) ) {
        ostringstream err;
        err << "Flit " << f->id << " arrived at incorrect output " << dest;
        Error( err.str( ) );
    }
  
    if((_slowest_flit[f->cl] < 0) ||
       (_flat_stats[f->cl]->Max() < (f->atime - f->itime)))
        _slowest_flit[f->cl] = f->id;
    _flat_stats[f->cl]->AddSample( f->atime - f->itime);
    if(_pair_stats){
        _pair_flat[f->cl][f->src*_nodes+dest]->AddSample( f->atime - f->itime );
    }

    // HANS: Additionals for recording reordering latency
    // Packet latency does not include the reordering latency
    if ( f->head ){
        if ((f->type == Flit::READ_REPLY) || (f->type == Flit::WRITE_REPLY)){ // Only record reply traffic 
            _rlat_stats[f->cl]->AddSample( GetSimTime() - f->rtime );
        }

        if (f->type == Flit::READ_REPLY){
            _read_rlat_stats[f->cl]->AddSample( GetSimTime() - f->rtime );
        }

        if (f->type == Flit::WRITE_REPLY){
            _write_rlat_stats[f->cl]->AddSample( GetSimTime() - f->rtime );
        }
    }
      
    if ( f->tail ) {
        Flit * head;
        if(f->head) {
            head = f;
        } else {
            map<int, Flit *>::iterator iter = _retired_packets[f->cl].find(f->pid);
            assert(iter != _retired_packets[f->cl].end());
            head = iter->second;
            _retired_packets[f->cl].erase(iter);
            assert(head->head);
            assert(f->pid == head->pid);
        }
        if ( f->watch ) { 
            *gWatchOut << GetSimTime() << " | "
                       << "node" << dest << " | "
                       << "Retiring packet " << f->pid 
                       << " (plat = " << f->atime - head->ctime
                       << ", nlat = " << f->atime - head->itime
                       << ", frag = " << (f->atime - head->atime) - (f->id - head->id) // NB: In the spirit of solving problems using ugly hacks, we compute the packet length by taking advantage of the fact that the IDs of flits within a packet are contiguous.
                       << ", src = " << head->src 
                       << ", dest = " << head->dest
                       << ")." << endl;
        }

        //code the source of request, look carefully, its tricky ;)
        if (f->type == Flit::READ_REQUEST || f->type == Flit::WRITE_REQUEST) {
            PacketReplyInfo* rinfo = PacketReplyInfo::New();
            rinfo->source = f->src / gC;
            rinfo->time = f->atime;
            rinfo->record = f->record;
            rinfo->type = f->type;
            _repliesPending[physical_dest].push_back(rinfo);
        } else {
            if(f->type == Flit::READ_REPLY || f->type == Flit::WRITE_REPLY  ){
                _requestsOutstanding[physical_dest]--;
            } else if(f->type == Flit::ANY_TYPE) {
                _requestsOutstanding[f->src / gC]--;
            }
      
        }

        // Only record statistics once per packet (at tail)
        // and based on the simulation state
        if ( ( _sim_state == warming_up ) || f->record ) {
      
            _hop_stats[f->cl]->AddSample( f->hops );

            if((_slowest_packet[f->cl] < 0) ||
               (_plat_stats[f->cl]->Max() < (f->atime - head->ctime)))
                _slowest_packet[f->cl] = f->pid;
            _plat_stats[f->cl]->AddSample( f->atime - head->ctime);
            _nlat_stats[f->cl]->AddSample( f->atime - head->itime);
            _frag_stats[f->cl]->AddSample( (f->atime - head->atime) - (f->id - head->id) );
   
            if(_pair_stats){
                _pair_plat[f->cl][f->src*_nodes+dest]->AddSample( f->atime - head->ctime );
                _pair_nlat[f->cl][f->src*_nodes+dest]->AddSample( f->atime - head->itime );
            }

            // cout << GetSimTime() << " - Retire packet " << f->pid << ", flit " << f->id << ", packet latency " << f->atime - head->ctime << ", network latency " << f->atime - head->itime << endl;

            // HANS: Additionals for recording latency distribution
            int plat_class_idx = (f->atime - head->ctime) / _resolution;
            if (plat_class_idx > (_num_cell - 1))   plat_class_idx = _num_cell - 1;
            _plat_class[plat_class_idx]++;

            int nlat_class_idx = (f->atime - head->itime) / _resolution;
            if (nlat_class_idx > (_num_cell - 1))   nlat_class_idx = _num_cell - 1;
            _nlat_class[nlat_class_idx]++;

            // HANS: Additionals for request-reply traffic
            if ((f->type == Flit::READ_REQUEST) || (f->type == Flit::READ_REPLY)){
                _read_plat_stats[f->cl]->AddSample( f->atime - head->ctime);
            } else {
                assert((f->type == Flit::WRITE_REQUEST) || (f->type == Flit::WRITE_REPLY));
                _write_plat_stats[f->cl]->AddSample( f->atime - head->ctime);
            }

        }
    
        if(f != head) {
            head->Free();
        }
    }
  
    if(f->head && !f->tail) {
        _retired_packets[f->cl].insert(make_pair(f->pid, f));
    } else {
        f->Free();
    }
}