/*
 *  Xen backend base class
 *
 *  Based on
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

#ifndef XENBE_BACKENDBASE_HPP_
#define XENBE_BACKENDBASE_HPP_

#include <atomic>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "Exception.hpp"
#include "FrontendHandlerBase.hpp"
#include "XenStore.hpp"
#include "XenStat.hpp"
#include "Log.hpp"

namespace XenBackend {

/***************************************************************************//**
 * @defgroup backend Backend
 * Contains classes and primitives to create the full featured Xen backend.
 ******************************************************************************/

/***************************************************************************//**
 * Exception generated by BackendBase.
 * @ingroup backend
 ******************************************************************************/
class XENDLL BackendException : public Exception
{
	using Exception::Exception;
};

/***************************************************************************//**
 * Base class for a backend implementation.
 *
 * The main functionality is detecting a new frontend and deleting
 * terminated frontends.
 *
 * This class checks Xen store to detect new frontends.
 *
 * Once the new frontend is detected it calls onNewFrontend() method.
 * This method should be overridden by the custom instance of BackendBase class.
 * It is expected that in this method a new instance of FrontendHandlerBase
 * class will be created and added with addFrontendHandler() method.
 * Adding the frontend handler is required to allow the backend class deleting
 * terminated frontends.
 *
 * The client should create a class inherited from BackendBase and implement
 * onNewFrontend() method.
 *
 * Example of the client backend class:
 *
 * @snippet ExampleBackend.hpp ExampleBackend
 *
 * onNewFrontend() example:
 *
 * @snippet ExampleBackend.cpp onNewFrontend
 *
 * The client may change the new frontend detection algorithm. For this
 * reason it may override getNewFrontend() method.
 *
 * When the backend instance is created, it should be started by calling start()
 * method. The backend will process frontends till stop() method is called.
 *
 * @snippet ExampleBackend.cpp main
 *
 * @ingroup backend
 ******************************************************************************/
class XENDLL BackendBase
{
public:
	/**
	 * @param[in] name       optional backend name
	 * @param[in] deviceName device name
	 * @param[in] domId      domain id
	 */
	BackendBase(const std::string& name, const std::string& deviceName, bool wait = false);
	virtual ~BackendBase();

	/**
	 * Starts backend.
	 */
	void start();

	/**
	 * Stops backend.
	 */
	void stop();

	/**
	 * Waits for backend is finished.
	 */
	void waitForFinish();

	/**
	 * Returns backend device name
	 */
	const std::string& getDeviceName() const { return mDeviceName; }

	/**
	 * Returns domain id
	 */
	domid_t getDomId() const { return mDomId; }

protected:

	XenStore mXenStore;

	/**
	 * Is called when new frontend detected.
	 * Basically the client should create
	 * the instance of FrontendHandlerBase class and pass it to
	 * addFrontendHandler().
	 * @param[in] domId domain id
	 * @param[in] devId device id
	 */
	virtual void onNewFrontend(domid_t domId, uint16_t devId) = 0;

	/**
	 * Adds new frontend handler
	 * @param[in] frontendHandler frontend instance
	 */
	void addFrontendHandler(FrontendHandlerPtr frontendHandler);

private:

	domid_t mDomId;
	std::string mDeviceName;
	std::string mFrontendsPath;
	std::list<domid_t> mDomainList;
	std::list<FrontendHandlerPtr> mFrontendHandlers;

	Log mLog;

	void domainListChanged(const std::string& path);
	void deviceListChanged(const std::string& path, domid_t domId);
	void frontendPathChanged(const std::string& path, domid_t domId,
							 uint16_t devId);
	FrontendHandlerPtr getFrontendHandler(domid_t domId, uint16_t devId);
	void onError(const std::exception& e);
};

}

#endif /* XENBE_BACKENDBASE_HPP_ */
