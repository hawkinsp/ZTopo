/*
  ZTopo --- a viewer for topographic maps
  Copyright (C) 2010 Peter Hawkins
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef CONST_H
#define CONST_H 1

#include <cmath>

static const qreal epsilon = 0.00001;
static const qreal metersPerInch = 0.0254;
static const qreal metersPerMile = 1609.344;
static const qreal minutesPerDegree = 60.0;
static const qreal degreesPerMinute = 1/60.0;
static const qreal degreesPerRadian = 180.0 / M_PI;
static const QChar degree(0x00b0);
static const unsigned int bytesPerMb = 1 << 20;

#endif
