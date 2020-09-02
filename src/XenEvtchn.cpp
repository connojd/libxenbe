/*
 *  Xen evtchn wrapper
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

#include "XenEvtchn.hpp"

#ifndef _WIN32
#include <poll.h>
#endif

using std::lock_guard;
using std::mutex;
using std::thread;
using std::to_string;

namespace XenBackend {

/*******************************************************************************
 * XenEvtchn
 ******************************************************************************/

XenEvtchn::XenEvtchn(domid_t domId, evtchn_port_t port, Callback callback,
					 ErrorCallback errorCallback) :
	mPort(-1),
	mHandle(nullptr),
	mCallback(callback),
	mErrorCallback(errorCallback),
	mStarted(false),
	mLog("XenEvtchn")
{
	try
	{
		init(domId, port);
	}
	catch(const std::exception& e)
	{
		release();

		throw;
	}
}

XenEvtchn::~XenEvtchn()
{
	stop();
	release();
}

/*******************************************************************************
 * Public
 ******************************************************************************/

void XenEvtchn::start()
{
	DLOG(mLog, DEBUG) << "Start event channel, port: " << mPort;

	if (mStarted)
	{
		throw XenEvtchnException("Event channel is already started", EPERM);
	}

	mStarted = true;

	mThread = thread(&XenEvtchn::eventThread, this);
}

void XenEvtchn::stop()
{
	if (!mStarted)
	{
		return;
	}

	DLOG(mLog, DEBUG) << "Stop event channel, port: " << mPort;
#ifndef _WIN32
	if (mPollFd)
	{
		mPollFd->stop();
	}
#endif
	if (mThread.joinable())
	{
		mThread.join();
	}

	mStarted = false;
}

void XenEvtchn::notify()
{
	DLOG(mLog, DEBUG) << "Notify event channel, port: " << mPort;

	if (xenevtchn_notify(mHandle, mPort) < 0)
	{
		throw XenEvtchnException("Can't notify event channel", errno);
	}
}

void XenEvtchn::setErrorCallback(ErrorCallback errorCallback)
{
	lock_guard<mutex> lock(mMutex);

	mErrorCallback = errorCallback;
}

/*******************************************************************************
 * Private
 ******************************************************************************/

void XenEvtchn::init(domid_t domId, evtchn_port_t port)
{
	mHandle = xenevtchn_open(nullptr, 0);

	if (!mHandle)
	{
		throw XenEvtchnException("Can't open event channel", errno);
	}
#ifndef _WIN32
	mPort = xenevtchn_bind_interdomain(mHandle, domId, port);
#else
	mEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	DWORD rc = XcEvtchnBindInterdomain((PXENCONTROL_CONTEXT)mHandle, domId, port, mEventHandle, FALSE, &mPort);
	SetEvent(mWatchThread);
#endif
	if (mPort == -1 || rc != 0)
	{
		throw XenEvtchnException("Can't bind event channel: " + to_string(port),
								 errno);
	}

#ifndef _WIN32
	mPollFd.reset(new UnixPollFd(xenevtchn_fd(mHandle), POLLIN));
#endif
	DLOG(mLog, INFO) << "Create event channel, dom: " << domId
					  << ", remote port: " << port << ", local port: "
					  << mPort;
}

void XenEvtchn::release()
{
	if (mPort != -1)
	{
		xenevtchn_unbind(mHandle, mPort);
	}

	if (mHandle)
	{
		xenevtchn_close(mHandle);

		DLOG(mLog, DEBUG) << "Delete event channel, port: " << mPort;
	}
}

void XenEvtchn::eventThread()
{
	try
	{
#ifndef _WIN32
		while(mCallback && mPollFd->poll())
		{
			auto port = xenevtchn_pending(mHandle);

			if (port < 0)
			{
				throw XenEvtchnException("Can't get pending port", errno);
			}

			if (xenevtchn_unmask(mHandle, port) < 0)
			{
				throw XenEvtchnException("Can't unmask event channel", errno);
			}

			if (port != mPort)
			{
				throw XenEvtchnException("Error port number: " +
										 to_string(port) + ", expected: " +
										 to_string(mPort), EINVAL);
			}

			DLOG(mLog, DEBUG) << "Event received, port: " << mPort;

			mCallback();
		}
#else
		bool watchLoop = true;
		LOG(mLog, INFO) << "1";
		DWORD haveWatches = WaitForSingleObject(mWatchThread, INFINITE);
		LOG(mLog, INFO) << "2";
		if (haveWatches == WAIT_OBJECT_0) {
			ResetEvent(mWatchThread);
			while (watchLoop) {
				LOG(mLog, INFO) << "3";
				DWORD wait = WaitForSingleObject(mEventHandle, INFINITE);
				LOG(mLog, INFO) << "4";
				if (wait == WAIT_OBJECT_0) {
					LOG(mLog, INFO) << "5";
					XcEvtchnUnmask((PXENCONTROL_CONTEXT)mHandle, mPort);
					LOG(mLog, INFO) << "6";
					if (mCallback) {
						LOG(mLog, INFO) << "7";
						mCallback();
						LOG(mLog, INFO) << "8";
					}
				}
				LOG(mLog, INFO) << "9";
				ResetEvent(mEventHandle);
				LOG(mLog, INFO) << "10";
			}
		}
#endif
	}
	catch(const std::exception& e)
	{
		lock_guard<mutex> lock(mMutex);

		if (mErrorCallback)
		{
			mErrorCallback(e);
		}
		else
		{
			LOG(mLog, ERROR) << e.what();
		}
	}
}

}

