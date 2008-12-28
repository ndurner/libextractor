/* Mac OS X CoreFoundation libintl emulator */

#ifndef LIBINTLEMU_H
#define LIBINTLEMU_H

#include <CoreFoundation/CoreFoundation.h>

#define gettext(msgid) \
	intlemu_bgettext(CFBundleGetMainBundle(), msgid)

#define dgettext(domainname, msgid) \
	intlemu_bgettext(CFBundleGetBundleWithIdentifier(CFSTR(domainname)), msgid)

#define gettext_noop(s) s

extern char * intlemu_bgettext (CFBundleRef bundle, const char *msgid);

#endif
