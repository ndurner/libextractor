#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>
#include <pthread.h>

static pthread_mutex_t intlemu_lock;
static CFMutableDictionaryRef intlemu_dict;

static void intlemu_cstring_release(CFAllocatorRef allocator, const void *value)
{
	//printf("intlemu_cstring_release: %p\n", value);
	free((void *)value);
}

void __attribute__ ((constructor)) intlemu_init_() {
        //printf("intlemu: init\n");
	CFDictionaryValueCallBacks cstring_value_callbacks =
		{
			0, /* version */
			NULL, /* retain callback */
			&intlemu_cstring_release, /* release callback */
			NULL, /* copy description */
			NULL /* equal */
		};
	pthread_mutex_init(&intlemu_lock, NULL);
	
	intlemu_dict = CFDictionaryCreateMutable(
		kCFAllocatorDefault, 
		0, 
		&kCFCopyStringDictionaryKeyCallBacks,
		&cstring_value_callbacks);
	if (intlemu_dict == NULL) {
		//printf("Error creating dictionary\n");
		return;
	}
}

void __attribute__ ((destructor)) intlemu_fini_() {
        //printf("intlemu: fini\n");
	CFRelease(intlemu_dict);

	pthread_mutex_destroy(&intlemu_lock);
}

char * intlemu_bgettext (CFBundleRef bundle, const char *msgid)
{
	CFStringRef key;
	const char *value;
	CFStringRef s;
	CFRange r;
	CFIndex len;
	CFIndex clen;
	char *buf;

	if (msgid == NULL)
		return NULL;

	key = CFStringCreateWithBytes(
		kCFAllocatorDefault, 
		(const UInt8 *)msgid,
		(CFIndex)strlen(msgid),
		kCFStringEncodingUTF8,
		false);
	
	pthread_mutex_lock(&intlemu_lock);
	value = (char *)CFDictionaryGetValue(intlemu_dict, key);
	pthread_mutex_unlock(&intlemu_lock);
	//printf("CFDictionaryGetValue: [%s]\n", value);
	if (value != NULL) {
		CFRelease(key);
		return (char *)value;
	}

	/* no cached translaation, so, find one from the bundle */
	s = CFBundleCopyLocalizedString(
		bundle,
		key,
		NULL,
		NULL);
	if (s == key) {
		//printf("no translation found\n");
		CFRelease(key);
		return (char *)msgid;
	}
	/* get the length in bytes */
	r.location = 0;
	r.length = CFStringGetLength(s);
	len = 0;
	clen = CFStringGetBytes(
		s,
		r, 
		kCFStringEncodingUTF8,
		0,
		false,
		NULL,
		0,
		&len);
	buf = NULL;
	if (clen == r.length) {
		//printf("allocate dictionary value: %d\n", len+1);
		buf = malloc(len + 1);
	}
				
	if (buf == NULL) {
		CFRelease(s);
		CFRelease(key);
		return (char *)msgid;
	}

	clen = CFStringGetBytes(
		s,
		r, 
		kCFStringEncodingUTF8,
		0,
		false,
		(UInt8 *)buf,
		len,
		&len);
	buf[len] = '\0';
	if (clen == r.length) {
		pthread_mutex_lock(&intlemu_lock);
		CFDictionaryAddValue(intlemu_dict, key, buf);
		pthread_mutex_unlock(&intlemu_lock);
		value = buf;
	}
	else {
		free(buf);
		value = msgid;
	}

	CFRelease(s);

	CFRelease(key);

	return (char *)value;
}

