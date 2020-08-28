#include <xenctrl.h>
#include <string.h>
#include <unordered_map>

std::unordered_map<std::string, PVOID> gWatchHandles;

void addWatchHandle(const char *path, PVOID handle)
{
    gWatchHandles.insert(std::string(path), handle);
    return;
}

void removeWatchHandle(const char *path)
{
    gWatchHandles.erase(std::string(path));
}

PVOID getWatchHandle(const char *path)
{
    std::unordered_map<std::string, PVOID>::const_iterator watch = gWatchHandles.find(std::string(path));
    if (watch != gWatchHandles.end()) {
        return watch->second();
    }

    return NULL;
}

extern "C" {
static PXENCONTROL_CONTEXT xc;

// libxc wrappers
xc_interface *xc_interface_open(void *logger,
                                void *dombuild_logger,
                                unsigned open_flags)
{
    DWORD rc;

    rc = XcOpen(NULL, &xc);
    return (xc_interface*)xc;
}

int
xc_interface_close(xc_interface *xc)
{
    XcClose(xc);
    return 0;
}

int
xc_domain_getinfolist(xc_interface *xch, uint32_t first_domain, unsigned int max_domains, void *info)
{
    return 0;
}

// evtchn wrappers
xenevtchn_handle *xenevtchn_open(void *logger,
                                 unsigned open_flags)
{
    return (xenevtchn_handle *)xc;
}

int xenevtchn_close(xenevtchn_handle *xce)
{
    return 0;
}

int xenevtchn_notify(xenevtchn_handle *xce, evtchn_port_t port)
{
    return (int)XcEvtchnNotify(xce, port);
}

xenevtchn_port_or_error_t
xenevtchn_bind_interdomain(xenevtchn_handle *xce, uint32_t domid,
                           evtchn_port_t remote_port)
{
    HANDLE event = CreateEvent(NULL, FALSE, FALSE, TEXT("EVTCHNBIND"));
    ULONG localPort;
    return XcEvtchnBindInterdomain(xce, domid, remote_port, event, false, &localPort);
}

int xenevtchn_fd(xenevtchn_handle *xce)
{
    return -1;
}

xenevtchn_port_or_error_t
xenevtchn_pending(xenevtchn_handle *xce)
{
    xenevtchn_port_or_error_t port = -1;



    return port;
}

int xenevtchn_unmask(xenevtchn_handle *xce, evtchn_port_t port)
{
    return XcEvtchnUnmask(xce, port);
}

int xenevtchn_unbind(xenevtchn_handle *xce, evtchn_port_t port)
{
    return -1;
}

// xenstore wrappers
xs_handle *
xs_open(unsigned long flags)
{
    return (xs_handle *)xc;
}

void xs_close(xs_handle *xsh /* NULL ok */) {
    (void)xsh;
    return;
}

char *xs_get_domain_path(xs_handle *h, unsigned int domid)
{
    return NULL;
}

char **xs_directory(xs_handle *h, uint32_t t,
                    const char *path, unsigned int *num)
{
    return NULL;
}

void *xs_read(xs_handle *h, uint32_t t,
              const char *path, unsigned int *len)
{
    CHAR *value = NULL;
    DWORD rc = XcStoreRead(h, path, len, value);
    return value;
}

bool xs_write(xs_handle *h, uint32_t t,
              const char *path, const void *data, unsigned int len)
{
    DWORD rc = XcStoreWrite(h, path, data);
    return rc == 0;
}

bool xs_rm(xs_handle *h, uint32_t t,
           const char *path)
{
    DWORD rc = XcStoreRemove(h, path);
    return rc == 0;
}

bool xs_watch(xs_handle *h, const char *path, const char *token)
{
    HANDLE event;
    PVOID handle;
    DWORD rc = XcStoreAddWatch(h, path, event, &handle);

    addWatchHandle(path, handle);

    return rc == 0;
}

bool xs_unwatch(xs_handle *h, const char *path, const char *token)
{
    PVOID handle = getWatchHandle(path);

    DWORD rc = XcStoreRemoveWatch(h, handle);

    removeWatchHandle(path);

    return rc == 0;
}

char **xs_read_watch(xs_handle *h, unsigned int *num)
{
    *num = 0;
    return NULL;
}

int xs_fileno(xs_handle *h)
{
    return -1;
}

// gnttab wrappers
xengnttab_handle *xengnttab_open(void *logger,
                                 unsigned open_flags)
{
    return (xengnttab_handle *)xc;
}

int xengnttab_close(void *xgt)
{
    return 0;
}

void *
xengnttab_map_domain_grant_refs(void *xgt,
                                uint32_t count,
                                uint32_t domid,
                                grant_ref_t *refs,
                                int prot)
{
    PVOID address = NULL;
    DWORD rc = XcGnttabMapForeignPages(xgt, domid, count, refs, 0, 0, prot, &address);
    return address;
}

int
xengnttab_unmap(void *xgt, void *start_address, uint32_t count)
{
    (void)count;
    return XcGnttabUnmapForeignPages(xgt, start_address);
}

}
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
