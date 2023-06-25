#pragma once
#include <memory>
namespace mandeye_utils
{
class TimeStampProvider;
class TimeStampReceiver
{
public:
	//! Set timestamp provider
	void SetTimeStampProvider(std::shared_ptr<TimeStampProvider> timeStampProvider);
	//! Returns the current timestamp
	double GetTimeStamp();
protected:
	//! The timestamp provider
	std::shared_ptr<TimeStampProvider> m_timeStampProvider;
};
} // namespace mandeye_utils
