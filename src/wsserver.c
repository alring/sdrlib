/**
 * This is a very simple HTTP server that can also handle Websockets
 *
 * Authors:
 *   Bob Jamison
 *
 * Copyright (C) 2013 Bob Jamison
 * 
 *  This file is part of the SdrLib library.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>


#ifdef WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif



#include "wsserver.h"


#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif


static void trace(char *fmt, ...)
{
    fprintf(stdout, "WsServer: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
}


static void error(char *fmt, ...)
{
    fprintf(stderr, "WsServer err: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}




/*###################################################################################
##  B A S E    6 4
###################################################################################*/



static char *base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


static void base64encode(unsigned char *inbuf, int len, char *outbuf)
{
    char *out = outbuf;
    unsigned char *b = inbuf;
    while (len>0)
        {
        if (len >= 3)
            {
            int b0 = (int)*b++;
            int b1 = (int)*b++;
            int b2 = (int)*b++;
            *out++ = base64[ ((b0       ) >> 2)                      ];
            *out++ = base64[ ((b0 & 0x03) << 4) | ((b1 & 0xf0) >> 4) ];
            *out++ = base64[ ((b1 & 0x0f) << 2) | ((b2 & 0xc0) >> 6) ];
            *out++ = base64[ ((b2 & 0x3f)     )                      ];
            len -= 3;
            }
        else if (len == 2)
            {
            int b0 = (int)*b++;
            int b1 = (int)*b++;
            *out++ = base64[ ((b0 >> 2)       )                      ];
            *out++ = base64[ ((b0 & 0x03) << 4) | ((b1 & 0xf0) >> 4) ];
            *out++ = base64[ ((b1 & 0x0f) << 2)                      ];
            *out++ = '=';
            len -= 2;
            }
        else /* len == 1 */
            {
            int b0 = (int)*b++;
            *out++ = base64[ ((b0       ) >> 2)                      ];
            *out++ = base64[ ((b0 & 0x03) << 4)                      ];
            *out++ = '=';
            *out++ = '=';
            len -= 1;
            }
        }
    
    *out = '\0';
}


static int _dec64(int ch)
{
    int v;
    if (ch >= 'A' && ch <= 'Z')
        v = ch - 'A';
    else if (ch >= 'a' && ch <= 'z')
        v = ch - 'a' + 26;
    else if (ch >= '0' && ch <= '9')
        v = ch - '0' + 52;
    else if (ch == '+')
        v = 62;
    else if (ch == '/')
        v = 63;
    else if (ch == '=')
        v = 0;
    else
        {
        error("Bad base64 character: '%c'" , ch);
        v = -1;
        }
    return v;
}

static int base64decode(char *inbuf, unsigned char *outbuf, int outbufLen)
{
    int len = strlen(inbuf);
    int outputLen = len / 4 * 3;
    if (inbuf[len-1] == '=') outputLen--;
    if (inbuf[len-2] == '=') outputLen--;
    if (outbufLen < outputLen)
        {
        error("Output buffer too small. Needs to be %d bytes", outputLen);
        return -1;
        }
    char *in = inbuf;
    unsigned char *out = outbuf;
    unsigned char *end = outbuf+outputLen;
    for (; len > 0 ; len -= 4)
        {
        int v0 = _dec64(*in++);
        int v1 = _dec64(*in++);
        int v2 = _dec64(*in++);
        int v3 = _dec64(*in++);
        if (v0<0 || v1<0 || v2<0 || v3<0)
            return -1;
        if (out < end) *out++ = ((v0       ) << 2) | ((v1 & 0x30) >> 4);
        if (out < end) *out++ = ((v1 & 0x0f) << 4) | ((v2 & 0x3c) >> 2);
        if (out < end) *out++ = ((v2 & 0x03) << 6) | ((v3 & 0x3f)     );
        }
    return outputLen;
}




/*###################################################################################
##  S H A   1
###################################################################################*/

#define TR32(x) ((x) & 0xffffffffL)
#define SHA_ROTL(X,n) ((((X) << (n)) & 0xffffffffL) | (((X) >> (32-(n))) & 0xffffffffL))




static void _sha1transform(uint32_t *H, unsigned char *block)
{
    uint32_t W[80];

    int i = 0;
    for ( ; i < 16 ; i++)
        {
        uint32_t b0 = *block++; 
        uint32_t b1 = *block++; 
        uint32_t b2 = *block++; 
        uint32_t b3 = *block++;
        W[i] = b0 << 24 | b1 << 16 | b2 << 8 | b3; 
        //printf("W[%d] : %08x\n", i, W[i]);
        }

        
    //see 6.1.2
    for (i = 16; i < 80 ; i++)
        W[i] = SHA_ROTL((W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16]), 1);

    uint32_t a = H[0];
    uint32_t b = H[1];
    uint32_t c = H[2];
    uint32_t d = H[3];
    uint32_t e = H[4];

    uint32_t T;

    for (i=0 ; i < 20 ; i++)
        {
        //see 4.1.1 for the boolops on B,C, and D  //Ch(b,c,d))
        T = TR32(SHA_ROTL(a,5) + ((b&c)|((~b)&d)) + e + 0x5a827999L + W[i]);
        e = d; d = c; c = SHA_ROTL(b, 30); b = a; a = T;
        //printf("%2d %08x %08x %08x %08x %08x\n", i, a, b, c, d, e);
        }
    for ( ; i < 40 ; i++)
        {
        T = TR32(SHA_ROTL(a,5) + (b^c^d) + e + 0x6ed9eba1L + W[i]);
        e = d; d = c; c = SHA_ROTL(b, 30); b = a; a = T;
        //printf("%2d %08x %08x %08x %08x %08x\n", i, a, b, c, d, e);
        }
    for ( ; i < 60 ; i++)
        {
        T = TR32(SHA_ROTL(a,5) + ((b&c)^(b&d)^(c&d)) + e + 0x8f1bbcdcL + W[i]);
        e = d; d = c; c = SHA_ROTL(b, 30); b = a; a = T;
        //printf("%2d %08x %08x %08x %08x %08x\n", i, a, b, c, d, e);
        }
    for ( ; i < 80 ; i++)
        {
        T = TR32(SHA_ROTL(a,5) + (b^c^d) + e + 0xca62c1d6L + W[i]);
        e = d; d = c; c = SHA_ROTL(b, 30); b = a; a = T;
        //printf("%2d %08x %08x %08x %08x %08x\n", i, a, b, c, d, e);
        }

    H[0] = TR32(H[0] + a);
    H[1] = TR32(H[1] + b);
    H[2] = TR32(H[2] + c);
    H[3] = TR32(H[3] + d);
    H[4] = TR32(H[4] + e);
        
}



/**
 * Small and simple SHA1 hash implementation for small message sizes.
 * Note that outbuf is assumed to be 20 bytes long.
 */
static void sha1hash(unsigned char *data, int len, unsigned char *outbuf)
{
    // Initialize H with the magic constants (see FIPS180 for constants)
    uint32_t H[5];
    H[0] = 0x67452301L;
    H[1] = 0xefcdab89L;
    H[2] = 0x98badcfeL;
    H[3] = 0x10325476L;
    H[4] = 0xc3d2e1f0L;
    
    int i;

    int bytesLeft = len;
    
    unsigned char *d = data;
    
    unsigned char block[64];
    
    int cont = 1;
    
    while (cont)
        {
        if (bytesLeft >= 64)
            {
            unsigned char *b = block;
            for (i=0 ; i < 64 ; i++)
                *b++ = *d++;
            bytesLeft -= 64;
            _sha1transform(H, block);
            }
        else
            {
            unsigned char *b = block;
            for (i=0 ; i < bytesLeft ; i++)
                *b++ = *d++;
            *b++ = 0x80;
            int pad = 64 - bytesLeft - 1;
            if (pad > 0 && pad < 8) //if not enough room, finish block and start another
                {
                while (pad--)
                    *b++ = 0;
                _sha1transform(H, block);
                b = block; //reset
                pad = 64;  //reset
                }
            pad -= 8;
            while (pad--)
                *b++ = 0;
            long nrBits = 8L * len;
            *b++ = (unsigned char) ((nrBits>>56) & 0xff);
            *b++ = (unsigned char) ((nrBits>>48) & 0xff);
            *b++ = (unsigned char) ((nrBits>>40) & 0xff);
            *b++ = (unsigned char) ((nrBits>>32) & 0xff);
            *b++ = (unsigned char) ((nrBits>>24) & 0xff);
            *b++ = (unsigned char) ((nrBits>>16) & 0xff);
            *b++ = (unsigned char) ((nrBits>> 8) & 0xff);
            *b++ = (unsigned char) ((nrBits    ) & 0xff);
            _sha1transform(H, block);
            cont = 0;
            }
        }
    
    //copy out answer
    unsigned char *out = outbuf;
    for (i=0 ; i<5 ; i++)
        {
        uint32_t h = H[i];
        *out++ = (unsigned char)((h >> 24) & 0xff);
        *out++ = (unsigned char)((h >> 16) & 0xff);
        *out++ = (unsigned char)((h >>  8) & 0xff);
        *out++ = (unsigned char)((h      ) & 0xff);
        }

}


/**
 * Note: b64buf should be at least 26 bytes
 */
static void sha1hash64(unsigned char *data, int len, char *b64buf)
{
    unsigned char hash[20];
    sha1hash(data, len, hash);
    base64encode(hash, 20, b64buf);
}



/*###################################################################################
##  S E R V E R
###################################################################################*/




ClientInfo *infoCreate()
{
    ClientInfo *obj = (ClientInfo *)malloc(sizeof(ClientInfo));
    if (!obj)
        {
        return NULL;
        }
    memset(obj, 0, sizeof(ClientInfo));
    return obj;
}

void infoDelete(ClientInfo *obj)
{
    if (obj)
        {
        free(obj);
        }
}



struct WsServer
{
    int sock;
    int port;
    pthread_t thread;
    int cont;
    WsHandlerFunc *handlerFunc;
    void *context;
};




static char *trim(char *str)
{
  char *end;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}




static int wsSendPacket(ClientInfo *info, int opcode, unsigned char *dat, long len)
{
    int sock = info->socket;
    
    unsigned char buf[14];
    
    unsigned char *b = buf;
    
    *b++ = 0x80 + opcode;
    if (len < 126)
        *b++ = len;
    else if (len < 65536)
        {
        *b++ = 126;
        *b++ = (unsigned char) ((len>>8) & 0xff);
        *b++ = (unsigned char) ((len   ) & 0xff);
        }
    else
        {
        *b++ = 127;
        *b++ = (unsigned char) ((len>>56) & 0xff);
        *b++ = (unsigned char) ((len>>48) & 0xff);
        *b++ = (unsigned char) ((len>>40) & 0xff);
        *b++ = (unsigned char) ((len>>32) & 0xff);
        *b++ = (unsigned char) ((len>>24) & 0xff);
        *b++ = (unsigned char) ((len>>16) & 0xff);
        *b++ = (unsigned char) ((len>> 8) & 0xff);
        *b++ = (unsigned char) ((len    ) & 0xff);
        }
    int size = b-buf;
    
    if (write(sock, buf, size) < size)
        {
        error("Write: could not write header to socket");
        return -1;
        }
    long count = write(sock, dat, len);
    if (count < len)
        {
        error("Write: could not write %ld bytes to socket", len);
        return -1;
        }
    return count;
}


int wsSend(ClientInfo *info, char *str)
{
    return wsSendPacket(info, 0x01, (unsigned char *)str, strlen(str));
}


int wsSendBinary(ClientInfo *info, unsigned char *dat, long len)
{
    return wsSendPacket(info, 0x02, dat, len);
}



static int getInt(int sock, int size)
{
    unsigned char buf[16];
    long v = 0;
    if (read(sock, buf, size)< size)
        return -1;
    unsigned char *b = buf;
    while (size--)
        {
        v = (v << 8) + *b++;
        }
    return v;
}

int wsRecv(ClientInfo *info, unsigned char *dat, int len)
{
    int sock = info->socket;
    unsigned char *buf = (unsigned char *)info->buf;
    unsigned char b;
    if (read(sock, &b, 1)<0)
        return -1;
    int fin    = b & 0x80;
    int rsv1   = b & 0x40;
    int rsv2   = b & 0x20;
    int rsv3   = b & 0x10;
    int opcode = b & 0x0f;
    if (read(sock, &b, 1)<0)
        return -1;
    int hasMask = b & 0x80;
    long paylen = b & 0x7f;
    if (paylen == 126)
        paylen = getInt(sock, 2);
    else if (paylen == 127)
        paylen = getInt(sock, 8);
    unsigned char mask[4];
    if (hasMask)
        {
        if (read(sock, mask, 4)<4)
            return -1;
        }
    

    trace("fin: %d opcode:%d hasMask:%d len:%ld mask:%d", fin, opcode, hasMask, paylen, mask);
    
    if (paylen > len)
        {
        error("Buffer too small for data");
        paylen = len;
        }
        
    if (read(sock, dat, paylen)<0)
        {
        error("Read payload");
        return -1;
        }    
        
    if (hasMask)
        {
        int i;
        for (i=0 ; i < paylen ; i++)
            dat[i] ^= mask[i%4];
        }
        
    dat[paylen] = '\0';
    
    return paylen;
}



static void defaultHandler(ClientInfo *info)
{
    trace("resource : '%s'", info->resourceName);
    unsigned char buf[1024];
    while (1)
        {
        int ret = wsRecv(info, buf, 1025);
        if (ret < 0)
            break;
        trace("I just received: '%s'", buf);
        if (strcmp((const char *)buf, "done") == 0)
            break;
        ret = wsSend(info, "return the favor");
        trace("send ret:%d", ret);
        } 

}




WsServer *wsCreate(WsHandlerFunc *func, void *context, int port)
{
    WsServer *obj = (WsServer *)malloc(sizeof(WsServer));
    if (!obj)
        {
        return NULL;
        }
    obj->handlerFunc = (func) ? func : defaultHandler;
    obj->context = context;
    obj->port = port;
    obj->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (obj->sock < 0)
        {
        error("Could not create server socket");
        return NULL;
        }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    int ret = bind(obj->sock, (struct sockaddr *) &addr, sizeof(addr));
    if (ret < 0)
        {
        error("Could not bind to port %d : %s", port, strerror(errno));
        close(obj->sock);
        free(obj);
        return NULL;
        }
    listen(obj->sock, 5);
    return obj;
}


void wsDelete(WsServer *obj)
{
    if (obj)
        {
        close(obj->sock);
        free(obj);
        }
}





typedef struct 
{
    char *ext;
    char *mimeType;
} MimeEntry;


static MimeEntry mimeTable[] = 
{
    { "txt",  "text/plain"       },
    { "htm",  "text/html"        },
    { "html", "text/html"        },
    { "jpg",  "image/jpeg"       },
    { "jpeg", "image/jpeg"       },
    { "js",   "text/javascript"  },
    { "css",  "text/css"         },
    { "png",  "image/png"        },
    { NULL,   NULL               }
};


static char *getMimeType(char *fname)
{
    char *pos = strrchr(fname, '/');
    if (pos)
        fname = pos;
    pos = strrchr(fname, '.');
    if (pos)
        {
        MimeEntry *mime = mimeTable;
        char *ext = pos+1;
        while (mime->ext)
            {
            if (strcmp(mime->ext, ext)==0)
                return mime->mimeType;
            mime++;
            }
        }

    return "application/octet-stream";
}


static void outf(int fd, char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start (args, fmt);
    vsnprintf(buf, 255, fmt, args);
    va_end(args);
    write(fd, buf, strlen(buf));
}



/**
 * Deliver a static file
 */
static void serveFile(ClientInfo *info)
{
    char *resName = info->resourceName;
    char *buf = info->buf;
    int sock = info->socket;
    
    snprintf(buf, 255, "./%s", info->resourceName);
    FILE *f = fopen(buf, "rb");
    if (!f)
        {
        outf(sock, "404 Resource '%s' not found\r\n", resName);
        }
    else
        {
        struct stat finfo;
        int ret = fstat(fileno(f), &finfo);
        if (ret < 0)
            {
            outf(sock, "404 Cannot stat resource '%s'\r\n", resName);
            fclose(f);
            return;
            }
        int len = finfo.st_size;
        outf(sock, "HTTP/1.1 200 OK\r\n");
        outf(sock, "Content-Type: %s\r\n", getMimeType(resName));
        outf(sock, "Content-Length: %d\r\n", len);
        outf(sock, "\r\n");
        
        while (len-- && !feof(f))
            {
            int count = fread(buf, 1, CLIENT_BUFLEN, f);
            if (count > 0)
                write(sock, buf, count);
            }
            
        fclose(f);
        }
}



static char *header =
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: %s\r\n"
    "\r\n";
    
/**
 * This is the thread for handling a client request.  One is spawned per client
 */
static void *handleClient(void *ctx)
{
    ClientInfo *info = (ClientInfo *)ctx;
    char *buf = info->buf;
    int sock = info->socket;
    
    int wsrequest = FALSE;
    FILE *in = fdopen(sock, "r");
    
    fgets(buf, CLIENT_BUFLEN, in);
    char *str = trim(buf);
    char *savptr;
    char *tok = strtok_r(str, " ", &savptr);
    if (strcmp(tok, "GET")!=0)
        {
        outf(sock, "405 Method '%s' not supported by this server", tok);
        }
    else
        {
        tok = strtok_r(NULL, " ", &savptr);
        strncpy(info->resourceName, tok, CLIENT_BUFLEN);
        char keybuf[40];
        keybuf[0]=0;
        while (fgets(buf, 255, in))
            {
            char *str = trim(buf);
            int len = strlen(str);
            if (len == 0)
                break;
            //trace("%s", str);
            char *name = strtok_r(str, ": ", &savptr);
            char *value = strtok_r(NULL, ": ", &savptr);
            //trace("name:'%s' value:'%s'", name, value);
            if (strcmp(name, "Sec-WebSocket-Key")==0)
                {
                char encbuf[128];
                snprintf(encbuf, 128, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", value);
                sha1hash64((unsigned char *)encbuf, strlen(encbuf), keybuf);
                wsrequest = TRUE;
                } 
            }
        trace("ready to process: %d", wsrequest);
        if (wsrequest)
            {
            outf(sock, header, keybuf);
            
            WsHandlerFunc *func = info->server->handlerFunc;
            if (func)
                {
                (*func)(info);
                }
            }
        else
            {
            WsServer *srv = info->server;
            serveFile(info);
            }
        }

    fclose(in);
    
    close(info->socket);
    
    infoDelete(info);
    
    return NULL;
}



/**
 * This is the listening thread
 */
static void *listenForClients(void *ctx)
{
    WsServer *obj = (WsServer *)ctx;
    obj->cont = TRUE;
    while (obj->cont)
        {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int clisock = accept(obj->sock, (struct sockaddr *) &addr, &addrlen); 
        if (clisock < 0)
            {
            error("Could not accept connection: %s", strerror(errno));
            }
        trace("have new client");
        ClientInfo *info = infoCreate();
        if (!info)
            {
            error("Could not create client info record");
            continue;
            }
        info->server  = obj;
        info->socket  = clisock;
        info->context = obj->context;
        
        pthread_t thread;
        int rc = pthread_create(&thread, NULL, handleClient, (void *)info);
        if (rc)
            {
            error("ERROR; return code from pthread_create() is %d", rc);
            return FALSE;
            }
        
        }
    return NULL;
}

int wsServe(WsServer *obj)
{
    trace("starting");
    pthread_t thread;
    int rc = pthread_create(&thread, NULL, listenForClients, (void *)obj);
    if (rc)
        {
        error("ERROR; return code from pthread_create() is %d", rc);
        return FALSE;
        }
    obj->thread = thread;
    pthread_join(obj->thread, NULL);
    return TRUE;
}







