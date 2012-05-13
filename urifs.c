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
 * Requirement: libfuse, libcurl and libroxml (https://code.google.com/p/libroxml/)
 *
 * Compile with: gcc -o urifs urifs.c -Wall -ansi -W -std=c99 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -lfuse -lcurl -lroxml -O3 -Wno-unused-parameter
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
#include <roxml.h>

#define MAX_ENTRIES	512
#define LOG_FILE	"/var/log/urifs.log"

typedef struct {
	char *uri;
	size_t size;
} uri_fd;

uri_fd ** opened_files = NULL;

char *source_xml;

FILE *debug_f = NULL;

#define DEBUG(...) { if(debug_f) { fprintf(debug_f,"%s::%s:%d ",__FILE__,__func__,__LINE__);fprintf(debug_f,__VA_ARGS__); fprintf(debug_f,"\n"); fflush(debug_f); } }

static inline void xfree(void *p)
{
	if(p != NULL)
		free(p);
	p=NULL;
}

static char *xpath_from_path(const char *path)
{
	char *p;
	char *xpath;
	size_t len = strlen(path);
	char *tmp = (char*)malloc(len+1);
	strncpy(tmp, path, len+1);
	asprintf(&xpath, "/root");
	p = strtok(tmp, "/");
	while( p != NULL )
	{
		char *new;
		asprintf(&new, "%s/*[@name=\"%s\"]", xpath, p);
		xfree(xpath);
		xpath = new;
		p = strtok(NULL, "/");
	}
	return xpath;
}

struct curl_buffer {
	char *data;
	size_t size;
	size_t read;
};

static size_t curl_get_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	DEBUG("here")
	size_t realsize = size * nmemb;
	struct curl_buffer *buffer = (struct curl_buffer *)userp;
	if(buffer->read + realsize > buffer->size)
		realsize = buffer->size - buffer->read;

	memcpy(buffer->data+buffer->read, contents, realsize);
	buffer->read += realsize;

	return realsize;
}

static int urifs_getattr(const char *path, struct stat *stbuf)
{
	DEBUG("here")
	int nb;
	node_t *n = NULL;
	node_t *a = NULL;
	node_t *root = fuse_get_context()->private_data;
	char *xpath = xpath_from_path(path);

	DEBUG("trying '%s'",path);
	node_t **ans = roxml_xpath(root, xpath, &nb);
	DEBUG("xpath: %s  -> %d nodes found",xpath,nb);
	xfree(xpath);

	if(ans) {
		n = ans[0];

		char *type = roxml_get_name(n, NULL, 0);
		if(strcmp(type, "dir")==0 || strcmp(type, "root")==0)
		{
			DEBUG("dir")
			stbuf->st_mode = S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
		}
		else
		{
			DEBUG("file")
			stbuf->st_mode = S_IFREG;
		}
		roxml_release(type);

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

		if( (a = roxml_get_attr(n, "size", 0)) != NULL )
		{
			char * value = roxml_get_content(a, NULL, 0, NULL);
			stbuf->st_blocks = stbuf->st_size = atoll(value);
			roxml_release(value);
		}
		if( (a = roxml_get_attr(n, "uid", 0)) != NULL )
		{
			char * value = roxml_get_content(a, NULL, 0, NULL);
			stbuf->st_uid = atoi(value);
			roxml_release(value);
		}
		if( (a = roxml_get_attr(n, "gid", 0)) != NULL )
		{
			char * value = roxml_get_content(a, NULL, 0, NULL);
			stbuf->st_uid = atoi(value);
			roxml_release(value);
		}
		if( (a = roxml_get_attr(n, "mode", 0)) != NULL )
		{
			char * value = roxml_get_content(a, NULL, 0, NULL);
			stbuf->st_mode &= S_IFMT;
			stbuf->st_mode |= strtol (value, NULL, 8);
			roxml_release(value);
		}
		if( (a = roxml_get_attr(n, "ctime", 0)) != NULL )
		{
			char * value = roxml_get_content(a, NULL, 0, NULL);
			stbuf->st_ctime = atoi(value);
			roxml_release(value);
		}
		if( (a = roxml_get_attr(n, "atime", 0)) != NULL )
		{
			char * value = roxml_get_content(a, NULL, 0, NULL);
			stbuf->st_atime = atoi(value);
			roxml_release(value);
		}
		if( (a = roxml_get_attr(n, "mtime", 0)) != NULL )
		{
			char * value = roxml_get_content(a, NULL, 0, NULL);
			stbuf->st_mtime = atoi(value);
			roxml_release(value);
		}
		DEBUG("mode: %d", stbuf->st_mode)

		roxml_release(ans);
		return 0;
	}
	return -ENOENT;
}
 
static int urifs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	DEBUG("here")
	DEBUG("trying '%s'",path);
	int i;
	int nb;
	node_t *n = NULL;
	node_t *root = fuse_get_context()->private_data;
	char *xpath = xpath_from_path(path);
	node_t **ans = roxml_xpath(root, xpath, &nb);
	DEBUG("xpath: %s  -> %d nodes found",xpath,nb);
	xfree(xpath);

	if(ans)	{
		n = ans[0];
		roxml_release(ans);

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		nb = roxml_get_chld_nb(n);
		DEBUG("%d dirs", nb)
		for(i = 0; i < nb; i++)	{
			node_t *tmp = roxml_get_chld(n, NULL, i);
			tmp = roxml_get_attr(tmp, "name", 0);
			if(!tmp)
				continue;
			char *name = roxml_get_content(tmp, NULL, 0, NULL);
			filler(buf, name, NULL, 0);
			roxml_release(name);
		}
		return 0;
	}
	return -ENOENT;
}
 
static int urifs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	DEBUG("here %s", path)
	return -ENOENT;
}
 
static int urifs_open(const char *path, struct fuse_file_info *fi)
{
	DEBUG("here")
	int i = 0;
	int nb;
	uri_fd *fd = NULL;
	node_t *a;
	char *value;
	node_t *root = fuse_get_context()->private_data;
	char *xpath = xpath_from_path(path);
	node_t **ans = roxml_xpath(root, xpath, &nb);
	DEBUG("xpath: %s  -> %d nodes found",xpath,nb);
	xfree(xpath);

	if(ans)	{
		node_t *n = ans[0];
		roxml_release(ans);
		fd = (uri_fd*)malloc(sizeof(uri_fd));
		if( (a = roxml_get_attr(n, "size", 0)) == NULL)
		{
			xfree(fd);
			return -ENOENT;
		}

		value = roxml_get_content(a, NULL, 0, NULL);
		fd->size = atoll(value);
		roxml_release(value);

		if( (a = roxml_get_attr(n, "uri", 0)) == NULL)
		{
			xfree(fd);
			return -ENOENT;
		}

		value = roxml_get_content(a, NULL, 0, NULL);
		fd->uri = strdup(value);
		roxml_release(value);

		while((i < MAX_ENTRIES)&&(opened_files[i]))	{ i++; } 
		if(i < MAX_ENTRIES)	{
			fi->fh = i;
			opened_files[i] = fd;

			return 0;
		}
		xfree(fd->uri);
		xfree(fd);
		return -ENOENT;
	}
	return -ENOENT;
}

static int urifs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	DEBUG("here")
	size_t bytes;
	uri_fd *fd;
	char *range;
	CURL *curl_handle;
	struct curl_buffer buffer;

	if(size == 0) { return 0; }

	fd = opened_files[fi->fh];

	if (!fd) { return -ENOENT; }
	
	if((size + offset) >= fd->size)	{
		bytes = fd->size - offset;
	} else {
		bytes = fd->size;
	}

	asprintf(&range, "%llu-%llu", (unsigned long long)offset, (unsigned long long)offset+(unsigned long long)bytes-1);
	DEBUG("Range: %s (bytes: %llu)", range, (unsigned long long)bytes);

	buffer.data = buf;
	buffer.size = bytes;
	buffer.read = 0;

	curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, fd->uri);

	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_get_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&buffer);
	curl_easy_setopt(curl_handle, CURLOPT_RANGE, range);

	curl_easy_perform(curl_handle);
	curl_easy_cleanup(curl_handle);

	xfree(range);

	return buffer.read;
}

static int urifs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	DEBUG("here")
	return -ENOENT;
}

static int urifs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
	DEBUG("here")
	return -ENOENT;
}

static int urifs_release(const char *path, struct fuse_file_info *fi)
{
	DEBUG("here")
	if(fi->fh > MAX_ENTRIES) { return -ENOENT; }

	DEBUG("closing file %lu",fi->fh)
	xfree(opened_files[fi->fh]->uri);
	xfree(opened_files[fi->fh]);
	return 0;
}

void urifs_cleanup(void *data)
{
	DEBUG("here")
	DEBUG("freeing file table")
	node_t *root = fuse_get_context()->private_data;
	int i;
	for(i=0; i<MAX_ENTRIES; i++)
	{
		if(opened_files[i])
		{
			xfree(opened_files[i]->uri);
			xfree(opened_files[i]);
		}
	}
	xfree(opened_files);
	curl_global_cleanup();
	roxml_close(root);
}

int urifs_flush(const char *path, struct fuse_file_info *fi)
{
	DEBUG("here")
	return -ENOENT;
}

int urifs_statfs(const char *path, struct statvfs *stats)
{
	DEBUG("here")
	stats->f_bsize = 0;
	stats->f_frsize = 0;
	stats->f_blocks = 0;
	stats->f_bfree = 0;
	stats->f_bavail = 0;
	stats->f_namemax = 512;
	stats->f_files = 1000000000;
	stats->f_ffree = 1000000000;
	return 0;
}

int urifs_truncate(const char *path, off_t size)
{
	DEBUG("here")
	return -ENOENT;
}

int urifs_ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
	DEBUG("here")
	return -ENOENT;
}

int urifs_unlink(const char *path)
{
	DEBUG("here")
	return -ENOENT;
}

int urifs_rename(const char *from, const char *to)
{
	DEBUG("here")
	return -ENOENT;
}

int urifs_mkdir(const char *dir, mode_t ignored)
{
	DEBUG("here")
	return -ENOENT;
}

void *urifs_init(struct fuse_conn_info *conn)
{
	DEBUG("here")
	node_t *n = NULL;

	opened_files = (uri_fd**)malloc(sizeof(uri_fd*)*MAX_ENTRIES);

	DEBUG("mounting %s",source_xml)

	n = roxml_load_doc(source_xml);
	DEBUG("loaded doc to %p mount init ok", n);

	curl_global_init(CURL_GLOBAL_ALL);

	return n;
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
	DEBUG("here")
	static int num = 0;

	switch(key) {
		case FUSE_OPT_KEY_OPT:
			if(strcmp(arg, "--debug") == 0) {
				debug_f = fopen(LOG_FILE,"w");
				fprintf(stderr,"debug mode started\n");
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
	DEBUG("here")
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

	return fuse_main(args.argc, args.argv, &urifs_oper, NULL);
}
