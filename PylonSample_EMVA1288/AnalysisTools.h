// AnalysisTools.h
// Basic Image Analysis functions.
//
// Copyright (c) 2022 Matthew Breit - matt.breit@baslerweb.com or matt.breit@gmail.com
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef ANALYSISTOOLS_H
#define ANALYSISTOOLS_H

#ifndef LINUX_BUILD
#define WIN_BUILD
#endif

#ifdef WIN_BUILD
#define _CRT_SECURE_NO_WARNINGS // suppress fopen_s warnings for convinience (if this header included first)
#endif

// We will use with Pylon
#include <pylon/PylonIncludes.h>

#include <cstdio>
#include <string>
#include <stdint.h>

namespace AnalysisTools
{
	uint32_t FindAvg(Pylon::CPylonImage& image);

	uint32_t FindMin(Pylon::CPylonImage& image);

	uint32_t FindMax(Pylon::CPylonImage& image);

	double FindSNR(Pylon::CPylonImage& image);
}

// *********************************************************************************************************
inline uint32_t AnalysisTools::FindAvg(Pylon::CPylonImage& image)
{
	uint32_t sum = 0;
	uint32_t avg = 0;
	uint8_t* pImage = (uint8_t*)image.GetBuffer();

	for (uint32_t i = 0; i < image.GetImageSize(); i++)
	{
		sum = sum + (uint32_t)pImage[i];
	}
	avg = sum / image.GetImageSize();

	return avg;
}

inline uint32_t AnalysisTools::FindMin(Pylon::CPylonImage& image)
{
	uint32_t min = UINT32_MAX;
	uint8_t* pImage = (uint8_t*)image.GetBuffer();

	for (uint32_t i = 0; i < image.GetImageSize(); i++)
	{
		if ((uint32_t)pImage[i] < min)
			min = pImage[i];
	}

	return min;
}

inline uint32_t AnalysisTools::FindMax(Pylon::CPylonImage& image)
{
	uint32_t max = 0;
	uint8_t* pImage = (uint8_t*)image.GetBuffer();
	for (uint32_t i = 0; i < image.GetImageSize(); i++)
	{
		if ((uint32_t)pImage[i] > max)
			max = pImage[i];
	}

	return max;
}

inline double AnalysisTools::FindSNR(Pylon::CPylonImage& image)
{
	double var = 0;
	double stddev = 0;
	double snr = 0;
	uint32_t mean = 0;
	uint8_t* pImage = (uint8_t*)image.GetBuffer();

	mean = AnalysisTools::FindAvg(image);

	for (uint32_t i = 0; i < image.GetImageSize(); i++)
	{
		var = var + (((uint32_t)pImage[i] - mean) * ((uint32_t)pImage[i] - mean));
	}

	var = var / image.GetImageSize();
	stddev = sqrt(var);

	if (stddev == 0)
		snr = 0;
	else
		snr = mean / stddev;

	if (snr > 255)
		snr = 255;

	return snr;
}
// *********************************************************************************************************
#endif