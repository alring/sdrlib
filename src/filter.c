/**
 * Various filter definitions and implementations.
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

#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <math.h>
#include "private.h"
#include "filter.h"



//########################################################################
//#  F I R    F I L T E R S
//########################################################################


Fir *firCreate(int size)
{
    Fir *fir = (Fir *)malloc(sizeof(Fir));
    if (!fir)
        return NULL;
    fir->size       = size;
    fir->coeffs     = (float *)malloc(size * sizeof(float));
    fir->delayLine  = (float *)malloc(size * sizeof(float));
    fir->delayLineC = (float complex *)malloc(size * sizeof(float complex));
    if (!fir->coeffs || !fir->delayLine || !fir->delayLineC)
        {
        free(fir->coeffs);
        free(fir->delayLine);
        free(fir->delayLineC);
        free(fir);
        return NULL;
        }
    int i = 0;
    for ( ; i<size ; i++)
        {
        fir->coeffs[i] = 0.0;
        fir->delayLine[i] = 0.0;
        fir->delayLineC[i] = 0.0;
        }
    fir->delayIndex = 0;
    return fir;
}



void firDelete(Fir *fir)
{
    if (fir)
        {
        free(fir->coeffs);
        free(fir->delayLine);
        free(fir->delayLineC);
        free(fir);
        }
}


float firUpdate(Fir *fir, float sample)
{
    float *delayLine = fir->delayLine;
    int delayIndex = fir->delayIndex;
    delayLine[delayIndex] = sample;
    float sum = 0.0;
    //walk the coefficients from first to last
    //and the delay line from newest to oldest
    int idx = delayIndex;
    float *coeff = fir->coeffs;
    int size = fir->size;
    int count = size;
    while (count--)
        {
        float v = delayLine[idx];
        idx--;
        if (idx < 0)
            idx = size - 1;
        sum += v * (*coeff++);
        }
    fir->delayIndex = (delayIndex + 1) % size;
    return sum;
}




float complex firUpdateC(Fir *fir, float complex sample)
{
    float complex *delayLine = fir->delayLineC;
    int delayIndex = fir->delayIndex;
    delayLine[delayIndex] = sample;
    float complex sum = 0.0;
    //walk the coefficients from first to last
    //and the delay line from newest to oldest
    int idx = delayIndex;
    float *coeff = fir->coeffs;
    int size = fir->size;
    int count = size;
    while (count--)
        {
        float complex v = delayLine[idx];
        idx--;
        if (idx < 0)
            idx = size - 1;
        sum += v * (*coeff++);
        }
    fir->delayIndex = (delayIndex + 1) % size;
    return sum;
}


static void windowize(int size, float *coeffs, int windowType)
{
    int i = 0;
    switch (windowType)
        {
        case W_HAMMING: 
            {
            for ( ; i<size ; i++)
                coeffs[i] *= 0.54 - 0.46 * cos(TWOPI * i / (size-1));
            break;
            }
        case W_HANN: 
            {
            for ( ; i<size ; i++)
                coeffs[i] *= 0.5 - 0.5 * cos(TWOPI * i / (size-1));
            break;
            }
        case W_BLACKMAN: 
            {
            for ( ; i<size ; i++)
                coeffs[i] *= 
                    (
                    0.42 -
                    0.5 * cos(TWOPI * i / (size-1)) +
                    0.08 * cos(4.0 * PI * i / (size-1))
                    );
            break;
            }
        default: 
            {
            }
        }
}


static void normalize(int size, float *coeffs)
{
    float sum = 0.0;
    int i = 0;
    for ( ; i < size ; i++)
        sum += coeffs[i];
    float scale = 1.0 / sum;
    for (i=0 ; i < size ; i++)
        sum *= scale;
    
}




static void firLPCoeffs(int size, float *coeffs, float cutoffFreq, float sampleRate)
{
    float omega = 2.0 * PI * cutoffFreq / sampleRate;
    int center = (size - 1) / 2;
    int idx = 0;
    for ( ; idx < size ; idx++)
        {
        int i = idx - center;
        coeffs[idx] = (i == 0) ? omega / PI : sin(omega * i) / (PI * i);
        }
}

Fir *firLP(int size, float cutoffFreq, float sampleRate, int windowType)
{
    Fir *fir = firCreate(size);
    firLPCoeffs(size, fir->coeffs, cutoffFreq, sampleRate);
    windowize(size, fir->coeffs, windowType);
    normalize(size, fir->coeffs);
    return fir;
}


static void firHPCoeffs(int size, float *coeffs, float cutoffFreq, float sampleRate)
{
    float omega = 2.0 * PI * cutoffFreq / sampleRate;
    int center = (size - 1) / 2;
    int i = 0;
    for ( ; i < size ; i++)
        coeffs[i] = (i == center) ? 1.0 - omega / PI : -sin(omega * i) / (PI * i);
}

Fir *firHP(int size, float cutoffFreq, float sampleRate, int windowType)
{
    Fir *fir = firCreate(size);
    firHPCoeffs(size, fir->coeffs, cutoffFreq, sampleRate);
    windowize(size, fir->coeffs, windowType);
    normalize(size, fir->coeffs);
    return fir;
}


static void firBPCoeffs(int size, float *coeffs, float loCutoffFreq, float hiCutoffFreq, float sampleRate)
{
    float omega1 = 2.0 * PI * loCutoffFreq / sampleRate;
    float omega2 = 2.0 * PI * hiCutoffFreq / sampleRate;
    int center = (size - 1) / 2;
    int i = 0;
    for ( ; i < size ; i++)
        coeffs[i] = (i == center) ? 
            1.0 - (omega2-omega1) / PI :
            (sin(omega1*i) - sin(omega2 * i)) / (PI * i);
}

Fir *firBP(int size, float loCutoffFreq, float hiCutoffFreq, float sampleRate, int windowType)
{
    Fir *fir = firCreate(size);
    firBPCoeffs(size, fir->coeffs, loCutoffFreq, hiCutoffFreq, sampleRate);
    windowize(size, fir->coeffs, windowType);
    normalize(size, fir->coeffs);
    return fir;
}



//########################################################################
//#  D E C I M A T O R
//########################################################################

Decimator *decimatorCreate(int size, float highRate, float lowRate)
{
    Decimator *dec = (Decimator *)malloc(sizeof(Decimator));
    dec->size = size;
    dec->coeffs = (float *)malloc(size * sizeof(float));
    firLPCoeffs(size, dec->coeffs, lowRate, highRate);
    dec->delayLine = (float complex *)malloc(size * sizeof(float complex));
    int i = 0;
    for (; i < size ; i++)
        dec->delayLine[i] = 0;
    dec->delayIndex = 0;
    dec->ratio = lowRate/highRate;
    dec->acc = 0.0;
    dec->bufPtr = 0;
    return dec;
}


void decimatorDelete(Decimator *dec)
{
    free(dec->delayLine);
    free(dec->coeffs);
    free(dec);
}

void decimatorUpdate(Decimator *dec, float complex *data, int dataLen, DecimatorFunc *func, void *context)
{
    int   size         = dec->size;
    float *coeffs      = dec->coeffs;
    float complex *delayLine = dec->delayLine;
    int   delayIndex   = dec->delayIndex;
    float ratio        = dec->ratio;
    float acc          = dec->acc;
    float complex *buf = dec->buf;
    int   bufPtr       = dec->bufPtr;
    
    float complex *cpx = data;
    while (dataLen--)
        {
        delayLine[delayIndex] = *cpx++;
        acc += ratio;
        if (acc > 0.0)
            {
            acc -= 1.0;
            float complex sum = 0.0;
            //walk the coefficients from first to last
            //and the delay line from newest to oldest
            int idx = delayIndex;
            float *coeff = coeffs; 
            int c = size;
            while (c--)
                {
                float complex v = delayLine[idx];
                idx -= 1;
                if (idx < 0)
                    idx = size - 1;
                sum += v * (*coeff++);
                }
            //trace("sum:%f", sum * 1000.0);
            buf[bufPtr++] = sum;
            if (bufPtr >= DECIMATOR_BUFSIZE)
                {
                func(buf, bufPtr, context);
                bufPtr = 0;
                }
            }
        delayIndex = (delayIndex + 1) % size;
        }
    dec->delayIndex = delayIndex + 1;
    dec->acc = acc;
    dec->bufPtr = bufPtr;
}


//########################################################################
//#  B I Q U A D
//########################################################################

static Biquad *biquadCreate()
{
    Biquad *bq = (Biquad *)malloc(sizeof(Biquad));
    memset(bq, 0, sizeof(Biquad));
    return bq;
}

void biquadDelete(Biquad *bq)
{
    free(bq);
}

float biquadUpdate(Biquad *bq, float v)
{
   float y = v * bq->a0 + bq->x1 * bq->a1 + bq->x2 * bq->a2 - bq->y1 * bq->b1 - bq->y2 * bq->b2;
   bq->x2 = bq->x1 ; bq->x1 = v ; bq->y2 = bq->y1 ; bq->y1 = y;
   return y;
}

float complex biquadUpdateC(Biquad *bq, float complex v)
{
   float y = v * bq->a0 + bq->x1c * bq->a1 + bq->x2c * bq->a2 - bq->y1c * bq->b1 - bq->y2c * bq->b2;
   bq->x2c = bq->x1c ; bq->x1c = v ; bq->y2c = bq->y1c ; bq->y1c = y;
   return y;
}

Biquad *biquadLP(float frequency, float sampleRate, float q)
{
    Biquad *bq = biquadCreate();
    if (q == 0) q = 0.707;
    float freq = TWOPI * frequency / sampleRate;
    float alpha = sin(freq) / (2.0 * q);
    bq->b0 = (1.0 - cos(freq)) / 2.0;
    bq->b1 =  1.0 - cos(freq);
    bq->b2 = (1.0 - cos(freq)) / 2.0;
    bq->a0 =  1.0 + alpha;
    bq->a1 = -2.0 * cos(freq);
    bq->a2 =  1.0 - alpha;
    return bq;
}

Biquad *biquadHP(float frequency, float sampleRate, float q)
{
    Biquad *bq = biquadCreate();
    if (q == 0) q = 0.707;
    float freq = TWOPI * frequency / sampleRate;
    float alpha = sin(freq) / (2.0 * q);
    bq->b0 = (1.0 + cos(freq)) / 2.0;
    bq->b1 =  1.0 + cos(freq);
    bq->b2 = (1.0 + cos(freq)) / 2.0;
    bq->a0 =  1.0 + alpha;
    bq->a1 = -2.0 * cos(freq);
    bq->a2 =  1.0 - alpha;
    return bq;
}

Biquad *biquadBP(float frequency, float sampleRate, float q)
{
    Biquad *bq = biquadCreate();
    if (q == 0) q = 0.707;
    float freq = TWOPI * frequency / sampleRate;
    float alpha = sin(freq) / (2.0 * q);
    bq->b0 = sin(freq) / 2.0;
    bq->b1 =  0.0;
    bq->b2 = -sin(freq) / 2.0;
    bq->a0 =  1.0 + alpha;
    bq->a1 = -2.0 * cos(freq);
    bq->a2 =  1.0 - alpha;
    return bq;
}

Biquad *biquadBR(float frequency, float sampleRate, float q)
{
    Biquad *bq = biquadCreate();
    if (q == 0) q = 0.707;
    float freq = TWOPI * frequency / sampleRate;
    float alpha = sin(freq) / (2.0 * q);
    bq->b0 =  1.0;
    bq->b1 = -2.0 * cos(freq);
    bq->b2 =  1.0;
    bq->a0 =  1.0 + alpha;
    bq->a1 = -2.0 * cos(freq);
    bq->a2 =  1.0 - alpha;
    return bq;
}



    







