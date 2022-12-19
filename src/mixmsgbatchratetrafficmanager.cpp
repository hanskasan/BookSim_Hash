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
  _batch_time = new Stats( this, "batch_time", 1.0, 1000 );
  _stats["batch_time"] = _batch_time;
  
  /*
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

  // HANS: Mixed message size configuration
  _big_message_size   = config.GetInt("big_message_size");
  _small_message_size = config.GetInt("small_message_size");
  _big_message_ratio  = config.GetFloat("big_message_ratio");

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
          assert(0); // HANS: Should not use request-reply here. Use the msgbatchratetrafficmanager instead
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
    for(int c = 0; c < _classes; ++c){
      cout << "LOAD TEST: " << _load[c] << ", " << GetAverageMessageSize(c) << endl;
      _load[c] /= GetAverageMessageSize(c);
    }
  }

  // HANS: Get injection rate and process
  vector<string> injection_process = config.GetStrArray("injection_process");
  injection_process.resize(_classes, injection_process.back());

  for(int c = 0; c < _classes; ++c){
    cout << "Register load: " << _load[c] << endl;
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

int MixMsgBatchRateTrafficManager::GetNextMessageSize(int cl) const
{
    assert(cl >= 0 && cl < _classes);

    if (RandomFloat() < _big_message_ratio)
      return _big_message_size;
    else
      return _small_message_size;
}

double MixMsgBatchRateTrafficManager::GetAverageMessageSize(int cl) const
{
    assert(!_use_read_write[cl]);

    double message_size = _big_message_ratio * _big_message_size + (1 - _big_message_ratio) * _small_message_size;
    
    vector<int> const & psize = _packet_size[cl];
    int sizes = psize.size();
    assert(sizes == 1); // HANS: Just for now..

    double packet_size = (double)psize[0];

    return message_size * packet_size;
    
    /*
    vector<int> const & prate = _packet_size_rate[cl];
    int sum = 0;
    for(int i = 0; i < sizes; ++i) {
        sum += psize[i] * prate[i];
    }
    return (double)sum / (double)(_packet_size_max_val[cl] + 1);
    */
}