extern "C" {
#include <xenctrl.h>
}

static PXENCONTROL_CONTEXT xc = NULL;

#include <unordered_map>

std::unordered_map<std::string, void *> gWatchHandles;
std::unordered_map<void *, std::string> gWatchHandlesReverse;

void addWatchHandle(const char *path, void *handle)
{
	std::string s(path);
	gWatchHandles.insert({ s, handle });
    gWatchHandlesReverse.insert({ handle, s });
	return;
}

void *getWatchHandle(const char *path)
{
	std::unordered_map<std::string, void *>::const_iterator watch = gWatchHandles.find(std::string(path));
	if (watch != gWatchHandles.end()) {
		return watch->second;
	}

	return NULL;
}

void removeWatchHandle(const char *path)
{
    void *ptr = getWatchHandle(path);
	gWatchHandles.erase(std::string(path));
    gWatchHandlesReverse.erase(ptr);
}

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
    return (int)XcEvtchnNotify(xc, port);
}

xenevtchn_port_or_error_t
xenevtchn_bind_interdomain(xenevtchn_handle *xce, uint32_t domid,
                           evtchn_port_t remote_port)
{
    HANDLE event = CreateEvent(NULL, FALSE, FALSE, TEXT("EVTCHNBIND"));
    ULONG localPort;
    return XcEvtchnBindInterdomain(xc, domid, remote_port, event, false, &localPort);
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
    return XcEvtchnUnmask(xc, port);
}

int xenevtchn_unbind(xenevtchn_handle *xce, evtchn_port_t port)
{
    return -1;
}

// xenstore wrappers
xs_handle *
xs_open(unsigned long flags)
{
	DWORD rc = 0;
	if (!xc) {
		rc = XcOpen(NULL, &xc);
	}
    return (xs_handle *)xc;
}

void xs_close(xs_handle *xsh /* NULL ok */) {
    (void)xsh;
    return;
}

char *xs_get_domain_path(xs_handle *h, unsigned int domid)
{

    CHAR *value = (CHAR*)malloc(4096);
    memset(value, 0x00, 4096);
    snprintf(value, 4096, "/local/domain/%d", domid);
    return value;
}

char **xs_directory(xs_handle *h, uint32_t t,
                    const char *path, unsigned int *num)
{
    (void)t;
	PCHAR *buf = NULL;
	PCHAR *start = NULL;
	int count = 0;
    PCHAR buf2 = (PCHAR)malloc(4096);
    memset(buf2, 0x00, 4096);
    *num = 0;
    DWORD rc = XcStoreDirectory(xc, (PCHAR) path, 4096, buf2);

    if(!rc) {
        for(PCHAR p = buf2;; p++) {
            if(p[0] != 0x00 && p[1] == 0x00) {
                count++;
            }

            if (p[0] == 0x00 && p[1] == 0x00) {
                break;
            }
        }

        buf = start = (PCHAR *)malloc(sizeof(PCHAR *)*count);
        memset(buf, 0x00, sizeof(PCHAR *)*count);

        *buf++ = buf2;
        for (PCHAR p = buf2;;p++) {
            if (p[0] == 0x00 && p[1] != 0x00) {
                *buf++ = (PCHAR)(p + 1);
            }

            if (p[0] == 0x00 && p[1] == 0x00) {
                break;
            }
        }
    }

    *num = count;

    return start;
}

void *xs_read(xs_handle *h, uint32_t t,
              const char *path, unsigned int *len)
{
    CHAR *value = (CHAR*)malloc(4096);
	memset(value, 0x00, 4096);
    DWORD rc = XcStoreRead(xc, (PCHAR)path, (DWORD)4096, (PCHAR)value);
	errno = rc;
    return value;
}

bool xs_write(xs_handle *h, uint32_t t,
              const char *path, const void *data, unsigned int len)
{
    DWORD rc = XcStoreWrite(xc, (PCHAR)path, (PCHAR)data);
    return rc == 0;
}

bool xs_rm(xs_handle *h, uint32_t t,
           const char *path)
{
    DWORD rc = XcStoreRemove(xc, (PCHAR)path);
    return rc == 0;
}

bool xs_watch(xs_handle *h, const char *path, const char *token)
{
    HANDLE event = CreateEvent(NULL, FALSE, FALSE, TEXT("XSWATCH"));
    PVOID handle;
    DWORD rc = XcStoreAddWatch(xc, (PCHAR)path, event, &handle);

    addWatchHandle(path, handle);

    return rc == 0;
}

bool xs_unwatch(xs_handle *h, const char *path, const char *token)
{
    PVOID handle = getWatchHandle(path);

    DWORD rc = XcStoreRemoveWatch(xc, handle);

    removeWatchHandle(path);

    return rc == 0;
}

char **xs_read_watch(xs_handle *h, unsigned int *num)
{
    *num = 2;
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
    DWORD rc = XcGnttabMapForeignPages(xc, domid, count, (PULONG)refs, 0, 0, (XENIFACE_GNTTAB_PAGE_FLAGS)prot, &address);
    return address;
}

int
xengnttab_unmap(void *xgt, void *start_address, uint32_t count)
{
    (void)count;
    return XcGnttabUnmapForeignPages(xc, start_address);
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
