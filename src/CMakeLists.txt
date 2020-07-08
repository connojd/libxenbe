project(libxenbe)


################################################################################
# Includes
################################################################################

if(WITH_WIN)
        include(FetchContent)
        FetchContent_Declare(
                xen
                GIT_REPOSITORY https://xenbits.xen.org/git-http/xen.git
                GIT_TAG RELEASE-4.13.1
                GIT_SHALLOW ON
        )
        FetchContent_MakeAvailable(xen)

        set(XEN_SRC ${CMAKE_CURRENT_BINARY_DIR}/../_deps/xen-src)
        set(XEN_INCS ${CMAKE_CURRENT_BINARY_DIR}/global-include)

        file(MAKE_DIRECTORY global-include)
        file(MAKE_DIRECTORY global-include/tmp)
        file(REMOVE_RECURSE ${XEN_INCS}/xen)
        file(COPY ${XEN_SRC}/xen/include/public DESTINATION ${XEN_INCS}/tmp/ PATTERN "*")
        file(RENAME ${XEN_INCS}/tmp/public ${XEN_INCS}/xen)

	include_directories(${XEN_INCS})
	include_directories(${XENBUS_WINDOWS_INCLUDE_PATH})	
	include_directories(${XENIFACE_WINDOWS_INCLUDE_PATH})
endif()

################################################################################
# Sources
################################################################################

set(SOURCES
	xenctrl.c
	BackendBase.cpp
	FrontendHandlerBase.cpp
	RingBufferBase.cpp
	Utils.cpp
	XenCtrl.cpp
	XenEvtchn.cpp
	XenGnttab.cpp
	XenStat.cpp
	XenStore.cpp
)

################################################################################
# Libraries
################################################################################
if(WITH_WIN)
	link_directories(${XENIFACE_WINDOWS_LIB_PATH})
endif()



################################################################################
# Targets
################################################################################

add_library(xenbe SHARED ${SOURCES} ${XEN_HEADERS})

if(WITH_TEST)
	add_library(xenbemock SHARED ${SOURCES})
endif()

if(WITH_WIN)
	target_link_libraries(xenbe
		xencontrol
		xenbus
	)
else()
	target_link_libraries(xenbe
		xenstore
		xenevtchn
		xengnttab
		xenctrl
		pthread
	)
endif()

if(WITH_TEST)
	target_link_libraries(xenbemock
		xenmock
		pthread
	)
endif()

install(TARGETS xenbe LIBRARY DESTINATION lib)
