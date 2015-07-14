/* urifs - add remote file from different locations in one filesystem.
 *
 * Copyright (C) 2011 ravomavain
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 * Requirement: libfuse, libcurl, libxml2 and libcrypto (openssl)
 *
 * Compile with: gcc -g -o urifs urifs.c -Wall -ansi -W -std=c99 -D_GNU_SOURCE `pkg-config --cflags --libs libxml-2.0 fuse libcurl libcrypto`
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <curl/curl.h>
#include <pthread.h>
#include <openssl/crypto.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define MAX_ENTRIES	512
#define LOG_FILE	"/var/log/urifs.log"

typedef struct {
	char *uri;
	size_t size;
	struct curl_slist *header;
} uri_fd;

uri_fd ** opened_files = NULL;

char *source_xml;

FILE *debug_f = NULL;
FILE *error_f = NULL;

#define DEBUG(...) { if(debug_f) { fprintf(debug_f,"%s::%s:%d ",__FILE__,__func__,__LINE__);fprintf(debug_f,__VA_ARGS__); fprintf(debug_f,"\n"); fflush(debug_f); } }

static inline void xfree(void *p)
{
	if(p)
		free(p);
	p=NULL;
}

static pthread_mutex_t *lockarray;

static void lock_callback(int mode, int type, char *file, int line)
{
	(void)file;
	(void)line;
	if (mode & CRYPTO_LOCK) {
		pthread_mutex_lock(&(lockarray[type]));
	}
	else {
		pthread_mutex_unlock(&(lockarray[type]));
	}
}
 
static unsigned long thread_id(void)
{
	unsigned long ret;
 
	ret=(unsigned long)pthread_self();
	return(ret);
}
 
static void init_locks(void)
{
	int i;
 
	lockarray=(pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
	for (i=0; i<CRYPTO_num_locks(); i++) {
		pthread_mutex_init(&(lockarray[i]),NULL);
	}
 
	CRYPTO_set_id_callback((unsigned long (*)())thread_id);
	CRYPTO_set_locking_callback((void (*)())lock_callback);
}
 
static void kill_locks(void)
{
	int i;
 
	CRYPTO_set_locking_callback(NULL);
	for (i=0; i<CRYPTO_num_locks(); i++)
		pthread_mutex_destroy(&(lockarray[i]));
 
	OPENSSL_free(lockarray);
}


static char *xpath_from_path(const char *path)
{
	char *p;
	char *xpath = NULL;
	size_t len = strlen(path);
	char *tmp = (char*)malloc(len+1);
	if (!tmp)
		return NULL;
	strncpy(tmp, path, len+1);
	if (asprintf(&xpath, "/root") == -1)
	{
		xfree(tmp);
		return NULL;
	}
	p = strtok(tmp, "/");
	while( p != NULL )
	{
		char *new;
		if (asprintf(&new, "%s/""*[@name=\"%s\"]", xpath, p) == -1)
		{
			xfree(xpath);
			xfree(tmp);
			return NULL;
		}
		xfree(xpath);
		xpath = new;
		p = strtok(NULL, "/");
	}
	xfree(tmp);
	return xpath;
}

struct curl_buffer {
	char *data;
	size_t size;
	size_t read;
};

static size_t curl_get_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	DEBUG("args: void *contents = %p, size_t size = %lu, size_t nmemb = %lu, void *userp = %p", contents, size, nmemb, userp)
	size_t realsize = size * nmemb;
	struct curl_buffer *buffer = (struct curl_buffer *)userp;
	if(buffer->read + realsize > buffer->size)
		realsize = buffer->size - buffer->read;

	memcpy(buffer->data+buffer->read, contents, realsize);
	buffer->read += realsize;

	DEBUG("return: %lu", realsize)
	return realsize;
}

static void header_cmd(uri_fd *fd, const char *cmd)
{
	DEBUG("args: uri_fd *fd = %p, const char *cmd = \"%s\"", fd, cmd)
	FILE *fp = popen(cmd, "r");
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	if (fp != NULL)
	{
		while ((read = getline(&line, &len, fp)) > 0 && line)
		{
			line[read-1] = '\0';
			fd->header = curl_slist_append(fd->header,line);
			DEBUG("Added header \"%s\"", line)
		}
		xfree(line);
		pclose(fp);
	}
}

static int urifs_getattr(const char *path, struct stat *stbuf)
{
	DEBUG("args: const char *path = \"%s\", struct stat *stbuf = %p", path, stbuf)
	xmlChar *value;
	xmlNodeSetPtr nodes;
	xmlNodePtr node;
	xmlXPathObjectPtr xpathObj;

	char *xpath = xpath_from_path(path);
	if (!xpath)
	{
		DEBUG("return: -ENOENT(%d)", -ENOENT)
		return -ENOENT;
	}

	DEBUG("xpath: %s", xpath);

	xpathObj = xmlXPathEvalExpression((xmlChar*)xpath, fuse_get_context()->private_data);
	xfree(xpath);
	if(xpathObj == NULL)
	{
		DEBUG("return: -ENOENT(%d)", -ENOENT)
		return -ENOENT;
	}

	nodes = xpathObj->nodesetval;

	if (nodes && nodes->nodeNr > 0)
	{
		DEBUG("%d nodes found", nodes->nodeNr)
		node = nodes->nodeTab[0];

		if(strcmp((char*)node->name, "dir")==0 || strcmp((char*)node->name, "root")==0)
		{
			DEBUG("dir")
			stbuf->st_mode = S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
		}
		else
		{
			DEBUG("file")
			stbuf->st_mode = S_IFREG;
		}

		stbuf->st_ino = 0;
		stbuf->st_mode |= S_IRUSR | S_IRGRP | S_IROTH /*| S_IWUSR | S_IWGRP | S_IWOTH*/;
		stbuf->st_nlink = 0;
		stbuf->st_uid = 0;
		stbuf->st_gid = 0;
		stbuf->st_rdev = 0;
		stbuf->st_size = 1;
		stbuf->st_blksize = 1;
		stbuf->st_blocks = 1;
		stbuf->st_atime = 0;
		stbuf->st_mtime = 0;
		stbuf->st_ctime = 0;

		value = xmlGetProp(node, (xmlChar*)"size");
		if(value)
		{
			stbuf->st_blocks = stbuf->st_size = atoll((char*)value);
			xmlFree(value);
		}

		value = xmlGetProp(node, (xmlChar*)"uid");
		if(value)
		{
			stbuf->st_blocks = stbuf->st_size = atoll((char*)value);
			xmlFree(value);
		}

		value = xmlGetProp(node, (xmlChar*)"gid");
		if(value)
		{
			stbuf->st_uid = atoi((char*)value);
			xmlFree(value);
		}

		value = xmlGetProp(node, (xmlChar*)"mode");
		if(value)
		{
			stbuf->st_mode &= S_IFMT;
			stbuf->st_mode |= strtol ((char*)value, NULL, 8);
			xmlFree(value);
		}

		value = xmlGetProp(node, (xmlChar*)"ctime");
		if(value)
		{
			stbuf->st_ctime = atoi((char*)value);
			xmlFree(value);
		}

		value = xmlGetProp(node, (xmlChar*)"atime");
		if(value)
		{
			stbuf->st_atime = atoi((char*)value);
			xmlFree(value);
		}

		value = xmlGetProp(node, (xmlChar*)"mtime");
		if(value)
		{
			stbuf->st_mtime = atoi((char*)value);
			xmlFree(value);
		}
		DEBUG("mode: %d", stbuf->st_mode)

		xmlXPathFreeObject(xpathObj);
		DEBUG("return: 0")
		return 0;
	}
	else
		DEBUG("Invalid Xpath")

	xmlXPathFreeObject(xpathObj);

	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

static int urifs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	(void)offset;
	(void)fi;
	DEBUG("args: const char *path = \"%s\", void *buf = %p, fuse_fill_dir_t filler = %p, off_t offset = %lu, int fi->fh = %lu", path, buf, filler, offset, fi->fh)
	int i;
	int nb;
	char *xpath;
	xmlChar *value;
	xmlNodeSetPtr nodes;
	xmlXPathObjectPtr xpathObj;

	char *tmp = xpath_from_path(path);
	if (!tmp)
	{
		DEBUG("return: -ENOENT(%d)", -ENOENT)
		return -ENOENT;
	}

	if (asprintf(&xpath, "%s/*[@name]", tmp) == -1)
	{
		xfree(tmp);
		DEBUG("return: -ENOENT(%d)", -ENOENT)
		return -ENOENT;
	}
	xfree(tmp);

	DEBUG("xpath: %s", xpath);

	xpathObj = xmlXPathEvalExpression((xmlChar*)xpath, fuse_get_context()->private_data);
	xfree(xpath);
	if(xpathObj == NULL)
	{
		DEBUG("return: -ENOENT(%d)", -ENOENT)
		return -ENOENT;
	}

	nodes = xpathObj->nodesetval;

	if (nodes)
	{
		nb = nodes->nodeNr;
		DEBUG("%d nodes found", nb)

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		for(i=0;i<nb;i++)
		{
			value = xmlGetProp(nodes->nodeTab[i], (xmlChar*)"name");
			if(value)
			{
				filler(buf, (char*)value, NULL, 0);
				xmlFree(value);
			}
		}
		xmlXPathFreeObject(xpathObj);
		DEBUG("return: 0")
		return 0;
	}
	else
	{
		DEBUG("Invalid Xpath")
	}

	xmlXPathFreeObject(xpathObj);

	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

static int urifs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	(void)mode;
	(void)fi;
	DEBUG("args: const char *path = \"%s\", mode_t mode = %d, int fi->fh = %lu", path, mode, fi->fh)
	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

static int urifs_open(const char *path, struct fuse_file_info *fi)
{
	DEBUG("args: const char *path = \"%s\", int fi->fh = %lu", path, fi->fh)
	DEBUG("here")
	int i = 0;
	uri_fd *fd = NULL;
	xmlChar *value;
	xmlNodeSetPtr nodes;
	xmlNodePtr node;
	xmlXPathObjectPtr xpathObj;

	char *xpath = xpath_from_path(path);
	if (!xpath)
	{
		DEBUG("return: -ENOENT(%d)", -ENOENT)
		return -ENOENT;
	}

	DEBUG("xpath: %s", xpath);

	xpathObj = xmlXPathEvalExpression((xmlChar*)xpath, fuse_get_context()->private_data);
	xfree(xpath);
	if(xpathObj == NULL)
	{
		DEBUG("return: -ENOENT(%d)", -ENOENT)
		return -ENOENT;
	}

	nodes = xpathObj->nodesetval;

	if (nodes && nodes->nodeNr > 0)
	{
		DEBUG("%d nodes found", nodes->nodeNr)
		node = nodes->nodeTab[0];

		fd = (uri_fd*)malloc(sizeof(uri_fd));
		if (fd)
		{
			fd->uri = NULL;
			fd->header = NULL;
			value = xmlGetProp(node, (xmlChar*)"size");
			if(value)
			{
				fd->size = atoll((char*)value);
				xmlFree(value);
				value = xmlGetProp(node, (xmlChar*)"uri");
				if(value)
				{
					fd->uri = strdup((char*)value);
					xmlFree(value);
					if(fd->uri)
					{
						value = xmlGetProp(node, (xmlChar*)"header");
						if(value)
						{
							fd->header = curl_slist_append(fd->header,(char*)value);
							DEBUG("Added header \"%s\"", (char*)value)
							xmlFree(value);
						}
						value = xmlGetProp(node, (xmlChar*)"header-cmd");
						if(value)
						{
							header_cmd(fd, (char*)value);
							xmlFree(value);
						}
						while((i < MAX_ENTRIES)&&(opened_files[i]))
							i++;
						if(i < MAX_ENTRIES)
						{
							fi->fh = i;
							opened_files[i] = fd;
							xmlXPathFreeObject(xpathObj);
							DEBUG("return: 0")
							return 0;
						}
						curl_slist_free_all(fd->header);
						xfree(fd->uri);
					}
				}
			}
			xfree(fd);
		}
	}
	else
		DEBUG("0 node found")

	xmlXPathFreeObject(xpathObj);

	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

static int urifs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	(void)path;
	DEBUG("args: const char *path = \"%s\", char *buf = %p, size_t size = %lu, off_t offset = %lu, int fi->fh = %lu", path, buf, size, offset, fi->fh)
	size_t bytes;
	uri_fd *fd;
	char *range;
	CURL *curl_handle;
	CURLcode res;
	struct curl_buffer buffer;
	long http_code = 0;

	if(size == 0)
	{
		DEBUG("return: 0")
		return 0;
	}

	fd = opened_files[fi->fh];

	if (!fd)
	{
		DEBUG("return: -ENOENT(%d)", -ENOENT)
		return -ENOENT;
	}

	if((size + offset) >= fd->size)	{
		bytes = fd->size - offset;
	} else {
		bytes = size;
	}

	if(asprintf(&range, "%llu-%llu", (unsigned long long)offset, (unsigned long long)offset+(unsigned long long)bytes-1) == -1)
	{
		DEBUG("return: -ENOENT(%d)", -ENOENT)
		return -ENOENT;
	}
	DEBUG("Range: %s (bytes: %llu)", range, (unsigned long long)bytes);

	curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, fd->uri);

	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_get_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&buffer);
	curl_easy_setopt(curl_handle, CURLOPT_RANGE, range);
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, fd->header);
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1);

	buffer.data = buf;
	buffer.size = bytes;
	buffer.read = 0;
	res = curl_easy_perform(curl_handle);
	curl_easy_getinfo (curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
	DEBUG("curl_easy_perform() HTTP code %li", http_code);
	
	if(res != CURLE_OK)
	{
		DEBUG("curl_easy_perform() failed: %s (%s)", curl_easy_strerror(res),fd->uri);
		curl_easy_cleanup(curl_handle);
		xfree(range);
		return -ENOENT;
	}

	curl_easy_cleanup(curl_handle);
	xfree(range);

	DEBUG("return: %lu", buffer.read)
	return buffer.read;
}

static int urifs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	(void)path;
	(void)buf;
	(void)size;
	(void)offset;
	(void)fi;
	DEBUG("args: const char *path = \"%s\", char *buf = %p, size_t size = %lu, off_t offset = %lu, int fi->fh = %lu", path, buf, size, offset, fi->fh)
	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

static int urifs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
	(void)path;
	(void)datasync;
	(void)fi;
	DEBUG("args: const char *path = \"%s\", int datasync = %d, int fi->fh = %lu", path, datasync, fi->fh)
	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

static int urifs_release(const char *path, struct fuse_file_info *fi)
{
	(void)path;
	DEBUG("args: const char *path = \"%s\", int fi->fh = %lu", path, fi->fh)
	if(fi->fh > MAX_ENTRIES)
	{
		DEBUG("return: -ENOENT(%d)", -ENOENT)
		return -ENOENT;
	}

	DEBUG("closing file %lu",fi->fh)
	curl_slist_free_all(opened_files[fi->fh]->header);
	xfree(opened_files[fi->fh]->uri);
	xfree(opened_files[fi->fh]);
	opened_files[fi->fh] = NULL;

	DEBUG("return: 0")
	return 0;
}

void urifs_cleanup(void *data)
{
	(void)data;
	DEBUG("args: void *data = %p", data)
	DEBUG("here")
	xmlXPathContextPtr xpathCtx = fuse_get_context()->private_data;
	xmlDocPtr doc = xpathCtx->doc;
	int i;
	for(i=0; i<MAX_ENTRIES; i++)
	{
		if(opened_files[i]!=NULL)
		{
			curl_slist_free_all(opened_files[i]->header);
			xfree(opened_files[i]->uri);
			xfree(opened_files[i]);
		}
	}
	xfree(opened_files);
	curl_global_cleanup();

	xmlXPathFreeContext(xpathCtx);
	xmlFreeDoc(doc);
	xmlCleanupParser();
}

int urifs_flush(const char *path, struct fuse_file_info *fi)
{
	(void)path;
	(void)fi;
	DEBUG("args: const char *path = \"%s\", int fi->fh = %lu", path, fi->fh)
	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

int urifs_statfs(const char *path, struct statvfs *stats)
{
	(void)path;
	DEBUG("args: const char *path = \"%s\", struct statvfs *stats = %p", path, stats)
	stats->f_bsize = 0;
	stats->f_frsize = 0;
	stats->f_blocks = 0;
	stats->f_bfree = 0;
	stats->f_bavail = 0;
	stats->f_namemax = 512;
	stats->f_files = 1000000000;
	stats->f_ffree = 1000000000;
	DEBUG("return: 0")
	return 0;
}

int urifs_truncate(const char *path, off_t size)
{
	(void)path;
	(void)size;
	DEBUG("args: const char *path = \"%s\", off_t size = %lu", path, size)
	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

int urifs_ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
	(void)path;
	(void)size;
	(void)fi;
	DEBUG("args: const char *path = \"%s\", off_t size = %lu, int fi->fh = %lu", path, size, fi->fh)
	DEBUG("here")
	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

int urifs_unlink(const char *path)
{
	(void)path;
	DEBUG("args: const char *path = \"%s\"", path)
	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

int urifs_rename(const char *from, const char *to)
{
	(void)from;
	(void)to;
	DEBUG("args: const char *from = \"%s\", const char *to = \"%s\"", from, to)
	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

int urifs_mkdir(const char *dir, mode_t ignored)
{
	(void)dir;
	(void)ignored;
	DEBUG("args: const char *dir = \"%s\", mode_t ignored = %u", dir, ignored)
	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return -ENOENT;
}

void *urifs_init(struct fuse_conn_info *conn)
{
	(void)conn;
	DEBUG("args: struct fuse_conn_info *conn = %p", conn)
	int i;
	xmlDocPtr doc;
	xmlXPathContextPtr xpathCtx;

	DEBUG("mounting %s",source_xml)

	xmlInitParser();
	LIBXML_TEST_VERSION

	doc = xmlParseFile(source_xml);

	if (doc == NULL)
	{
		DEBUG("Can't parse %s", source_xml)
		exit(1);
	}

	xpathCtx = xmlXPathNewContext(doc);
	if (xpathCtx == NULL)
	{
		xmlFreeDoc(doc);
		DEBUG("Can't get xmlXPathNewContext")
		exit(1);
	}

	opened_files = (uri_fd**)malloc(sizeof(uri_fd*)*MAX_ENTRIES);
	for(i=0;i<MAX_ENTRIES;i++)
		opened_files[i] = NULL;

	curl_global_init(CURL_GLOBAL_ALL);

	DEBUG("return: -ENOENT(%d)", -ENOENT)
	return xpathCtx;
}

static struct fuse_operations urifs_oper = {
	.getattr = urifs_getattr,
	.statfs = urifs_statfs,
	.readdir = urifs_readdir,
	.mkdir = urifs_mkdir,
	.rmdir = urifs_unlink,
	.create = urifs_create,
	.open = urifs_open,
	.read = urifs_read,
	.write = urifs_write,
	.truncate = urifs_truncate,
	.ftruncate = urifs_ftruncate,
	.unlink = urifs_unlink,
	.rename = urifs_rename,
	.fsync = urifs_fsync,
	.release = urifs_release,
	.init = urifs_init,
	.destroy = urifs_cleanup
};

static int urifs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
	(void)data;
	(void)outargs;
	DEBUG("args: void *data = %p, const char *arg = \"%s\", int key = %d, struct fuse_args *outargs = %p", data, arg, key, outargs)
	static int num = 0;

	switch(key) {
		case FUSE_OPT_KEY_OPT:
			if(strcmp(arg, "--debug") == 0) {
				debug_f = fopen(LOG_FILE,"w");
				DEBUG("debug mode started")
				return 0;
			} else if(strcmp(arg, "-oallow-other") == 0) {
				return 0;
			}
			break;
		case FUSE_OPT_KEY_NONOPT:
			num++;
			if(num == 1)	{
				source_xml = strdup(arg);
				return 0;
			}
			break;
	}
	return 1;
}

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	if (argc < 2) {
		return -1;
	}

	if(fuse_opt_parse(&args, NULL, NULL, urifs_opt_proc) == -1) {
		return -1;
	}
	fuse_opt_add_arg(&args, "-oallow_other");

	if(source_xml == NULL)
		return -1;

	init_locks();

	int ret = fuse_main(args.argc, args.argv, &urifs_oper, NULL);
	DEBUG("return: %d", ret)

	kill_locks();

	return ret;
}
