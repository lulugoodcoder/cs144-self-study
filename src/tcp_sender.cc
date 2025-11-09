#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// Returns the total number of sequence numbers currently in flight (outstanding data).
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t total = 0;
  for (const auto& pair : pendings) {
    total += pair.second.sequence_length();
  }
  return total;
}

// Returns the number of consecutive retransmissions that have occurred without an ACK.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive;
}

// Attempts to send new segments (SYN, data, and FIN) to fill the advertised window.
void TCPSender::push( const TransmitFunction& transmit )
{ 
  const uint64_t MAX_PAYLOAD_SIZE = TCPConfig::MAX_PAYLOAD_SIZE; 
  
  // Send SYN segment if it hasn't been sent yet.
  if (!SYN_sent_) {
    TCPSenderMessage msg = make_empty_message();
    msg.SYN = true;
    SYN_sent_ = true;
    
    // Calculate available window space. Treat window_size=0 as 1 for probing/SYN.
    uint64_t effective_window = window_size == 0 ? 1 : window_size;
    uint64_t available_window = (sequence_numbers_in_flight() >= effective_window) ? 0 : (effective_window - sequence_numbers_in_flight());
    
    // Account for the SYN flag taking 1 sequence number.
    if (available_window > 0) {
      available_window--; 
    }

    // Try to send data along with the SYN if space is available.
    Reader& reader = input_.reader();
    uint64_t bytes_buf = reader.bytes_buffered();
    
    if (bytes_buf > 0 && available_window > 0) {
      uint64_t byte_should_read = min(bytes_buf, min(available_window, MAX_PAYLOAD_SIZE));
      std::string out{};
      read(reader, byte_should_read, out);
      msg.payload = out;
      available_window -= msg.payload.size(); // Update available window after reading payload
    }
    
    // Add FIN if stream is finished and space is available.
    if (reader.is_finished() && available_window > 0 && !FIN_sent_) {
      msg.FIN = true;
      FIN_sent_ = true;
    }
    
    // Transmit and record the message.
    bool was_empty = pendings.empty();
    pendings[sent_] = msg;
    sent_ += msg.sequence_length();
    transmit(msg);
    
    if (was_empty) {
      oldest_ms_since_last_tick = 0;
    }
  }
  
  // Keep sending data segments to fill the window.
  while (true) {
    // Recalculate available window based on current state. Treat 0 as 1.
    uint64_t effective_window = window_size == 0 ? 1 : window_size;
    uint64_t bytes_in_flight = sequence_numbers_in_flight();
    uint64_t available_window = (bytes_in_flight >= effective_window) ? 0 : (effective_window - bytes_in_flight);
    
    Reader& reader = input_.reader();
    uint64_t bytes_buf = reader.bytes_buffered();
    
    // Check if we can send a segment (data or FIN)
    bool can_send_data = (bytes_buf > 0 && available_window > 0);
    bool can_send_fin = (reader.is_finished() && available_window > 0 && !FIN_sent_);

    if (!can_send_data && !can_send_fin) {
      break;
    }
    
    // --- Send FIN-only segment if stream is closed and window allows ---
    if (!can_send_data && can_send_fin) {
      TCPSenderMessage fin_msg = make_empty_message();
      fin_msg.FIN = true;
      FIN_sent_ = true;
      
      bool was_empty = pendings.empty();
      pendings[sent_] = fin_msg;
      sent_ += fin_msg.sequence_length();
      transmit(fin_msg);
      
      if (was_empty) {
        oldest_ms_since_last_tick = 0;
      }
      break; // FIN-only segment completes the transmission attempt for this push
    }
    
    // --- Send data segment (potentially with FIN) ---
    if (can_send_data) {
      TCPSenderMessage msg = make_empty_message();
      uint64_t byte_should_read = min(bytes_buf, min(available_window, MAX_PAYLOAD_SIZE));
      
      std::string out{};
      read(reader, byte_should_read, out);
      msg.payload = out;
      
      // Add FIN if stream closed and this segment uses all available window space (or close to it)
      if (reader.is_finished() && (available_window - byte_should_read) > 0 && !FIN_sent_) {
        msg.FIN = true;
        FIN_sent_ = true;
      }
      
      bool was_empty = pendings.empty();
      pendings[sent_] = msg;
      sent_ += msg.sequence_length();
      transmit(msg);
      
      if (was_empty) {
        oldest_ms_since_last_tick = 0;
      }
    }
  }
}

// Creates a basic TCPSenderMessage with the current sequence number and checks for RST.
TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap(sent_, isn_); 
  msg.payload = {};
  
  // Set RST flag if the stream has encountered an error, indicating connection failure.
  if (input_.reader().has_error()) {
      msg.RST = true;
  }
  
  return msg;
}

// Processes an incoming TCPReceiverMessage (ACK, window update, RST).
void TCPSender::receive( const TCPReceiverMessage& msg )
{ 
  // If the remote receiver sends an RST, the connection is aborted.
  if (msg.RST) {
    input_.writer().set_error();
    return;
  }
  
  // Store the raw advertised window size.
  advertised_window_size_ = msg.window_size;

  // Update window size. We internally treat a zero window as one byte for probing.
  window_size = msg.window_size;
  if (window_size == 0) {
    window_size = 1;
  }
  
  // Process the acknowledgment number (ackno).
  std::optional<Wrap32> ackno = msg.ackno;
  if (ackno.has_value()) {
    Wrap32 ackno_wrap  = msg.ackno.value();
    uint64_t abs_ackno = ackno_wrap.unwrap(isn_, sent_);

    // Validate ACK: ignore if it acknowledges data not yet sent.
    if (abs_ackno > sent_) {
      return;
    }

    // Remove acknowledged segments from the pending queue.
    bool acked_new_data = false;
    for (auto it = pendings.begin(); it != pendings.end(); ) {
      uint64_t abs_seqno = it->first;
      uint64_t seg_length = it->second.sequence_length();

      // If the segment is fully acknowledged (end_seqno <= abs_ackno)
      if (abs_seqno + seg_length <= abs_ackno) {
        acked_new_data = true;
        it = pendings.erase(it);
      } else {
        ++it;
      }
    }

    // If new data was acknowledged, reset RTO and congestion control state.
    if (acked_new_data) {
      cur_RTO_ms_ = initial_RTO_ms_;
      consecutive = 0;
      oldest_ms_since_last_tick = 0; // Reset timer for oldest unacked segment
    }
  }
}

// Handles the passage of time, checking for retransmission timeouts.
void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{ 
  // Do not accumulate time if nothing is in flight.
  if (pendings.empty()) {
    return;
  }
  
  oldest_ms_since_last_tick += ms_since_last_tick; 

  // Check for RTO expiry.
  if (oldest_ms_since_last_tick >= cur_RTO_ms_ ) {
    auto& msg = pendings.begin()->second;
    
    // Retransmit the oldest unacknowledged segment.
    transmit(msg);
    oldest_ms_since_last_tick = 0; // Reset timer for this segment
    
    // Double RTO only if the advertised window is non-zero (TCP congestion control rules).
    if (advertised_window_size_ > 0) {
      consecutive++;
      cur_RTO_ms_ = 2 * cur_RTO_ms_;
    }
  }
}
