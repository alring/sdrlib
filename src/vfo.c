/**
 * VFO for tuning around the center frequency
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
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "vfo.h"
#include "private.h"




Vfo *vfoCreate(float frequency, float sampleRate)
{
    Vfo *vfo = (Vfo *)malloc(sizeof(Vfo));
    if (!vfo)
        return NULL;
    vfo->sampleRate = sampleRate;
    vfo->phase = 0.0 + I * 1.0;
    vfoSetFrequency(vfo, frequency);
    return vfo;
}

void vfoDelete(Vfo *vfo)
{
    free(vfo);
}

void vfoSetFrequency(Vfo *vfo, float frequency)
{
    float angle = TWOPI * frequency / vfo->sampleRate;
    vfo->freq = cos(angle) + I * sin(angle);
}

float complex vfoUpdate(Vfo *vfo, float complex sample)
{
    vfo->phase *= vfo->freq;
    return sample * vfo->phase;
}






