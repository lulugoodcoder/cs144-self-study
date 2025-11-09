#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <functional>
#include <map>
class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), sent_(0), initial_RTO_ms_( initial_RTO_ms ), cur_RTO_ms_(initial_RTO_ms_), 
      SYN_sent_(false),FIN_sent_(false), oldest_ms_since_last_tick(0), window_size(0), consecutive(0), advertised_window_size_(1), pendings({}) {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  struct Wrap32Less {
    public:
    bool operator()(const Wrap32 left, const Wrap32 right) const {
        return left.get_raw_value() < right.get_raw_value();
    }
  };
  Reader& reader() { return input_.reader(); }
  
  ByteStream input_;
  Wrap32 isn_;
  uint64_t sent_;
  uint64_t initial_RTO_ms_;
  uint64_t cur_RTO_ms_;
  bool SYN_sent_;
  bool FIN_sent_;
  uint64_t oldest_ms_since_last_tick;
  uint16_t window_size;
  uint16_t consecutive;
  uint16_t advertised_window_size_;
  std::map<uint64_t, TCPSenderMessage> pendings; 
};
