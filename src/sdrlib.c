/**
 * This is the core engine of the sdr library  
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
 *  but WITHOUT ANY WARRANTY; without even the sdried warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <string.h>
#include <stdlib.h>


#include "sdrlib.h"

#include "audio.h"
#include "codec.h"
#include "demod.h"
#include "device.h"
#include "fft.h"
#include "filter.h"
#include "samplerate.h"
#include "vfo.h"

#include "private.h"


static void *sdrReaderThread(void *ctx);

/**
 * Our main context
 */
struct SdrLib
{
    int            deviceCount;
    Device         *devices[SDR_MAX_DEVICES];
    Device         *device;
    pthread_t      thread;
    int            running; //state of the reader thread
    Fft            *fft;
    void           *context; //context for any client code calling me
    UintOutputFunc *psFunc; //for outputting the power spectrum
    ByteOutputFunc *codecFunc;
    Ddc            *ddc;
    Mode           mode;
    Demodulator    *demod;
    Demodulator    *demodNull;
    Demodulator    *demodAm;
    Demodulator    *demodFm;
    Demodulator    *demodLsb;
    Demodulator    *demodUsb;
    Resampler      *resampler;
    int            audioEnabled;
    Audio          *audio;
    Codec          *codec;
};


/**
 */  
SdrLib *sdrCreate(void *context, UintOutputFunc *psFunc, ByteOutputFunc *codecFunc)
{
    SdrLib * sdr = (SdrLib *) malloc(sizeof(SdrLib));
    memset(sdr, 0, sizeof(SdrLib));
    sdr->deviceCount = deviceScan(DEVICE_SDR, sdr->devices, SDR_MAX_DEVICES);
    if (!sdr->deviceCount)
        {
        error("No devices found");
        //but dont fail. wait until start()
        }
    sdr->context   = context;
    sdr->psFunc    = psFunc;
    sdr->codecFunc = codecFunc;
    sdr->fft       = fftCreate(16384);
    sdr->ddc       = ddcCreate(21, 0.0, -5000.0, 5000.0, 2048000.0);
    sdr->demodNull = demodNullCreate();
    sdr->demodFm   = demodFmCreate();
    sdr->demodAm   = demodAmCreate();
    sdr->demodLsb  = demodLsbCreate();
    sdr->demodUsb  = demodUsbCreate();
    sdr->demod     = sdr->demodFm;
    sdr->mode      = MODE_FM;
    sdr->audio     = audioCreate();
    sdr->codec     = codecCreate();
    sdr->resampler = resamplerCreate(21, sdr->audio->sampleRate, sdr->audio->sampleRate);
    
    sdrSetAfGain(sdr, 0.0);
    
    return sdr;
}



/**
 */   
int sdrDelete(SdrLib *sdr)
{
    for (int i = 0 ; i < sdr->deviceCount ; i++)
        {
        Device *d = sdr->devices[i];
        d->delete(d->ctx);
        }
    audioDelete(sdr->audio);
    codecDelete(sdr->codec);
    fftDelete(sdr->fft);
    //vfoDelete(sdr->vfo);
    //firDelete(sdr->bpf);
    ddcDelete(sdr->ddc);
    demodDelete(sdr->demodFm);
    demodDelete(sdr->demodAm);
    demodDelete(sdr->demodLsb);
    demodDelete(sdr->demodUsb);
    resamplerDelete(sdr->resampler);
    free(sdr);
    return TRUE;
}




/**
 */   
int sdrStart(SdrLib *sdr)
{
    pthread_t thread;
    if (sdr->running || sdr->device)
        {
        error("Device already started");
        return FALSE;
        }
    if (!sdr->deviceCount)
        {
        error("No devices found");
        return FALSE;
        }
    Device *d = sdr->devices[0];
    if (!d->open(d->ctx))
        {
        error("Could not start device");
        return FALSE;
        }
    sdr->device = d;
    d->setGain(d->ctx, 1.0);
    d->setCenterFrequency(d->ctx, 88700000.0);
    trace("starting");
    int rc = pthread_create(&thread, NULL, sdrReaderThread, (void *)sdr);
    if (rc)
        {
        error("ERROR; return code from pthread_create() is %d", rc);
        return FALSE;
        }
    trace("started");
    sdr->thread = thread;
    sdr->running = 1;
    return TRUE;
}


/**
 */   
int sdrStop(SdrLib *sdr)
{
    if (!sdr->running)
        return TRUE;
    void *status;
    pthread_join(sdr->thread, &status);
    Device *d = sdr->device;
    if (d)
        {
        d->close(d->ctx);
        sdr->device = NULL;
        }
    sdr->running = 0;
    return TRUE;
}

/**
 */   
double sdrGetCenterFrequency(SdrLib *sdr)
{
    Device *d = sdr->device;
    return (d) ? d->getCenterFrequency(d->ctx) : 0.0;
}


/**
 */   
int sdrSetCenterFrequency(SdrLib *sdr, double freq)
{
    Device *d = sdr->device;
    return (d) ? d->setCenterFrequency(d->ctx, freq) : 0;
}

/**
 */   
void sdrSetDdcFreqs(SdrLib *sdr, float vfo, float pbLo, float pbHi)
{
    Ddc *ddc = sdr->ddc;
    ddcSetFreqs(ddc, vfo, ddc->pbLo, ddc->pbHi);
    float rate = ddcGetOutRate(ddc);
    trace("if rate: %f", rate);
    resamplerSetInRate(sdr->resampler, rate);
}


/**
 */   
void sdrSetVfo(SdrLib *sdr, float vfo)
{
    Ddc *ddc = sdr->ddc;
    sdrSetDdcFreqs(sdr, vfo, ddc->pbLo, ddc->pbHi);
}


/**
 */   
float sdrGetVfo(SdrLib *sdr)
{
    return sdr->ddc->vfo;
}

/**
 */   
void sdrSetPbLo(SdrLib *sdr, float pbLo)
{
    Ddc *ddc = sdr->ddc;
    sdrSetDdcFreqs(sdr, ddc->vfo, pbLo, ddc->pbHi);
}


/**
 */   
float sdrGetPbLo(SdrLib *sdr)
{
    return sdr->ddc->pbLo;
}

/**
 */   
void sdrSetPbHi(SdrLib *sdr, float pbHi)
{
    Ddc *ddc = sdr->ddc;
    sdrSetDdcFreqs(sdr, ddc->vfo, ddc->pbLo, pbHi);
}


/**
 */   
float sdrGetPbHi(SdrLib *sdr)
{
    return sdr->ddc->pbHi;
}

/**
 */   
float sdrGetSampleRate(SdrLib *sdr)
{
    Device *d = sdr->device;
    return (d) ? d->getSampleRate(d->ctx) : 0.0;
}



/**
 */   
int sdrSetSampleRate(SdrLib *sdr, float rate)
{
    Device *d = sdr->device;
    return (d) ? d->setSampleRate(d->ctx, rate) : 0;
}



/**
 */   
float sdrGetRfGain(SdrLib *sdr)
{
    Device *d = sdr->device;
    return (d) ? d->getGain(d->ctx) : 0.0;
}



/**
 */   
int sdrSetRfGain(SdrLib *sdr, float gain)
{
    Device *d = sdr->device;
    return (d) ? d->setGain(d->ctx, gain) : 0;
}


/**
 */   
float sdrGetAfGain(SdrLib *sdr)
{
    Audio *a = sdr->audio;
    return (a) ? audioGetGain(a) : 0.0;
}



/**
 */   
int sdrSetAfGain(SdrLib *sdr, float gain)
{
    Audio *a = sdr->audio;
    return (a) ? audioSetGain(a, gain) : 0;
}


/**
 * Get current demodulation Mode
 * @param sdrlib an SDRLib instance.
 */   
int sdrGetMode(SdrLib *sdr)
{
    return sdr->mode;
}



/**
 * Set current demodulation Mode
 * @param sdrlib an SDRLib instance.
 */   
int sdrSetMode(SdrLib *sdr, Mode mode)
{
    int ret = TRUE;
    switch (mode)
        {
        case MODE_NULL:
            sdr->demod = sdr->demodNull;
            break;
        case MODE_AM:
            sdr->demod = sdr->demodAm;
            break;
        case MODE_FM:
            sdr->demod = sdr->demodFm;
            break;
        case MODE_LSB:
            sdr->demod = sdr->demodLsb;
            break;
        case MODE_USB:
            sdr->demod = sdr->demodUsb;
            break;
        default:
            error("Unhandled mode: %d", mode);
            ret = FALSE;
        }
    return ret;
}


/**
 * Determine if we want speaker output
 * @param sdrlib an SDRLib instance.
 * @param enabled 0 to disable, !=0 enabled
 */   
void sdrEnableAudio(SdrLib *sdr, int enabled)
{
    sdr->audioEnabled = enabled;   
}




/*############################################################################
## R E A D E R    T H R E A D
############################################################################*/



#define READSIZE (8 * 16384)

static void fftOutput(unsigned int *vals, int size, void *ctx)
{
    SdrLib *sdr = (SdrLib *)ctx;
    UintOutputFunc *psFunc = sdr->psFunc;
    if (psFunc)
        (*psFunc)(vals, size, sdr->context);
}


static void resamplerOutput(float *buf, int size, void *ctx)
{
    SdrLib *sdr = (SdrLib *)ctx;
    //trace("Push audio:%d", size);
    if (sdr->audioEnabled)
        audioPlay(sdr->audio, buf, size);
    if (sdr->codecFunc)
        codecEncode(sdr->codec, buf, size, sdr->codecFunc, sdr->context);
}


static void demodOutput(float *buf, int size, void *ctx)
{
    SdrLib *sdr = (SdrLib *)ctx;
    //trace("Demod:%d", size);
    resamplerUpdate(sdr->resampler, buf, size, resamplerOutput, sdr);
}

static void ddcOutput(float complex *data, int size, void *ctx)
{
    SdrLib *sdr = (SdrLib *)ctx;
    //trace("Ddc:%d", size);
    sdr->demod->update(sdr->demod, data, size, demodOutput, sdr);
}

static void *sdrReaderThread(void *ctx)
{
    SdrLib *sdr = (SdrLib *)ctx;
    Device *dev = sdr->device;
    
    int bufsize = 1024*1024;
    float complex *readbuf = (float complex *)malloc(bufsize * sizeof(float complex));
    
    sdr->running = 1;
    
    while (sdr->running && dev->isOpen(dev->ctx))
        {
        int readCount = dev->read(dev->ctx, readbuf, bufsize);
        if (readCount)
            {
            fftUpdate(sdr->fft, readbuf, readCount, fftOutput, sdr);
            ddcUpdate(sdr->ddc, readbuf, readCount, ddcOutput, sdr);
            }
        else
            {
            sched_yield();
            }
        }

    free(readbuf);
    sdr->running = 0;
    return NULL;
}





