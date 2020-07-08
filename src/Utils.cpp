/*
 *  Backend utils
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Copyright (C) 2016 EPAM Systems Inc.
 */

#include "Utils.hpp"

#include <cstring>
#include <vector>

#include "Exception.hpp"
#include "Version.hpp"

using std::chrono::milliseconds;
using std::cv_status;
using std::function;
using std::lock_guard;
using std::mutex;
using std::string;
using std::thread;
using std::to_string;
using std::vector;
using std::unique_lock;

namespace XenBackend {

/*******************************************************************************
 * Utils
 ******************************************************************************/

string Utils::logDomId(domid_t domId, uint16_t devId)
{
	return string("Dom(" + to_string(domId) + "/" + to_string(devId) + ") ");
}

string Utils::logState(xenbus_state state)
{
	static const vector<string> strStates = {"Unknown", "Initializing",
											 "InitWait", "Initialized",
											 "Connected",
											 "Closing", "Closed",
											 "Reconfiguring", "Reconfigured"};

	if (state >= strStates.size() || state < 0)
	{
		return "Error!!!";
	}
	else
	{
		return "[" + strStates[state] + "]";
	}
}

string Utils::getVersion()
{
	return VERSION;
}

#ifndef _WIN32
/*******************************************************************************
 * UnixPollFd
 ******************************************************************************/
UnixPollFd::UnixPollFd(int fd, short int events)
{
	try
	{
		init(fd, events);
	}
	catch(const std::exception& e)
	{
		release();

		throw;
	}
}

UnixPollFd::~UnixPollFd()
{
	release();
}

bool UnixPollFd::poll()
{
	mFds[PollIndex::FILE].revents = 0;
	mFds[PollIndex::PIPE].revents = 0;

	if (::poll(mFds, 2, -1) < 0)
	{
		if (errno != EINTR)
		{
			throw Exception("Error polling files", errno);
		}
	}

	if (mFds[PollIndex::PIPE].revents & POLLIN)
	{
		uint8_t data;

		if (read(mFds[PollIndex::PIPE].fd, &data, sizeof(data)) < 0)
		{
			throw Exception("Error reading pipe", errno);
		}

		return false;
	}

	if (mFds[PollIndex::FILE].revents & (~mFds[PollIndex::FILE].events))
	{
		if (mFds[PollIndex::FILE].revents & POLLERR)
		{
			throw Exception("Poll error condition", EPERM);
		}

		if (mFds[PollIndex::FILE].revents & POLLHUP)
		{
			throw Exception("Poll hang up", EPERM);
		}

		if (mFds[PollIndex::FILE].revents & POLLNVAL)
		{
			throw Exception("Poll invalid request", EINVAL);
		}
	}

	return true;
}

void UnixPollFd::stop()
{
	uint8_t data = 0xFF;

	if (write(mPipeFds[PipeType::WRITE], &data, sizeof(data)) < 0)
	{
		throw Exception("Error writing pipe", errno);
	}
}

void UnixPollFd::init(int fd, short int events)
{
	mPipeFds[PipeType::READ] = -1;
	mPipeFds[PipeType::WRITE] = -1;

	if (pipe(mPipeFds) < 0)
	{
		throw Exception("Can't create pipe", errno);
	}

	mFds[PollIndex::FILE].fd = fd;
	mFds[PollIndex::FILE].events = events;

	mFds[PollIndex::PIPE].fd = mPipeFds[PipeType::READ];
	mFds[PollIndex::PIPE].events = POLLIN;
}

void UnixPollFd::release()
{
	if (mPipeFds[PipeType::READ] >= 0)
	{
		close(mPipeFds[PipeType::READ]);
	}

	if (mPipeFds[PipeType::WRITE] >= 0)
	{
		close(mPipeFds[PipeType::WRITE]);
	}
}

#else
/*******************************************************************************
 * WinPollFd
 ******************************************************************************/
WinPollFd::WinPollFd()
{
	try
	{
		//init(fd, events);
	}
	catch(const std::exception& e)
	{
		throw;
	}
}

WinPollFd::~WinPollFd()
{
}

bool WinPollFd::poll()
{
	/*
    DWORD   wait = WaitForMultipleObjectsEx(4, events, FALSE, INFINITE, TRUE);

    switch (wait) {
    case WAIT_OBJECT_0:
        ResetEvent(m_svc_stop);
        return false; // exit loop

    case WAIT_OBJECT_0+1:
        ResetEvent(m_xeniface.m_evt_shutdown);
        return !m_xeniface.CheckShutdown();

    case WAIT_OBJECT_0+2:
        ResetEvent(m_xeniface.m_evt_suspend);
        m_xeniface.CheckSuspend();
        return true; // continue loop

    case WAIT_OBJECT_0+3: {
        std::string mode;

        ResetEvent(m_xeniface.m_evt_slate_mode);
        if (m_xeniface.CheckSlateMode(&mode))
            m_conv.SetSlateMode(mode);

        return true; // continue loop
    }
    case WAIT_IO_COMPLETION:
    case WAIT_TIMEOUT:
        m_xeniface.CheckSuspend();
        return !m_xeniface.CheckShutdown();

    default:
        CXenAgent::Log("WaitForMultipleObjects failed (%08x)\n", wait);
        EventLog(EVENT_XENUSER_UNEXPECTED);
        return true; // continue loop
    }
	*/
    return true;
}

void WinPollFd::stop()
{

}
#endif
  
/*******************************************************************************
 * AsyncContext
 ******************************************************************************/

AsyncContext::AsyncContext() :
	mTerminate(false)
{
	mThread = thread(&AsyncContext::run, this);
}

AsyncContext::~AsyncContext()
{
	stop();
}

void AsyncContext::stop()
{
	{
		unique_lock<mutex> lock(mMutex);

		mTerminate = true;

		mCondVar.notify_all();
	}

	if (mThread.joinable())
	{
		mThread.join();
	}
}

void AsyncContext::call(AsyncCall f)
{
	unique_lock<mutex> lock(mMutex);

	mAsyncCalls.push_back(f);

	mCondVar.notify_all();
}

void AsyncContext::run()
{
	unique_lock<mutex> lock(mMutex);

	while(!mTerminate)
	{
		mCondVar.wait(lock, [this] { return mTerminate ||
									 !mAsyncCalls.empty(); });

		while(!mAsyncCalls.empty())
		{

			auto asyncCall = mAsyncCalls.front();

			lock.unlock();

			asyncCall();

			lock.lock();

			mAsyncCalls.pop_front();
		}
	}
}

/*******************************************************************************
 * Timer
 ******************************************************************************/

Timer::Timer(function<void()> callback, bool periodic) :
	mCallback(callback),
	mPeriodic(periodic),
	mTerminate(true)
{
}

Timer::~Timer()
{
	stop();
}

void Timer::start(milliseconds time)
{
	lock_guard<mutex> lock(mItfMutex);

	if (mTerminate)
	{
		mTime = time;

		mTerminate = false;

		mThread = thread(&Timer::run, this);
	}
	else
	{
		throw Exception("Timer is already started", EPERM);
	}
}

void Timer::stop()
{
	lock_guard<mutex> lock(mItfMutex);

	if (mPeriodic)
	{
		unique_lock<mutex> lock(mMutex);

		mTerminate = true;

		mCondVar.notify_all();
	}

	if (mThread.joinable())
	{
		mThread.join();
	}
}

void Timer::run()
{
	unique_lock<mutex> lock(mMutex);

	do
	{
		if (!mCondVar.wait_for(lock, mTime, [this]
							   { return static_cast<bool>(mTerminate); }))
		{
			if (mCallback)
			{
				mCallback();
			}
		}
	}
	while(!mTerminate && mPeriodic);
}

}
