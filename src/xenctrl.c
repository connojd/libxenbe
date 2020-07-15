#include <xenctrl.h>

// libxc wrappers
xc_interface *xc_interface_open(void *logger,
                                void *dombuild_logger,
                                unsigned open_flags)
{
    xc_interface *xc;
    DWORD rc;

    rc = XcOpen(NULL, &xc);

    return xc;
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
    return NULL;
}

int xenevtchn_close(xenevtchn_handle *xce)
{
    return -1;
}

int xenevtchn_notify(xenevtchn_handle *xce, evtchn_port_t port)
{
    return -1;
}

xenevtchn_port_or_error_t
xenevtchn_bind_interdomain(xenevtchn_handle *xce, uint32_t domid,
                           evtchn_port_t remote_port)
{
    return 0;
}

int xenevtchn_fd(xenevtchn_handle *xce)
{
    return -1;
}

xenevtchn_port_or_error_t
xenevtchn_pending(xenevtchn_handle *xce)
{
    return 0;
}

int xenevtchn_unmask(xenevtchn_handle *xce, evtchn_port_t port)
{
    return -1;
}

int xenevtchn_unbind(xenevtchn_handle *xce, evtchn_port_t port)
{
    return -1;
}

// xenstore wrappers
xs_handle *
xs_open(unsigned long flags)
{
    return NULL;
}

void xs_close(xs_handle *xsh /* NULL ok */) {

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
    return NULL;
}

bool xs_write(xs_handle *h, uint32_t t,
              const char *path, const void *data, unsigned int len)
{
    return false;
}

bool xs_rm(xs_handle *h, uint32_t t,
           const char *path)
{
    return false;
}

bool xs_watch(xs_handle *h, const char *path, const char *token)
{
    return false;
}

bool xs_unwatch(xs_handle *h, const char *path, const char *token)
{
    return false;
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
    return NULL;
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
    return NULL;
}

int
xengnttab_unmap(void *xgt, void *start_address, uint32_t count)
{
    return 0;
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
