#ifndef XENCTRL__H
#define XENCTRL__H
#ifdef _WIN32
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <xencontrol.h>
typedef struct _XENCONTROL_CONTEXT xc_interface;

#include <store_interface.h>
typedef uint32_t xs_transaction_t;
typedef unsigned short domid_t;
typedef PXENCONTROL_CONTEXT *xs_handle;
#define XBT_NULL 0

#include <evtchn_interface.h>
typedef ULONG xenevtchn_port_or_error_t;
typedef ULONG evtchn_port_t;
typedef HANDLE xenevtchn_handle;

#include <gnttab_interface.h>
typedef uint32_t grant_ref_t;
typedef HANDLE xengnttab_handle;
// From sys/mman.h
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define PROT_NONE 0x0
#define XC_PAGE_SIZE 4096
#ifdef _WIN32
#define XENDLL __declspec(dllexport)
#else
#define XENDLL
#endif

#define XEN_DOMINF_running 0x1
struct xc_domaininfo_t {
  uint32_t flags;
  domid_t domain;
};

// libxc wrappers
xc_interface *xc_interface_open(void *logger, void *dombuild_logger, unsigned open_flags);
int xc_interface_close(xc_interface *xc);
int xc_domain_getinfolist(xc_interface *xch, uint32_t first_domain, unsigned int max_domains, void *info);
// evtchn wrappers
xenevtchn_handle *xenevtchn_open(void *logger,
                                 unsigned open_flags);
int xenevtchn_close(xenevtchn_handle *xce);
int xenevtchn_notify(xenevtchn_handle *xce, evtchn_port_t port);
xenevtchn_port_or_error_t xenevtchn_bind_interdomain(xenevtchn_handle *xce, uint32_t domid, evtchn_port_t remote_port);
int xenevtchn_fd(xenevtchn_handle *xce);
xenevtchn_port_or_error_t xenevtchn_pending(xenevtchn_handle *xce);
int xenevtchn_unmask(xenevtchn_handle *xce, evtchn_port_t port);
int xenevtchn_unbind(xenevtchn_handle *xce, evtchn_port_t port);

// xenstore wrappers
enum xs_watch_type
  {
   XS_WATCH_PATH = 0,
   XS_WATCH_TOKEN
  };

xs_handle *xs_open(unsigned long flags);
void xs_close(xs_handle *xsh);
char *xs_get_domain_path(xs_handle *h, unsigned int domid);
char **xs_directory(xs_handle *h, uint32_t t, const char *path, unsigned int *num);
void *xs_read(xs_handle *h, uint32_t t, const char *path, unsigned int *len);
bool xs_write(xs_handle *h, uint32_t t, const char *path, const void *data, unsigned int len);
bool xs_rm(xs_handle *h, uint32_t t, const char *path);
bool xs_watch(xs_handle *h, const char *path, const char *token);
bool xs_unwatch(xs_handle *h, const char *path, const char *token);
char **xs_read_watch(xs_handle *h, unsigned int *num);
int xs_fileno(xs_handle *h);

// gnttab wrappers
xengnttab_handle *xengnttab_open(void *logger, unsigned open_flags);
int xengnttab_close(void *xgt);
void *xengnttab_map_domain_grant_refs(void *xgt,
				      uint32_t count,
				      uint32_t domid,
				      grant_ref_t *refs,
				      int prot);
int xengnttab_unmap(void *xgt, void *start_address, uint32_t count);

#endif // _WIN32
#endif // XENCTRL__H

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
