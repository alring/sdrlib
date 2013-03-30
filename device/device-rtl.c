/**
 * Sample plugin
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
#include <complex.h>
#include <pthread.h>
#include <rtl-sdr.h>

#include "device.h"

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define BUFSIZE 1024*1024

typedef struct
{
    rtlsdr_dev_t *dev;
    float complex lut[256 * 256 * sizeof(float complex)];
    float gainscale;
    unsigned char readbuf[BUFSIZE];
    float complex queue[BUFSIZE];
    pthread_t queueThread;
    pthread_mutex_t queueMutex;
    pthread_cond_t  queueCond;
    int queueSize;
    int head,tail;
    Parent *par;
    int isOpen;
} Context;




static int queuePush(Context *ctx, float complex v)
{
    pthread_mutex_lock(&(ctx->queueMutex));
    while (ctx->queueSize >= BUFSIZE)
        pthread_cond_wait(&(ctx->queueCond), &(ctx->queueMutex));
    int head = (ctx->head + 1) & 0xfffff;
    //trace("head:%d tail:%d", head, ctx->tail);
    ctx->queue[head] = v;
    ctx->queueSize++;
    ctx->head = head;
    pthread_cond_signal(&(ctx->queueCond));
    pthread_mutex_unlock(&(ctx->queueMutex));
    return TRUE;
}

static int queuePop(Context *ctx, float complex *v)
{
    pthread_mutex_lock(&(ctx->queueMutex));
    while (ctx->queueSize== 0)
        pthread_cond_wait(&(ctx->queueCond), &(ctx->queueMutex));
    int tail = (ctx->tail + 1) & 0xfffff;
    *v = ctx->queue[tail];
    ctx->queueSize--;
    ctx->tail = tail;
    pthread_cond_signal(&(ctx->queueCond));
    pthread_mutex_unlock(&(ctx->queueMutex));
    return TRUE;
}



static int setGain(void *context, float gain)
{
    Context *ctx = (Context *)context;
    //the int param for rtl gain is tenths of a dB
    int dgain = (int)(gain * ctx->gainscale);
    printf("dgain:%d\n", dgain);
    int ret = rtlsdr_set_tuner_gain(ctx->dev, dgain);
    if (ret)
        {
        ctx->par->error("Failed to set gain");
        }
    return !ret;
}

static float getGain(void *context)
{
    Context *ctx = (Context *)context;
    int dgain = rtlsdr_get_tuner_gain(ctx->dev);
    return ((float)dgain) / ctx->gainscale;
}

static int setSampleRate(void *context, float rate)
{
    Context *ctx = (Context *)context;
    int ret = rtlsdr_set_sample_rate(ctx->dev, (int)rate);
    if (ret)
        {
        ctx->par->error("Failed to set sample rate");
        }
    return !ret;
}

static float getSampleRate(void *context)
{
    Context *ctx = (Context *)context;
    if (!ctx->isOpen)
        return 0.0;
    return (float) rtlsdr_get_sample_rate(ctx->dev);
}

static int setCenterFrequency(void *context, double freq)
{
    Context *ctx = (Context *)context;
    int ret = rtlsdr_set_center_freq(ctx->dev, (uint32_t)freq);
    if (ret)
        {
        ctx->par->error("Failed to set center frequency");
        }
    return !ret;
}

static double getCenterFrequency(void *context)
{
    Context *ctx = (Context *)context;
    uint32_t f = rtlsdr_get_center_freq(ctx->dev);
    return (double)f;
}



static int read(void *context, float complex *cbuf, int buflen)
{
    Context *ctx = (Context *)context;
    if (!ctx->isOpen)
        return 0;
    float complex v;
    int count = 0;
    for ( ; count < buflen ; count++)
        {
        if (!queuePop(ctx, &v))
            break;
        *cbuf++ = v;
        }
    return count;
}


/*
static int read(void *context, float complex *cbuf, int buflen)
{
    Context *ctx = (Context *)context;
    if (!ctx->isOpen)
        return 0;
    unsigned char *bbuf = ctx->readbuf;
    float complex *lut = ctx->lut;
    int count;
    int ret = rtlsdr_read_sync(ctx->dev, bbuf, buflen<<1, &count);
    if (ret < 0)
        {
        ctx->par->error("rtlsdr_read_sync failed");
        return 0;
        }
    //cpx->par->trace("count:%d", count);
    int nrCpx = count >> 1;
    float complex *cpx = cbuf;
    unsigned char *b = bbuf;
    int i=nrCpx;
    while (i--)
        {
        int hi = (int)*b++;
        int lo = (int)*b++;
        *cpx++ = lut[(hi<<8) + lo];
        }
    return nrCpx;
}
*/


static void async_read_callback(unsigned char *buf, uint32_t len, void *context)
{
    Context *ctx = (Context *)context;
    float complex *lut = ctx->lut;
    unsigned char *b = buf;
    int i = len >> 1;
    while (i--)
        {
        int hi = (int)*b++;
        int lo = (int)*b++;
        float complex v = lut[(hi<<8) + lo];
        queuePush(ctx, v);
        }
    //ctx->par->trace("len:%d", len);
}

static int write(void *context, float complex *cbuf, int datalen)
{
    //Context *ctx = (Context *)context;
    return 0;
}

static int transmit(void *context, int truefalse)
{
    //Context *ctx = (Context *)context;
    return 0;
}

static void *asyncLoop(void *context)
{
    Context *ctx = (Context *)context;
    rtlsdr_read_async(ctx->dev, async_read_callback, ctx, 0, 0);
    return NULL;
}

static int open(void *context)
{
    Context *ctx = (Context *)context;
    if (ctx->isOpen)
        return 0;
    rtlsdr_dev_t *dev = NULL;
    int devCount = rtlsdr_get_device_count();
    ctx->par->trace("devices:%d", devCount);
    int ret = rtlsdr_open(&dev, 0);
    if (!dev)
        {
        ctx->par->error("Could not open device");
        return 0;
        }

    ret = rtlsdr_set_tuner_gain_mode(dev, 1);
    int count = rtlsdr_get_tuner_gains(dev, NULL);
    int *gains = (int *)malloc(count * sizeof(int));
    ret = rtlsdr_get_tuner_gains(dev, gains);
    int higain = gains[count - 1];

    ctx->par->trace("higain: %d", higain);
    ctx->dev = dev;
    ctx->gainscale = (float)higain;
    
    setGain(ctx, 1.0);
    setSampleRate(ctx, 2048000);
    setCenterFrequency(ctx, 93700000.0);
    
    ret = rtlsdr_reset_buffer(dev);
    ctx->isOpen = 1;
    ctx->queueSize = 0;
    ctx->head = 1;
    ctx->tail = 0;
    pthread_mutex_init(&(ctx->queueMutex), NULL);
    pthread_cond_init(&(ctx->queueCond), NULL);
    int rc = pthread_create(&(ctx->queueThread), NULL, asyncLoop, ctx);
    if (rc)
        {
        ctx->par->error("ERROR; return code from pthread_create() is %d", rc);
        return FALSE;
        }
    ctx->par->trace("started");
    return 1;
}

static int isOpen(void *context)
{
    Context *ctx = (Context *)context;
    return ctx->isOpen;
}
    

static int close(void *context)
{
    Context *ctx = (Context *)context;
    //do shutdowny things here
    ctx->isOpen = 0;
    rtlsdr_cancel_async(ctx->dev);
    rtlsdr_close(ctx->dev);
    return 1;
}

static int delete(void *context)
{
    Context *ctx = (Context *)context;
    pthread_mutex_destroy(&(ctx->queueMutex));
    pthread_cond_destroy(&(ctx->queueCond));
    free(ctx);   
    return 1;
}


int deviceCreate(Device *dv, Parent *parent)
{
    Context *ctx = (Context *)malloc(sizeof(Context));
    if (!ctx)
        {
        return 0;
        }
    memset(ctx, 0, sizeof(Context));
    ctx->par               = parent;
    pthread_mutex_init(&(ctx->queueMutex), NULL);
    int idx = 0;
    int hi,lo;
    for (hi = 0 ; hi < 256 ; hi++)
        {
        float hival = ((float)(hi - 127)) / 128.0; 
        for (lo = 0; lo < 256 ; lo++)
            {
            float loval = ((float)(lo - 127)) / 128.0;
            ctx->lut[idx++] = hival+loval*I; 
            }    
        }
    
    dv->type               = DEVICE_SDR,
    dv->name               = "RTL - SDR Device";
    dv->ctx                = (void *)ctx;
    dv->open               = open;
    dv->isOpen             = isOpen;
    dv->close              = close;
    dv->delete             = delete;
    dv->setGain            = setGain;
    dv->getGain            = getGain;
    dv->setSampleRate      = setSampleRate;
    dv->getSampleRate      = getSampleRate;
    dv->setCenterFrequency = setCenterFrequency;
    dv->getCenterFrequency = getCenterFrequency;
    dv->read               = read;
    dv->write              = write;
    dv->transmit           = transmit;
    return 1;
}

