/*
 *  Xen Store wrapper
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
#include "XenStore.hpp"

#ifndef _WIN32
#include <poll.h>
#endif
using std::lock_guard;
using std::mutex;
using std::string;
using std::thread;
using std::to_string;
using std::vector;

namespace XenBackend {

/*******************************************************************************
 * XenStore
 ******************************************************************************/

XenStore::XenStore(ErrorCallback errorCallback) :
	mXsHandle(nullptr),
	mErrorCallback(errorCallback),
	mStarted(false),
	mLog("XenStore")
{
	try
	{
		init();
	}
	catch(const std::exception& e)
	{
		release();

		throw;
	}
}

XenStore::~XenStore()
{
	clearWatches();

	stop();

	release();
}

/*******************************************************************************
 * Public
 ******************************************************************************/

string XenStore::getDomainPath(domid_t domId)
{
	auto domPath = xs_get_domain_path(mXsHandle, domId);

	if (!domPath)
	{
		throw XenStoreException("Can't get domain path", errno);
	}

	string result(domPath);

	free(domPath);

	return result;
}

int XenStore::readInt(const string& path)
{
	int result = stoi(readString(path));

	LOG(mLog, DEBUG) << "Read int " << path << " : " << result;

	return result;
}

unsigned int XenStore::readUint(const string& path)
{
	unsigned int result = stoul(readString(path));

	LOG(mLog, DEBUG) << "Read unsigned int " << path << " : " << result;

	return result;
}

string XenStore::readString(const string& path)
{
	unsigned length;
	auto pData = static_cast<char*>(xs_read(mXsHandle, XBT_NULL, path.c_str(),
											&length));

	if (!pData)
	{
		throw XenStoreException("Can't read from: " + path, errno);
	}

	string result(pData);

	free(pData);

	LOG(mLog, INFO) << "Read string " << path << " : " << result;

	return result;
}

void XenStore::writeInt(const string& path, int value)
{
	auto strValue = to_string(value);

	LOG(mLog, DEBUG) << "Write int " << path << " : " << value;

	writeString(path, strValue);
}

void XenStore::writeUint(const string& path, unsigned int value)
{
	auto strValue = to_string(value);

	LOG(mLog, DEBUG) << "Write uint " << path << " : " << value;

	writeString(path, strValue);
}

void XenStore::writeString(const string& path, const string& value)
{
	LOG(mLog, DEBUG) << "Write string " << path << " : " << value;

	if (!xs_write(mXsHandle, XBT_NULL, path.c_str(), value.c_str(),
				  value.length()))
	{
		throw XenStoreException("Can't write value to " + path, errno);
	}
}

void XenStore::removePath(const string& path)
{
	LOG(mLog, DEBUG) << "Remove path " << path;

	if (!xs_rm(mXsHandle, XBT_NULL, path.c_str()))
	{
		throw XenStoreException("Can't remove path " + path, errno);
	}
}

vector<string> XenStore::readDirectory(const string& path)
{
	unsigned int num;
	auto items = xs_directory(mXsHandle, XBT_NULL, path.c_str(), &num);

	if (items && num)
	{
		vector<string> result;

		result.reserve(num);

		for(unsigned int i = 0; i < num; i++)
		{
			result.push_back(items[i]);
		}

		free(items[0]);
		free(items);

		return result;
	}
	else {
		LOG(mLog, INFO) << "No domains!";
	}

	return vector<string>();
}

bool XenStore::checkIfExist(const string& path)
{
	unsigned length;
	auto pData = xs_read(mXsHandle, XBT_NULL, path.c_str(), &length);

	if (!pData)
	{
		return false;
	}

	free(pData);

	return true;
}

void XenStore::setWatch(const string& path, WatchCallback callback)
{
	lock_guard<mutex> lock(mMutex);

#ifndef _WIN32
	if (!xs_watch(mXsHandle, path.c_str(), path.c_str()))
	{
		throw XenStoreException("Can't set xs watch for " + path, errno);
	}

	mWatches[path] = callback;
#else
        if (mWatchThreads.count(path) != 0) {
            LOG(mLog, INFO) << "Path " << path << " already has watch set...bailing";
            return;
        }

        const HANDLE watch_event = CreateEvent(NULL,  /* use default attributes */
                                               TRUE,  /* manual-reset event */
                                               FALSE, /* initial state is clear */
                                               NULL); /* an event has no name */
        if (watch_event == NULL) {
            LOG(mLog, ERROR) << "CreateEvent failed, LastError=0x" << std::hex
                             << GetLastError() << ". setWatch failed "
                             << "at path " << path;
            return;
        }

	PVOID watch_handle = NULL;
	std::string str = path;
        DWORD rc = XcStoreAddWatch((PXENCONTROL_CONTEXT)mXsHandle,
                                   (PCHAR)str.c_str(),
                                   watch_event,
                                   &watch_handle);
	if (rc) {
		throw XenStoreException("Can't set xs watch for " + path, rc);
	}

        auto ret = mWatchThreads.try_emplace(str,
                                             str,
                                             watch_handle,
                                             watch_event,
                                             callback);
        if (!ret.second) {
            LOG(mLog, ERROR) << "Failed to add watch thread at path " << path;

            if (XcStoreRemoveWatch((PXENCONTROL_CONTEXT)mXsHandle, watch_handle)) {
                LOG(mLog, ERROR) << "Failed to remove watch at path " << path;
            }

            CloseHandle(watch_event);
            return;
        }

        LOG(mLog, INFO) << "Launching watch thread at path: " << path;
        ret.first->second.launch();
#endif
}

void XenStore::clearWatch(const string& path)
{
	lock_guard<mutex> lock(mMutex);

	LOG(mLog, INFO) << "Clear watch: " << path;
#ifndef _WIN32
	if (!xs_unwatch(mXsHandle, path.c_str(), path.c_str()))
	{
		LOG(mLog, ERROR) << "Failed to clear watch: " << path;
	}

	mWatches.erase(path);
#else
        auto itr = mWatchThreads.find(path);
        if (itr == mWatchThreads.end()) {
            LOG(mLog, INFO) << "clearWatch: no watch found at path " << path;
            return;
        }

        auto thread = &itr->second;
	auto rc = XcStoreRemoveWatch((PXENCONTROL_CONTEXT)mXsHandle, thread->mXcHandle);
	if (rc) {
		throw XenStoreException("Failed to clear watch:" + path, rc);
	}

        if (thread->is_joinable()) {
            thread->alert();
            thread->join();
        }

        CloseHandle(thread->mEvent);
        mWatchThreads.erase(path);
#endif
}

void XenStore::clearWatches()
{
	lock_guard<mutex> lock(mMutex);

#ifndef _WIN32
	if (mWatches.size())
	{
		LOG(mLog, INFO) << "Clear watches";

		for (auto watch : mWatches)
		{
                        auto path = watch.first;
			if (!xs_unwatch(mXsHandle, path.c_str(), path.c_str()))
			{
				LOG(mLog, ERROR) << "Failed to clear watch: " << path;
			}
		}

		mWatches.clear();
	}
#else
        for (auto &watch : mWatchThreads) {
            auto thread = &watch.second;
	    auto rc = XcStoreRemoveWatch((PXENCONTROL_CONTEXT)mXsHandle, thread->mXcHandle);
	    if (rc) {
		throw XenStoreException("Failed to clear watch:" + thread->mPath, rc);
	    }

            if (thread->is_joinable()) {
                thread->alert();
                thread->join();
            }

            CloseHandle(thread->mEvent);
        }

        mWatchThreads.clear();
#endif
}

void XenStore::start()
{
	LOG(mLog, INFO) << "Start";

	if (mStarted)
	{
		throw XenStoreException("XenStore is already started", errno);
	}

	mStarted = true;

#ifndef _WIN32
	mThread = thread(&XenStore::watchesThread, this);
#endif
}

void XenStore::stop()
{
	if (!mStarted)
	{
		return;
	}

	DLOG(mLog, DEBUG) << "Stop";

#ifndef _WIN32
	if (mPollFd)
	{
		mPollFd->stop();
	}

        if (mThread.joinable())
        {
                mThread.join();
        }
#else
	LOG(mLog, INFO) << "quitting...";
#endif

	mStarted = false;
}

/*******************************************************************************
 * Private
 ******************************************************************************/

void XenStore::init()
{
	mXsHandle = xs_open(0);

	if (!mXsHandle)
	{
		throw XenStoreException("Can't open xs daemon", errno);
	}

#ifndef _WIN32
	mPollFd.reset(new UnixPollFd(xs_fileno(mXsHandle), POLLIN));
#endif
	LOG(mLog, DEBUG) << "Create xen store";
}

void XenStore::release()
{
	if (mXsHandle)
	{
		xs_close(mXsHandle);

		LOG(mLog, DEBUG) << "Delete xen store";
	}
}

#ifndef _WIN32
string XenStore::readXsWatch(string& token)
{
	string path;
	unsigned int num;

	auto result = xs_read_watch(mXsHandle, &num);

	if (result)
	{
		path = result[XS_WATCH_PATH];
		token = result[XS_WATCH_TOKEN];

		free(result);
	}

	return path;
}

XenStore::WatchCallback XenStore::getWatchCallback(const string& path)
{
	lock_guard<mutex> lock(mMutex);

	WatchCallback callback = nullptr;
/*
	auto result = find_if(mWatches.begin(), mWatches.end(),
						  [&path] (const pair<string, WatchCallback> &val)
						  { return path.compare(0, val.first.length(),
						                           val.first) == 0; });
*/
	auto result = mWatches.find(path);

	if (result != mWatches.end())
	{
		callback = result->second;
	}

	return callback;
}

void XenStore::watchesThread()
{
	try
	{
		while(mPollFd->poll())
		{
			string token;

			auto path = readXsWatch(token);

			if (!token.empty())
			{
				auto callback = getWatchCallback(token);

				if (callback)
				{
					LOG(mLog, DEBUG) << "Watch triggered: " << token;

					callback(token);
				}
			}
		}
        }
	catch(const std::exception& e)
	{
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
#endif

}
