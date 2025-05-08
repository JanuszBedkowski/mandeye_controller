#pragma once

namespace mandeye_utils
{
//! Interface for a class that provides a timestamp
class TimeStampProvider
{
public:
	//! Retrieve timestamp (e.g. UNIX timestamp in seconds)
	virtual double getTimestamp() = 0;
	//! Retrieve relative timestamp to start
	virtual double getSessionDuration() = 0;

	//! Initializes duration count, can be called once
	virtual void initializeDuration() = 0;

	virtual double getSessionStart() = 0;
};
} // namespace mandeye_utils