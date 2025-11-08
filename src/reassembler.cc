#include "reassembler.hh"
#include "debug.hh"
#include <string>
#include <map>
#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{    
 
    if (is_last_substring) {
        last_idx_ = first_index + data.size();
        last_string_received_ = true;
    }
    
    
    // Skip data that's already been written
    //  
    //  first_index               expected_idx_
    //                                           data_end   
    if (first_index < expected_idx_) {
        uint64_t offset = expected_idx_ - first_index;
        if (offset >= data.size()) {
            return;
        }
        data = data.substr(offset);
        first_index = expected_idx_;
    }
    
    // Check capacity
    Writer& writer = output_.writer();
    uint64_t first_unacceptable_index = expected_idx_ + writer.available_capacity();
    
    if (first_index >= first_unacceptable_index) {
        return;
    }
    
    if (first_index + data.size() > first_unacceptable_index) {
        data = data.substr(0, first_unacceptable_index - first_index);
    }
    
    // Merge with existing segments - insert non-overlapping parts
    // Insert "b" @ index 1 → stored in segments_[1] = "b", not pushed yet (waiting for index 0)
    // Insert "d" @ index 3 → stored in segments_[3] = "d", not pushed yet
    // Insert "abc" @ index 0
    // Build map of existing segment ranges
    size_t i = 0;
    while (i < data.size()) {
        uint64_t curr_pos = first_index + i;

        // Find first segment at or after curr_pos
        auto it = segments_.lower_bound(curr_pos);

        // Check if curr_pos is inside a previous segment
        if (it != segments_.begin()) {
            auto prev = std::prev(it);
            uint64_t prev_end = prev->first + prev->second.size();
            if (curr_pos < prev_end) {
                // Skip the overlapping part
                uint64_t skip = prev_end - curr_pos;
                i += skip;
                continue;
            }
        }

        // Check if curr_pos exactly matches an existing segment start
        if (it != segments_.end() && it->first == curr_pos) {
            // Skip past this entire segment
            i += it->second.size();
            continue;
        }

        // Find end position (either next segment start or end of data)
        uint64_t end_pos = first_index + data.size();
        if (it != segments_.end() && it->first < end_pos) {
            end_pos = it->first;
        }

        // Insert non-overlapping chunk
        size_t len = end_pos - curr_pos;
        segments_[curr_pos] = data.substr(i, len);
        storedBytes_ += len;
        i += len;
    }    
    
    // push all segment that can be pushed 
    while (segments_.find(expected_idx_) != segments_.end()) {
        string newData = segments_[expected_idx_];
        segments_.erase(expected_idx_);
        storedBytes_ -= newData.size();
        
        if (writer.available_capacity() > 0) {
            size_t cap = writer.available_capacity();
            size_t size = std::min(cap, newData.size());
            writer.push(newData.substr(0, size));
            expected_idx_ += size;
            
            if (size < newData.size()) {
                segments_[expected_idx_] = newData.substr(size);
                storedBytes_ += newData.size() - size;
            }
        } else {
            segments_[expected_idx_] = newData;
            storedBytes_ += newData.size();
            break;
        }
    }
    
    if (last_string_received_ && expected_idx_ == last_idx_) {
        writer.close();
    }
    std::cout << "expected_idx_" << expected_idx_ << std::endl;
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
    return storedBytes_;
}
