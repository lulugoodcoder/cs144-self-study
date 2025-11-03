#include "byte_stream.hh"
#include "debug.hh"
#include <algorithm>
#include <iostream>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), usedCapacity_(0), poppedBytes_(0), pushedBytes_(0) {}

// Push data to stream, but only as much as available capacity allows.
void Writer::push( string data )
{  
  
  uint64_t leftCap = capacity_ - usedCapacity_;
  if (leftCap > data.size()) {
    datas_ += data;
    usedCapacity_ += data.size();
    pushedBytes_ += data.size();
  } else {
    uint64_t copySize = leftCap;
    usedCapacity_ += copySize;
    pushedBytes_ += copySize;
    datas_ += data.substr(0, copySize);
  }
}

// Signal that the stream has reached its ending. Nothing more will be written.
void Writer::close()
{
  close_ = true;
}

// Has the stream been closed?
bool Writer::is_closed() const
{
  return close_;
}

// How many bytes can be pushed to the stream right now?
uint64_t Writer::available_capacity() const
{ 
   uint64_t leftCap = capacity_ - usedCapacity_;
  return leftCap;
}

// Total number of bytes cumulatively pushed to the stream
uint64_t Writer::bytes_pushed() const
{  
  //debug( "Writer::bytes_pushed() not yet implemented" );
  return pushedBytes_; // Your code here.
}

// Peek at the next bytes in the buffer -- ideally as many as possible.
// It's not required to return a string_view of the *whole* buffer, but
// if the peeked string_view is only one byte at a time, it will probably force
// the caller to do a lot of extra work.
string_view Reader::peek() const
{ 
  return string_view(datas_);
}

// Remove `len` bytes from the buffer.
void Reader::pop( uint64_t len )
{
  datas_ = datas_.substr(len);
  usedCapacity_ -= len;
  poppedBytes_ += len;  
}

// Is the stream finished (closed and fully popped)?
bool Reader::is_finished() const
{ 
  return usedCapacity_ == 0  && close_ ;
}

// Number of bytes currently buffered (pushed and not popped)
uint64_t Reader::bytes_buffered() const
{
  return usedCapacity_;
}

// Total number of bytes cumulatively popped from stream
uint64_t Reader::bytes_popped() const
{
  return poppedBytes_;
}
