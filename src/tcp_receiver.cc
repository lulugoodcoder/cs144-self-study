#include "tcp_receiver.hh"
#include "debug.hh"
#include <iostream>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{ 
  if (message.RST) {
     reassembler_.reader().set_error();
    return;
  }
  if (!message.SYN && !ISNReceived) {
    return;
  }
  if (message.SYN) {
    ISNReceived = true;
    ISN = message.seqno;
  }
  if (message.payload.size() > 0 || message.FIN) {
    uint64_t checkpoint = ISNReceived ? reassembler_.getNextId() : 0;
    uint64_t abs_seqno = Wrap32(message.seqno).unwrap(ISN, checkpoint);
    uint64_t stream_idx = message.SYN ? 0 : abs_seqno - 1;
    reassembler_.insert(stream_idx, message.payload, message.FIN);
  }
  send();
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage message;
  uint64_t capacity = reassembler_.writer().available_capacity();
  message.window_size = capacity > UINT16_MAX ? UINT16_MAX : capacity;
  if (ISNReceived) {
    uint64_t nextId = reassembler_.getNextId();
    // Add 1 for SYN, add 1 more if stream is closed (FIN received)
    uint64_t ackno_value = nextId + 1;
    if (reassembler_.writer().is_closed()) {
      ackno_value++;
    }
    message.ackno = Wrap32::wrap(ackno_value, ISN);
  }
  message.RST = reassembler_.writer().has_error();
  return message;
}
