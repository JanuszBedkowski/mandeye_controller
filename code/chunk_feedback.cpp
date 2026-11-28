#include "chunk_feedback.h"
#include "extras_monitor.h"
#include "hardware_config/mandeye.h"
#include <iostream>

namespace mandeye
{
void onChunkSaved(const std::string& path, bool saved)
{
	if(!saved)
	{
		std::cout << "Chunk NOT saved, no beep: " << path << std::endl;
		return;
	}
	const auto extras = ExtrasMonitor::instance().produceStatus();
	if(!extras["all_ok"].get<bool>())
	{
		std::string failing;
		for(const auto& [name, service] : extras["services"].items())
		{
			if(!service["ok"].get<bool>())
			{
				failing += " " + name + " (" + service["message"].get<std::string>() + ")";
			}
		}
		std::cout << "Saved " << path << ", no beep: extras failing:" << failing << std::endl;
		return;
	}
	hardware::OnSavedLaz(path);
}
} // namespace mandeye
