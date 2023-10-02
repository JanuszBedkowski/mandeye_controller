#pragma once

namespace mandeye_utils
{
//! Interface for a class that provides a timestamp
class TimeStampProvider
{
public:
	virtual double getTimestamp() = 0;
};
} // namespace mandeye_utils