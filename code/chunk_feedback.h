#pragma once
#include <string>

namespace mandeye
{
//! Per-chunk feedback, the confirmation beep (hardware::OnSavedLaz):
//!  - LAZ clients: called by savePointcloudData() once lidarNNNN.laz is saved,
//!  - raw clients (HESAI_PCAP): called when the recorder has lidarNNNN.pcap finished on disk.
//! Beeps only when the chunk file was saved (saved == true) and every extra service reports healthy
//! (ExtrasMonitor); otherwise it logs why there was no beep. Blocks for the length of the beep.
void onChunkSaved(const std::string& path, bool saved);
} // namespace mandeye
