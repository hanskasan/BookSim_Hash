// CSNL KAIST, Sept 2022

#include <limits>
#include <sstream>
#include <fstream>
#include <math.h>

#include "packet_reply_info.hpp"
#include "random_utils.hpp"
#include "fatmsgbatchratetrafficmanager.hpp"

FatMsgBatchRateTrafficManager::FatMsgBatchRateTrafficManager( const Configuration &config, 
					  const vector<Network *> & net )
: MsgBatchRateTrafficManager(config, net)
{
  _fat_ratio = config.GetInt("fat_ratio");
  _physical_nodes = gNodes / _fat_ratio;

  cout << "Number of physical nodes: " << _physical_nodes << endl;
 
  // Adjust batch size to according to _fat_ratio
  _batch_size = _fat_ratio * _batch_size;

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

  _reordering_vect_size.resize(_physical_nodes);

    for (int i = 0; i < _physical_nodes; i++){
        _reordering_vect_size[i].resize(_physical_nodes);
        
        for (int j = 0; j < _physical_nodes; j++){
            _reordering_vect_size[i][j] = 0;
        }
    }

  _reordering_vect_maxsize = 0;

  // HANS: Manual compute and memory node selection
  _compute_nodes.clear();
  _memory_nodes.clear();

  // vector<int> temp_comp = {0, 1, 2, 3};
  // vector<int> temp_mem  = {};
  
  // HANS: No compute and memory node differentiation
  vector<int> temp_comp;
  for (int iter = 0; iter < _physical_nodes; iter++){
    temp_comp.push_back(iter);
  }
  vector<int> temp_mem  = {};

  set<int> temp_set_comp(temp_comp.begin(), temp_comp.end());
  set<int> temp_set_mem (temp_mem.begin(),  temp_mem.end());
  _compute_nodes = temp_set_comp;
  _memory_nodes  = temp_set_mem;

  cout << "Fat compute nodes are: ";
  for (set<int>::iterator i = _compute_nodes.begin(); i != _compute_nodes.end(); i++) {
    cout << *i << " ";
  }
  cout << endl;
  cout << "Fat memory nodes are: ";
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
      // _load[c] *= _fat_ratio;

      // Calculation total intersection probability
      double ini_prob = _load[c];
      double intersection = 0.0;
      for (int iter = 2; iter <= _fat_ratio; iter++){
        intersection += pow(ini_prob, iter) * (double)(factorial(_fat_ratio)/(factorial(iter) * factorial(_fat_ratio - iter)));
      }

      _load[c] = _fat_ratio * ini_prob - intersection;

      // cout << "Initial: " << ini_prob << ", Intersection: " << intersection << ", After: " << _load[c] << endl;
    }
  }

  // HANS: Get injection rate and process
  vector<string> injection_process = config.GetStrArray("injection_process");
  injection_process.resize(_classes, injection_process.back());

  for(int c = 0; c < _classes; ++c){
    cout << "Register load: " << _load[c] << " for fat-node." << endl;
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

void FatMsgBatchRateTrafficManager::GenerateMessage( int source, int stype, int cl, int time )
{
    int physical_source = source / _fat_ratio;
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
      int temp_source = physical_source * _fat_ratio + ((source + i) % _fat_ratio);

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
              f->dest = message_destination * _fat_ratio + (f->src % _fat_ratio);            
            //   f->dest = message_destination * _fat_ratio + RandomInt(_fat_ratio - 1);            
          } else {
              f->head = false;
            //   f->dest = -1;
              f->dest = message_destination * _fat_ratio + (f->src % _fat_ratio); // HANS: Just for debugging
          }

          // HANS: For debugging
        //   if (f->id == 0){
        //   if (f->pid == 54){
        //   if (f->mid == 4){
            // f->watch = true;
        //   }

          int type = FindType(f->type);
#ifdef PACKET_GRAN_ORDER
          f->packet_seq = _reordering_vect[physical_source][message_destination][type]->send;
#else
          f->packet_seq = i;
#endif

        //   if ((f->msg_head) && (f->head)){
            // if ((physical_source == 3) && (message_destination == 2))
                // cout << GetSimTime() << " - Send message " << f->mid << " packet " << f->pid << " with sequence " << f->packet_seq << endl;
        //   }

          if (f->watch)
            cout << GetSimTime() << " - Assign message " << f->mid << " packet " << f->pid << " with sequence " << f->packet_seq << endl;

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
#ifdef PACKET_GRAN_ORDER
              int type = FindType(f->type);
              _reordering_vect[physical_source][message_destination][type]->send += 1;
#endif
              
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

          // HANS: For debugging
          /*
          if (f->head){
          // if ((f->msg_head) && (f->head)){
            if ((f->src / _fat_ratio) == 3)
              cout << GetSimTime() << " - Enqueuing fID " << f->id << ", pID: " << f->pid << ", mID: " << f->mid << ", Src: " << f->src << ", Dest: " << f->dest << ", CTime: " << f->ctime << ", ITime: " << f->itime << ", OutPort: " << f->out_port << ". Enqueued at: " << time << endl;
          }
          */

          _partial_packets[f->src][cl].push_back( f );
      }
    }
}

void FatMsgBatchRateTrafficManager::_Inject(){

    for ( int phy = 0; phy < _physical_nodes; ++phy ) {
        for ( int c = 0; c < _classes; ++c ) {
            // Potentially generate packets for any (phy,class)
            // that is currently empty

            int empty_port;
            bool is_empty = true;
            for ( int n = 0; n < _fat_ratio; ++n ){
                if (!_partial_packets[phy * _fat_ratio + n][c].empty()){
                    // empty_port = phy * _fat_ratio + n;
                    is_empty = false;
                    break;
                }
            }

            if ( is_empty ) {
                bool generated = false;
                while( !generated && ( _qtime[phy][c] <= _time ) ) {
                    int stype = IssueMessage( phy, c );
	  
                    if ( stype != 0 ) { //generate a packet
                        empty_port = phy * _fat_ratio;
                        GenerateMessage( empty_port, stype, c, 
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
  int physical_source = f->src / _fat_ratio;
  int physical_dest = dest / _fat_ratio;

//   if (f->head){
  if (f->tail){
    // assert((f->dest / _fat_ratio) == physical_dest);
    f->rtime = GetSimTime();
  }

  int type = FindType(f->type);

//   if ((f->msg_head) && (f->head)){
    // if ((physical_source == 3) && (physical_dest == 2))
        // cout << GetSimTime() << " - Push message " << f->mid << " with sequence " << f->packet_seq << " to reordering buffer" << endl;
//   }

#ifdef PACKET_GRAN_ORDER
  _reordering_vect[physical_source][physical_dest][type]->q.push(f);
#else
  auto search = _reordering_vect[physical_source][physical_dest][type]->q.find(f->mid);

  if (search == _reordering_vect[physical_source][physical_dest][type]->q.end()){ // Entry does not exists
    priority_queue<Flit*, vector<Flit*>, Compare > temp;

    auto temp_pair = make_pair(f->mid, make_pair(0, temp));

    assert(_reordering_vect[physical_source][physical_dest][type]->q.insert(temp_pair).second);
    search = _reordering_vect[physical_source][physical_dest][type]->q.find(f->mid);
  }

  search->second.second.push(f);
#endif

  // HANS: Increment reordering buffer size count
  _reordering_vect_size[physical_source][physical_dest] += 1;
  assert(_reordering_vect_size[physical_source][physical_dest] >= 0);

  int sum = 0;
  for (int iter = 0; iter < _physical_nodes; iter++){
    sum += _reordering_vect_size[iter][physical_dest];
  }

  if (sum > _reordering_vect_maxsize){
    _reordering_vect_maxsize = sum;
  }

#ifdef PACKET_GRAN_ORDER
  while ((!_reordering_vect[physical_source][physical_dest][type]->q.empty()) && (_reordering_vect[physical_source][physical_dest][type]->q.top()->packet_seq == _reordering_vect[physical_source][physical_dest][type]->recv)){
    Flit* temp = _reordering_vect[physical_source][physical_dest][type]->q.top();

    if (temp->tail){
        // if ((physical_source == 3) && (physical_dest == 2))
        //   cout << GetSimTime() << " - Pop message " << temp->mid << " packet " << temp->pid << " flit " << temp->id << " with sequence " << temp->packet_seq << " from reordering buffer." << endl;

      _reordering_vect[physical_source][physical_dest][type]->recv += 1;
    }
#else
  bool is_delete = false;

  while ((!search->second.second.empty()) && (search->second.second.top()->packet_seq <= (unsigned)search->second.first)){
    Flit* temp = search->second.second.top();

    if (temp->tail)
      search->second.first += 1;
#endif

    _last_id = temp->id;
    _last_pid = temp->pid;

    // HANS: For debugging
    // if (temp->tail){
      // if ((temp->dest / _fat_ratio) == 3)
        // cout << GetSimTime() << " - Retire flit " << temp->id << ", pID: " << temp->pid << ", mID: " << temp->mid << ", Src: " << temp->src << ", Dest: " << temp->dest << ", CTime: " << temp->ctime << ", ITime: " << temp->itime << ", RLat: " << GetSimTime() - temp->rtime << ", OutPort: " << temp->out_port << ". Retired at: " << temp->rtime << endl;
    // }

    // HANS: Decrement reordering buffer size count
    _reordering_vect_size[physical_source][physical_dest] -= 1;
    assert(_reordering_vect_size[temp->src][dest] >= 0);
    
    OriRetireFlit(temp, dest);

#ifdef PACKET_GRAN_ORDER
    _reordering_vect[physical_source][physical_dest][type]->q.pop();
#else
    search->second.second.pop();

    if ((temp->msg_tail) && (temp->tail)){
      assert(search->second.second.empty());
      is_delete = true;
    }
#endif
  }

#ifndef PACKET_GRAN_ORDER
  if (is_delete){
    _reordering_vect[physical_source][physical_dest][type]->q.erase(search);
  }
#endif

}

void FatMsgBatchRateTrafficManager::OriRetireFlit( Flit *f, int dest )
{
    int physical_source = f->src /_fat_ratio;
    int physical_dest = dest / _fat_ratio;

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

    if ( f->head && ( f->dest / _fat_ratio != physical_dest ) ) {
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

    // HANS: Additionals for recordering endpoint waiting time
    if (f->head){
        if ((f->src / gC) != (f->dest / gC)){
            _uplat_stats[f->cl]->AddSample( f->uptime );

            // if ((f->src / gC) == 0)
                // cout << GetSimTime() << "- Uptime: " << f->uptime << ", fID: " << f->id << endl;
        }
        _ewlat_stats[f->cl]->AddSample( f->ewtime );
    }

    // HANS: Additionals for recording reordering latency
    // Packet latency does not include the reordering latency
    // if ( f->head ){
    if (f->tail){

        if (_use_read_write[f->cl]){
          if ((f->type == Flit::READ_REPLY) || (f->type == Flit::WRITE_REPLY)){ // Only record reply traffic 
              // HANS: For debugging
              // cout << "ID: " << f->id << ", Dest: " << f->dest << ", Rlat: " << GetSimTime() - f->rtime << endl;

              _rlat_stats[f->cl]->AddSample( GetSimTime() - f->rtime );
          }
        } else {
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
            rinfo->source = f->src / _fat_ratio;
            rinfo->time = f->atime;
            rinfo->record = f->record;
            rinfo->type = f->type;
            _repliesPending[physical_dest].push_back(rinfo);
        } else {
            if(f->type == Flit::READ_REPLY || f->type == Flit::WRITE_REPLY  ){
                _requestsOutstanding[physical_dest]--;
            } else if(f->type == Flit::ANY_TYPE) {
                _requestsOutstanding[f->src / _fat_ratio]--;
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
            if (_use_read_write[f->cl]){
              if ((f->type == Flit::READ_REQUEST) || (f->type == Flit::READ_REPLY)){
                  _read_plat_stats[f->cl]->AddSample( f->atime - head->ctime );
                  _read_nlat_stats[f->cl]->AddSample( f->atime - head->itime );
              } else {
                  assert((f->type == Flit::WRITE_REQUEST) || (f->type == Flit::WRITE_REPLY));
                  _write_plat_stats[f->cl]->AddSample( f->atime - head->ctime );
                  _write_nlat_stats[f->cl]->AddSample( f->atime - head->itime );

              }
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

int factorial(int n){
  if ((n==0)||(n==1))
    return 1;
  else
    return n*factorial(n-1);
}