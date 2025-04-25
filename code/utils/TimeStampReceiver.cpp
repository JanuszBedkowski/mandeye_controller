#include "TimeStampReceiver.h"
#include "TimeStampProvider.h"
namespace mandeye_utils
{

void TimeStampReceiver::SetTimeStampProvider(std::shared_ptr<TimeStampProvider> timeStampProvider)
{
	m_timeStampProvider = timeStampProvider;
}

double TimeStampReceiver::GetTimeStamp()
{
	if (m_timeStampProvider)
	{
		return m_timeStampProvider->getTimestamp();
	}
	return 0.0;
}


double TimeStampReceiver::GetSessionDuration() {
	if (m_timeStampProvider) {
		return m_timeStampProvider->getSessionDuration();
	}
	return 0.0;
}
double TimeStampReceiver::GetSessionStart() {
	if (m_timeStampProvider) {
		return m_timeStampProvider->getSessionStart();
	}
	return 0.0;
}

}

